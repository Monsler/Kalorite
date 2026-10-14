#include "MainWindow.hpp"
#include "AboutDialog.hpp"
#include "Mixer.hpp"
#include "CddaSource.hpp"
#include "PluginManager.hpp"
#include "PluginApi.hpp"
#include "PlaybackController.hpp"
#include "Playlist.hpp"
#include "PlaylistModel.hpp"
#include "PlaylistPanel.hpp"
#include "CoverImageProvider.hpp"
#include "EqualizerPanel.hpp"
#include "ClipboardWatcher.hpp"
#include "MenuNode.hpp"
#include "SongDownloader.hpp"
#include "WinampDisplay.hpp"
#include "PatternVisualizer.hpp"


#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QInputDialog>
#include <QMessageBox>
#include <QQmlContext>
#include <QQuickItem>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <cmath>

namespace Kalorite {

MainWindow::MainWindow() {
    setTitle("Kalorite");
    setIcon(QIcon("kalorite"));
    setFlags((flags() | Qt::MSWindowsFixedSizeDialogHint) & ~Qt::WindowMaximizeButtonHint);
    resize(300, 150);
    setResizeMode(QQuickView::SizeRootObjectToView);
    setColor(QGuiApplication::palette().color(QPalette::Window));
    mixer = new Mixer();
    mixer->setVolume(100);

    playlist = new Playlist(this);
    playlistBackend = new PlaylistBackend(playlist, tr("RemoveFromList"), this);
    playback = new PlaybackController(mixer, playlist, this);
    eqBackend = new EqualizerBackend(mixer, this);

    connect(playlistBackend, &PlaylistBackend::trackActivated, playback, &PlaybackController::selectTrack);
    connect(playlistBackend, &PlaylistBackend::queueToggleRequested, playback, &PlaybackController::toggleQueue);
    connect(playlistBackend, &PlaylistBackend::trackRemoved, this, &MainWindow::onTrackRemoved);

    connect(playback, &PlaybackController::playStateChanged, this, &MainWindow::onPlayStateChanged);
    connect(playback, &PlaybackController::trackLoaded, this, &MainWindow::onTrackLoaded);
    connect(playback, &PlaybackController::tick, this, &MainWindow::onTick);
    connect(playback, &PlaybackController::loopTypeChanged, this, &MainWindow::onLoopTypeChangedSlot);
    connect(playback, &PlaybackController::controlsAvailable, this, &MainWindow::enableControls);

    winampDisplay = new WinampDisplay();
    winampDisplay->setMixer(mixer);
    winampDisplay->setVolume(100);

    patternVisualizer = new PatternVisualizer();
    patternVisualizer->setMixer(mixer);
    patternVisualizer->setVolume(100);


    connect(winampDisplay, &WinampDisplay::modeChanged, this, &MainWindow::setModernMode);
    connect(winampDisplay, &WinampDisplay::contextMenuRequested, this, [this](const QPointF& pos) {
        rebuildContextMenu();
        if (rootObject()) {
            QPointF scenePos = winampDisplay->mapToScene(pos);
            QMetaObject::invokeMethod(rootObject(), "showContextMenu",
                                      Q_ARG(QVariant, scenePos.x()),
                                      Q_ARG(QVariant, scenePos.y()));
        }
    });
    connect(patternVisualizer, &PatternVisualizer::contextMenuRequested, this, [this](const QPointF& pos) {
        rebuildContextMenu();
        if (rootObject()) {
            QPointF scenePos = patternVisualizer->mapToScene(pos);
            QMetaObject::invokeMethod(rootObject(), "showContextMenu",
                                      Q_ARG(QVariant, scenePos.x()),
                                      Q_ARG(QVariant, scenePos.y()));
        }
    });

    setupMenus();
    applyLoadedSettings();

    // Bring up the LuaJIT plugin layer
    pluginApi = new PluginApi(playback, playlist, mixer, this);
    connect(pluginApi, &PluginApi::nextRequested, this, &MainWindow::onSkipNext);
    connect(pluginApi, &PluginApi::prevRequested, this, &MainWindow::onSkipBack);
    connect(pluginApi, &PluginApi::statusMessage, this, [this](const QString& text) {
        showStatusMessage(text, 5000);
    });
    pluginManager = new PluginManager(pluginApi);
    pluginManager->loadAllPlugins();
    pluginManager->populateMenu(m_pluginsMenu);

    songDownloader = new SongDownloader(this);
    aboutDialog = new AboutDialog(this);
    clipboardWatcher = new ClipboardWatcher(nullptr, this);
    connect(clipboardWatcher, &ClipboardWatcher::downloadRequested, this, [this](const QString& url) {
        songDownloader->showWithUrl(url);
    });

    // Expose context properties to QML
    rootContext()->setContextProperty(QStringLiteral("mainWindow"), this);
    rootContext()->setContextProperty(QStringLiteral("playlistModel"), playlist->model());
    rootContext()->setContextProperty(QStringLiteral("pl"), playlistBackend);
    rootContext()->setContextProperty(QStringLiteral("eq"), eqBackend);

    engine()->addImageProvider(QStringLiteral("cover"), new CoverImageProvider(playlist->model()));

    setSource(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));

