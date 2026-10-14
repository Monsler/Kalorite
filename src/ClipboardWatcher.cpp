#include "ClipboardWatcher.hpp"
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QStringList>
#include <QUrl>
#include <QWidget>

namespace Kalorite {

ClipboardWatcher::ClipboardWatcher(QWidget* dialogParent, QObject* parent)
    : QObject(parent), m_dialogParent(dialogParent) {
    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &ClipboardWatcher::onClipboardChanged);
}

void ClipboardWatcher::onClipboardChanged() {
    QString text = QApplication::clipboard()->text().trimmed();

    if (text.isEmpty()) return;

    QUrl url(text);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https"))
        return;

    static const QStringList audioExtensions = {
        "mp3", "wav", "flac", "ogg", "opus", "aac", "m4a", "wma"
    };
    QString path = url.path().toLower();
    bool looksLikeAudio = false;
    for (const auto& ext : audioExtensions) {
        if (path.endsWith("." + ext)) {
            looksLikeAudio = true;
            break;
        }
    }
    if (!looksLikeAudio) return;

    if (text == m_lastUrl) return;
    m_lastUrl = text;

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_dialogParent,
        tr("&DownloadSound"),
        tr("&ClipboardAudioDetected").arg(url.fileName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );

    if (reply == QMessageBox::Yes) {
        emit downloadRequested(text);
    }
}

} // namespace Kalorite
