-- Spotify Downloader — a Kalorite plugin.
--
-- Paste a Spotify *track* link and the plugin fetches the matching audio and
-- adds the downloaded MP3 to the player's playlist.
--
-- How it works (and why it works this way)
-- ----------------------------------------
-- The public site https://spotubedl.com/ downloads Spotify tracks like this:
--   1. resolve the Spotify track's metadata (title + artist),
--   2. source the audio from YouTube through a signing backend
--      (yt-dl.click / ezsrv.net / …),
--   3. hand the browser a short-lived, *server-signed* tunnel URL
--      (HMAC `sig` / an HS256 JWT with `exp`), which is then streamed through
--      an open `/api/download/audiorelay?url=…` CORS proxy.
-- Step 2's request (`POST /api/download`) is sealed inside an ECDH-P256 +
-- AES-GCM envelope sent to a randomized path, and every media URL it returns
-- carries an expiring signature we cannot forge. So we cannot reuse their
-- backend from a plugin.
--
-- Instead we do the exact same thing they do, locally: read the track's
-- metadata straight from Spotify's public embed page (no auth), then let
-- `yt-dlp` + `ffmpeg` fetch the matching YouTube audio and transcode it. The
-- output format is selectable (MP3 / FLAC / OGG-Vorbis) via the plugin menu;
-- all three decode natively in Kalorite's miniaudio backend (which handles
-- mp3/flac/wav/ogg, but not m4a/opus). The finished file is added to the
-- playlist. Because the source is lossy YouTube audio, FLAC output is a
-- lossless *container*, not true lossless.
--
-- Requirements: `yt-dlp` and `ffmpeg` on PATH. If `yt-dlp` is missing, the
-- plugin offers to download the standalone binary into Kalorite's data dir.

local plugin = {
    name        = "Spotify Downloader",
    version     = "1.0.0",
    author      = "monsler",
    description = "Add a Spotify track to the playlist by link (via yt-dlp).",
}

-- Small helpers ------------------------------------------------------------

local function file_exists(p)
    local f = io.open(p, "r"); if f then f:close(); return true end
    return false
end

-- Single-quote a string for safe inclusion in a /bin/sh command line.
local function shq(s)
    return "'" .. tostring(s):gsub("'", "'\\''") .. "'"
end

-- Running commands ---------------------------------------------------------
--
-- yt-dlp and ffmpeg are the *user's* tools, not part of Kalorite, so we never
-- bundle them. We run every external command through `kalorite.sys.run`, which
-- transparently forwards to the host via `flatpak-spawn --host` when Kalorite
-- is sandboxed — so we don't have to care whether we're in Flatpak. Finished
-- files go to ~/Downloads, which Flatpak shares into the sandbox at the same
-- path, so the player can read them back.
local IN_FLATPAK = kalorite.sys.is_sandboxed()

-- Run a shell command and return its combined stdout+stderr.
local function run(cmd) return (kalorite.sys.run(cmd)) or "" end

-- Fire-and-forget command whose output we don't care about.
local function exec(cmd) kalorite.sys.run(cmd) end

-- Paths --------------------------------------------------------------------

-- Home directory — resolved via the command channel, so it is the *host* home
-- when sandboxed (paths handed to host tools are valid there and stay visible
-- inside the sandbox).
local function home()
    local h = run('printf %s "$HOME"'):gsub("%s+$", "")
    return h ~= "" and h or (os.getenv("HOME") or ".")
end

-- ~/Downloads/Kalorite holds the finished files (shared with the sandbox).
local function downloads_dir()
    local d = home() .. "/Downloads/Kalorite"
    exec("mkdir -p " .. shq(d))
    return d
end

-- Where a self-bootstrapped yt-dlp binary lives (non-Flatpak only).
local function bundled_ytdlp() return home() .. "/.local/share/kalorite/bin/yt-dlp" end

local YTDLP_URL =
    "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp"

-- Locate a usable yt-dlp: PATH first, then a bootstrapped copy (native only).
local function find_ytdlp()
    local p = run("command -v yt-dlp"):gsub("%s+$", "")
    if p ~= "" then return p end
    if not IN_FLATPAK and file_exists(bundled_ytdlp()) then
        return bundled_ytdlp()
    end
    return nil
end

-- Spotify -----------------------------------------------------------------

