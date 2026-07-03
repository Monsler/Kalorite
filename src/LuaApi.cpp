#include "LuaApi.hpp"
#include "PluginManager.hpp"
#include "MainWindow.hpp"
#include "Mixer.hpp"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <QString>
#include <QByteArray>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QStatusBar>
#include <QVariant>
#include <QLocale>
#include <QCoreApplication>
#include <QProcess>
#include <nlohmann/json.hpp>
#include <fstream>

namespace Kalorite {
namespace LuaApi {

// Unique registry key (its address is the key) holding the PluginManager*.
static const char MANAGER_KEY = 'K';

PluginManager* manager(lua_State* L) {
    lua_pushlightuserdata(L, (void*)&MANAGER_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* m = static_cast<PluginManager*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return m;
}

namespace {

MainWindow* win(lua_State* L) { return manager(L)->window(); }

QString argStr(lua_State* L, int i) {
    size_t len = 0;
    const char* s = luaL_checklstring(L, i, &len);
    return QString::fromUtf8(s, static_cast<int>(len));
}

// ---------------------------------------------------------------------------
// Deferred Lua callbacks (network layer). We stash the callable in the
// registry and invoke it later, protected, reporting failures via a dialog.
// ---------------------------------------------------------------------------

void callRef(lua_State* L, int ref, int nargsPushed /* already on stack */) {
    // Stack layout on entry: [ ...args ]. We insert the function underneath.
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);          // [ ...args, fn ]
    lua_insert(L, lua_gettop(L) - nargsPushed);       // [ fn, ...args ]
    if (lua_pcall(L, nargsPushed, 0, 0) != 0) {
        QString err = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
        // We can't easily map L back to a Plugin* here, so report generically.
        manager(L)->showError(nullptr, err);
    }
}

// ---------------------------------------------------------------------------
// kalorite.player
// ---------------------------------------------------------------------------

int l_play(lua_State* L)  { win(L)->pluginPlay();  return 0; }
int l_pause(lua_State* L) { win(L)->pluginPause(); return 0; }
int l_stop(lua_State* L)  { win(L)->pluginStop();  return 0; }
int l_next(lua_State* L)  { win(L)->pluginNext();  return 0; }
int l_prev(lua_State* L)  { win(L)->pluginPrev();  return 0; }

int l_is_playing(lua_State* L)  { lua_pushboolean(L, win(L)->pluginIsPlaying()); return 1; }
int l_position_ms(lua_State* L) { lua_pushinteger(L, win(L)->pluginPositionMs()); return 1; }
int l_duration_ms(lua_State* L) { lua_pushinteger(L, win(L)->pluginDurationMs()); return 1; }
int l_seek_ms(lua_State* L)     { win(L)->pluginSeekMs((int)luaL_checkinteger(L, 1)); return 0; }

int l_current_track(lua_State* L) {
    MainWindow* w = win(L);
    int idx = w->pluginCurrentIndex();
    if (idx < 0) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushstring(L, w->pluginPlaylistPath(idx).toUtf8().constData());  lua_setfield(L, -2, "path");
    lua_pushstring(L, w->pluginPlaylistTitle(idx).toUtf8().constData()); lua_setfield(L, -2, "title");
    lua_pushinteger(L, idx);                                             lua_setfield(L, -2, "index");
    return 1;
}

// ---------------------------------------------------------------------------
// kalorite.mixer
// ---------------------------------------------------------------------------

int l_get_volume(lua_State* L) { lua_pushinteger(L, win(L)->pluginMixer()->volume); return 1; }
int l_set_volume(lua_State* L) {
    int v = (int)luaL_checkinteger(L, 1);
    if (v < 0) v = 0; if (v > 100) v = 100;
    win(L)->pluginMixer()->setVolume(v);
    return 0;
}
int l_set_eq_band(lua_State* L) {
    int band = (int)luaL_checkinteger(L, 1);
    float gain = (float)luaL_checknumber(L, 2);
    if (band < 0 || band > 9) return luaL_error(L, "eq band index must be 0..9");
    win(L)->pluginMixer()->setEqBand(band, gain);
    return 0;
}
int l_set_eq_enabled(lua_State* L) {
    win(L)->pluginMixer()->setEqEnabled(lua_toboolean(L, 1));
    return 0;
}
int l_set_crossfade(lua_State* L) {
    Mixer* m = win(L)->pluginMixer();
    m->setCrossfadeEnabled(lua_toboolean(L, 1));
    if (!lua_isnoneornil(L, 2)) m->setCrossfadeDuration((float)luaL_checknumber(L, 2));
    return 0;
}
int l_get_device(lua_State* L) {
    lua_pushstring(L, win(L)->pluginMixer()->getCurrentDeviceName().c_str());
    return 1;
}
int l_set_device(lua_State* L) {
    win(L)->pluginMixer()->setDeviceByName(argStr(L, 1).toStdString());
    return 0;
}

// ---------------------------------------------------------------------------
// kalorite.playlist
// ---------------------------------------------------------------------------

int l_pl_count(lua_State* L)  { lua_pushinteger(L, win(L)->pluginPlaylistCount()); return 1; }
int l_pl_add(lua_State* L)    { win(L)->pluginPlaylistAdd(argStr(L, 1)); return 0; }
int l_pl_remove(lua_State* L) { win(L)->pluginPlaylistRemove((int)luaL_checkinteger(L, 1)); return 0; }
int l_pl_clear(lua_State* L)  { win(L)->pluginPlaylistClear(); return 0; }
int l_pl_current(lua_State* L){ lua_pushinteger(L, win(L)->pluginCurrentIndex()); return 1; }
int l_pl_play(lua_State* L)   { win(L)->pluginPlayIndex((int)luaL_checkinteger(L, 1)); return 0; }
int l_pl_get(lua_State* L) {
    MainWindow* w = win(L);
    int idx = (int)luaL_checkinteger(L, 1);
    if (idx < 0 || idx >= w->pluginPlaylistCount()) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushstring(L, w->pluginPlaylistPath(idx).toUtf8().constData());  lua_setfield(L, -2, "path");
    lua_pushstring(L, w->pluginPlaylistTitle(idx).toUtf8().constData()); lua_setfield(L, -2, "title");
    lua_pushinteger(L, idx);                                             lua_setfield(L, -2, "index");
    return 1;
}

// ---------------------------------------------------------------------------
// kalorite.net  (asynchronous; callbacks fire back on the GUI thread)
// ---------------------------------------------------------------------------

int l_net_get(lua_State* L) {
    QString url = argStr(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);

    PluginManager* pm = manager(L);
    QNetworkReply* reply = pm->network()->get(QNetworkRequest(QUrl(url)));
    QObject::connect(reply, &QNetworkReply::finished, pm, [L, cbRef, reply]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool ok = (reply->error() == QNetworkReply::NoError);
        QByteArray body = reply->readAll();

        if (ok) lua_pushnil(L);
        else    lua_pushstring(L, reply->errorString().toUtf8().constData());
        lua_pushlstring(L, body.constData(), body.size());
        lua_pushinteger(L, status);
        callRef(L, cbRef, 3);

        luaL_unref(L, LUA_REGISTRYINDEX, cbRef);
        reply->deleteLater();
    });
    return 0;
}

int l_net_download(lua_State* L) {
    QString url  = argStr(L, 1);
    QString dest = argStr(L, 2);
    int progressRef = LUA_NOREF;
    int doneRef     = LUA_NOREF;
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "on_progress");
        if (lua_isfunction(L, -1)) progressRef = luaL_ref(L, LUA_REGISTRYINDEX); else lua_pop(L, 1);
        lua_getfield(L, 3, "on_done");
        if (lua_isfunction(L, -1)) doneRef = luaL_ref(L, LUA_REGISTRYINDEX); else lua_pop(L, 1);
    }

