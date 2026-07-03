# Kalorite Plugin API (LuaJIT)

Plugins are LuaJIT scripts that extend Kalorite. Add one via **File → Add
Plugin**; it is copied into the per-user plugins directory and loaded on every
launch:

- Linux: `~/.local/share/kalorite/plugins/`
- macOS: `~/Library/Application Support/kalorite/plugins/`

Each plugin runs in its own isolated `lua_State`, so an error in one plugin
never affects the others or the player. Errors (load-time or runtime) are shown
in a Qt dialog titled *Error in plugin "<name>"* with a full traceback.

This is a **trusted** model: the full LuaJIT standard library (`io`, `os`,
`ffi`, `require`, …) is available in addition to the `kalorite.*` API below.

## Plugin shape

A plugin script must `return` a table:

```lua
local plugin = {
    name = "My Plugin", version = "1.0.0",
    author = "you", description = "what it does",
}

function plugin.on_load()    end   -- after the script loads
function plugin.on_unload()  end   -- on player shutdown
function plugin.on_enable()  end   -- toggled on in the Plugins menu
function plugin.on_disable() end   -- toggled off

-- Player events (only delivered while the plugin is enabled):
function plugin.on_track_changed(track) end   -- track = {path, title, index}
function plugin.on_playback_state(state) end  -- "playing"|"paused"|"stopped"
function plugin.on_track_finished(path) end

-- Context-menu items shown under Plugins → <name>:
plugin.menu = {
    { title = "Do thing", action = function() ... end },
}

return plugin
```

The **Plugins** menu sits right after **View**. Hover a plugin to get its
submenu: its declared menu actions, an *About* entry, and a *Delete* entry that
removes the plugin (its `.lua` file and stored settings are deleted).

## API reference — the global `kalorite` table

### `kalorite.player`
| Call | Returns |
|------|---------|
| `play()` `pause()` `stop()` `next()` `prev()` | – |
| `is_playing()` | bool |
| `position_ms()` / `duration_ms()` | int |
| `seek_ms(ms)` | – |
| `current_track()` | `{path, title, index}` or `nil` |

### `kalorite.mixer`
| Call | Notes |
|------|-------|
| `get_volume()` / `set_volume(0..100)` | |
| `set_eq_band(i, gain_db)` | `i` = 0..9 |
| `set_eq_enabled(bool)` | |
| `set_crossfade(enabled [, seconds])` | |
| `get_device()` / `set_device(name)` | output device by name |

### `kalorite.playlist`
`count()`, `add(path)`, `remove(index)`, `clear()`, `get(index)` →
`{path,title,index}`, `current_index()`, `play_index(index)`.

### `kalorite.net` (asynchronous — callbacks fire on the GUI thread)
```lua
kalorite.net.get(url, function(err, body, status) end)
kalorite.net.download(url, dest_path, {
    on_progress = function(received, total) end,
    on_done     = function(err, path) end,
})
```

### `kalorite.ui`
`message(text)`, `error(text)`, `confirm(text)`→bool, `input(prompt [,default])`
→string|nil, `notify(text)` (status bar).

### `kalorite.storage` (persisted per plugin, scalar values)
`set(key, value)` / `get(key [, default])`. Values may be string, number,
boolean or nil.

### `kalorite.app`
| Call | Returns |
|------|---------|
| `language()` | full UI locale, e.g. `"ru_RU"` |
| `language_code()` | ISO language code, e.g. `"ru"` |
| `version()` | application version string |

### `kalorite.sys`
Run external commands. In a Flatpak sandbox these are transparently forwarded
to the host via `flatpak-spawn --host`, so a plugin can call host tools
(`yt-dlp`, `ffmpeg`, …) without caring whether it is sandboxed.

| Call | Returns |
|------|---------|
| `run(command)` | `output` (combined stdout+stderr string), `exit_code` (int) |
| `spawn(command, {on_output, on_done})` | – (asynchronous) |
| `is_sandboxed()` | bool — true inside Flatpak |

`command` is passed to `/bin/sh -c`. **`run` is synchronous and blocks the GUI
thread** for the command's duration — fine for quick commands. For anything
slow (downloads, transcodes) use `spawn`, which is non-blocking:

```lua
kalorite.sys.spawn(cmd, {
    on_output = function(chunk) end,   -- combined stdout+stderr, may fire often
    on_done   = function(exit_code) end,  -- once; -1 if the process crashed
})
```

Callbacks run on the GUI thread, so they may freely use the rest of the
`kalorite.*` API. Files you want the player to read back should be written
somewhere shared with the sandbox (e.g. `~/Downloads`, granted by
`--filesystem=xdg-download`).

**Flatpak note:** running host tools requires `flatpak-spawn --host`, which
needs the `org.freedesktop.Flatpak` talk-name. Flathub forbids shipping that
permission, so Kalorite does **not** request it by default. A user who wants a
plugin to reach host tools must opt in themselves:

```sh
flatpak override --user \
  --talk-name=org.freedesktop.Flatpak io.github.monsler.Kalorite
```

Without it, `run`/`spawn` still work but execute *inside* the sandbox.

### `kalorite.log`
`info(msg)`, `warn(msg)`, `error(msg)` — goes to the application log.

See [`example.lua`](plugins/example.lua) for a complete working plugin, and
[`spotify_downloader.lua`](plugins/spotify_downloader.lua) for a real-world one
that resolves a Spotify track link and adds the downloaded MP3 to the playlist
(via `yt-dlp` + `ffmpeg`).