-- Extract the 22-char base62 track id from any Spotify track link/URI, or a
-- bare id.  Returns nil for non-track links (album/playlist/artist).
local function track_id(s)
    s = (s or ""):gsub("%s+", "")
    local id = s:match("track[/:]([%w]+)")
    if id then return id end
    if s:match("^[%w]+$") and #s == 22 then return s end
    return nil
end

-- A minimal \uXXXX unescaper so titles like "Beyoncé" read correctly.
local function unescape_json(s)
    if not s then return s end
    s = s:gsub("\\\"", "\""):gsub("\\/", "/")
    s = s:gsub("\\u(%x%x%x%x)", function(h)
        local n = tonumber(h, 16)
        if n < 0x80 then return string.char(n) end
        if n < 0x800 then
            return string.char(0xC0 + math.floor(n / 0x40),
                               0x80 + (n % 0x40))
        end
        return string.char(0xE0 + math.floor(n / 0x1000),
                           0x80 + (math.floor(n / 0x40) % 0x40),
                           0x80 + (n % 0x40))
    end)
    return s
end

-- Fetch {title, artist} from Spotify's public embed page (no auth). Async;
-- calls cb(title, artist) — either may be nil on failure.
local function fetch_meta(id, cb)
    local url = "https://open.spotify.com/embed/track/" .. id
    kalorite.net.get(url, function(err, body, status)
        if err or not body or body == "" then cb(nil, nil); return end
        local title  = body:match('"title"%s*:%s*"(.-[^\\])"')
        local artist = body:match('"artists":%[%s*{%s*"name"%s*:%s*"(.-[^\\])"')
        cb(unescape_json(title), unescape_json(artist))
    end)
end

-- Download ----------------------------------------------------------------

-- Output formats we offer. Each maps the user-facing key to yt-dlp's
-- --audio-format value and the resulting file extension. All three decode
-- natively in Kalorite's miniaudio backend.
--
-- Reminder: the audio source is YouTube (lossy Opus/AAC). "flac"/"ogg" are
-- transcodes of that, so FLAC here is lossless-container, not true lossless.
local FORMATS = {
    mp3  = { af = "mp3",    ext = "mp3" },
    flac = { af = "flac",   ext = "flac" },
    ogg  = { af = "vorbis", ext = "ogg" },  -- Vorbis, not Opus (miniaudio)
}

-- The persisted preferred format ("mp3" | "flac" | "ogg"), defaulting to mp3.
local function current_format()
    local f = kalorite.storage.get("format", "mp3")
    return FORMATS[f] and f or "mp3"
end

