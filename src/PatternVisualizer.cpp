#include "PatternVisualizer.hpp"
#include "AudioSpectrumAnalyzer.hpp"
#include "Mixer.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <cmath>

namespace Kalorite {

namespace {
    constexpr double HOLD_SECONDS = 12.0;  // time a mode stays fully on screen
    constexpr double BLEND_SECONDS = 4.0;  // crossfade duration between modes
}

PatternVisualizer::PatternVisualizer(QWidget* parent)
    : QWidget(parent)
{
    m_bands.resize(NUM_BANDS);
    m_bands.fill(0.0);
    // Fresh seed each launch so the mode order differs every session.
    m_rng.seed(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, NUM_MODES - 1);
    m_currentMode = dist(m_rng);
    m_nextMode = pickNextMode();
    m_timerId = startTimer(16); // ~60 FPS
    setMinimumSize(160, 100);
}

int PatternVisualizer::pickNextMode() {
    if (NUM_MODES <= 1) return m_currentMode;
    // Pick uniformly among all modes except the one on screen now.
    std::uniform_int_distribution<int> dist(0, NUM_MODES - 2);
    int pick = dist(m_rng);
    if (pick >= m_currentMode) ++pick;
    return pick;
}

void PatternVisualizer::setMixer(Mixer* mixer) {
    m_mixer = mixer;
}

void PatternVisualizer::setPlaying(bool playing) {
    m_isPlaying = playing;
}

void PatternVisualizer::setVolume(int volume) {
    m_volume = volume;
}

QSize PatternVisualizer::sizeHint() const {
    return QSize(200, 120);
}

void PatternVisualizer::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_canvas = QImage(size() * devicePixelRatio(), QImage::Format_ARGB32_Premultiplied);
    m_canvas.setDevicePixelRatio(devicePixelRatio());
    m_canvas.fill(Qt::black);
}

void PatternVisualizer::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Manual mode skip: jump straight into a crossfade to the next mode.
        if (m_modeClock < HOLD_SECONDS) m_modeClock = HOLD_SECONDS;
    } else if (event->button() == Qt::RightButton) {
        emit contextMenuRequested(event->globalPosition().toPoint());
        return;
    }
    QWidget::mousePressEvent(event);
}

void PatternVisualizer::timerEvent(QTimerEvent* event) {
    if (event->timerId() != m_timerId) {
        QWidget::timerEvent(event);
        return;
    }

    const double dt = 0.016;
    m_time += dt;
    m_hueBase = std::fmod(m_hueBase + dt * 8.0, 360.0);

    updateAudio();

    // Advance the mode clock: hold, then blend into the next mode.
    m_modeClock += dt;
    if (m_modeClock < HOLD_SECONDS) {
        m_blend = 0.0;
    } else {
        double t = (m_modeClock - HOLD_SECONDS) / BLEND_SECONDS;
        if (t >= 1.0) {
            m_currentMode = m_nextMode;
            m_nextMode = pickNextMode();
            m_modeClock = 0.0;
            m_blend = 0.0;
        } else {
            // Smoothstep for an eased crossfade
            m_blend = t * t * (3.0 - 2.0 * t);
        }
    }

    renderFrame();
    update();
}

void PatternVisualizer::updateAudio() {
    QVector<double> target(NUM_BANDS, 0.0);
    if (m_isPlaying && m_mixer) {
        std::vector<float> samples = m_mixer->getLatestSamples();
        target = AudioSpectrumAnalyzer::analyzeSamples(samples, 2, NUM_BANDS, 44100.0);
    }

    double volFactor = double(m_volume) / 100.0;
    double energy = 0.0, bass = 0.0;
    for (int i = 0; i < NUM_BANDS; ++i) {
        double t = target[i] * volFactor;
        m_bands[i] += (t - m_bands[i]) * 0.15;
        energy += m_bands[i];
        if (i < 4) bass += m_bands[i];
    }
    energy /= NUM_BANDS;
    bass /= 4.0;
    m_energy += (energy - m_energy) * 0.2;
    m_bass += (bass - m_bass) * 0.3;
}

