#pragma once

#include <QWidget>
#include <QImage>
#include <QVector>
#include <random>

namespace Kalorite {
    class Mixer;

    // WMP9-style pattern visualizer ("Battery"/"Alchemy" ambience): procedural
    // patterns driven by the live spectrum, rendered onto a feedback canvas for
    // trails, with several modes that crossfade smoothly into each other.
    class PatternVisualizer : public QWidget {
        Q_OBJECT

    public:
        explicit PatternVisualizer(QWidget* parent = nullptr);
        ~PatternVisualizer() override = default;

        void setMixer(Mixer* mixer);
        void setPlaying(bool playing);
        void setVolume(int volume);

    signals:
        // Emitted (with a global position) on right-click so the window can show
        // the same context menu as the frequency visualizer.
        void contextMenuRequested(const QPoint& pos);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void timerEvent(QTimerEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        QSize sizeHint() const override;

    private:
        static const int NUM_MODES = 11;
        static const int NUM_BANDS = 20;

        void updateAudio();
        void renderFrame();
        // Picks a random mode different from the current one.
        int pickNextMode();
        // Draws one mode's geometry with the given opacity weight (0..1).
        void drawMode(QPainter& p, int mode, double weight);

        void drawSpiral(QPainter& p, double weight);
        void drawKaleidoscope(QPainter& p, double weight);
        void drawTunnel(QPainter& p, double weight);
        void drawWaveRibbons(QPainter& p, double weight);
        void drawLissajous(QPainter& p, double weight);
        void drawMandala(QPainter& p, double weight);
        void drawStarburst(QPainter& p, double weight);
        void drawFlowField(QPainter& p, double weight);
        // WMP-"Ambience"-style smoky plasma warp with a dark swirling vortex.
        void drawPlasma(QPainter& p, double weight);   // blue nebula palette
        void drawPinwheel(QPainter& p, double weight); // magenta spoked swirl
        void drawAmbienceSpokes(QPainter& p, double weight); // red spiralling spokes
        // Shared per-pixel plasma renderer. spokes = angular spoke strength,
        // hueOffset biases the palette (blue vs. magenta).
        void renderPlasmaField(QPainter& p, double weight, double spokes, double hueOffset);

        Mixer* m_mixer = nullptr;
        bool m_isPlaying = false;
        int m_volume = 100;

        int m_timerId;
        double m_time = 0.0;

        // Smoothed spectrum bands plus overall energy/bass drive.
        QVector<double> m_bands;
        double m_energy = 0.0;
        double m_bass = 0.0;

        // Mode crossfade state: currentMode fades into nextMode over
        // BLEND_SECONDS once HOLD_SECONDS have elapsed.
        int m_currentMode = 0;
        int m_nextMode = 1;
        double m_modeClock = 0.0;
        double m_blend = 0.0; // 0 = fully current, 1 = fully next

        // Feedback canvas for the classic trailing/melting look.
        QImage m_canvas;
        double m_hueBase = 0.0;

        // RNG for non-repeating random mode order (seeded once at startup).
        std::mt19937 m_rng;

        // Reused low-res buffer for the per-pixel plasma modes.
        QImage m_plasmaBuf;
    };

} // namespace Kalorite
