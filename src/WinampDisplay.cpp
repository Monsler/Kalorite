#include "WinampDisplay.hpp"
#include "AudioSpectrumAnalyzer.hpp"
#include "Mixer.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include <cmath>
#include <random>

namespace Kalorite {

// Simple 5x7 font definition for letters/numbers in Retro mode
static const unsigned char font5x7[][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // '0'
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // '3'
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}, // ':'
    {0x08, 0x08, 0x08, 0x08, 0x08}, // '-'
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x20, 0x10, 0x08, 0x04, 0x02}, // '/'
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, // 'A'
    {0x7f, 0x49, 0x49, 0x49, 0x36}, // 'B'
    {0x3e, 0x41, 0x41, 0x41, 0x22}, // 'C'
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // 'D'
    {0x7f, 0x49, 0x49, 0x49, 0x41}, // 'E'
    {0x7f, 0x09, 0x09, 0x09, 0x01}, // 'F'
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, // 'G'
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, // 'H'
    {0x00, 0x41, 0x7f, 0x41, 0x00}, // 'I'
    {0x20, 0x40, 0x41, 0x3f, 0x01}, // 'J'
    {0x7f, 0x08, 0x14, 0x22, 0x41}, // 'K'
    {0x7f, 0x40, 0x40, 0x40, 0x40}, // 'L'
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, // 'M'
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, // 'N'
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // 'O'
    {0x7f, 0x09, 0x09, 0x09, 0x06}, // 'P'
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, // 'Q'
    {0x7f, 0x09, 0x19, 0x29, 0x46}, // 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S'
    {0x01, 0x01, 0x7f, 0x01, 0x01}, // 'T'
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, // 'U'
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, // 'V'
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, // 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 'X'
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43}  // 'Z'
};

static int getCharIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c == ':') return 10;
    if (c == '-') return 11;
    if (c == ' ') return 12;
    if (c == '/') return 13;
    if (c >= 'A' && c <= 'Z') return 14 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 14 + (c - 'a');
    return 12; // default to space
}

WinampDisplay::WinampDisplay(QWidget* parent)
    : QWidget(parent), m_decoder(nullptr)
{
    // Initialize frequencies
    m_bands.resize(NUM_BANDS);
    for (int i = 0; i < NUM_BANDS; ++i) {
        m_bands[i] = Band();
    }

    // Refresh display at ~60 FPS (16ms)
    m_timerId = startTimer(16);
    
    // Set minimal size
    setMinimumSize(385, 100);
}

void WinampDisplay::setMixer(Mixer* mixer) {
    m_mixer = mixer;
}

WinampDisplay::~WinampDisplay() {
    if (m_decoder) {
        m_decoder->stop();
    }
}

void WinampDisplay::loadAudioFile(const QString& filePath) {
    if (m_decoder) {
        m_decoder->stop();
        delete m_decoder;
        m_decoder = nullptr;
    }
    m_spectrumFrames.clear();
    m_currentFilePath = filePath;
    if (filePath.isEmpty()) return;

    // Disabled to prevent ffmpeg from suspending the application
    // m_decoder = new QAudioDecoder(this);
    // m_decoder->setSource(QUrl::fromLocalFile(filePath));

    // connect(m_decoder, &QAudioDecoder::bufferReady, this, &WinampDisplay::onBufferReady);
    // connect(m_decoder, &QAudioDecoder::finished, this, &WinampDisplay::onDecoderFinished);

    // m_decoder->start();
}

int WinampDisplay::computeBitrateKbps() const {
    if (m_currentFilePath.isEmpty() || m_durationMs <= 0) return 0;
    QFileInfo info(m_currentFilePath);
    if (!info.exists()) return 0;
    qint64 bytes = info.size();
    if (bytes <= 0) return 0;
    // kbps = bytes * 8 / durationSec / 1000
    return (int)((bytes * 8.0) / (m_durationMs / 1000.0) / 1000.0);
}

