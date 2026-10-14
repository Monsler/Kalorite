#pragma once

#include <QQuickPaintedItem>
#include <QTimer>
#include <QVector>
#include <QMap>
#include <QString>

namespace Kalorite {
    class Mixer;

    class WinampDisplay : public QQuickPaintedItem {
        Q_OBJECT

    public:
        explicit WinampDisplay(QQuickItem* parent = nullptr);

        void setMixer(Mixer* mixer);
        void loadAudioFile(const QString& filePath);
        void setPlaybackState(bool playing, int positionMs, int durationMs);
        void setVolume(int volume);
        void setModernMode(bool modern);
        bool isModernMode() const { return m_modernMode; }
        void setThemeAccent(const QColor& color, bool isSystem);

    signals:
        void modeChanged(bool modern);
        void contextMenuRequested(const QPointF& pos);

    protected:
        void paint(QPainter* painter) override;
        void timerEvent(QTimerEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

    private:
        QRect bounds() const { return QRect(0, 0, int(width()), int(height())); }
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
        QColor m_accentColor = QColor(50, 240, 50);
        bool m_isSystemTheme = true;
    };

} // namespace Kalorite
