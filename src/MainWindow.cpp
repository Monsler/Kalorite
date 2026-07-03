#include "MainWindow.hpp"
#include "Mixer.hpp"
#include "PluginManager.hpp"
#include <fstream>
#include <qaction.h>
#include <QActionGroup>
#include <qapplication.h>
#include <qnamespace.h>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <qfiledialog.h>
#include <qaudiooutput.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <random>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <QMediaDevices>
#include <QAudioDevice>

#define ICON_PLAY QIcon::fromTheme("media-playback-start")
#define ICON_PAUSE QIcon::fromTheme("media-playback-pause")
#define ICON_REPEAT QIcon::fromTheme("media-playlist-repeat")
#define ICON_REPEAT_CURRENT QIcon::fromTheme("media-playlist-repeat-song")
#define ICON_REPEAT_THE_LIST QIcon::fromTheme("media-playlist-repeat-amarok")
#define ICON_REPEAT_SHUFFLE QIcon::fromTheme("media-playlist-shuffle")

#include <QDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QPainter>
#include <cmath>

namespace {

// A clickable logo that, WinRAR-style, bounces around its parent with gravity
// and springy wall collisions when clicked. Each extra click gives it a kick.
// Plain QLabel subclass (no signals/slots) so it needs no moc.
class BouncingLogo : public QLabel {
public:
    BouncingLogo(QWidget* parent, const QPixmap& pm) : QLabel(parent) {
        setPixmap(pm);
        setFixedSize(pm.size());
        setCursor(Qt::PointingHandCursor);
        setToolTip(QObject::tr("Click me!"));
        m_timer = new QTimer(this);
        QObject::connect(m_timer, &QTimer::timeout, this, [this] { step(); });
    }

protected:
    void mousePressEvent(QMouseEvent*) override {
        auto* rng = QRandomGenerator::global();
        if (!m_timer->isActive()) {
            m_x = x(); m_y = y();
            m_vx = 3.0 + rng->bounded(6);
            m_vy = -13.0 - rng->bounded(5);
            m_timer->start(16); // ~60 fps
        } else {
            // A kick: launch it upward again with a random horizontal nudge.
            m_vy -= 11.0 + rng->bounded(4);
            m_vx += rng->bounded(11) - 5;
        }
    }

private:
    void step() {
        const double gravity = 0.9, restitution = 0.78, friction = 0.98;
        QWidget* p = parentWidget();
        if (!p) { m_timer->stop(); return; }
        const double maxX = p->width()  - width();
        const double maxY = p->height() - height();

        m_vy += gravity;
        m_x  += m_vx;
        m_y  += m_vy;

        if (m_x < 0)    { m_x = 0;    m_vx = -m_vx * restitution; }
        if (m_x > maxX) { m_x = maxX; m_vx = -m_vx * restitution; }
        if (m_y < 0)    { m_y = 0;    m_vy = -m_vy * restitution; }
        if (m_y > maxY) {
            m_y = maxY;
            m_vy = -m_vy * restitution;
            m_vx *= friction;
            // Once it has essentially settled on the floor, stop animating.
            if (std::abs(m_vy) < 1.6 && std::abs(m_vx) < 0.35) {
                m_vx = m_vy = 0.0;
                move(int(m_x), int(m_y));
                m_timer->stop();
                return;
            }
        }
        move(int(m_x), int(m_y));
    }

    QTimer* m_timer = nullptr;
    double m_x = 0, m_y = 0, m_vx = 0, m_vy = 0;
};

} // anonymous namespace