void WinampDisplay::onBufferReady() {
    /*
    if (!m_decoder) return;
    QAudioBuffer buffer = m_decoder->read();
    if (!buffer.isValid()) return;

    qint64 timeMs = buffer.startTime() / 1000;
    QVector<double> bandValues = AudioSpectrumAnalyzer::analyzeBuffer(buffer, NUM_BANDS);
    m_spectrumFrames[timeMs] = bandValues;
    */
}

void WinampDisplay::onDecoderFinished() {
    /*
    if (m_decoder) {
        m_decoder->deleteLater();
        m_decoder = nullptr;
    }
    */
}

void WinampDisplay::setPlaybackState(bool playing, int positionMs, int durationMs) {
    m_isPlaying = playing;
    m_positionMs = positionMs;
    m_durationMs = durationMs;
}

void WinampDisplay::setVolume(int volume) {
    m_volume = volume;
}

void WinampDisplay::setModernMode(bool modern) {
    if (m_modernMode != modern) {
        m_modernMode = modern;
        emit modeChanged(modern);
        update();
    }
}

void WinampDisplay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        emit contextMenuRequested(event->globalPosition().toPoint());
    } else {
        QWidget::mousePressEvent(event);
    }
}

QSize WinampDisplay::sizeHint() const {
    return QSize(300, 120);
}

void WinampDisplay::timerEvent(QTimerEvent* event) {
    if (event->timerId() == m_timerId) {
        updateSimulation();
        update();
    } else {
        QWidget::timerEvent(event);
    }
}

void WinampDisplay::updateSimulation() {
    m_timeAccumulator += 0.016;
    m_phase += m_isPlaying ? 0.15 : 0.0;

    static std::mt19937 rnd(std::random_device{}());
    std::uniform_real_distribution<double> dist(-0.08, 0.08);

    double volFactor = double(m_volume) / 100.0;

    QVector<double> realBands;
    if (m_isPlaying && m_mixer) {
        std::vector<float> samples = m_mixer->getLatestSamples();
        realBands = AudioSpectrumAnalyzer::analyzeSamples(samples, 2, NUM_BANDS, 44100.0);
    }

    for (int i = 0; i < NUM_BANDS; ++i) {
        Band& b = m_bands[i];
        
        if (m_isPlaying) {
            if (!realBands.isEmpty() && i < realBands.size()) {
                b.target = realBands[i] * volFactor;
            } else {
                // Fallback to simulated audio band values if decoding is in progress
                double wave = std::sin(m_phase + i * 0.6) * 0.4 + 0.4;
                double subWave = std::cos(m_phase * 0.7 - i * 0.3) * 0.2 + 0.2;
                double freqSlope = 1.0 - (double(i) / NUM_BANDS) * 0.6;
                b.target = (wave + subWave + dist(rnd)) * freqSlope * volFactor;
            }
            if (b.target < 0.0) b.target = 0.0;
            if (b.target > 1.0) b.target = 1.0;
        } else {
            b.target = 0.0;
        }

        // Apply physics/interpolation
        if (m_modernMode) {
            // Smooth ease-out towards target
            if (b.current < b.target) {
                b.current += (b.target - b.current) * 0.25; // responsive rise
            } else {
                b.current += (b.target - b.current) * 0.12; // smooth ease-out decay
            }

            // Peak physics: Gravity-based ease-out falling
            const double gravity = 0.0035;
            if (b.peak < b.current) {
                b.peak = b.current;
                b.peakSpeed = 0.0;
            } else {
                b.peakSpeed += gravity;
                b.peak -= b.peakSpeed;
                if (b.peak < 0.0) {
                    b.peak = 0.0;
                    b.peakSpeed = 0.0;
                }
            }
        } else {
            // Retro physics (floating peak, then steady/stepped fall)
            if (b.current < b.target) {
                b.current = b.target;
            } else {
                b.current -= 0.04;
                if (b.current < 0.0) b.current = 0.0;
            }

            if (b.current >= b.peak) {
                b.peak = b.current;
                b.peakHoldTicks = 12; // hold peak for ~12 frames
            } else {
                if (b.peakHoldTicks > 0) {
                    b.peakHoldTicks--;
                } else {
                    b.peak -= 0.02; // slow drift down
                    if (b.peak < 0.0) b.peak = 0.0;
                }
            }
        }
    }
}