-- Run yt-dlp for `query`, extracting the best audio in the preferred format
-- into the downloads dir. Asynchronous (kalorite.sys.spawn) so the UI stays
-- responsive during the download; calls done(path_or_nil, full_log).
local function download_track(yt, query, done)
    local fmt = FORMATS[current_format()]
    local template = downloads_dir() .. "/%(title)s [%(id)s].%(ext)s"
    local cmd = table.concat({
        shq(yt),
        "--no-playlist", "--newline",
        "-x", "--audio-format", fmt.af, "--audio-quality", "0",
        "--embed-thumbnail", "--add-metadata",
        "-o", shq(template),
        "--print", "after_move:filepath",
        shq("ytsearch1:" .. query),
    }, " ")

    local chunks = {}
    local want = "%." .. fmt.ext .. "$"
    kalorite.sys.spawn(cmd, {
        on_output = function(chunk)
            chunks[#chunks + 1] = chunk
            -- Surface yt-dlp's progress percentage in the status bar.
            local pct = chunk:match("(%d+%.?%d*)%%")
            if pct then kalorite.ui.notify("Downloading… " .. pct .. "%") end
        end,
        on_done = function(_code)
            local log = table.concat(chunks)
            -- The final path is whatever `--print after_move:filepath` emitted;
            -- pick the last line that is an existing file of the expected
            -- extension so stray log lines don't fool us.
            local path
            for line in log:gmatch("[^\r\n]+") do
                if line:match(want) and file_exists(line) then path = line end
            end
            done(path, log)
        end,
    })
end

-- Orchestrate metadata → download → playlist for a resolved yt-dlp binary.
local function run_pipeline(yt, id)
    kalorite.ui.notify("Fetching track info…")
    fetch_meta(id, function(title, artist)
        local query = ((title or "") .. " " .. (artist or "")):gsub("^%s+", "")
                                                               :gsub("%s+$", "")
        if query == "" then query = id end -- fall back to the bare id

        kalorite.log.info("Spotify Downloader: query = " .. query)
        kalorite.ui.notify(('Downloading "%s" as %s…')
            :format(query, current_format():upper()))

        -- Runs asynchronously; the UI stays responsive until on_done fires.
        download_track(yt, query, function(path, log)
            if not path then
                kalorite.ui.error("Download failed.\n\n" ..
                    (log ~= "" and log:sub(-800) or "yt-dlp produced no output.") ..
                    "\n\n(Is ffmpeg installed?)")
                return
            end
            kalorite.playlist.add(path)
            kalorite.ui.message("Added to playlist:\n" .. path)
            kalorite.log.info("Spotify Downloader: added " .. path)
        end)
    end)
end

-- Ensure yt-dlp exists, bootstrapping the standalone binary on request, then
-- invoke cb(ytdlp_path).
local function with_ytdlp(cb, id)
    local yt = find_ytdlp()
    if yt then cb(yt, id); return end

    if IN_FLATPAK then
        kalorite.ui.error(
            "yt-dlp could not be reached.\n\n" ..
            "Kalorite runs in a Flatpak sandbox and uses your host's yt-dlp " ..
            "and ffmpeg (nothing is bundled). Two steps:\n\n" ..
            "1) Install them on the host:\n" ..
            "   sudo <your-package-manager> install yt-dlp ffmpeg\n\n" ..
            "2) Allow this app to call host tools (not granted by default, as " ..
            "Flathub forbids it):\n" ..
            "   flatpak override --user \\\n" ..
            "     --talk-name=org.freedesktop.Flatpak io.github.monsler.Kalorite\n\n" ..
            "then restart Kalorite and try again.")
        return
    end

    if not kalorite.ui.confirm(
        "yt-dlp is required to download tracks but was not found.\n\n" ..
        "Download the standalone yt-dlp binary now?\n" ..
        "(Requires python3 and ffmpeg to be installed.)") then
        return
    end

    os.execute('mkdir -p "' .. home() .. '/.local/share/kalorite/bin"')
    local dest = bundled_ytdlp()
    kalorite.net.download(YTDLP_URL, dest, {
        on_progress = function(recv, total)
            if total > 0 then
                kalorite.ui.notify(("Downloading yt-dlp… %d%%")
                    :format(math.floor(recv / total * 100)))
            end
        end,
        on_done = function(err, path)
            if err then
                kalorite.ui.error("Could not download yt-dlp: " .. err)
                return
            end
            os.execute('chmod +x "' .. path .. '"')
            cb(path, id)
        end,
    })
end

-- Entry point: prompt for a link (or accept one) and kick off the pipeline.
local function download_from_link(url)
    if not url or url == "" then
        url = kalorite.ui.input("Spotify track link:")
        if not url then return end
    end
    local id = track_id(url)
    if not id then
        kalorite.ui.error("That doesn't look like a Spotify *track* link.\n" ..
            "Expected e.g. https://open.spotify.com/track/XXXXXXXXXXXXXXXXXXXXXX")
        return
    end
    with_ytdlp(run_pipeline, id)
end

-- Lifecycle & menu ---------------------------------------------------------

function plugin.on_load()
    kalorite.log.info("Spotify Downloader loaded")
end

-- Persist a new preferred format and confirm it in the status bar.
local function set_format(key)
    kalorite.storage.set("format", key)
    kalorite.ui.notify("Download format set to " .. key:upper())
end

plugin.menu = {
    {
        title  = "Download from Spotify link…",
        action = function() download_from_link(nil) end,
    },
    {
        title  = "Format: MP3",
        action = function() set_format("mp3") end,
    },
    {
        title  = "Format: FLAC (lossless container)",
        action = function() set_format("flac") end,
    },
    {
        title  = "Format: OGG (Vorbis)",
        action = function() set_format("ogg") end,
    },
    {
        title  = "Show current format",
        action = function()
            kalorite.ui.message("Current download format: " ..
                current_format():upper())
        end,
    },
}

return plugin