    // Mount visualizers and volume signal into QML containers
    if (rootObject()) {
        auto* winampParent = rootObject()->findChild<QQuickItem*>("winampContainer");
        if (winampParent) {
            winampDisplay->setParentItem(winampParent);
            winampDisplay->setParent(this);
            auto sync = [winampParent, this]() {
                winampDisplay->setSize(winampParent->size());
            };
            connect(winampParent, &QQuickItem::widthChanged, this, sync);
            connect(winampParent, &QQuickItem::heightChanged, this, sync);
            sync();
        }

        auto* patternParent = rootObject()->findChild<QQuickItem*>("patternContainer");
        if (patternParent) {
            patternVisualizer->setParentItem(patternParent);
            patternVisualizer->setParent(this);
            auto sync = [patternParent, this]() {
                patternVisualizer->setSize(patternParent->size());
            };
            connect(patternParent, &QQuickItem::widthChanged, this, sync);
            connect(patternParent, &QQuickItem::heightChanged, this, sync);
            sync();
        }

        auto updateSize = [this]() {
            if (rootObject()) {
                int w = static_cast<int>(rootObject()->implicitWidth());
                int h = static_cast<int>(rootObject()->implicitHeight());
                if (w > 0 && h > 0) {
                    setMinimumWidth(w);
                    setMaximumWidth(w);
                    setMinimumHeight(h);
                    setMaximumHeight(h);
                    resize(w, h);
                }
            }
        };
        connect(rootObject(), &QQuickItem::implicitWidthChanged,  this, updateSize);
        connect(rootObject(), &QQuickItem::implicitHeightChanged, this, updateSize);
        updateSize();
    }

    show();

    if (!m_settings.get("greeting_played", false)) {
        m_settings.set("greeting_played", true);
        QTimer::singleShot(300, this, &MainWindow::playFirstRunGreeting);
    }
}

MainWindow::~MainWindow() {
    delete pluginManager;
    delete mixer;
}

void MainWindow::playFirstRunGreeting() {
    QString outPath = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                          .filePath("greeting.mp3");
    QDir().mkpath(QFileInfo(outPath).absolutePath());
    QFile::remove(outPath);
    if (QFile::copy(":/audio/greeting.mp3", outPath)) {
        QFile::setPermissions(outPath, QFile::ReadOwner | QFile::WriteOwner);
    }
    if (!QFileInfo::exists(outPath)) return;

    winampDisplay->loadAudioFile(outPath);
    playback->playDetached(outPath);
}

