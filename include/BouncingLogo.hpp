#pragma once

#include <qlabel.h>
#include <qtimer.h>


namespace Kalorite {
class BouncingLogo : public QLabel {
public:
    BouncingLogo(QWidget* parent, const QPixmap& pm);

protected:
    void mousePressEvent(QMouseEvent*) override;

private:
    void step();
    
    QTimer* m_timer = nullptr;
    double m_x = 0, m_y = 0, m_vx = 0, m_vy = 0;
};

} // anonymous namespace