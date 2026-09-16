#include "BouncingLogo.hpp"
#include <qevent.h>
#include <qlabel.h>
#include <qpixmap.h>
#include <QRandomGenerator>

namespace Kalorite {
    BouncingLogo::BouncingLogo(QWidget* parent, const QPixmap& pm) : QLabel(parent) {
        setPixmap(pm);
        setFixedSize(pm.size());
        setCursor(Qt::PointingHandCursor);
        setToolTip(QObject::tr("Click me!"));
        m_timer = new QTimer(this);
        QObject::connect(m_timer, &QTimer::timeout, this, [this] { step(); });
    }

    void BouncingLogo::mousePressEvent(QMouseEvent*) {
         auto* rng = QRandomGenerator::global();
        if (!m_timer->isActive()) {
            m_x = x(); m_y = y();
            m_vx = 3.0 + rng->bounded(6);
            m_vy = -13.0 - rng->bounded(5);
            // note: 16 msec are smth around 60 fps
            m_timer->start(16);
        } else {
            m_vy -= 11.0 + rng->bounded(4);
            m_vx += rng->bounded(11) - 5;
        }
    }

    void BouncingLogo::step() {
          const double gravity = 0.9, restitution = 0.78, friction = 0.98;
        QWidget* p = parentWidget();
        if (!p) { m_timer->stop(); return; }
        const double maxX = p->width()  - width();
        const double maxY = p->height() - height();

        m_vy += gravity;
        m_x  += m_vx;
        m_y  += m_vy;

        if (m_x < 0)    { m_x = 0;    m_vx = -m_vx * restitution; }
        if (m_x > maxX) { m_x = maxX; m_vx = -m_vx * restitution; }
        if (m_y < 0)    { m_y = 0;    m_vy = -m_vy * restitution; }
        if (m_y > maxY) {
            m_y = maxY;
            m_vy = -m_vy * restitution;
            m_vx *= friction;

            if (std::abs(m_vy) < 1.6 && std::abs(m_vx) < 0.35) {
                m_vx = m_vy = 0.0;
                move(int(m_x), int(m_y));
                m_timer->stop();
                return;
            }
        }
        move(int(m_x), int(m_y));
    }
}