namespace Kalorite
{
    MainWindow::MainWindow() {
        resize(500, 150);
        setWindowTitle("Kalorite");
        setWindowIcon(QIcon("kalorite"));

        centralWidget = new QWidget(this);

        this->songDownloader = new SongDownloader(this);

        this->currentMenuBar = menuBar();
        this->fileMenu = this->currentMenuBar->addMenu(tr("&File"));

        QAction* openSongAction = new QAction(tr("&OpenSong"));
        openSongAction->setShortcut(QKeySequence::fromString("Ctrl+O"));

        QAction* savePlaylistAction = new QAction(tr("&SavePlaylistAs"));
        savePlaylistAction->setShortcut(QKeySequence::fromString("Ctrl+S"));

        QAction* loadPlaylistAction = new QAction(tr("&LoadPlaylistFrom"));
        loadPlaylistAction->setShortcut(QKeySequence::fromString("Ctrl+V"));

        QAction* downloadSoundAction = new QAction(tr("&DownloadSound"));
        downloadSoundAction->setShortcut(QKeySequence::fromString("Ctrl+D"));

        QAction* addPluginAction = new QAction(tr("&AddPlugin"));

        this->fileMenu->addAction(openSongAction);
        this->fileMenu->addAction(savePlaylistAction);
        this->fileMenu->addAction(loadPlaylistAction);
        this->fileMenu->addAction(downloadSoundAction);
        this->fileMenu->addAction(addPluginAction);
        addPluginAction->setIcon(QIcon::fromTheme("insert-object"));
        connect(addPluginAction, &QAction::triggered, this, &MainWindow::openAddPluginDialog);


        QAction *exitAction = new QAction(tr("&Exit"), this);
        this->fileMenu->addAction(exitAction);
        exitAction->setShortcut(QKeySequence::fromString("Ctrl+Esc"));

        openSongAction->setIcon(QIcon::fromTheme("list-add"));
        exitAction->setIcon(QIcon::fromTheme("application-exit"));
        savePlaylistAction->setIcon(QIcon::fromTheme("document-save-as"));
        loadPlaylistAction->setIcon(QIcon::fromTheme("document-open"));
        downloadSoundAction->setIcon(QIcon::fromTheme("download"));

        connect(openSongAction, &QAction::triggered, this, &MainWindow::openButtonTriggered);
        connect(savePlaylistAction, &QAction::triggered, this, &MainWindow::savePlaylistTriggered);
        connect(loadPlaylistAction, &QAction::triggered, this, &MainWindow::loadPlaylistTriggered);
        connect(downloadSoundAction, &QAction::triggered, this, &MainWindow::openSoundFileDownloadDialog);

        this->mixer = new Mixer();
        connect(this->mixer, &Mixer::playbackFinished, this, &MainWindow::onCrossfadeAdvance);

        this->mixer->setVolume(100);

        connect(exitAction, &QAction::triggered, this, &QApplication::quit);

        // Pre-fetch/warmup audio outputs list in constructor to avoid first-right-click GUI freeze
        QTimer::singleShot(100, [this]() {
            QMediaDevices::audioOutputs();
        });

        this->winampDisplay = new WinampDisplay(this);
        this->winampDisplay->setMixer(this->mixer);
        this->winampDisplay->setVolume(100);
        this->winampDisplay->setFixedHeight(120);

        this->volumeSignal = new VolumeSignalWidget(this);
        this->volumeSignal->setVolume(100);

        // View menu for Retro/Modern modes
        QMenu* viewMenu = this->currentMenuBar->addMenu(tr("&View"));
        QActionGroup* modeGroup = new QActionGroup(this);

        QAction* retroAction = new QAction(tr("Retro Display Mode"), this);
        retroAction->setCheckable(true);
        retroAction->setChecked(true);
        modeGroup->addAction(retroAction);
        viewMenu->addAction(retroAction);

        QAction* modernAction = new QAction(tr("Modern Display Mode"), this);
        modernAction->setCheckable(true);
        modeGroup->addAction(modernAction);
        viewMenu->addAction(modernAction);

        connect(retroAction, &QAction::triggered, [this]() {
            winampDisplay->setModernMode(false);
            volumeSignal->setModernMode(false);
        });
        connect(modernAction, &QAction::triggered, [this]() {
            winampDisplay->setModernMode(true);
            volumeSignal->setModernMode(true);
        });

        connect(winampDisplay, &WinampDisplay::modeChanged, [this, retroAction, modernAction](bool modern) {
            retroAction->setChecked(!modern);
            modernAction->setChecked(modern);
            volumeSignal->setModernMode(modern);
        });

        connect(winampDisplay, &WinampDisplay::contextMenuRequested, this, &MainWindow::onContextMenuWinampDisplay);

        // Plugins menu comes right after View in the menu bar. It is filled in
        // once the PluginManager has scanned the plugins directory (below).
        this->pluginsMenu = this->currentMenuBar->addMenu(tr("&Plugins"));

        // Help menu with the About dialog (and its little easter egg).
        QMenu* helpMenu = this->currentMenuBar->addMenu(tr("&Help"));
        QAction* aboutAction = new QAction(tr("&About"), this);
        aboutAction->setIcon(QIcon::fromTheme("help-about"));
        helpMenu->addAction(aboutAction);
        connect(aboutAction, &QAction::triggered, this, &MainWindow::openAboutDialog);

        setMenuBar(this->currentMenuBar);
        if (this->layout()) {
            this->layout()->setSizeConstraint(QLayout::SetFixedSize);
        }

        this->mainLayout = new QVBoxLayout(centralWidget);
        this->mainLayout->setSizeConstraint(QLayout::SetFixedSize);
        this->playerLayout = new QHBoxLayout();

        soundList = new QListWidget();
        soundList->setFixedHeight(160);
        soundList->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(soundList, &QListWidget::itemDoubleClicked, this, &MainWindow::onListSelection);
        connect(soundList, &QListWidget::customContextMenuRequested, this, &MainWindow::onContextMenuSoundList);

        playbackButton = new QPushButton();
        playbackButton->setIcon(ICON_PLAY);
        playbackButton->setToolTip(tr("PlayTooltip"));
        playbackButton->setEnabled(false);
        connect(playbackButton, &QPushButton::clicked, this, &MainWindow::onPlayTriggered);

        playbackSlider = new QSlider(Qt::Horizontal);
        playbackSlider->setEnabled(false);
        connect(playbackSlider, &QSlider::sliderPressed, this, &MainWindow::playbackSliderPressed);
        connect(playbackSlider, &QSlider::sliderReleased, this, &MainWindow::playbackSliderReleased);

        timeLabel = new QLabel("00:00:00 / 00:00:00");
        timeLabel->setVisible(false); // Hide the old time label in favor of the new display

        skipBackButton = new QPushButton();
        skipBackButton->setIcon(QIcon::fromTheme("media-skip-backward"));
        skipBackButton->setEnabled(false);
        skipBackButton->setToolTip(tr("SkipBackward"));
        connect(skipBackButton, &QPushButton::clicked, this, &MainWindow::onSkipBack);

        skipForwardButton = new QPushButton();
        skipForwardButton->setIcon(QIcon::fromTheme("media-skip-forward"));
        skipForwardButton->setEnabled(false);
        skipForwardButton->setToolTip(tr("SkipForward"));

        repeatButton = new QPushButton();
        repeatButton->setIcon(ICON_REPEAT);
        repeatButton->setEnabled(false);
        connect(repeatButton, &QPushButton::clicked, this, &MainWindow::onRepeatButtonTriggered);
        repeatButton->setToolTip(tr("TooltipNoRepeat"));


        connect(volumeSignal, &VolumeSignalWidget::volumeChanged, this, &MainWindow::onSpinTriggered);

        connect(skipForwardButton, &QPushButton::clicked, this, &MainWindow::onSkipNext);
        setCentralWidget(this->centralWidget);

        this->playerLayout->addWidget(skipBackButton);
        this->playerLayout->addWidget(playbackButton);
        this->playerLayout->addWidget(skipForwardButton);
        this->playerLayout->addWidget(repeatButton);
        this->playerLayout->addWidget(playbackSlider);
        this->playerLayout->addWidget(volumeSignal);

        // Equalizer UI
        eqToggleBtn = new QPushButton(QString("▼ %1").arg(tr("Equalizer")), this);
        eqToggleBtn->setCheckable(true);
        eqToggleBtn->setChecked(false);
        eqToggleBtn->setText(QString("▶ %1").arg(tr("Equalizer")));
        eqToggleBtn->setStyleSheet("text-align: left; padding: 6px; font-weight: bold;");

        eqContainer = new QWidget(this);
        QVBoxLayout* eqContainerLayout = new QVBoxLayout(eqContainer);
        eqContainerLayout->setContentsMargins(0, 0, 0, 0);

        eqEnabledCheckbox = new QCheckBox(tr("Enabled"), this);
        eqEnabledCheckbox->setChecked(false);

        // Reset button with theme icon
        QPushButton* eqResetBtn = new QPushButton(this);
        eqResetBtn->setIcon(QIcon::fromTheme("edit-clear-all"));
        eqResetBtn->setToolTip(tr("Reset Equalizer"));
        eqResetBtn->setFixedSize(26, 26);

        // Preset actions/buttons
        QHBoxLayout* presetLayout = new QHBoxLayout();
        presetLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel* presetLabel = new QLabel(tr("Presets:"), this);
        presetLayout->addWidget(presetLabel);

        struct Preset {
            QString name;
            std::vector<int> values;
        };
        std::vector<Preset> presets = {
            {tr("Flat"), {50, 50, 50, 50, 50, 50, 50, 50, 50, 50}},
            {tr("Rock"), {65, 60, 55, 45, 42, 45, 52, 60, 65, 68}},
            {tr("Pop"), {40, 45, 55, 62, 65, 62, 55, 48, 42, 40}},
            {tr("Classical"), {60, 58, 55, 52, 48, 48, 52, 55, 58, 60}}
        };

        for (auto& preset : presets) {
            QPushButton* presetBtn = new QPushButton(preset.name, this);
            presetBtn->setStyleSheet("padding: 2px 6px; font-size: 10px;");
            presetLayout->addWidget(presetBtn);
            connect(presetBtn, &QPushButton::clicked, this, [this, preset]() {
                for (int i = 0; i < (int)eqSliders.size() && i < (int)preset.values.size(); ++i) {
                    eqSliders[i]->setValue(preset.values[i]);
                }
            });
        }
        presetLayout->addStretch();

        QHBoxLayout* eqHeaderLayout = new QHBoxLayout();
        eqHeaderLayout->addWidget(eqEnabledCheckbox);
        eqHeaderLayout->addWidget(eqResetBtn);
        eqHeaderLayout->addLayout(presetLayout);

        eqSeparator = new QFrame(this);
        eqSeparator->setFrameShape(QFrame::HLine);
        eqSeparator->setFrameShadow(QFrame::Sunken);

        QHBoxLayout* eqSlidersLayout = new QHBoxLayout();
        eqSlidersLayout->setContentsMargins(10, 10, 10, 10);
        eqSlidersLayout->setSpacing(10);

        // Common EQ bands: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
        QStringList bands = {"31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};
        for (int i = 0; i < bands.size(); ++i) {
            const QString& band = bands[i];
            QVBoxLayout* sliderCol = new QVBoxLayout();
            QSlider* slider = new QSlider(Qt::Vertical, this);
            slider->setRange(0, 100);
            slider->setValue(50); // middle by default
            slider->setEnabled(false); // EQ disabled by default
            slider->setFixedHeight(80);
            
            QLabel* bandLabel = new QLabel(band, this);
            bandLabel->setAlignment(Qt::AlignCenter);
            bandLabel->setStyleSheet("font-size: 9px;");

            sliderCol->addWidget(slider);
            sliderCol->addWidget(bandLabel);
            eqSlidersLayout->addLayout(sliderCol);
            eqSliders.append(slider);

            connect(slider, &QSlider::valueChanged, this, [this, i](int value) {
                // Map 0-100 to -15dB to +15dB (value=50 maps to 0dB)
                float gainDb = ((float)value - 50.0f) * (15.0f / 50.0f);
                this->mixer->setEqBand(i, gainDb);
            });
        }

        connect(eqEnabledCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
            this->mixer->setEqEnabled(checked);
            for (auto* slider : eqSliders) {
                slider->setEnabled(checked);
            }
        });

        connect(eqResetBtn, &QPushButton::clicked, this, [this]() {
            for (auto* slider : eqSliders) {
                slider->setValue(50);
            }
        });

        eqContainerLayout->addLayout(eqHeaderLayout);
        eqContainerLayout->addWidget(eqSeparator);
        eqContainerLayout->addLayout(eqSlidersLayout);
        eqContainer->setLayout(eqContainerLayout);
        eqContainer->setVisible(false); // collapsed by default
        this->mixer->setEqEnabled(false); // disabled by default

        connect(eqToggleBtn, &QPushButton::clicked, [this](bool checked) {
            eqContainer->setVisible(checked);
            eqToggleBtn->setText(QString("%1 %2").arg(checked ? "▼" : "▶").arg(tr("Equalizer")));
        });

        playlistToggleBtn = new QPushButton(QString("▼ %1").arg(tr("Playlist")), this);
        playlistToggleBtn->setCheckable(true);
        playlistToggleBtn->setChecked(true);
        playlistToggleBtn->setStyleSheet("text-align: left; padding: 6px; font-weight: bold;");

        playlistContainer = new QWidget(this);
        QVBoxLayout* containerLayout = new QVBoxLayout(playlistContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);

        QPushButton* addFileButton = new QPushButton(QIcon::fromTheme("list-add"), "", this);
        addFileButton->setToolTip(tr("Add file to playlist"));
        addFileButton->setFixedSize(32, 32);
        connect(addFileButton, &QPushButton::clicked, this, &MainWindow::openButtonTriggered);

        QPushButton* addFolderButton = new QPushButton(QIcon::fromTheme("folder-new"), "", this);
        addFolderButton->setToolTip(tr("Add folder to playlist"));
        addFolderButton->setFixedSize(32, 32);
        connect(addFolderButton, &QPushButton::clicked, this, &MainWindow::addFolderTriggered);

        QHBoxLayout* bottomButtonsLayout = new QHBoxLayout();
        bottomButtonsLayout->setAlignment(Qt::AlignLeft);
        bottomButtonsLayout->addWidget(addFileButton);
        bottomButtonsLayout->addWidget(addFolderButton);

        containerLayout->addWidget(soundList);
        containerLayout->addLayout(bottomButtonsLayout);
        playlistContainer->setLayout(containerLayout);

        connect(playlistToggleBtn, &QPushButton::clicked, [this](bool checked) {
            playlistContainer->setVisible(checked);
            playlistToggleBtn->setText(QString("%1 %2").arg(checked ? "▼" : "▶").arg(tr("Playlist")));
        });

        this->mainLayout->addWidget(winampDisplay);
        this->mainLayout->addLayout(playerLayout);
        this->mainLayout->addWidget(eqToggleBtn);
        this->mainLayout->addWidget(eqContainer);
        this->mainLayout->addWidget(playlistToggleBtn);
        this->mainLayout->addWidget(playlistContainer);

        this->playbackTimer = new QTimer();
        connect(this->playbackTimer, &QTimer::timeout, this, &MainWindow::onPlayback);

        // Bring up the LuaJIT plugin layer and load whatever the user installed.
        this->pluginManager = new PluginManager(this);
        this->pluginManager->loadAllPlugins();
        this->pluginManager->populateMenu(this->pluginsMenu);

        show();

        // On first show the SetFixedSize constraint is computed before every
        // child widget reports its final height, which leaves the bottom
        // playlist buttons overlapped until the playlist is toggled. Re-activate
        // the layout once after the event loop settles to fix it up front.
        QTimer::singleShot(0, this, [this]() {
            this->mainLayout->invalidate();
            this->mainLayout->activate();
            this->centralWidget->adjustSize();
            this->adjustSize();
        });
    }