    PluginManager* pm = manager(L);
    QFile* file = new QFile(dest);
    if (!file->open(QIODevice::WriteOnly)) {
        QString err = QString("cannot open %1 for writing").arg(dest);
        delete file;
        if (doneRef != LUA_NOREF) {
            lua_pushstring(L, err.toUtf8().constData());
            lua_pushnil(L);
            callRef(L, doneRef, 2);
            luaL_unref(L, LUA_REGISTRYINDEX, doneRef);
        }
        if (progressRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, progressRef);
        return 0;
    }

    QNetworkReply* reply = pm->network()->get(QNetworkRequest(QUrl(url)));

    QObject::connect(reply, &QNetworkReply::readyRead, pm, [reply, file]() {
        file->write(reply->readAll());
    });
    if (progressRef != LUA_NOREF) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, pm,
            [L, progressRef](qint64 received, qint64 total) {
                lua_pushinteger(L, (lua_Integer)received);
                lua_pushinteger(L, (lua_Integer)total);
                // Do not unref here: progress fires repeatedly.
                lua_rawgeti(L, LUA_REGISTRYINDEX, progressRef);
                lua_insert(L, lua_gettop(L) - 2);
                if (lua_pcall(L, 2, 0, 0) != 0) { lua_pop(L, 1); }
            });
    }
    QObject::connect(reply, &QNetworkReply::finished, pm,
        [L, reply, file, dest, doneRef, progressRef]() {
            file->write(reply->readAll());
            file->close();
            bool ok = (reply->error() == QNetworkReply::NoError);
            if (!ok) file->remove();
            delete file;

            if (doneRef != LUA_NOREF) {
                if (ok) lua_pushnil(L);
                else    lua_pushstring(L, reply->errorString().toUtf8().constData());
                lua_pushstring(L, dest.toUtf8().constData());
                callRef(L, doneRef, 2);
                luaL_unref(L, LUA_REGISTRYINDEX, doneRef);
            }
            if (progressRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, progressRef);
            reply->deleteLater();
        });
    return 0;
}

