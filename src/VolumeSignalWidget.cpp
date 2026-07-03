#include "VolumeSignalWidget.hpp"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <cmath>

namespace Kalorite {

VolumeSignalWidget::VolumeSignalWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(60, 24);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void VolumeSignalWidget::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (m_volume != volume) {
        m_volume = volume;
        emit volumeChanged(m_volume);
        update();
    }
}

void VolumeSignalWidget::setModernMode(bool modern) {
    if (m_modernMode != modern) {
        m_modernMode = modern;
        update();
    }
}

QSize VolumeSignalWidget::sizeHint() const {
    return QSize(80, 28);
}

void VolumeSignalWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();

    int spacing = 2;
    int totalSpacing = spacing * (NUM_BARS - 1);
    int barWidth = (w - totalSpacing) / NUM_BARS;
    if (barWidth < 2) barWidth = 2;

    int activeBarsCount = std::ceil((double)m_volume / 100.0 * NUM_BARS);

    for (int i = 0; i < NUM_BARS; ++i) {
        int bx = i * (barWidth + spacing);
        
        double heightRatio = (double)(i + 1) / NUM_BARS;
        int barHeight = std::round(heightRatio * (h - 4));
        if (barHeight < 2) barHeight = 2;

        int by = h - barHeight - 2;

        bool isActive = (i < activeBarsCount && m_volume > 0);

        QColor barColor = isActive ? QColor(255, 255, 255, 240) : QColor(255, 255, 255, 50);
        
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(barColor);
        painter.drawRoundedRect(bx, by, barWidth, barHeight, 1.5, 1.5);
        painter.restore();
    }
}

void VolumeSignalWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        updateVolumeFromPos(event->position().x());
    }
}

void VolumeSignalWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        updateVolumeFromPos(event->position().x());
    }
}

void VolumeSignalWidget::updateVolumeFromPos(int x) {
    int w = width();
    if (w <= 0) return;
    double ratio = (double)x / w;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    int vol = std::round(ratio * 100);
    setVolume(vol);
}

} // namespace Kalorite
