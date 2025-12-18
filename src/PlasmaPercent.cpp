#include "PlasmaPercent.hpp"

#if defined(Q_OS_LINUX)
#include <qcontainerfwd.h>
#include <qdbusmessage.h>
#include <qdbusconnection.h>
#endif

#define APPLICATION_ID "io.github.monsler.Kalorite"

namespace Kalorite
{
    void PlasmaPercent::emitPercent(const int percent) {
#if defined(Q_OS_LINUX)
        double progressValue = percent / 100.0;

        QVariantMap properties;
        properties.insert("progress", progressValue);
        properties.insert("progress-visible", progressValue > 0.0 && progressValue < 1.0);

        QDBusMessage message = QDBusMessage::createSignal(
            "/io/github/monsler/Kalorite",
            "com.canonical.Unity.LauncherEntry",
            "Update"
        );

        message << QString("application://io.github.monsler.Kalorite.desktop");
        message << properties;

        QDBusConnection::sessionBus().send(message);
#endif
    }
} // namespace Kalorite