void PatternVisualizer::renderFrame() {
    if (m_canvas.isNull()) return;

    QPainter p(&m_canvas);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QImage prev = m_canvas.copy();
    p.fillRect(QRect(QPoint(0, 0), size()), Qt::black);
    const double shrink = 0.975;
    p.save();
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(shrink, shrink);
    p.translate(-width() / 2.0, -height() / 2.0);
    p.setOpacity(0.96);
    p.drawImage(QRect(QPoint(0, 0), size()), prev);
    p.restore();

    if (m_blend <= 0.0) {
        drawMode(p, m_currentMode, 1.0);
    } else {
        drawMode(p, m_currentMode, 1.0 - m_blend);
        drawMode(p, m_nextMode, m_blend);
    }
}

void PatternVisualizer::drawMode(QPainter& p, int mode, double weight) {
    if (weight <= 0.01) return;
    switch (mode % NUM_MODES) {
        case 0: drawSpiral(p, weight); break;
        case 1: drawKaleidoscope(p, weight); break;
        case 2: drawTunnel(p, weight); break;
        case 3: drawWaveRibbons(p, weight); break;
        case 4: drawLissajous(p, weight); break;
        case 5: drawMandala(p, weight); break;
        case 6: drawStarburst(p, weight); break;
        case 7: drawFlowField(p, weight); break;
        case 8: drawPlasma(p, weight); break;
        case 9: drawPinwheel(p, weight); break;
        case 10: drawAmbienceSpokes(p, weight); break;
    }
}

void PatternVisualizer::drawSpiral(QPainter& p, double weight) {
    double cx = width() / 2.0, cy = height() / 2.0;
    double maxR = std::min(cx, cy) * 0.95;
    p.save();
    p.setPen(Qt::NoPen);
    const int points = 60;
    for (int i = 0; i < points; ++i) {
        double frac = double(i) / points;
        double band = m_bands[i % NUM_BANDS];
        double angle = m_time * 1.4 + frac * 4.0 * M_PI;
        double r = maxR * (0.15 + frac * 0.8) * (0.7 + band * 0.5 + m_energy * 0.3);
        double x = cx + std::cos(angle) * r;
        double y = cy + std::sin(angle) * r * 0.85;
        double dotSize = 1.5 + band * 5.0;
        QColor col = QColor::fromHsv(int(m_hueBase + frac * 120.0) % 360, 220,
                                     150 + int(band * 105));
        col.setAlphaF(weight * (0.35 + band * 0.65));
        p.setBrush(col);
        p.drawEllipse(QPointF(x, y), dotSize, dotSize);
    }
    p.restore();
}

// Mode 1: kaleidoscope — a bezier petal mirrored across 6 sectors.
void PatternVisualizer::drawKaleidoscope(QPainter& p, double weight) {
    double cx = width() / 2.0, cy = height() / 2.0;
    double maxR = std::min(cx, cy) * 0.95;
    const int sectors = 6;

    QPainterPath petal;
    petal.moveTo(0, 0);
    double r1 = maxR * (0.4 + m_bands[2] * 0.5);
    double r2 = maxR * (0.7 + m_bass * 0.3);
    double wob = std::sin(m_time * 2.1) * 0.5;
    petal.cubicTo(r1 * 0.5, -r1 * (0.6 + wob * 0.2),
                  r2 * (0.8 + m_bands[8] * 0.3), -r2 * 0.15,
                  r2, m_bands[14] * maxR * 0.3);
    petal.cubicTo(r1 * 0.7, r1 * 0.4, r1 * 0.3, r1 * 0.2, 0, 0);

    p.save();
    p.translate(cx, cy);
    p.rotate(m_time * 20.0);
    QColor col = QColor::fromHsv(int(m_hueBase + 40.0) % 360, 200, 230);
    col.setAlphaF(weight * (0.25 + m_energy * 0.5));
    p.setPen(QPen(col, 1.2));
    QColor fill = col;
    fill.setAlphaF(weight * 0.12);
    p.setBrush(fill);
    for (int s = 0; s < sectors; ++s) {
        p.drawPath(petal);
        p.scale(1, -1); // mirror for the kaleidoscope symmetry
        p.drawPath(petal);
        p.scale(1, -1);
        p.rotate(360.0 / sectors);
    }
    p.restore();
}

