#include "EqualizerPanel.hpp"
#include "Mixer.hpp"
#include <QQmlContext>
#include <QQuickWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace Kalorite {

EqualizerBackend::EqualizerBackend(Mixer* mixer, QObject* parent)
    : QObject(parent), m_mixer(mixer) {}

void EqualizerBackend::setEnabled(bool enabled) {
    m_mixer->setEqEnabled(enabled);
}

void EqualizerBackend::setBand(int index, int value) {
    // Map 0-100 to -15dB..+15dB, where 50 is 0dB.
    float gainDb = (static_cast<float>(value) - 50.0f) * (15.0f / 50.0f);
    m_mixer->setEqBand(index, gainDb);
}

EqualizerPanel::EqualizerPanel(Mixer* mixer, QWidget* parent)
    : QWidget(parent) {
    m_backend = new EqualizerBackend(mixer, this);

    m_view = new QQuickWidget(this);
    m_view->setClearColor(palette().color(QPalette::Window));
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->rootContext()->setContextProperty(QStringLiteral("eq"), m_backend);
    m_view->setSource(QUrl(QStringLiteral("qrc:/qml/EqualizerPanel.qml")));
    m_view->setFixedHeight(160);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    mixer->setEqEnabled(false);
}

} // namespace Kalorite
