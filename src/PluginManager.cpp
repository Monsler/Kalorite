#include "PluginManager.hpp"
#include "LuaApi.hpp"
#include "MainWindow.hpp"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QMessageBox>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Kalorite {

// ---------------------------------------------------------------------------
// Small helpers for protected Lua calls with a traceback message handler.
// ---------------------------------------------------------------------------
namespace {

// Pushes debug.traceback and returns its stack index (to use as msgh).
int pushTraceback(lua_State* L) {
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_remove(L, -2);
    return lua_gettop(L);
}

QString optField(lua_State* L, int tableIdx, const char* key, const QString& def) {
    lua_getfield(L, tableIdx, key);
    QString out = def;
    if (lua_isstring(L, -1)) out = QString::fromUtf8(lua_tostring(L, -1));
    lua_pop(L, 1);
    return out;
}

} // anonymous namespace

// ---------------------------------------------------------------------------

PluginManager::PluginManager(MainWindow* window)
    : QObject(window), m_window(window) {
    connect(m_window, &MainWindow::pluginTrackChanged,
            this, &PluginManager::onTrackChanged);
    connect(m_window, &MainWindow::pluginPlaybackStateChanged,
            this, &PluginManager::onPlaybackStateChanged);
    connect(m_window, &MainWindow::pluginTrackFinished,
            this, &PluginManager::onTrackFinished);
}

PluginManager::~PluginManager() {
    for (Plugin* p : m_plugins) {
        unloadPlugin(p);
        delete p;
    }
}

QString PluginManager::pluginsDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dir = base + "/plugins";
    QDir().mkpath(dir);
    return dir;
}

QString PluginManager::storagePathFor(const Plugin* p) const {
    QFileInfo fi(p->filePath);
    return pluginsDir() + "/" + fi.completeBaseName() + ".json";
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

void PluginManager::loadAllPlugins() {
    QDir dir(pluginsDir());
    const QStringList files = dir.entryList(QStringList() << "*.lua", QDir::Files, QDir::Name);
    for (const QString& f : files) {
        loadPluginFile(dir.filePath(f));
    }
}

Plugin* PluginManager::loadPluginFile(const QString& path) {
    Plugin* p = new Plugin();
    p->filePath = path;
    p->name = QFileInfo(path).completeBaseName();

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    LuaApi::install(L, this);
    p->L = L;

    int base = pushTraceback(L);
    if (luaL_loadfile(L, path.toUtf8().constData()) != 0 ||
        lua_pcall(L, 0, 1, base) != 0) {
        QString err = QString::fromUtf8(lua_tostring(L, -1));
        lua_settop(L, 0);
        showError(p, err);
        m_plugins.push_back(p); // keep it around (disabled/broken) so it shows in the menu
        return p;
    }

    if (!lua_istable(L, -1)) {
        lua_settop(L, 0);
        showError(p, QObject::tr("Plugin script must return a table."));
        m_plugins.push_back(p);
        return p;
    }

    int tableIdx = lua_gettop(L);
    p->name        = optField(L, tableIdx, "name", p->name);
    p->version     = optField(L, tableIdx, "version", "");
    p->author      = optField(L, tableIdx, "author", "");
    p->description = optField(L, tableIdx, "description", "");

    // Capture plugin.menu entries as (title, funcRef).
    lua_getfield(L, tableIdx, "menu");
    if (lua_istable(L, -1)) {
        int menuIdx = lua_gettop(L);
        int n = (int)lua_objlen(L, menuIdx);
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, menuIdx, i);           // entry
            if (lua_istable(L, -1)) {
                QString title = optField(L, lua_gettop(L), "title", QObject::tr("Action"));
                lua_getfield(L, -1, "action");    // function
                if (lua_isfunction(L, -1)) {
                    int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops the function
                    p->menuItems.push_back(qMakePair(title, ref));
                } else {
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1); // entry
        }
    }
    lua_pop(L, 1); // menu

    // Ref the plugin table itself (pops it).
    p->tableRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_settop(L, 0);
    p->loaded = true;

    m_plugins.push_back(p);

    callHook(p, "on_load");
    if (p->enabled) callHook(p, "on_enable");
    return p;
}

void PluginManager::unloadPlugin(Plugin* p) {
    if (!p->L) return;
    if (p->loaded) {
        callHook(p, "on_disable");
        callHook(p, "on_unload");
    }
    lua_close(p->L);
    p->L = nullptr;
    p->loaded = false;
}

