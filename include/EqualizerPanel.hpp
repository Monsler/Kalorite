#pragma once

#include <QObject>
#include <QWidget>

class QQuickWidget;

namespace Kalorite {

    class Mixer;

    // Exposed to QML as `eq`; forwards UI changes to the mixer.
    class EqualizerBackend : public QObject {
        Q_OBJECT

    public:
        EqualizerBackend(Mixer* mixer, QObject* parent = nullptr);

        Q_INVOKABLE void setEnabled(bool enabled);
        Q_INVOKABLE void setBand(int index, int value);

    private:
        Mixer* m_mixer;
    };

    // Collapsible 10-band equalizer body. The expand/collapse toggle lives in the window.
    class EqualizerPanel : public QWidget {
        Q_OBJECT

    public:
        EqualizerPanel(Mixer* mixer, QWidget* parent = nullptr);

    private:
        EqualizerBackend* m_backend;
        QQuickWidget* m_view;
    };

} // namespace Kalorite