// ---------------------------------------------------------------------------
// kalorite.ui
// ---------------------------------------------------------------------------

int l_ui_message(lua_State* L) {
    QMessageBox::information(win(L), "Kalorite", argStr(L, 1));
    return 0;
}
int l_ui_error(lua_State* L) {
    QMessageBox::critical(win(L), QObject::tr("Plugin"), argStr(L, 1));
    return 0;
}
int l_ui_confirm(lua_State* L) {
    auto r = QMessageBox::question(win(L), "Kalorite", argStr(L, 1),
                                   QMessageBox::Yes | QMessageBox::No);
    lua_pushboolean(L, r == QMessageBox::Yes);
    return 1;
}
int l_ui_input(lua_State* L) {
    bool ok = false;
    QString def = lua_isnoneornil(L, 2) ? QString() : argStr(L, 2);
    QString text = QInputDialog::getText(win(L), "Kalorite", argStr(L, 1),
                                         QLineEdit::Normal, def, &ok);
    if (ok) lua_pushstring(L, text.toUtf8().constData());
    else    lua_pushnil(L);
    return 1;
}
int l_ui_notify(lua_State* L) {
    win(L)->statusBar()->showMessage(argStr(L, 1), 5000);
    return 0;
}

// ---------------------------------------------------------------------------
// kalorite.storage  (scalar key/value persisted per plugin as JSON)
// ---------------------------------------------------------------------------

nlohmann::json loadStorage(lua_State* L) {
    // The active plugin is identified by the manager; we key storage by state.
    // For simplicity each state maps to exactly one plugin file, and its
    // storage path is resolvable via the manager.
    PluginManager* pm = manager(L);
    for (Plugin* p : pm->plugins()) {
        if (p->L == L) {
            QString path = pm->storagePathFor(p);
            std::ifstream f(path.toStdString());
            if (f.good()) { try { nlohmann::json j; f >> j; return j; } catch (...) {} }
            break;
        }
    }
    return nlohmann::json::object();
}
void saveStorage(lua_State* L, const nlohmann::json& j) {
    PluginManager* pm = manager(L);
    for (Plugin* p : pm->plugins()) {
        if (p->L == L) {
            std::ofstream f(pm->storagePathFor(p).toStdString());
            f << j.dump(2);
            break;
        }
    }
}

int l_st_get(lua_State* L) {
    QString key = argStr(L, 1);
    nlohmann::json j = loadStorage(L);
    auto it = j.find(key.toStdString());
    if (it == j.end()) { lua_pushvalue(L, 2); return 1; } // default (or nil)
    const nlohmann::json& v = *it;
    if (v.is_string())        lua_pushstring(L, v.get<std::string>().c_str());
    else if (v.is_boolean())  lua_pushboolean(L, v.get<bool>());
    else if (v.is_number())   lua_pushnumber(L, v.get<double>());
    else                      lua_pushvalue(L, 2);
    return 1;
}
int l_st_set(lua_State* L) {
    QString key = argStr(L, 1);
    nlohmann::json j = loadStorage(L);
    std::string k = key.toStdString();
    switch (lua_type(L, 2)) {
        case LUA_TSTRING:  j[k] = std::string(lua_tostring(L, 2)); break;
        case LUA_TBOOLEAN: j[k] = (bool)lua_toboolean(L, 2); break;
        case LUA_TNUMBER:  j[k] = lua_tonumber(L, 2); break;
        case LUA_TNIL:     j.erase(k); break;
        default: return luaL_error(L, "storage values must be string/number/boolean/nil");
    }
    saveStorage(L, j);
    return 0;
}

