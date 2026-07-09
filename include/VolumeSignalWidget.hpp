#pragma once

#include <QWidget>

namespace Kalorite {

class VolumeSignalWidget : public QWidget {
    Q_OBJECT

public:
    explicit VolumeSignalWidget(QWidget* parent = nullptr);
    ~VolumeSignalWidget() override = default;

    int volume() const { return m_volume; }
    void setVolume(int volume);
    void setModernMode(bool modern);
    void setThemeAccent(const QColor& color, bool isSystem);

signals:
    void volumeChanged(int volume);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;

private:
    void updateVolumeFromPos(int x);

    int m_volume = 100; // 0 to 100
    bool m_modernMode = false;
    static const int NUM_BARS = 10;
    QColor m_accentColor = QColor(255, 255, 255);
    bool m_isSystemTheme = true;
};

} // namespace Kalorite
