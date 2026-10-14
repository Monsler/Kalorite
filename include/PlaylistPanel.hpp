#pragma once

#include <QObject>
#include <QString>
#include <QWidget>

class QQuickWidget;

namespace Kalorite {

    class Playlist;

    // Exposed to QML as `pl`; row actions from the list are forwarded to the panel's signals.
    class PlaylistBackend : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString removeText READ removeText CONSTANT)

    public:
        PlaylistBackend(Playlist* playlist, const QString& removeText, QObject* parent = nullptr);

        QString removeText() const { return m_removeText; }

        Q_INVOKABLE void select(int row);
        Q_INVOKABLE void activate(int row);
        Q_INVOKABLE void toggleQueue(int row);
        Q_INVOKABLE void remove(int row);

    signals:
        void trackActivated(int row);
        void queueToggleRequested(int row);
        void trackRemoved();

    private:
        Playlist* m_playlist;
        QString m_removeText;
    };

    // Playlist body: the track list (QML) with its drag-reorder, queue shortcut and context menu,
    // plus the add buttons. The collapse toggle lives in the window.
    class PlaylistPanel : public QWidget {
        Q_OBJECT

    public:
        explicit PlaylistPanel(QWidget* parent = nullptr);

        Playlist* playlist() const { return m_playlist; }

    signals:
        void addFileRequested();
        void addFolderRequested();
        void openCdRequested();
        void trackActivated(int row);
        void queueToggleRequested(int row);
        void trackRemoved();

    private:
        Playlist* m_playlist;
        PlaylistBackend* m_backend;
        QQuickWidget* m_view;
    };

} // namespace Kalorite