void MainWindow::setupMenus() {
    m_rootMenu = new MenuNode(this);
    m_contextMenu = new MenuNode(this);

    // File Menu
    m_fileMenu = m_rootMenu->addItem(tr("&File"));

    auto* openSongAction = m_fileMenu->addItem(tr("&OpenSong"), QStringLiteral("Ctrl+O"), QStringLiteral("list-add"));
    connect(openSongAction, &MenuNode::triggered, this, &MainWindow::openButtonTriggered);

    auto* savePlaylistAction = m_fileMenu->addItem(tr("&SavePlaylistAs"), QStringLiteral("Ctrl+S"), QStringLiteral("document-save-as"));
    connect(savePlaylistAction, &MenuNode::triggered, this, &MainWindow::onSavePlaylist);

    auto* loadPlaylistAction = m_fileMenu->addItem(tr("&LoadPlaylistFrom"), QStringLiteral("Ctrl+V"), QStringLiteral("document-open"));
    connect(loadPlaylistAction, &MenuNode::triggered, this, &MainWindow::onLoadPlaylist);

    auto* downloadSoundAction = m_fileMenu->addItem(tr("&DownloadSound"), QStringLiteral("Ctrl+D"), QStringLiteral("download"));
    connect(downloadSoundAction, &MenuNode::triggered, this, &MainWindow::openSoundFileDownloadDialog);

    auto* addPluginAction = m_fileMenu->addItem(tr("&AddPlugin"), {}, QStringLiteral("insert-object"));
    connect(addPluginAction, &MenuNode::triggered, this, &MainWindow::openAddPluginDialog);

    auto* exitAction = m_fileMenu->addItem(tr("&Exit"), QStringLiteral("Ctrl+Esc"), QStringLiteral("application-exit"));
    connect(exitAction, &MenuNode::triggered, this, []() {
        QGuiApplication::quit();
    });

    // View Menu
    m_viewMenu = m_rootMenu->addItem(tr("&View"));

    m_retroAction = m_viewMenu->addItem(tr("Retro Display Mode"));
    m_retroAction->setCheckable(true);
    m_retroAction->setChecked(true);
    connect(m_retroAction, &MenuNode::triggered, this, [this]() {
        setModernMode(false);
    });

    m_modernAction = m_viewMenu->addItem(tr("Modern Display Mode"));
    m_modernAction->setCheckable(true);
    connect(m_modernAction, &MenuNode::triggered, this, [this]() {
        setModernMode(true);
    });

    m_viewMenu->addSeparator();

    m_showPatternVizAction = m_viewMenu->addItem(tr("Show Pattern Visualizer"));
    m_showPatternVizAction->setCheckable(true);
    bool patternVizVisible = m_settings.get("pattern_visualizer_visible", true);
    m_showPatternVizAction->setChecked(patternVizVisible);
    m_patternVizVisible = patternVizVisible;
    connect(m_showPatternVizAction, &MenuNode::triggered, this, [this](bool checked) {
        setPatternVizVisible(checked);
        m_settings.set("pattern_visualizer_visible", checked);
    });

    // Plugins Menu
    m_pluginsMenu = m_rootMenu->addItem(tr("&Plugins"));

    // Help Menu
    m_helpMenu = m_rootMenu->addItem(tr("&Help"));

    auto* aboutAction = m_helpMenu->addItem(tr("&About"), {}, QStringLiteral("help-about"));
    connect(aboutAction, &MenuNode::triggered, this, &MainWindow::openAboutDialog);

    auto* telegramAction = m_helpMenu->addItem(tr("Telegram Channel"), {}, QStringLiteral("telegram"));
    connect(telegramAction, &MenuNode::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("https://t.me/kalorite_app"));
    });

    rebuildContextMenu();
}

