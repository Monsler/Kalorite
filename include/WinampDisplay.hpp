#pragma once

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QMap>
#include <QString>
#include <QAudioDecoder>

namespace Kalorite {
    class Mixer;

    class WinampDisplay : public QWidget {
        Q_OBJECT

    public:
        explicit WinampDisplay(QWidget* parent = nullptr);
        ~WinampDisplay() override;

        void setMixer(Mixer* mixer);
        void loadAudioFile(const QString& filePath);
        void setPlaybackState(bool playing, int positionMs, int durationMs);
        void setVolume(int volume);
        void setModernMode(bool modern);
        bool isModernMode() const { return m_modernMode; }

    signals:
        void modeChanged(bool modern);
        void contextMenuRequested(const QPoint& pos);

    private slots:
        void onBufferReady();
        void onDecoderFinished();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void timerEvent(QTimerEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        QSize sizeHint() const override;

    private:
        void updateSimulation();
        void drawRetroDisplay(QPainter& painter);
        void drawModernDisplay(QPainter& painter);
        void drawPixelText(QPainter& painter, const QString& text, int x, int y, const QColor& color, int scale = 1);

        // Average bitrate (kbps) derived from file size and duration; 0 if unknown.
        int computeBitrateKbps() const;

        // Playback and decoding state
        bool m_isPlaying = false;
        int m_positionMs = 0;
        int m_durationMs = 0;
        int m_volume = 100;
        bool m_modernMode = false; // false = Retro, true = Modern

        int m_timerId;
        double m_timeAccumulator = 0.0;

        Mixer* m_mixer = nullptr;
        QAudioDecoder* m_decoder = nullptr;
        QMap<qint64, QVector<double>> m_spectrumFrames;
        QString m_currentFilePath;

        // Visualizer bands (e.g. 20 bands)
        static const int NUM_BANDS = 20;
        struct Band {
            double current = 0.0;    // 0.0 to 1.0
            double target = 0.0;     // 0.0 to 1.0
            double peak = 0.0;       // 0.0 to 1.0
            double peakSpeed = 0.0;  // For physics simulation
            int peakHoldTicks = 0;   // For retro peak hold
        };
        QVector<Band> m_bands;
        double m_phase = 0.0;
    };

} // namespace Kalorite