// Mode 2: tunnel of concentric polygon rings flying at the viewer.
void PatternVisualizer::drawTunnel(QPainter& p, double weight) {
    double cx = width() / 2.0, cy = height() / 2.0;
    double maxR = std::min(cx, cy) * 1.1;
    const int rings = 10;
    const int sides = 8;

    p.save();
    p.setBrush(Qt::NoBrush);
    for (int ring = 0; ring < rings; ++ring) {
        // rings emerge from the center and expand outward over time
        double phase = std::fmod(m_time * 0.6 + double(ring) / rings, 1.0);
        double r = phase * phase * maxR * (1.0 + m_bass * 0.4);
        if (r < 2.0) continue;
        double band = m_bands[(ring * 2) % NUM_BANDS];
        double rot = m_time * 15.0 + ring * 6.0;

        QPolygonF poly;
        for (int v = 0; v < sides; ++v) {
            double a = rot * M_PI / 180.0 + v * 2.0 * M_PI / sides;
            double rr = r * (1.0 + band * 0.25 * std::sin(a * 3.0 + m_time * 4.0));
            poly << QPointF(cx + std::cos(a) * rr, cy + std::sin(a) * rr * 0.8);
        }
        QColor col = QColor::fromHsv(int(m_hueBase + 200.0 + ring * 8.0) % 360, 230,
                                     120 + int(phase * 135));
        col.setAlphaF(weight * (0.2 + phase * 0.6) * (0.5 + band * 0.5));
        p.setPen(QPen(col, 1.0 + phase * 2.0));
        p.drawPolygon(poly);
    }
    p.restore();
}