void MainWindow::rebuildContextMenu() {
    m_contextMenu->clear();

    auto* modeSubMenu = m_contextMenu->addItem(tr("&View"));
    auto* retroAct = modeSubMenu->addItem(tr("Retro Display Mode"));
    retroAct->setCheckable(true);
    retroAct->setChecked(!winampDisplay->isModernMode());
    connect(retroAct, &MenuNode::triggered, this, [this]() {
        setModernMode(false);
    });

    auto* modernAct = modeSubMenu->addItem(tr("Modern Display Mode"));
    modernAct->setCheckable(true);
    modernAct->setChecked(winampDisplay->isModernMode());
    connect(modernAct, &MenuNode::triggered, this, [this]() {
        setModernMode(true);
    });

    m_contextMenu->addSeparator();

    auto* devicesMenu = m_contextMenu->addItem(tr("Audio Device"));
    std::string currentDeviceName = mixer->getCurrentDeviceName();
    for (const std::string& name : mixer->getAudioDeviceNames()) {
        auto* devAct = devicesMenu->addItem(QString::fromStdString(name));
        devAct->setCheckable(true);
        devAct->setChecked(name == currentDeviceName);
        connect(devAct, &MenuNode::triggered, this, [this, name]() {
            mixer->setDeviceByName(name);
            m_settings.set("audio_device", name);
        });
    }

    auto* crossfadeAct = m_contextMenu->addItem(tr("Enable Crossfade"));
    crossfadeAct->setCheckable(true);
    crossfadeAct->setChecked(playback->crossfadeEnabled());
    connect(crossfadeAct, &MenuNode::triggered, this, [this](bool checked) {
        playback->setCrossfadeEnabled(checked);
        m_settings.set("crossfade_enabled", checked);
    });

    auto* gaplessAct = m_contextMenu->addItem(tr("Enable Gapless Playback"));
    gaplessAct->setCheckable(true);
    gaplessAct->setChecked(mixer->isGaplessEnabled());
    connect(gaplessAct, &MenuNode::triggered, this, [this](bool checked) {
        mixer->setGaplessEnabled(checked);
        m_settings.set("gapless_enabled", checked);
    });

    auto* dbAct = m_contextMenu->addItem(tr("Enable Double Buffering"));
    dbAct->setCheckable(true);
    dbAct->setChecked(mixer->isDoubleBufferingEnabled());
    connect(dbAct, &MenuNode::triggered, this, [this](bool checked) {
        mixer->setDoubleBufferingEnabled(checked);
        m_settings.set("double_buffering_enabled", checked);
    });

    auto* smartGainAct = m_contextMenu->addItem(tr("Enable Smart Gain"));
    smartGainAct->setCheckable(true);
    smartGainAct->setChecked(playback->smartGainEnabled());
    smartGainAct->setEnabled(!playback->bitPerfectEnabled());
    connect(smartGainAct, &MenuNode::triggered, this, [this](bool checked) {
        playback->setSmartGainEnabled(checked);
        m_settings.set("smart_gain_enabled", checked);
    });

    auto* bitPerfectAct = m_contextMenu->addItem(tr("Enable Bit Perfect"));
    bitPerfectAct->setCheckable(true);
    bitPerfectAct->setChecked(playback->bitPerfectEnabled());
    connect(bitPerfectAct, &MenuNode::triggered, this, [this](bool checked) {
        playback->setBitPerfectEnabled(checked);
        if (checked) {
            playback->setSmartGainEnabled(false);
            playback->setCrossfadeEnabled(false);
            m_settings.set("smart_gain_enabled", false);
            m_settings.set("crossfade_enabled", false);
        }
        m_settings.set("bit_perfect_enabled", checked);
    });
}

void MainWindow::setModernMode(bool modern) {
    winampDisplay->setModernMode(modern);
    if (m_retroAction) m_retroAction->setChecked(!modern);
    if (m_modernAction) m_modernAction->setChecked(modern);
    m_settings.set("modern_mode", modern);
}

void MainWindow::setPatternVizVisible(bool visible) {
    if (m_patternVizVisible == visible) return;
    m_patternVizVisible = visible;
    if (m_showPatternVizAction) m_showPatternVizAction->setChecked(visible);
    emit patternVizVisibleChanged();
}

void MainWindow::setEqVisible(bool visible) {
    if (m_eqVisible == visible) return;
    m_eqVisible = visible;
    emit eqVisibleChanged();
}

void MainWindow::setPlaylistVisible(bool visible) {
    if (m_playlistVisible == visible) return;
    m_playlistVisible = visible;
    emit playlistVisibleChanged();
}

bool MainWindow::isPlaying() const {
    return playback ? playback->isPlaying() : false;
}

int MainWindow::loopType() const {
    return playback ? playback->loopType() : 0;
}

QString MainWindow::repeatIcon() const {
    int lt = loopType();
    if (lt == 1) return QStringLiteral("media-playlist-repeat-song");
    if (lt == 2) return QStringLiteral("media-playlist-repeat-amarok");
    if (lt == 3) return QStringLiteral("media-playlist-shuffle");
    return QStringLiteral("media-playlist-repeat");
}