// ---------------------------------------------------------------------------
// kalorite.log
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// kalorite.app
// ---------------------------------------------------------------------------

// Full UI locale name, e.g. "ru_RU" or "en_US".
int l_app_language(lua_State* L) {
    (void)L;
    lua_pushstring(L, QLocale::system().name().toUtf8().constData());
    return 1;
}
// Just the ISO language code, e.g. "ru" or "en".
int l_app_language_code(lua_State* L) {
    lua_pushstring(L, QLocale::languageToCode(QLocale::system().language()).toUtf8().constData());
    return 1;
}
int l_app_version(lua_State* L) {
    lua_pushstring(L, QCoreApplication::applicationVersion().toUtf8().constData());
    return 1;
}

// ---------------------------------------------------------------------------
// kalorite.log
// ---------------------------------------------------------------------------

int l_log_info(lua_State* L)  { qInfo()    << "[plugin]" << argStr(L, 1); return 0; }
int l_log_warn(lua_State* L)  { qWarning() << "[plugin]" << argStr(L, 1); return 0; }
int l_log_error(lua_State* L) { qCritical()<< "[plugin]" << argStr(L, 1); return 0; }

// ---------------------------------------------------------------------------
// kalorite.sys  (run external commands; transparently host-spawned in Flatpak)
// ---------------------------------------------------------------------------

// True when the app runs inside a Flatpak sandbox, in which case external
// tools live on the host and must be reached via `flatpak-spawn --host`.
bool isSandboxed() { return QFileInfo::exists("/.flatpak-info"); }

// kalorite.sys.is_sandboxed() -> bool
int l_sys_sandboxed(lua_State* L) {
    lua_pushboolean(L, isSandboxed());
    return 1;
}

// kalorite.sys.run(command) -> output(string), exit_code(int)
//
// Runs `command` through /bin/sh synchronously and returns its combined
// stdout+stderr plus the process exit code. Inside Flatpak the command is
// transparently forwarded to the host via `flatpak-spawn --host`, so plugins
// can call host tools (yt-dlp, ffmpeg, …) without knowing they are sandboxed.
// Blocks the GUI thread for the duration of the command.
int l_sys_run(lua_State* L) {
    QString cmd = argStr(L, 1);
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    if (isSandboxed())
        proc.start("flatpak-spawn", {"--host", "/bin/sh", "-c", cmd});
    else
        proc.start("/bin/sh", {"-c", cmd});

    proc.waitForFinished(-1);
    QByteArray out = proc.readAll();
    int code = (proc.exitStatus() == QProcess::NormalExit) ? proc.exitCode() : -1;

    lua_pushlstring(L, out.constData(), out.size());
    lua_pushinteger(L, code);
    return 2;
}

// Push string `chunk` and invoke the registry callback `ref` with it,
// protected. Does not unref (the caller decides when the stream is done).
void fireOutput(lua_State* L, int ref, const QByteArray& chunk) {
    lua_pushlstring(L, chunk.constData(), chunk.size());
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_insert(L, lua_gettop(L) - 1);
    if (lua_pcall(L, 1, 0, 0) != 0) lua_pop(L, 1);
}

// kalorite.sys.spawn(command, { on_output = f(chunk), on_done = f(exit_code) })
//
// Asynchronous sibling of run(): starts `command` without blocking the GUI
// thread. `on_output` fires (possibly many times) with combined stdout+stderr
// as it arrives; `on_done` fires once with the exit code (-1 on crash). Like
// run(), the command is host-spawned in Flatpak. Callbacks run on the GUI
// thread, so they may freely use the rest of the kalorite.* API.
int l_sys_spawn(lua_State* L) {
    QString cmd = argStr(L, 1);
    int outRef = LUA_NOREF, doneRef = LUA_NOREF;
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "on_output");
        if (lua_isfunction(L, -1)) outRef = luaL_ref(L, LUA_REGISTRYINDEX); else lua_pop(L, 1);
        lua_getfield(L, 2, "on_done");
        if (lua_isfunction(L, -1)) doneRef = luaL_ref(L, LUA_REGISTRYINDEX); else lua_pop(L, 1);
    }

    PluginManager* pm = manager(L);
    QProcess* proc = new QProcess(pm);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    if (outRef != LUA_NOREF) {
        QObject::connect(proc, &QProcess::readyReadStandardOutput, pm, [L, proc, outRef]() {
            fireOutput(L, outRef, proc->readAll());
        });
    }
    QObject::connect(proc,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), pm,
        [L, proc, outRef, doneRef](int code, QProcess::ExitStatus st) {
            if (outRef != LUA_NOREF) {
                QByteArray rest = proc->readAll();      // flush the tail
                if (!rest.isEmpty()) fireOutput(L, outRef, rest);
                luaL_unref(L, LUA_REGISTRYINDEX, outRef);
            }
            if (doneRef != LUA_NOREF) {
                lua_pushinteger(L, st == QProcess::NormalExit ? code : -1);
                callRef(L, doneRef, 1);
                luaL_unref(L, LUA_REGISTRYINDEX, doneRef);
            }
            proc->deleteLater();
        });

    if (isSandboxed())
        proc->start("flatpak-spawn", {"--host", "/bin/sh", "-c", cmd});
    else
        proc->start("/bin/sh", {"-c", cmd});
    return 0;
}

