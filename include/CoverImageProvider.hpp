#pragma once

#include <QQuickImageProvider>
#include "PlaylistModel.hpp"

namespace Kalorite {

    class CoverImageProvider : public QQuickImageProvider {
    public:
        explicit CoverImageProvider(PlaylistModel* model)
            : QQuickImageProvider(QQuickImageProvider::Image), m_model(model) {}

        QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
            if (!m_model) return {};

            bool ok = false;
            quint64 trackId = id.toULongLong(&ok);
            QImage img;
            if (ok) {
                img = m_model->coverById(trackId);
            }

            if (size) *size = img.size();

            if (!requestedSize.isEmpty() && !img.isNull()) {
                return img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            return img;
        }

    private:
        PlaylistModel* m_model;
    };

} // namespace Kalorite