QString MainWindow::repeatTooltip() const {
    int lt = loopType();
    if (lt == 1) return tr("TooltipRepeatSingleTrack");
    if (lt == 2) return tr("TooltipRepeatList");
    if (lt == 3) return tr("TooltipRepeatShuffle");
    return tr("TooltipNoRepeat");
}

void MainWindow::showStatusMessage(const QString& text, int timeoutMs) {
    m_statusMessage = text;
    emit statusMessageChanged();
    QTimer::singleShot(timeoutMs, this, [this, text]() {
        if (m_statusMessage == text) {
            m_statusMessage.clear();
            emit statusMessageChanged();
        }
    });
}

void MainWindow::applyLoadedSettings() {
    bool modern = m_settings.get("modern_mode", false);
    setModernMode(modern);

    playback->setCrossfadeEnabled(m_settings.get("crossfade_enabled", false));
    playback->setSmartGainEnabled(m_settings.get("smart_gain_enabled", false));
    playback->setBitPerfectEnabled(m_settings.get("bit_perfect_enabled", false));
    mixer->setGaplessEnabled(m_settings.get("gapless_enabled", false));
    mixer->setDoubleBufferingEnabled(m_settings.get("double_buffering_enabled", false));

    if (m_settings.contains("audio_device")) {
        std::string dev = m_settings.get<std::string>("audio_device", std::string());
        if (!dev.empty()) mixer->setDeviceByName(dev);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    m_settings.save();
    QGuiApplication::quit();
}

void MainWindow::addSoundFile(const QString& filePath, const QString& displayName, qint64 durationMs) {
    playback->addTrack(filePath, displayName, durationMs);
}

void MainWindow::openSoundFileDownloadDialog() {
    songDownloader->show();
}

void MainWindow::openAddPluginDialog() {
    QString path = QFileDialog::getOpenFileName(
        nullptr, tr("&AddPlugin"), QDir::homePath(), tr("Lua Plugins (*.lua)"));
    if (!path.isEmpty()) {
        pluginManager->installPlugin(path);
    }
}

void MainWindow::openAboutDialog() {
    aboutDialog->show();
}

void MainWindow::openButtonTriggered() {
    QString openPath = QFileDialog::getOpenFileName(nullptr, tr("OpenSound"), QDir::homePath(), tr("FileFilters"));
    if (!openPath.isEmpty()) {
        playback->addTrack(openPath);
    }
}

void MainWindow::addFolderTriggered() {
    QString dirPath = QFileDialog::getExistingDirectory(nullptr, tr("Add Folder"), QDir::homePath());
    if (dirPath.isEmpty()) return;

    QDirIterator it(dirPath, QStringList() << "*.mp3" << "*.ogg" << "*.wav" << "*.flac" << "*.m4a", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        playback->addTrack(it.next());
    }
}

void MainWindow::openAudioCdTriggered() {
#ifdef KALORITE_HAVE_CDDA
    std::vector<std::string> drives = CddaSource::listDrives();
    if (drives.empty()) {
        QMessageBox::warning(nullptr, tr("Open Audio CD"),
            tr("No optical drive was found."));
        return;
    }

    std::string device = drives.front();
    if (drives.size() > 1) {
        QStringList items;
        for (const std::string& d : drives) items << QString::fromStdString(d);
        bool ok = false;
        QString chosen = QInputDialog::getItem(nullptr, tr("Open Audio CD"),
            tr("Select a drive:"), items, 0, false, &ok);
        if (!ok || chosen.isEmpty()) return;
        device = chosen.toStdString();
    }

    std::vector<CddaTrackInfo> tracks = CddaSource::readToc(device);
    if (tracks.empty()) {
        QMessageBox::warning(nullptr, tr("Open Audio CD"),
            tr("No audio disc could be read in the selected drive."));
        return;
    }

    for (const auto& track : tracks) {
        int totalSec = static_cast<int>(track.lengthMs / 1000);
        QString label = tr("Audio CD — Track %1 (%2:%3)")
            .arg(track.number, 2, 10, QChar('0'))
            .arg(totalSec / 60, 2, 10, QChar('0'))
            .arg(totalSec % 60, 2, 10, QChar('0'));
        playback->addTrack(QString::fromStdString(CddaSource::makePath(device, track.number)),
                           label, track.lengthMs);
    }
#else
    QMessageBox::warning(nullptr, tr("Open Audio CD"),
        tr("Audio CD support is disabled."));
#endif
}

void MainWindow::onSavePlaylist() {
    QString saveFilePath = QFileDialog::getSaveFileName(nullptr, tr("&SavePlaylistAs"), QDir::homePath(), tr("PlaylistFileFilters"));
    if (!saveFilePath.isEmpty()) {
        playlist->saveToFile(saveFilePath);
    }
}

void MainWindow::onLoadPlaylist() {
    QString openPath = QFileDialog::getOpenFileName(nullptr, tr("&LoadPlaylistFrom"), QDir::homePath(), tr("PlaylistFileFilters"));
    if (openPath.isEmpty()) return;

    playlist->clear();
    const QStringList paths = Playlist::readPathsFromFile(openPath);
    for (const QString& path : paths) {
        playback->addTrack(path);
    }
    if (playlist->count() > 0) {
        playlist->setCurrentRow(0);
        playback->loadTrack(playlist->pathAt(0).toStdString());
    }
}

void MainWindow::onPlayClicked() {
    playback->togglePlay();
}

void MainWindow::onRepeatClicked() {
    playback->cycleLoopType();
}

void MainWindow::onSliderPressed() {
    playback->pause();
}

void MainWindow::onSliderReleased() {
    playback->play();
    updateProgress();
}

void MainWindow::onSeek(int value) {
    int durationMs = mixer->getDurationMs();
    int ms = value * durationMs / 100;
    playback->seekMs(ms);
}

void MainWindow::onSkipBack() {
    if (m_progress > 5) {
        playback->seekMs(0);
    } else {
        playback->skipPrevious();
    }
}

void MainWindow::onSkipNext() {
    if (m_progress < 95) {
        int ms = 95 * mixer->getDurationMs() / 100;
        playback->seekMs(ms);
    } else {
        playback->skipNext();
    }
}

void MainWindow::onVolumeChanged(int value) {
    mixer->setVolume(value);
    winampDisplay->setVolume(value);
    patternVisualizer->setVolume(value);
}

void MainWindow::onPlayStateChanged(bool playing) {
    Q_UNUSED(playing);
    emit playStateChanged();
    updateProgress();
}

void MainWindow::onTrackLoaded(const QString& path) {
    winampDisplay->loadAudioFile(path);
    enableControls();
    updateProgress();
    if (playlist && mixer->getDurationMs() > 0) {
        int row = playlist->currentRow();
        if (row >= 0) {
            playlist->setRowDuration(row, mixer->getDurationMs());
        }
    }
}

void MainWindow::onTick() {
    updateProgress();
}

void MainWindow::onLoopTypeChangedSlot(int loopType) {
    Q_UNUSED(loopType);
    emit loopTypeChanged();
}

void MainWindow::enableControls() {
    if (!m_controlsEnabled) {
        m_controlsEnabled = true;
        emit controlsEnabledChanged();
    }
}

void MainWindow::onTrackRemoved() {
    // If current track was removed, playback will advance or stop
}

void MainWindow::updateProgress() {
    int durationMs = mixer->getDurationMs();
    int trackLengthSeconds = durationMs / 1000;
    int posMs = mixer->getPositionMs();
    int totalSeconds = posMs / 1000;

    if (durationMs > 0 && playlist) {
        int row = playlist->currentRow();
        if (row >= 0 && playlist->model()->durationAt(row) == QStringLiteral("--:--")) {
            playlist->setRowDuration(row, durationMs);
        }
    }

    float played = 0.0f;
    if (trackLengthSeconds > 0) {
        played = std::round((float(totalSeconds) / float(trackLengthSeconds)) * 100.0f);
    }

    int progressVal = static_cast<int>(played);
    if (m_progress != progressVal) {
        m_progress = progressVal;
        emit progressChanged();
    }

    winampDisplay->setPlaybackState(playback->isPlaying(), posMs, durationMs);
    patternVisualizer->setPlaying(playback->isPlaying());
    percent.emitPercent(played);
}

} // namespace Kalorite