void setTable(lua_State* L, const char* name, const luaL_Reg* fns) {
    lua_newtable(L);
    for (const luaL_Reg* f = fns; f->name; ++f) {
        lua_pushcfunction(L, f->func);
        lua_setfield(L, -2, f->name);
    }
    lua_setfield(L, -2, name); // kalorite[name] = table  (kalorite is at -2)
}

} // anonymous namespace

void install(lua_State* L, PluginManager* mgr) {
    // Stash the manager pointer in the registry.
    lua_pushlightuserdata(L, (void*)&MANAGER_KEY);
    lua_pushlightuserdata(L, (void*)mgr);
    lua_settable(L, LUA_REGISTRYINDEX);

    lua_newtable(L); // the `kalorite` table

    static const luaL_Reg player[] = {
        {"play", l_play}, {"pause", l_pause}, {"stop", l_stop},
        {"next", l_next}, {"prev", l_prev},
        {"is_playing", l_is_playing}, {"position_ms", l_position_ms},
        {"duration_ms", l_duration_ms}, {"seek_ms", l_seek_ms},
        {"current_track", l_current_track}, {nullptr, nullptr}
    };
    static const luaL_Reg mixer[] = {
        {"get_volume", l_get_volume}, {"set_volume", l_set_volume},
        {"set_eq_band", l_set_eq_band}, {"set_eq_enabled", l_set_eq_enabled},
        {"set_crossfade", l_set_crossfade},
        {"get_device", l_get_device}, {"set_device", l_set_device},
        {nullptr, nullptr}
    };
    static const luaL_Reg playlist[] = {
        {"count", l_pl_count}, {"add", l_pl_add}, {"remove", l_pl_remove},
        {"clear", l_pl_clear}, {"get", l_pl_get},
        {"current_index", l_pl_current}, {"play_index", l_pl_play},
        {nullptr, nullptr}
    };
    static const luaL_Reg net[] = {
        {"get", l_net_get}, {"download", l_net_download}, {nullptr, nullptr}
    };
    static const luaL_Reg ui[] = {
        {"message", l_ui_message}, {"error", l_ui_error},
        {"confirm", l_ui_confirm}, {"input", l_ui_input},
        {"notify", l_ui_notify}, {nullptr, nullptr}
    };
    static const luaL_Reg storage[] = {
        {"get", l_st_get}, {"set", l_st_set}, {nullptr, nullptr}
    };
    static const luaL_Reg log[] = {
        {"info", l_log_info}, {"warn", l_log_warn}, {"error", l_log_error},
        {nullptr, nullptr}
    };
    static const luaL_Reg app[] = {
        {"language", l_app_language}, {"language_code", l_app_language_code},
        {"version", l_app_version}, {nullptr, nullptr}
    };
    static const luaL_Reg sys[] = {
        {"run", l_sys_run}, {"spawn", l_sys_spawn},
        {"is_sandboxed", l_sys_sandboxed},
        {nullptr, nullptr}
    };

    setTable(L, "player",   player);
    setTable(L, "mixer",    mixer);
    setTable(L, "playlist", playlist);
    setTable(L, "net",      net);
    setTable(L, "ui",       ui);
    setTable(L, "storage",  storage);
    setTable(L, "log",      log);
    setTable(L, "app",      app);
    setTable(L, "sys",      sys);

    lua_setglobal(L, "kalorite");
}

} // namespace LuaApi
} // namespace Kalorite