    void MainWindow::openAddPluginDialog() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("&AddPlugin"), QDir::homePath(), tr("Lua Plugins (*.lua)"));
        if (!path.isEmpty()) {
            this->pluginManager->installPlugin(path);
        }
    }

    void MainWindow::openAboutDialog() {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("About Kalorite"));
        dlg.setFixedSize(440, 260);

        // The app logo (installed as a themed icon under the app id). Fall back
        // through a few candidate paths, then a drawn placeholder, so the
        // easter egg always has something to bounce even from a dev build.
        QPixmap logo = QIcon::fromTheme("io.github.monsler.Kalorite").pixmap(96, 96);
        if (logo.isNull()) logo = windowIcon().pixmap(96, 96);
        if (logo.isNull()) {
            for (const QString& c : {
                    QStringLiteral("/app/share/icons/hicolor/512x512/apps/io.github.monsler.Kalorite.png"),
                    QCoreApplication::applicationDirPath() + "/data/io.github.monsler.Kalorite.png",
                    QStringLiteral("data/io.github.monsler.Kalorite.png") }) {
                logo = QPixmap(c);
                if (!logo.isNull()) break;
            }
        }
        if (logo.isNull()) {
            logo = QPixmap(96, 96);
            logo.fill(Qt::transparent);
            QPainter pr(&logo);
            pr.setRenderHint(QPainter::Antialiasing);
            pr.setBrush(QColor(0x2d, 0x74, 0xda));
            pr.setPen(Qt::NoPen);
            pr.drawRoundedRect(logo.rect().adjusted(2, 2, -2, -2), 16, 16);
            pr.setPen(Qt::white);
            QFont f = pr.font(); f.setBold(true); f.setPointSize(46); pr.setFont(f);
            pr.drawText(logo.rect(), Qt::AlignCenter, "K");
        }
        logo = logo.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Descriptive text sits to the right of the logo's resting spot.
        QString version = QCoreApplication::applicationVersion();
        if (version.isEmpty()) version = "3.0.0";
        QLabel* text = new QLabel(&dlg);
        text->setGeometry(140, 24, 280, 212);
        text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);
        text->setOpenExternalLinks(true);
        text->setText(tr(
            "<h2 style='margin-bottom:2px'>Kalorite</h2>"
            "<p style='color:gray;margin-top:0'>Version %1</p>"
            "<p>Kalorite is a lightweight audio player. It supports all modern "
            "codecs and have simple and elegant design.</p>"
            "<p>by monsler<br>"
            "<a href='https://github.com/Monsler/Kalorite'>github.com/Monsler/Kalorite</a></p>")
            .arg(version));

        // The logo starts pinned top-left with a small margin; clicking it
        // sets off the bouncing easter egg over the whole dialog.
        BouncingLogo* badge = new BouncingLogo(&dlg, logo);
        badge->move(16, 16);
        badge->raise();

        dlg.exec();
    }

    // ---- Plugin API surface -----------------------------------------------

    void MainWindow::pluginPlay()  { startPlayback(); }
    void MainWindow::pluginPause() { stopPlayback(); }
    void MainWindow::pluginStop()  { stopPlayback(); this->mixer->setPosition(0); }
    void MainWindow::pluginNext()  { onSkipNext(); }
    void MainWindow::pluginPrev()  { onSkipBack(); }

    int  MainWindow::pluginPositionMs() { return this->mixer->getPositionMs(); }
    int  MainWindow::pluginDurationMs() { return this->mixer->getDurationMs(); }
    void MainWindow::pluginSeekMs(int ms) { this->mixer->setPosition(ms); }

    int MainWindow::pluginPlaylistCount() const { return this->soundList->count(); }

    void MainWindow::pluginPlaylistAdd(const QString& path) { addSoundFile(path); }

    void MainWindow::pluginPlaylistRemove(int index) {
        if (index >= 0 && index < this->soundList->count()) {
            delete this->soundList->takeItem(index);
            genShuffle();
        }
    }

    void MainWindow::pluginPlaylistClear() {
        this->soundList->clear();
        genShuffle();
    }

    QString MainWindow::pluginPlaylistPath(int index) const {
        if (index < 0 || index >= this->soundList->count()) return QString();
        return this->soundList->item(index)->data(Qt::UserRole).toString();
    }

    QString MainWindow::pluginPlaylistTitle(int index) const {
        return QFileInfo(pluginPlaylistPath(index)).fileName();
    }

    int MainWindow::pluginCurrentIndex() const { return this->soundList->currentRow(); }

    void MainWindow::pluginPlayIndex(int index) { seekToTrack(index); }

    void MainWindow::savePlaylistTriggered() {
        QString saveFilePath = QFileDialog::getSaveFileName(this, tr("&SavePlaylistAs"), QDir::homePath(), tr("PlaylistFileFilters"));
        if (!saveFilePath.isEmpty()) {
            nlohmann::json playlist = nlohmann::json::array();
            for (int i = 0; i < this->soundList->count(); i++) {
                QListWidgetItem* item = this->soundList->item(i);
                playlist.push_back(item->data(Qt::UserRole).toString().toStdString());
            }

            std::ofstream file(saveFilePath.toStdString());
            file << playlist;
            file.close();
        }
    }

    void MainWindow::openSoundFileDownloadDialog() {
       this->songDownloader->show();
    }

    void MainWindow::loadPlaylistTriggered() {
        QString openPath = QFileDialog::getOpenFileName(this, tr("&LoadPlaylistFrom"), QDir::homePath(), tr("PlaylistFileFilters"));

        if (!openPath.isEmpty()) {
            this->soundList->clear();
            std::ifstream file(openPath.toStdString());

            nlohmann::json playlist;
            file >> playlist;
            file.close();

            for (int i = 0; i < playlist.size(); i++) {
                addSoundFile(QString::fromStdString(playlist[i]));
            }
            if (this->soundList->count() > 0) {
                this->soundList->setCurrentRow(0);
                QListWidgetItem* item = this->soundList->item(0);
                setCurrentSong(item->data(Qt::UserRole).toString().toStdString());
            }

            genShuffle();
        }
    }

    void MainWindow::onSpinTriggered(const int value) {
        this->mixer->setVolume(value);
        this->winampDisplay->setVolume(value);
        if (this->volumeSignal->volume() != value) {
            this->volumeSignal->setVolume(value);
        }
    }

    void MainWindow::onListSelection(QListWidgetItem *item) {
        int id = this->soundList->row(item);
        seekToTrack(id);
    }

    void MainWindow::playbackSliderPressed() {
        stopPlayback();
    }

    void MainWindow::playbackSliderReleased() {
        startPlayback();
        int pos = playbackSlider->value();

        int sec = pos * this->mixer->getDurationMs() / 100;
        this->mixer->setPosition(sec);
        updateTimeLabel();
    }

    void MainWindow::closeEvent(QCloseEvent* event) {
        QApplication::exit();
    }

    void MainWindow::seekToTrack(const int id) {
        int listSize = this->soundList->count();

        if (id >= 0 && id < listSize) {
            this->soundList->setCurrentRow(id);
            this->currentId = id;

            if (m_crossfadeEnabled || mixer->isGaplessEnabled()) {
                // Seamless transition: do NOT call stopPlayback()/startPlayback()
                // to prevent miniaudio sound slots from being brutally uninitialized/stopped
                setCurrentSong(this->soundList->item(id)->data(Qt::UserRole).toString().toStdString());
                this->isPlaying = true;
                this->playbackButton->setIcon(ICON_PAUSE);
                this->playbackTimer->start(50);
            } else {
                stopPlayback();
                setCurrentSong(this->soundList->item(id)->data(Qt::UserRole).toString().toStdString());
                startPlayback();
            }
        }
    }

    void MainWindow::setCurrentSong(const std::string path) {
        this->currentAudio = path;
        setWindowTitle(QStringLiteral("Kalorite - %1").arg(QString::fromStdString(this->currentAudio)));
        this->mixer->setCurrent(path);
        this->winampDisplay->loadAudioFile(QString::fromStdString(path));
        // Force track length to be read and immediately update list widget item duration
        int durMs = this->mixer->getDurationMs();
        this->trackLengthSeconds = durMs / 1000;
        onDurationChanged(durMs);
        updateTimeLabel();

        skipBackButton->setEnabled(true);
        playbackButton->setEnabled(true);
        playbackSlider->setEnabled(true);
        skipForwardButton->setEnabled(true);
        repeatButton->setEnabled(true);

        emit pluginTrackChanged(QString::fromStdString(path), this->soundList->currentRow());
    }

    bool containsItem(QListWidget *list, const QString &text) {
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->data(Qt::UserRole).toString() == text) {
                return true;
            }
        }
        return false;
    }

    void MainWindow::addSoundFile(const QString& filePath) {
        if (filePath.isEmpty()) return;

        // Check if already in list
        for (int i = 0; i < soundList->count(); ++i) {
            if (soundList->item(i)->data(Qt::UserRole).toString() == filePath) {
                return;
            }
        }

        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();

        QListWidgetItem* item = new QListWidgetItem(soundList);
        item->setData(Qt::UserRole, filePath);

        QWidget* widget = new QWidget();
        QHBoxLayout* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(8, 4, 8, 4);

        QLabel* nameLabel = new QLabel(fileName);
        nameLabel->setObjectName("nameLabel");

        QLabel* durationLabel = new QLabel("--:--");
        durationLabel->setObjectName("durationLabel");
        durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        durationLabel->setStyleSheet("color: #ffffff; font-family: monospace;");

        layout->addWidget(nameLabel);
        layout->addWidget(durationLabel);
        widget->setLayout(layout);

        soundList->setItemWidget(item, widget);

        if (this->currentAudio == "") {
            this->currentAudio = filePath.toStdString();
            this->setCurrentSong(this->currentAudio);
            this->currentId = 0;
            this->soundList->setCurrentRow(0);
        }

        skipBackButton->setEnabled(true);
        playbackButton->setEnabled(true);
        playbackSlider->setEnabled(true);
        skipForwardButton->setEnabled(true);
        repeatButton->setEnabled(true);
        genShuffle();
    }

    void MainWindow::addFolderTriggered() {
        QString dirPath = QFileDialog::getExistingDirectory(this, tr("Add Folder"), QDir::homePath());
        if (dirPath.isEmpty()) return;

        QDirIterator it(dirPath, QStringList() << "*.mp3" << "*.ogg" << "*.wav" << "*.flac" << "*.m4a", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            addSoundFile(it.next());
        }
    }

    void MainWindow::onDurationChanged(qint64 durationMs) {
        int currentId = this->soundList->currentRow();
        if (currentId >= 0 && currentId < this->soundList->count()) {
            QListWidgetItem* item = this->soundList->item(currentId);
            QWidget* widget = this->soundList->itemWidget(item);
            if (widget) {
                QLabel* durationLabel = widget->findChild<QLabel*>("durationLabel");
                if (durationLabel) {
                    int totalSeconds = durationMs / 1000;
                    int minutes = (totalSeconds % 3600) / 60;
                    int seconds = totalSeconds % 60;
                    durationLabel->setText(QString("%1:%2")
                        .arg(minutes, 2, 10, QChar('0'))
                        .arg(seconds, 2, 10, QChar('0')));
                }
            }
        }
    }

    void MainWindow::openButtonTriggered() {
        QString openPath = QFileDialog::getOpenFileName(this, tr("OpenSound"), QDir::homePath(), tr("FileFilters"));
        if (!openPath.isEmpty()) {
            addSoundFile(openPath);
        }
    }

    void MainWindow::onContextMenuSoundList(const QPoint &pos) {
        QListWidgetItem* item = this->soundList->itemAt(pos);

        if (item) {
            QMenu menu;

            QAction* actionRemove = menu.addAction(tr("RemoveFromList"));
            actionRemove->setIcon(QIcon::fromTheme("document-close"));
            connect(actionRemove, &QAction::triggered, [item, this] () {
                delete this->soundList->takeItem(this->soundList->currentRow());
                this->setWindowTitle("Kalorite");
                this->genShuffle();
            });

            menu.exec(this->soundList->viewport()->mapToGlobal(pos));
        }
    }

    void MainWindow::startPlayback() {
        this->isPlaying = true;
        this->playbackButton->setIcon(ICON_PAUSE);

        this->mixer->play();
        this->playbackTimer->start(50);

        emit pluginPlaybackStateChanged("playing");
    }

    void MainWindow::onPlayback() {
        updateTimeLabel();

        // Pass settings state directly to miniaudio Mixer
        mixer->setCrossfadeEnabled(m_crossfadeEnabled);
        mixer->setSmartGainEnabled(m_smartGainEnabled);
        mixer->setBitPerfectEnabled(m_bitPerfectEnabled);

        // Check if current song has finished playing (when not crossfading or if crossfade completes)
        // We only trigger track end if we are supposed to be playing (isPlaying is true),
        // the mixer is not playing, and we have actually progressed past the start (position > 0)
        // or the track has a duration and we are at the very end.
        if (isPlaying && !mixer->isPlaying() && mixer->getPositionMs() >= (mixer->getDurationMs() - 100)) {
            emit pluginTrackFinished(QString::fromStdString(this->currentAudio));
            // Track has ended - handle looping/next track logic
            if (loopType == 0) {
                stopPlayback();
                this->mixer->setPosition(0);
            } else if(loopType == 1) {
                this->mixer->setPosition(0);
                startPlayback();
            } else if (loopType == 2) {
                int newTrack = this->currentId + 1;
                if (newTrack < this->soundList->count()) {
                    this->currentId = newTrack;
                    this->soundList->setCurrentRow(this->currentId);
                    setCurrentSong(this->soundList->item(this->currentId)->data(Qt::UserRole).toString().toStdString());
                } else {
                    this->currentId = 0;
                    this->soundList->setCurrentRow(this->currentId);
                    setCurrentSong(this->soundList->item(this->currentId)->data(Qt::UserRole).toString().toStdString());
                }
                startPlayback();
            } else if (loopType == 3) {
                if (shufflePos == shuffle.size() - 1) {
                    qDebug() << "Reached shuffle end; Generating a new one";
                    genShuffle();
                }
                this->currentId = shuffle[shufflePos];
                this->soundList->setCurrentRow(this->currentId);
                setCurrentSong(this->soundList->item(this->currentId)->data(Qt::UserRole).toString().toStdString());
                startPlayback();
                shufflePos++;
            }
        }
    }

    void MainWindow::setPlaybackPos(const int percent) {

    }

    void MainWindow::updateTimeLabel() {
        int durationMs = this->mixer->getDurationMs();
        this->trackLengthSeconds = durationMs / 1000;
        int totalMinutes = (this->trackLengthSeconds % 3600) / 60;
        int totalHours = this->trackLengthSeconds / 3600;
        int totalSecondsLength = this->trackLengthSeconds % 60;

        int posMs = mixer->getPositionMs();
        int totalSeconds = posMs / 1000;
        int minutes = (totalSeconds % 3600) / 60;
        int hours = totalSeconds / 3600;
        int seconds = totalSeconds % 60;
        
        float played = 0.0f;
        if (trackLengthSeconds > 0) {
            played = std::round((float(totalSeconds) / float(trackLengthSeconds)) * 100.0f);
        }

        QString textValue = QString("%1:%2:%3 / %4:%5:%6")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(totalHours, 2, 10, QChar('0'))
            .arg(totalMinutes, 2, 10, QChar('0'))
            .arg(totalSecondsLength, 2, 10, QChar('0'));

        this->timeLabel->setText(textValue);
        this->playbackSlider->setValue(played);
        this->winampDisplay->setPlaybackState(this->isPlaying, posMs, durationMs);

        this->percent.emitPercent(played);
    }

    void MainWindow::onRepeatButtonTriggered() {
        loopType++;
        if (loopType > 3) {
            loopType = 0;
        }

        if (loopType == 0) {
            repeatButton->setIcon(ICON_REPEAT);
            repeatButton->setToolTip(tr("TooltipNoRepeat"));
        } else if (loopType == 1) {
            repeatButton->setIcon(ICON_REPEAT_CURRENT);
            repeatButton->setToolTip(tr("TooltipRepeatSingleTrack"));
        } else if (loopType == 2) {
            repeatButton->setToolTip(tr("TooltipRepeatList"));
            repeatButton->setIcon(ICON_REPEAT_THE_LIST);
        } else if (loopType == 3) {
            repeatButton->setToolTip(tr("TooltipRepeatShuffle"));
            genShuffle();
            shufflePos = 0;
            repeatButton->setIcon(ICON_REPEAT_SHUFFLE);
        }
    }

    void MainWindow::onSkipBack() {
        if (this->playbackSlider->value() > 5) {
            this->playbackSlider->setValue(0);
            this->mixer->setPosition(0);
        } else {
            int id = this->soundList->currentRow() - 1;
            seekToTrack(id);
        }
    }

    void MainWindow::onSkipNext() {
        if (this->playbackSlider->value() < 95) {
            this->playbackSlider->setValue(95);
            this->mixer->setPosition(95 * this->mixer->getDurationMs() / 100);
        } else {
            int id = this->soundList->currentRow() + 1;
            seekToTrack(id);
        }
    }

    // Fired by the mixer shortly before the current track ends (crossfade/gapless
    // modes). Loads the next track seamlessly so it overlaps the outgoing one,
    // rather than going through onSkipNext's in-track 95% seek shortcut.
    void MainWindow::onCrossfadeAdvance() {
        if (!isPlaying) return;

        int nextId = -1;
        switch (loopType) {
            case 0: // No repeat: advance if there is a next track, otherwise let it end.
                if (this->currentId + 1 < this->soundList->count()) nextId = this->currentId + 1;
                break;
            case 1: // Repeat one: restart the same track.
                nextId = this->currentId;
                break;
            case 2: // Repeat all: advance and wrap around.
                nextId = (this->currentId + 1 < this->soundList->count()) ? this->currentId + 1 : 0;
                break;
            case 3: // Shuffle.
                if (shufflePos >= (int)shuffle.size() - 1) {
                    qDebug() << "Reached shuffle end; Generating a new one";
                    genShuffle();
                }
                nextId = shuffle[shufflePos];
                shufflePos++;
                break;
        }

        if (nextId < 0) return;

        this->currentId = nextId;
        this->soundList->setCurrentRow(nextId);
        setCurrentSong(this->soundList->item(nextId)->data(Qt::UserRole).toString().toStdString());
        // Keep playback state; the mixer handles the seamless overlap internally.
        this->isPlaying = true;
        this->playbackButton->setIcon(ICON_PAUSE);
        if (!this->playbackTimer->isActive()) this->playbackTimer->start(50);
    }

    void MainWindow::genShuffle() {
        int size = this->soundList->count();
        shufflePos = 0;

        shuffle.resize(size);

        for (int i = 0; i < size; i++) {
            shuffle[i] = i;
        }

        std::random_device rd;
        std::mt19937_64 g(rd());

        std::shuffle(shuffle.begin(), shuffle.end(), g);
    }

    void MainWindow::onMediaPlayerStatusChanged(QMediaPlayer::PlaybackState status) {
        // Obsolete but kept for signature compilation compatibility
    }

    void MainWindow::stopPlayback() {
        this->isPlaying = false;
        this->playbackButton->setIcon(ICON_PLAY);

        this->mixer->pause();
        this->playbackTimer->stop();

        // The update timer that normally feeds setPlaybackState() has just been
        // stopped, so tell the visualizer about the pause directly. Otherwise it
        // stays in the "playing" state and, with no fresh samples (notably in
        // Bit Perfect mode where the engine is bypassed), falls back to random
        // simulation and visibly trembles.
        this->winampDisplay->setPlaybackState(false, mixer->getPositionMs(), mixer->getDurationMs());

        emit pluginPlaybackStateChanged("paused");
    }

    void MainWindow::onPlayTriggered() {
        this->isPlaying = !this->isPlaying;

        if (this->isPlaying) {
            startPlayback();
        } else {
            stopPlayback();
        }
    }

    void MainWindow::onContextMenuWinampDisplay(const QPoint &pos) {
        QMenu menu(this);

        // Display Mode submenu
        QMenu* modeSubMenu = menu.addMenu(tr("&View"));
        QAction* retroAct = modeSubMenu->addAction(tr("Retro Display Mode"));
        retroAct->setCheckable(true);
        retroAct->setChecked(!winampDisplay->isModernMode());
        QAction* modernAct = modeSubMenu->addAction(tr("Modern Display Mode"));
        modernAct->setCheckable(true);
        modernAct->setChecked(winampDisplay->isModernMode());

        connect(retroAct, &QAction::triggered, [this]() {
            winampDisplay->setModernMode(false);
            volumeSignal->setModernMode(false);
        });
        connect(modernAct, &QAction::triggered, [this]() {
            winampDisplay->setModernMode(true);
            volumeSignal->setModernMode(true);
        });

        menu.addSeparator();

        // Audio devices submenu
        QMenu* devicesMenu = menu.addMenu(tr("Audio Device"));
        
        // Cache devices or query them. Since querying QMediaDevices on demand causes stutters,
        // we can fetch them here. Note: to fully prevent the first-open stutter, we query it,
        // but QMediaDevices::audioOutputs() is slow.
        static QList<QAudioDevice> cachedDevices;
        static bool devicesFetched = false;
        if (!devicesFetched) {
            cachedDevices = QMediaDevices::audioOutputs();
            devicesFetched = true;
        }

        std::string currentDeviceName = mixer->getCurrentDeviceName();

        for (const QAudioDevice& device : cachedDevices) {
            QAction* devAct = devicesMenu->addAction(device.description());
            devAct->setCheckable(true);
            if (device.description().toStdString() == currentDeviceName) {
                devAct->setChecked(true);
            }
            connect(devAct, &QAction::triggered, [this, device]() {
                mixer->setDeviceByName(device.description().toStdString());
            });
        }

        // Crossfade Action
        QAction* crossfadeAct = menu.addAction(tr("Enable Crossfade"));
        crossfadeAct->setCheckable(true);
        crossfadeAct->setChecked(m_crossfadeEnabled);
        connect(crossfadeAct, &QAction::triggered, [this](bool checked) {
            m_crossfadeEnabled = checked;
        });

        // Gapless Playback Action
        QAction* gaplessAct = menu.addAction(tr("Enable Gapless Playback"));
        gaplessAct->setCheckable(true);
        gaplessAct->setChecked(mixer->isGaplessEnabled());
        connect(gaplessAct, &QAction::triggered, [this](bool checked) {
            mixer->setGaplessEnabled(checked);
        });

        // Double Buffering Action
        QAction* dbAct = menu.addAction(tr("Enable Double Buffering"));
        dbAct->setCheckable(true);
        dbAct->setChecked(mixer->isDoubleBufferingEnabled());
        connect(dbAct, &QAction::triggered, [this](bool checked) {
            mixer->setDoubleBufferingEnabled(checked);
        });

        // Smart Gain Action (peak limiting to avoid clipping)
        QAction* smartGainAct = menu.addAction(tr("Enable Smart Gain"));
        smartGainAct->setCheckable(true);
        smartGainAct->setChecked(m_smartGainEnabled);
        smartGainAct->setEnabled(!m_bitPerfectEnabled); // Bit Perfect bypasses all processing
        connect(smartGainAct, &QAction::triggered, [this](bool checked) {
            m_smartGainEnabled = checked;
        });

        // Bit Perfect Action (streams straight to the sound card, no processing)
        QAction* bitPerfectAct = menu.addAction(tr("Enable Bit Perfect"));
        bitPerfectAct->setCheckable(true);
        bitPerfectAct->setChecked(m_bitPerfectEnabled);
        connect(bitPerfectAct, &QAction::triggered, [this](bool checked) {
            m_bitPerfectEnabled = checked;
            if (checked) {
                // Bit Perfect is exclusive: it cannot coexist with mixing effects.
                m_smartGainEnabled = false;
                m_crossfadeEnabled = false;
            }
        });

        menu.exec(pos);
    }
} // namespace Kalorite