// Mode 3: flowing horizontal ribbons modulated by the spectrum.
void PatternVisualizer::drawWaveRibbons(QPainter& p, double weight) {
    const int ribbons = 3;
    p.save();
    p.setBrush(Qt::NoBrush);
    for (int rb = 0; rb < ribbons; ++rb) {
        double baseY = height() * (0.3 + 0.2 * rb);

        // Sample the ribbon at a handful of control points, then connect them
        // with cubic Beziers so the ribbon reads as a single smooth curve
        // rather than a chain of straight segments.
        const int segments = 24;
        QVector<QPointF> pts;
        pts.reserve(segments + 1);
        for (int i = 0; i <= segments; ++i) {
            double frac = double(i) / segments;
            double x = frac * width();
            double band = m_bands[int(frac * (NUM_BANDS - 1))];
            double y = baseY
                + std::sin(frac * 6.0 + m_time * (1.5 + rb * 0.7) + rb * 2.0)
                  * height() * 0.12 * (0.5 + m_energy)
                + std::sin(frac * 17.0 - m_time * 3.0) * band * height() * 0.18;
            pts << QPointF(x, y);
        }

        // Catmull-Rom -> cubic Bezier: each span uses tangents derived from the
        // neighbouring points, giving C1-continuous handles for the curveTo.
        QPainterPath path;
        path.moveTo(pts.first());
        for (int i = 0; i < pts.size() - 1; ++i) {
            const QPointF p0 = pts[i > 0 ? i - 1 : i];
            const QPointF p1 = pts[i];
            const QPointF p2 = pts[i + 1];
            const QPointF p3 = pts[i + 2 < pts.size() ? i + 2 : i + 1];
            const QPointF c1 = p1 + (p2 - p0) / 6.0;
            const QPointF c2 = p2 - (p3 - p1) / 6.0;
            path.cubicTo(c1, c2, p2);
        }
        QColor col = QColor::fromHsv(int(m_hueBase + 300.0 + rb * 30.0) % 360, 210, 235);
        col.setAlphaF(weight * (0.3 + m_energy * 0.6));
        p.setPen(QPen(col, 1.5 + m_bass * 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(path);
    }
    p.restore();
}

// Mode 4: Lissajous — a glowing parametric curve whose frequency ratio and
// phase drift with the audio, traced as one continuous smooth loop.
void PatternVisualizer::drawLissajous(QPainter& p, double weight) {
    double cx = width() / 2.0, cy = height() / 2.0;
    double ax = width() * 0.42, ay = height() * 0.42;
    // Frequencies shift slowly; bass nudges the ratio for extra wobble.
    double fx = 3.0 + std::sin(m_time * 0.13) * 2.0 + m_bass * 1.5;
    double fy = 4.0 + std::cos(m_time * 0.11) * 2.0;
    double phase = m_time * 0.7;

    const int steps = 200;
    QPainterPath path;
    for (int i = 0; i <= steps; ++i) {
        double t = double(i) / steps * 2.0 * M_PI;
        double band = m_bands[i % NUM_BANDS];
        double rr = 1.0 + band * 0.25 + m_energy * 0.2;
        double x = cx + std::sin(fx * t + phase) * ax * rr;
        double y = cy + std::sin(fy * t) * ay * rr;
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }
    p.save();
    // Draw the curve a few times with shifting hue for a chromatic-glow feel.
    for (int layer = 0; layer < 3; ++layer) {
        QColor col = QColor::fromHsv(int(m_hueBase + layer * 40.0) % 360, 230, 255);
        col.setAlphaF(weight * (0.5 - layer * 0.12) * (0.4 + m_energy * 0.6));
        p.setPen(QPen(col, 3.0 - layer * 0.8, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    p.restore();
}

// Mode 5: Mandala — nested rings of petals rotating at different speeds,
// classic hypnotic radial symmetry.
void PatternVisualizer::drawMandala(QPainter& p, double weight) {
    double cx = width() / 2.0, cy = height() / 2.0;
    double maxR = std::min(cx, cy) * 0.95;
    const int rings = 4;

    p.save();
    p.translate(cx, cy);
    for (int ring = 0; ring < rings; ++ring) {
        int petals = 6 + ring * 2;
        double band = m_bands[(ring * 5) % NUM_BANDS];
        double baseR = maxR * (0.25 + ring * 0.2);
        double petalR = baseR * (0.3 + band * 0.4 + m_bass * 0.2);
        double dir = (ring % 2 == 0) ? 1.0 : -1.0;

        p.save();
        p.rotate(m_time * (10.0 + ring * 6.0) * dir);
        QColor col = QColor::fromHsv(int(m_hueBase + ring * 60.0) % 360, 220,
                                     150 + int(band * 105));
        col.setAlphaF(weight * (0.3 + m_energy * 0.5));
        QColor fill = col;
        fill.setAlphaF(weight * 0.15);
        p.setPen(QPen(col, 1.4));
        p.setBrush(fill);
        for (int i = 0; i < petals; ++i) {
            QPainterPath petal;
            petal.moveTo(baseR, 0);
            petal.cubicTo(baseR + petalR, -petalR, baseR + petalR, petalR, baseR, 0);
            p.drawPath(petal);
            // small orbiting dot at the petal tip
            p.drawEllipse(QPointF(baseR + petalR, 0), 1.5 + band * 3.0,
                          1.5 + band * 3.0);
            p.rotate(360.0 / petals);
        }
        p.restore();
    }
    p.restore();
}

// Mode 6: Starburst — radial rays shooting out, length pulsing per band,
// with a rotating shimmer.
void PatternVisualizer::drawStarburst(QPainter& p, double weight) {
    double cx = width() / 2.0, cy = height() / 2.0;
    double maxR = std::min(cx, cy) * 1.05;
    const int rays = 48;

    p.save();
    p.translate(cx, cy);
    p.rotate(m_time * 8.0);
    for (int i = 0; i < rays; ++i) {
        double frac = double(i) / rays;
        double band = m_bands[i % NUM_BANDS];
        double len = maxR * (0.2 + band * 0.7 + m_energy * 0.3)
                     * (0.6 + 0.4 * std::sin(m_time * 3.0 + frac * 12.0));
        double a = frac * 2.0 * M_PI;
        double innerR = maxR * 0.08;
        QPointF p0(std::cos(a) * innerR, std::sin(a) * innerR);
        QPointF p1(std::cos(a) * len, std::sin(a) * len);
        QColor col = QColor::fromHsv(int(m_hueBase + frac * 360.0) % 360, 230,
                                     180 + int(band * 75));
        col.setAlphaF(weight * (0.3 + band * 0.7));
        p.setPen(QPen(col, 1.5 + band * 3.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(p0, p1);
    }
    p.restore();
}

void PatternVisualizer::drawFlowField(QPainter& p, double weight) {
    double w = width(), h = height();
    const int cols = 14, rows = 10;
    double dx = w / cols, dy = h / rows;

    p.save();
    p.setBrush(Qt::NoBrush);
    for (int gy = 0; gy < rows; ++gy) {
        for (int gx = 0; gx < cols; ++gx) {
            double px = (gx + 0.5) * dx;
            double py = (gy + 0.5) * dy;
            double band = m_bands[(gx + gy) % NUM_BANDS];
            // Swirling field angle from layered sines of position + time.
            double ang = std::sin(px * 0.012 + m_time * 0.9)
                       + std::cos(py * 0.014 - m_time * 0.7)
                       + std::sin((px + py) * 0.008 + m_time * 1.3);
            ang *= M_PI;
            double len = dx * (0.6 + band * 1.2 + m_energy * 0.6);

            QPointF a(px, py);
            QPointF b(px + std::cos(ang) * len, py + std::sin(ang) * len);
            QPointF mid = (a + b) / 2.0
                        + QPointF(std::cos(ang + M_PI / 2), std::sin(ang + M_PI / 2))
                          * len * 0.4;
            QPainterPath streak;
            streak.moveTo(a);
            streak.quadTo(mid, b);

            int hue = int(m_hueBase + (ang / M_PI) * 60.0 + gx * 6.0) % 360;
            if (hue < 0) hue += 360;
            QColor col = QColor::fromHsv(hue, 210, 160 + int(band * 95));
            col.setAlphaF(weight * (0.25 + band * 0.6));
            p.setPen(QPen(col, 1.5 + band * 2.5, Qt::SolidLine, Qt::RoundCap));
            p.drawPath(streak);
        }
    }
    p.restore();
}

// Modes 8/9: the classic WMP "Ambience" smoky plasma. A low-res field is
// evaluated per pixel from layered sines plus an angular swirl, wrapped around
// a dark central vortex, then scaled up smoothly over the whole widget.
void PatternVisualizer::renderPlasmaField(QPainter& p, double weight, double spokes,
                                          double hueOffset) {
    const int bw = 150;
    int bh = std::max(1, bw * height() / std::max(1, width()));
    if (m_plasmaBuf.width() != bw || m_plasmaBuf.height() != bh) {
        m_plasmaBuf = QImage(bw, bh, QImage::Format_RGB32);
    }

    const double t = m_time;
    const double swirl = t * 0.6 + m_bass * 2.0;
    const double cx = bw * 0.5, cy = bh * 0.5;
    const double en = m_energy, bassB = m_bass;

    for (int y = 0; y < bh; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(m_plasmaBuf.scanLine(y));
        for (int x = 0; x < bw; ++x) {
            double dx = (x - cx) / bw;
            double dy = (y - cy) / bh;
            double dist = std::sqrt(dx * dx + dy * dy);
            double ang = std::atan2(dy, dx);
            // Warp the angle by radius + time so the field spirals inward.
            double a = ang + swirl + dist * (4.0 + bassB * 6.0);

            double v = std::sin(x * 0.06 + t * 1.3)
                     + std::sin(y * 0.07 - t * 1.1)
                     + std::sin((x + y) * 0.05 + t * 0.9)
                     + std::sin(dist * 12.0 - t * 2.0 + a * 2.0) * 1.5
                     + std::sin(a * spokes) * (0.6 + en);
            v /= 5.0; // roughly -1..1

            double bright = 0.5 + 0.5 * std::sin(v * M_PI + t);
            // Dark vortex: fade to black toward the centre like the WMP look.
            double vign = std::min(1.0, dist * 2.3);
            bright *= vign * vign;

            int hue = int(m_hueBase + hueOffset + v * 70.0 + dist * 40.0) % 360;
            if (hue < 0) hue += 360;
            int sat = std::min(255, 200 + int(bassB * 55));
            int val = std::min(255, int(bright * 255.0 * (0.55 + en * 0.7)));
            line[x] = QColor::fromHsv(hue, sat, val).rgb();
        }
    }

    p.setOpacity(weight);
    p.drawImage(QRect(0, 0, width(), height()), m_plasmaBuf);
    p.setOpacity(1.0);
}

void PatternVisualizer::drawPlasma(QPainter& p, double weight) {
    renderPlasmaField(p, weight, 3.0, 160.0); // blue/cyan nebula
}

void PatternVisualizer::drawPinwheel(QPainter& p, double weight) {
    renderPlasmaField(p, weight, 10.0, 300.0); // magenta spoked pinwheel
}

// Mode 10: WMP-"Thingus"-style red spokes spiralling out of the centre. Each
// spoke is a curved wedge that widens outward, its width/brightness driven by
// the spectrum, with the whole fan slowly rotating.
void PatternVisualizer::drawAmbienceSpokes(QPainter& p, double weight) {
    const double cx = width() / 2.0, cy = height() / 2.0;
    const double maxR = std::hypot(cx, cy) * 1.05;
    const int spokes = 12;
    const double rot = m_time * 0.5;   // slow overall rotation
    const double spiral = 1.3;         // how much the spokes curl
    const int steps = 20;

    p.save();
    p.setPen(Qt::NoPen);
    for (int s = 0; s < spokes; ++s) {
        double band = m_bands[(s * NUM_BANDS / spokes) % NUM_BANDS];
        double baseAng = rot + s * (2.0 * M_PI / spokes);
        double halfW = 0.04 + band * 0.16 + m_bass * 0.08;

        QPainterPath path;
        path.moveTo(cx, cy);
        for (int k = 0; k <= steps; ++k) {
            double r = maxR * double(k) / steps;
            double a = baseAng + halfW + spiral * (r / maxR);
            path.lineTo(cx + std::cos(a) * r, cy + std::sin(a) * r);
        }
        for (int k = steps; k >= 0; --k) {
            double r = maxR * double(k) / steps;
            double a = baseAng - halfW + spiral * (r / maxR);
            path.lineTo(cx + std::cos(a) * r, cy + std::sin(a) * r);
        }
        path.closeSubpath();

        // Red radial gradient: bright near the hub, deep red toward the rim.
        int v = 120 + int(band * 135 + m_energy * 60);
        if (v > 255) v = 255;
        QRadialGradient grad(cx, cy, maxR);
        QColor inner(std::min(255, v + 40), 40, 40);
        QColor mid(v, 20, 20);
        QColor outer(std::max(0, v - 90), 0, 0);
        inner.setAlphaF(weight); mid.setAlphaF(weight); outer.setAlphaF(weight);
        grad.setColorAt(0.0, inner);
        grad.setColorAt(0.5, mid);
        grad.setColorAt(1.0, outer);
        p.setBrush(grad);
        p.drawPath(path);
    }
    p.restore();
}

void PatternVisualizer::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    if (!m_canvas.isNull()) {
        painter.drawImage(0, 0, m_canvas);
    } else {
        painter.fillRect(rect(), Qt::black);
    }
    // Thin frame to match the neighboring wave display
    painter.setPen(QColor(70, 70, 70));
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

} // namespace Kalorite
