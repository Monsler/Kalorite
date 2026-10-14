#pragma once

#include <QObject>
#include <QString>

class QWidget;

namespace Kalorite {

    // Offers to download a direct audio-file link when one lands on the clipboard.
    class ClipboardWatcher : public QObject {
        Q_OBJECT

    public:
        explicit ClipboardWatcher(QWidget* dialogParent, QObject* parent = nullptr);

    signals:
        void downloadRequested(const QString& url);

    private:
        void onClipboardChanged();

        QWidget* m_dialogParent;
        QString m_lastUrl;
    };

} // namespace Kalorite