void WinampDisplay::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, m_modernMode);

    if (m_modernMode) {
        drawModernDisplay(painter);
    } else {
        drawRetroDisplay(painter);
    }
}

void WinampDisplay::drawPixelText(QPainter& painter, const QString& text, int x, int y, const QColor& color, int scale) {
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    int curX = x;
    for (int charIdx = 0; charIdx < text.length(); ++charIdx) {
        char c = text.at(charIdx).toLatin1();
        int fontIdx = getCharIndex(c);

        for (int col = 0; col < 5; ++col) {
            unsigned char colVal = font5x7[fontIdx][col];
            for (int row = 0; row < 7; ++row) {
                if ((colVal >> row) & 1) {
                    painter.drawRect(curX + col * scale, y + row * scale, scale, scale);
                }
            }
        }
        curX += 6 * scale; // 5 pixels width + 1 pixel spacing
    }
    painter.restore();
}

void WinampDisplay::drawRetroDisplay(QPainter& painter) {
    // 1. Draw background (classic dark gray/green grid pattern)
    painter.fillRect(rect(), QColor(10, 12, 10));

    // Draw background pixel grids
    painter.setPen(QColor(16, 20, 16));
    for (int y = 0; y < height(); y += 4) {
        painter.drawLine(0, y, width(), y);
    }
    for (int x = 0; x < width(); x += 4) {
        painter.drawLine(x, 0, x, height());
    }

    // Outer pixel frame
    painter.setPen(QColor(120, 130, 110));
    painter.drawRect(0, 0, width() - 1, height() - 1);
    painter.setPen(QColor(40, 50, 40));
    painter.drawRect(1, 1, width() - 3, height() - 3);

    // 2. Draw Time display (retro pixelated big fonts)
    int minutes = (m_positionMs / 60000) % 60;
    int seconds = (m_positionMs / 1000) % 60;
    int ms = (m_positionMs / 10) % 100;
    QString timeStr = QString("%1:%2:%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(ms, 2, 10, QChar('0'));

    // Draw dark shadow text behind for pixel LCD effect
    QColor timeShadowColor = m_isSystemTheme ? QColor(20, 32, 20) : QColor(m_accentColor.red() * 0.1, m_accentColor.green() * 0.1, m_accentColor.blue() * 0.1);
    QColor timeColor = m_isSystemTheme ? QColor(50, 240, 50) : m_accentColor;

    drawPixelText(painter, "88:88:88", 15, 12, timeShadowColor, 3);
    drawPixelText(painter, timeStr, 15, 12, timeColor, 3);

    // Extra labels (KBPS, KHZ, MONO/STEREO)
    int channels = m_mixer ? m_mixer->getSourceChannels() : 0;
    int sampleRate = m_mixer ? m_mixer->getSourceSampleRate() : 0;
    QString khzStr = sampleRate > 0 ? QString::number(sampleRate / 1000) : "--";
    int bitrate = computeBitrateKbps();
    QString kbpsStr = bitrate > 0 ? QString::number(bitrate) : "--";

    // The 5x7 pixel font is fixed-width (6px per glyph). Draw each unit label
    // right after its number instead of on top of it — previously the bright
    // value was painted over the dim label at the same position, so the KBPS /
    // KHZ captions were hidden underneath the digits.
    const int glyphW = 6;
    const QColor valueColor = m_isSystemTheme ? QColor(50, 240, 50) : m_accentColor;
    const QColor unitColor = m_isSystemTheme ? QColor(20, 120, 20) : QColor(m_accentColor.red() * 0.4, m_accentColor.green() * 0.4, m_accentColor.blue() * 0.4);
    int x = 195;
    drawPixelText(painter, kbpsStr, x, 12, valueColor, 1);
    x += (kbpsStr.length() + 1) * glyphW;      // number + one space
    drawPixelText(painter, "KBPS", x, 12, unitColor, 1);
    x += (4 + 1) * glyphW;                      // "KBPS" + one space
    drawPixelText(painter, khzStr, x, 12, valueColor, 1);
    x += (khzStr.length() + 1) * glyphW;
    drawPixelText(painter, "KHZ", x, 12, unitColor, 1);

    QColor stereoShadowColor = m_isSystemTheme ? QColor(20, 32, 20) : QColor(m_accentColor.red() * 0.1, m_accentColor.green() * 0.1, m_accentColor.blue() * 0.1);
    QColor stereoColor = m_isSystemTheme ? QColor(20, 200, 20) : m_accentColor;

    drawPixelText(painter, "MONO  STEREO", 195, 23, stereoShadowColor, 1);
    if (channels == 1) {
        drawPixelText(painter, "MONO", 195, 23, stereoColor, 1);
    } else if (channels >= 2) {
        drawPixelText(painter, "      STEREO", 195, 23, stereoColor, 1);
    }

    // 3. Draw Pixel Visualizer
    // Area: x from 15 to width() - 15, y from 45 to height() - 15
    int startX = 15;
    int startY = 42;
    int visWidth = width() - 30;
    int visHeight = height() - startY - 15;

    int numBands = NUM_BANDS;
    // Use a fractional step so the bars span the full width with no gap left
    // over on the right edge (integer division alone truncates that space away).
    const double step = double(visWidth) / numBands;
    int barWidth = std::max(2, int(step) - 2);

    for (int i = 0; i < numBands; ++i) {
        const Band& b = m_bands[i];
        int bx = startX + int(std::round(i * step));

        // Winamp Green to Red color gradient on pixel-by-pixel basis
        int totalSegments = visHeight / 3;
        int activeSegments = std::round(b.current * totalSegments);
        int peakSegment = std::round(b.peak * totalSegments);

        for (int s = 0; s < totalSegments; ++s) {
            int sy = startY + visHeight - s * 3 - 2;
            
            // Segment Color
            QColor col;
            if (s > totalSegments * 0.8) {
                col = QColor(240, 50, 50); // Red
            } else if (s > totalSegments * 0.5) {
                col = QColor(240, 240, 50); // Yellow/Amber
            } else {
                col = m_isSystemTheme ? QColor(50, 240, 50) : m_accentColor; // Theme Accent!
            }

            // Draw inactive grid placeholder
            painter.fillRect(bx, sy, barWidth, 2, col.darker(400));

            // Draw active segment
            if (s < activeSegments) {
                painter.fillRect(bx, sy, barWidth, 2, col);
            }
        }

        // Draw Peak indicator (floating dot)
        if (peakSegment > 0 && peakSegment <= totalSegments) {
            int py = startY + visHeight - peakSegment * 3 - 2;
            QColor col = (peakSegment > totalSegments * 0.8) ? QColor(240, 50, 50) : 
                         ((peakSegment > totalSegments * 0.5) ? QColor(240, 240, 50) : 
                          (m_isSystemTheme ? QColor(50, 240, 50) : m_accentColor));
            painter.fillRect(bx, py, barWidth, 1, col.lighter(120));
        }
    }
}

void WinampDisplay::drawModernDisplay(QPainter& painter) {
    // 1. Draw solid black background
    painter.fillRect(rect(), QColor(0, 0, 0));

    // 2. Draw retro-futuristic white time display (no glow)
    int minutes = (m_positionMs / 60000) % 60;
    int seconds = (m_positionMs / 1000) % 60;
    int ms = (m_positionMs / 10) % 100;
    QString timeStr = QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(ms, 2, 10, QChar('0'));

    painter.save();
    QFont font("Outfit", 20, QFont::Bold);
    painter.setFont(font);
    
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(20, 40, timeStr);

    // Mini metadata info
    QFont miniFont("Outfit", 9, QFont::Medium);
    painter.setFont(miniFont);
    painter.setPen(QColor(180, 180, 180));
    int channels = m_mixer ? m_mixer->getSourceChannels() : 0;
    int sampleRate = m_mixer ? m_mixer->getSourceSampleRate() : 0;
    QString khzStr = sampleRate > 0 ? QString::number(sampleRate / 1000.0, 'f', 1) : "--";
    QString channelStr = channels == 1 ? "MONO" : (channels >= 2 ? "STEREO" : "---");
    int bitrate = computeBitrateKbps();
    QString kbpsStr = bitrate > 0 ? QString::number(bitrate) : "--";
    painter.drawText(width() - 140, 32, QString("%1kbps / %2kHz").arg(kbpsStr).arg(khzStr));
    painter.drawText(width() - 80, 48, channelStr);
    painter.restore();

    // 3. Draw Phosphor Green vector visualizer
    int startX = 20;
    int startY = 60;
    int visWidth = width() - 40;
    int visHeight = height() - startY - 20;

    // Fractional step so bars fill the full width (no leftover gap on the right).
    const double step = double(visWidth) / NUM_BANDS;
    int barWidth = std::max(4, int(step) - 4);

    // Phosphor gradients for retro CRT feel
    QLinearGradient barGrad(0, startY + visHeight, 0, startY);
    if (m_isSystemTheme) {
        barGrad.setColorAt(0.0, QColor(0, 180, 80));
        barGrad.setColorAt(0.8, QColor(0, 255, 128));
        barGrad.setColorAt(1.0, QColor(140, 255, 180));
    } else {
        barGrad.setColorAt(0.0, m_accentColor.darker(150));
        barGrad.setColorAt(0.8, m_accentColor);
        barGrad.setColorAt(1.0, m_accentColor.lighter(150));
    }

    for (int i = 0; i < NUM_BANDS; ++i) {
        const Band& b = m_bands[i];
        int bx = startX + int(std::round(i * step));
        int barValHeight = std::round(b.current * visHeight);

        // Draw rounded vector bar
        if (barValHeight > 2) {
            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setBrush(barGrad);
            painter.drawRoundedRect(bx, startY + visHeight - barValHeight, barWidth, barValHeight, 2, 2);
            painter.restore();
        }

        // Draw Peak indicator line
        int peakY = startY + visHeight - std::round(b.peak * visHeight);
        if (peakY < startY + visHeight) {
            painter.save();
            QColor peakColor = m_isSystemTheme ? QColor(180, 255, 220) : m_accentColor.lighter(150);
            painter.setPen(QPen(peakColor, 1.5, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(bx, peakY, bx + barWidth, peakY);
            painter.restore();
        }
    }

    // 4. CRT Scanlines Overlay
    painter.save();
    painter.setPen(QColor(0, 0, 0, 95)); // thin semi-transparent black lines
    for (int y = 0; y < height(); y += 2) {
        painter.drawLine(0, y, width(), y);
    }

    // 5. Bulbous Screen Vignette & Glass glare effect
    QRadialGradient vignette(rect().center(), width() * 0.7);
    vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
    vignette.setColorAt(0.8, QColor(0, 0, 0, 50));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 150));
    painter.setPen(Qt::NoPen);
    painter.setBrush(vignette);
    painter.drawRect(rect());

    // Screen Glare reflection (curved glass screen look)
    QLinearGradient glareGrad(0, 0, 0, height() * 0.5);
    glareGrad.setColorAt(0.0, QColor(255, 255, 255, 25));
    glareGrad.setColorAt(0.4, QColor(255, 255, 255, 5));
    glareGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setBrush(glareGrad);
    
    QPainterPath path;
    path.addRoundedRect(0, 0, width(), height(), 8, 8);
    painter.setClipPath(path);
    painter.drawRect(0, 0, width(), height());
    painter.restore();
}

void WinampDisplay::setThemeAccent(const QColor& color, bool isSystem) {
    m_accentColor = color;
    m_isSystemTheme = isSystem;
    update();
}

} // namespace Kalorite