bool PluginManager::installPlugin(const QString& sourceLuaPath) {
    QFileInfo fi(sourceLuaPath);
    QString dest = pluginsDir() + "/" + fi.fileName();

    if (QFile::exists(dest)) {
        auto r = QMessageBox::question(m_window, QObject::tr("Add plugin"),
            QObject::tr("A plugin named \"%1\" already exists. Overwrite?").arg(fi.fileName()),
            QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) return false;
        QFile::remove(dest);
    }

    if (!QFile::copy(sourceLuaPath, dest)) {
        QMessageBox::critical(m_window, QObject::tr("Add plugin"),
            QObject::tr("Failed to copy the plugin into %1.").arg(pluginsDir()));
        return false;
    }

    Plugin* p = loadPluginFile(dest);
    if (m_menu) populateMenu(m_menu);
    return p && p->loaded;
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------

void PluginManager::deletePlugin(Plugin* p) {
    auto r = QMessageBox::question(m_window, QObject::tr("Remove plugin"),
        QObject::tr("Remove the plugin \"%1\"? Its file will be deleted.").arg(p->name),
        QMessageBox::Yes | QMessageBox::No);
    if (r != QMessageBox::Yes) return;

    QString file = p->filePath;
    QString storage = storagePathFor(p);

    unloadPlugin(p);
    m_plugins.removeOne(p);
    delete p;

    QFile::remove(file);
    QFile::remove(storage);

    if (m_menu) populateMenu(m_menu);
}

void PluginManager::populateMenu(QMenu* menu) {
    m_menu = menu;
    menu->clear();

    if (m_plugins.isEmpty()) {
        QAction* empty = menu->addAction(QObject::tr("(no plugins installed)"));
        empty->setEnabled(false);
        return;
    }

    for (Plugin* p : m_plugins) {
        QMenu* sub = menu->addMenu(p->name);

        if (!p->menuItems.isEmpty()) {
            for (const auto& item : p->menuItems) {
                int ref = item.second;
                QAction* act = sub->addAction(item.first);
                connect(act, &QAction::triggered, this, [this, p, ref]() {
                    lua_State* L = p->L;
                    int base = pushTraceback(L);
                    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
                    if (lua_pcall(L, 0, 0, base) != 0) {
                        QString err = QString::fromUtf8(lua_tostring(L, -1));
                        lua_pop(L, 1);
                        showError(p, err);
                    }
                    lua_remove(L, base);
                });
            }
        }

        sub->addSeparator();
        QString info = p->description.isEmpty()
            ? QObject::tr("Version %1").arg(p->version.isEmpty() ? "?" : p->version)
            : p->description;
        QAction* about = sub->addAction(QObject::tr("About…"));
        connect(about, &QAction::triggered, this, [this, p, info]() {
            QMessageBox::information(m_window, p->name,
                QString("%1\n\n%2%3")
                    .arg(info)
                    .arg(p->author.isEmpty() ? "" : QObject::tr("Author: %1\n").arg(p->author))
                    .arg(p->version.isEmpty() ? "" : QObject::tr("Version: %1").arg(p->version)));
        });

        QAction* remove = sub->addAction(QObject::tr("Delete"));
        remove->setIcon(QIcon::fromTheme("edit-delete"));
        connect(remove, &QAction::triggered, this, [this, p]() { deletePlugin(p); });
    }
}

// ---------------------------------------------------------------------------
// Hook / event dispatch
// ---------------------------------------------------------------------------

void PluginManager::callHook(Plugin* p, const char* fnName) {
    if (!p->loaded || p->tableRef == LUA_NOREF) return;
    lua_State* L = p->L;
    int base = pushTraceback(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, p->tableRef);
    lua_getfield(L, -1, fnName);
    if (!lua_isfunction(L, -1)) { lua_settop(L, base - 1); return; }
    lua_remove(L, -2); // drop table, keep fn
    if (lua_pcall(L, 0, 0, base) != 0) {
        QString err = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
        showError(p, err);
    }
    lua_remove(L, base);
}

void PluginManager::dispatchEvent(const char* fnName,
                                  const std::function<int(lua_State*)>& pushArgs) {
    for (Plugin* p : m_plugins) {
        if (!p->loaded || !p->enabled || p->tableRef == LUA_NOREF) continue;
        lua_State* L = p->L;
        int base = pushTraceback(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, p->tableRef);
        lua_getfield(L, -1, fnName);
        if (!lua_isfunction(L, -1)) { lua_settop(L, base - 1); continue; }
        lua_remove(L, -2); // drop table, keep fn
        int n = pushArgs(L);
        if (lua_pcall(L, n, 0, base) != 0) {
            QString err = QString::fromUtf8(lua_tostring(L, -1));
            lua_pop(L, 1);
            showError(p, err);
        }
        lua_remove(L, base);
    }
}

void PluginManager::onTrackChanged(QString path, int index) {
    dispatchEvent("on_track_changed", [path, index](lua_State* L) {
        lua_newtable(L);
        lua_pushstring(L, path.toUtf8().constData());
        lua_setfield(L, -2, "path");
        lua_pushstring(L, QFileInfo(path).fileName().toUtf8().constData());
        lua_setfield(L, -2, "title");
        lua_pushinteger(L, index);
        lua_setfield(L, -2, "index");
        return 1;
    });
}

void PluginManager::onPlaybackStateChanged(QString state) {
    dispatchEvent("on_playback_state", [state](lua_State* L) {
        lua_pushstring(L, state.toUtf8().constData());
        return 1;
    });
}

void PluginManager::onTrackFinished(QString path) {
    dispatchEvent("on_track_finished", [path](lua_State* L) {
        lua_pushstring(L, path.toUtf8().constData());
        return 1;
    });
}

// ---------------------------------------------------------------------------

void PluginManager::showError(const Plugin* p, const QString& message) {
    QString title = p ? QObject::tr("Error in plugin \"%1\"").arg(p->name)
                      : QObject::tr("Plugin error");
    QMessageBox::critical(m_window, title, message);
}

} // namespace Kalorite
