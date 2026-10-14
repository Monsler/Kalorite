#include "AboutDialog.hpp"
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QUrl>
#include <QVariantMap>
#include <QCoreApplication>

namespace Kalorite {
    namespace {
        class LogoProvider : public QQuickImageProvider {
            public:
            explicit LogoProvider(QImage image)
                : QQuickImageProvider(QQuickImageProvider::Image), m_image(std::move(image)) {}

            QImage requestImage(const QString&, QSize* size, const QSize&) override {
                if (size) *size = m_image.size();
                return m_image;
            }

            private:
            QImage m_image;
        };

        QPixmap loadLogo() {
            QPixmap logo = QIcon::fromTheme("io.github.monsler.Kalorite").pixmap(96, 96);
            if (logo.isNull()) {
                for (const QString& c : {
                        QStringLiteral("/app/share/icons/hicolor/512x512/apps/io.github.monsler.Kalorite.png"),
                        QCoreApplication::applicationDirPath() + "/data/io.github.monsler.Kalorite.png",
                        QStringLiteral("data/io.github.monsler.Kalorite.png") }) {
                    logo = QPixmap(c);
                    if (!logo.isNull()) break;
                }
            }
            if (logo.isNull()) {
                logo = QPixmap(96, 96);
                logo.fill(Qt::transparent);
                QPainter pr(&logo);
                pr.setRenderHint(QPainter::Antialiasing);
                pr.setBrush(QColor(0x2d, 0x74, 0xda));
                pr.setPen(Qt::NoPen);
                pr.drawRoundedRect(logo.rect().adjusted(2, 2, -2, -2), 16, 16);
                pr.setPen(Qt::white);
                QFont f = pr.font(); f.setBold(true); f.setPointSize(46); pr.setFont(f);
                pr.drawText(logo.rect(), Qt::AlignCenter, "K");
            }
            return logo.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    AboutDialog::AboutDialog(QObject* parent) : QObject(parent), m_engine(new QQmlEngine(this)) {
        m_engine->addImageProvider(QStringLiteral("app"), new LogoProvider(loadLogo().toImage()));

        QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/qml/AboutDialog.qml")));
        if (component.isError()) {
            for (const QQmlError& e : component.errors()) qWarning() << e.toString();
            return;
        }

        const QPalette palette = QGuiApplication::palette();
        const QVariantMap properties {
            {QStringLiteral("bodyHtml"), tr(
                "<h2 style='margin-bottom:2px'>Kalorite</h2>"
                "<p style='color:gray;margin-top:0'>"
                "<a href='kalorite:reset-greeting' style='color:gray;text-decoration:none'>Version %1</a></p>"
                "<p>Kalorite is a lightweight audio player. It supports all modern "
                "codecs and have simple and elegant design.</p>"
                "<p>by monsler<br>"
                "<a href='https://github.com/Monsler/Kalorite'>github.com/Monsler/Kalorite</a></p>")
                .arg("3.5.0")},
            {QStringLiteral("background"), palette.color(QPalette::Window)},
            {QStringLiteral("foreground"), palette.color(QPalette::WindowText)},
            {QStringLiteral("linkColor"), palette.color(QPalette::Link)},
        };
        m_window = component.createWithInitialProperties(properties);
    }

    AboutDialog::~AboutDialog() {
        delete m_window;
    }

    void AboutDialog::show() {
        if (auto* window = qobject_cast<QQuickWindow*>(m_window)) {
            window->show();
            window->raise();
            window->requestActivate();
        }
    }
}
