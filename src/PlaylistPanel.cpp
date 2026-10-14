#include "PlaylistPanel.hpp"
#include "Playlist.hpp"
#include "PlaylistModel.hpp"
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QQmlContext>
#include <QQuickWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace Kalorite {

PlaylistBackend::PlaylistBackend(Playlist* playlist, const QString& removeText, QObject* parent)
    : QObject(parent), m_playlist(playlist), m_removeText(removeText) {}

void PlaylistBackend::select(int row) { m_playlist->setCurrentRow(row); }

void PlaylistBackend::activate(int row) { emit trackActivated(row); }

void PlaylistBackend::toggleQueue(int row) { emit queueToggleRequested(row); }

void PlaylistBackend::remove(int row) {
    m_playlist->removeRow(row);
    emit trackRemoved();
}

PlaylistPanel::PlaylistPanel(QWidget* parent) : QWidget(parent) {
    m_playlist = new Playlist(this);
    m_backend = new PlaylistBackend(m_playlist, tr("RemoveFromList"), this);

    m_view = new QQuickWidget(this);
    m_view->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_view->setClearColor(Qt::transparent);
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->rootContext()->setContextProperty(QStringLiteral("playlistModel"), m_playlist->model());
    m_view->rootContext()->setContextProperty(QStringLiteral("pl"), m_backend);
    m_view->setSource(QUrl(QStringLiteral("qrc:/qml/PlaylistPanel.qml")));
    m_view->setFixedHeight(160);

    connect(m_backend, &PlaylistBackend::trackActivated, this, &PlaylistPanel::trackActivated);
    connect(m_backend, &PlaylistBackend::queueToggleRequested, this, &PlaylistPanel::queueToggleRequested);
    connect(m_backend, &PlaylistBackend::trackRemoved, this, &PlaylistPanel::trackRemoved);

    QPushButton* addFileButton = new QPushButton(QIcon::fromTheme("list-add"), "", this);
    addFileButton->setToolTip(tr("Add file to playlist"));
    addFileButton->setFixedSize(32, 32);
    connect(addFileButton, &QPushButton::clicked, this, &PlaylistPanel::addFileRequested);

    QPushButton* addFolderButton = new QPushButton(QIcon::fromTheme("folder-new"), "", this);
    addFolderButton->setToolTip(tr("Add folder to playlist"));
    addFolderButton->setFixedSize(32, 32);
    connect(addFolderButton, &QPushButton::clicked, this, &PlaylistPanel::addFolderRequested);

    QHBoxLayout* bottomButtonsLayout = new QHBoxLayout();
    bottomButtonsLayout->setAlignment(Qt::AlignLeft);
    bottomButtonsLayout->addWidget(addFileButton);
    bottomButtonsLayout->addWidget(addFolderButton);

    // Audio CD ripping-on-the-fly. Macs have no optical drives any more, so
    // the whole feature (and its libcdio dependency) is absent there.
#ifndef Q_OS_MACOS
    QPushButton* openCdButton = new QPushButton(QIcon::fromTheme("media-optical"), "", this);
    openCdButton->setToolTip(tr("Open Audio CD"));
    openCdButton->setFixedSize(32, 32);
    connect(openCdButton, &QPushButton::clicked, this, &PlaylistPanel::openCdRequested);
    bottomButtonsLayout->addWidget(openCdButton);
#endif

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    layout->addLayout(bottomButtonsLayout);
}

} // namespace Kalorite
