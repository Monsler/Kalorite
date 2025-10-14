#pragma once
#include <qtmetamacros.h>
#include <qmainwindow.h>
#include <qmenubar.h>
#include <qpushbutton.h>
#include <QVBoxLayout>
#include <qslider.h>

namespace Kalorite
{
    class MainWindow : public QMainWindow {
        Q_OBJECT

        public:
        MainWindow();

        private slots:
        void onExitTriggered();
        void openButtonTriggered();

        private:
        QMenuBar* currentMenuBar;
        QMenu* fileMenu;

        QPushButton* playbackButton;
        QVBoxLayout* mainLayout;
        QHBoxLayout* playerLayout;
        QSlider* playbackSlider;
        QWidget* centralWidget;
        
        std::string currentAudio;
        bool isPlaying = false;
    };
} // namespace Kalorite
