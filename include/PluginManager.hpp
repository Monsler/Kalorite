#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>
#include <QPair>
#include <functional>

struct lua_State;

class QMenu;

namespace Kalorite {

    class MainWindow;

    // One loaded LuaJIT plugin. Every plugin lives in its own lua_State so a
    // crash, syntax error or runtime error in one cannot take down the others
    // (or the player). The plugin script must `return` a table describing
    // itself; see docs/plugin-api examples.
    struct Plugin {
        lua_State* L = nullptr;
        QString filePath;
        QString name;
        QString version;
        QString author;
        QString description;
        bool enabled = true;
        bool loaded  = false; // script parsed & table validated successfully

        int tableRef = -2;    // LUA_NOREF: registry ref to the plugin's table
        // Context-menu entries declared in plugin.menu, each a (title, funcRef).
        QVector<QPair<QString, int>> menuItems;
    };

    // Owns all plugins, wires them to the player through MainWindow's public
    // plugin surface, builds the "Plugins" menu, and marshals player events
    // (track changed / playback state / finished) into Lua callbacks.
    //
    // Everything here runs on the GUI thread. Never call into a lua_State from
    // the audio callback thread.
    class PluginManager : public QObject {
        Q_OBJECT

    public:
        explicit PluginManager(MainWindow* window);
        ~PluginManager();

        MainWindow* window() const { return m_window; }
        QNetworkAccessManager* network() { return &m_net; }

        // Directory (created on demand) where plugin .lua files are stored,
        // e.g. ~/.local/share/kalorite/plugins.
        static QString pluginsDir();
        // Per-plugin persistent settings file inside pluginsDir().
        QString storagePathFor(const Plugin* p) const;

        // Scan pluginsDir() and load every *.lua file found.
        void loadAllPlugins();
        // Copy an external .lua into pluginsDir() and load it immediately.
        // Returns true on success; shows a dialog and returns false otherwise.
        bool installPlugin(const QString& sourceLuaPath);

        // (Re)build the given menu with a submenu per plugin.
        void populateMenu(QMenu* menu);

        // Run a plugin table function `fnName` (no args) via a protected call,
        // reporting any error through showError(). Missing function = no-op.
        void callHook(Plugin* p, const char* fnName);

        // Surface a Lua error to the user as a critical Qt dialog.
        void showError(const Plugin* p, const QString& message);

        const QVector<Plugin*>& plugins() const { return m_plugins; }

    public slots:
        // Connected to MainWindow's plugin* signals.
        void onTrackChanged(QString path, int index);
        void onPlaybackStateChanged(QString state);
        void onTrackFinished(QString path);

    private:
        Plugin* loadPluginFile(const QString& path);
        void    unloadPlugin(Plugin* p);
        // Unload, remove from the list, delete the .lua (and its storage), and
        // rebuild the menu. Prompts for confirmation.
        void    deletePlugin(Plugin* p);

        // Dispatch an event callback (e.g. "on_track_changed") to every enabled
        // plugin that defines it, pushing `pushArgs(L)` values as arguments.
        void dispatchEvent(const char* fnName,
                           const std::function<int(lua_State*)>& pushArgs);

        MainWindow* m_window;
        QVector<Plugin*> m_plugins;
        QNetworkAccessManager m_net;
        QMenu* m_menu = nullptr;
    };

} // namespace Kalorite
