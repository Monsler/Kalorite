#include "MainWindow.hpp"
#include <qapplication.h>
#include <qnamespace.h>
#include <qfiledialog.h>

#define ICON_PLAY QIcon::fromTheme("media-playback-start")
#define ICON_PAUSE QIcon::fromTheme("media-playback-pause")

namespace Kalorite
{
    MainWindow::MainWindow() {
        resize(500, 150);
        setWindowTitle("Kalorite");

        centralWidget = new QWidget(this);

        this->currentMenuBar = menuBar();
        this->fileMenu = this->currentMenuBar->addMenu(tr("&File"));

        QAction* openSongAction = new QAction(tr("&OpenSong"), this);
        this->fileMenu->addAction(openSongAction);
        openSongAction->setShortcut(QKeySequence::fromString("Ctrl+O"));

        QAction *exitAction = new QAction(tr("&Exit"), this);
        this->fileMenu->addAction(exitAction);
        exitAction->setShortcut(QKeySequence::fromString("Ctrl+Esc"));

        openSongAction->setIcon(QIcon::fromTheme("document-open"));
        exitAction->setIcon(QIcon::fromTheme("application-exit"));

        connect(exitAction, &QAction::triggered, this, &QApplication::quit);
        connect(openSongAction, &QAction::triggered, this, &MainWindow::openButtonTriggered);

        setMenuBar(this->currentMenuBar);

        this->mainLayout = new QVBoxLayout(centralWidget);
        this->playerLayout = new QHBoxLayout();

        playbackButton = new QPushButton();
        playbackButton->setIcon(ICON_PLAY);
        playbackButton->setToolTip(tr("PlayTooltip"));

        playbackSlider = new QSlider(Qt::Horizontal);


        connect(playbackButton, &QPushButton::clicked, this, &MainWindow::onExitTriggered);
        
        setCentralWidget(this->centralWidget);

        this->playerLayout->addWidget(playbackButton);
        this->playerLayout->addWidget(playbackSlider);

        this->mainLayout->addLayout(playerLayout);

        show();
    }

    void MainWindow::openButtonTriggered() {
        QString openPath = QFileDialog::getOpenFileName(this, tr("OpenSound"), QDir::homePath(), tr("FileFilters"));
        qDebug() << openPath;

        if (!openPath.isEmpty()) {
            this->currentAudio = openPath.toStdString();
            setWindowTitle(QString::fromStdString(std::format("Kalorite - {}", this->currentAudio)));
        }
    }

    void MainWindow::onExitTriggered() {
        this->isPlaying = !this->isPlaying;

        if (this->isPlaying) {
            this->playbackButton->setIcon(ICON_PAUSE);
        } else {
            this->playbackButton->setIcon(ICON_PLAY);
        }
    }
} // namespace Kalorite
