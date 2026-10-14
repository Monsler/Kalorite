#pragma once

#include <QQuickImageProvider>
#include <QPainter>
#include <QPainterPath>
#include "PlaylistModel.hpp"

namespace Kalorite {

    class CoverImageProvider : public QQuickImageProvider {
    public:
        explicit CoverImageProvider(PlaylistModel* model)
            : QQuickImageProvider(QQuickImageProvider::Image), m_model(model) {}

        QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
            if (!m_model) return {};

            QString cleanId = id;
            int qMark = cleanId.indexOf('?');
            if (qMark >= 0) {
                cleanId = cleanId.left(qMark);
            }

            bool ok = false;
            quint64 trackId = cleanId.toULongLong(&ok);
            QImage img;
            if (ok) {
                img = m_model->coverById(trackId);
            }

            if (img.isNull()) return {};

            if (size) *size = img.size();

            QSize targetSize = !requestedSize.isEmpty() ? requestedSize : QSize(72, 72);
            QImage scaled = img.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

            if (scaled.width() > targetSize.width() || scaled.height() > targetSize.height()) {
                int x = qMax(0, (scaled.width() - targetSize.width()) / 2);
                int y = qMax(0, (scaled.height() - targetSize.height()) / 2);
                scaled = scaled.copy(x, y, targetSize.width(), targetSize.height());
            }

            QImage rounded(targetSize, QImage::Format_ARGB32_Premultiplied);
            rounded.fill(Qt::transparent);

            QPainter painter(&rounded);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

            QPainterPath path;
            qreal radius = qMax(4.0, targetSize.width() * 6.0 / 36.0);
            path.addRoundedRect(QRectF(0, 0, targetSize.width(), targetSize.height()), radius, radius);
            painter.setClipPath(path);
            painter.drawImage(0, 0, scaled);
            painter.end();

            return rounded;
        }

    private:
        PlaylistModel* m_model;
    };

} // namespace Kalorite
