#include "AboutDialog.hpp"
#include "BouncingLogo.hpp"
#include <memory>
#include <qapplication.h>
#include <qdialog.h>
#include <qlabel.h>
#include <qpainter.h>
#include <qwidget.h>

namespace Kalorite {
    AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle("About Kalorite");
        setFixedSize(440, 260);

        QPixmap logo = QIcon::fromTheme("io.github.monsler.Kalorite").pixmap(96, 96);
        if (logo.isNull()) logo = windowIcon().pixmap(96, 96);
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
        logo = logo.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        this->text = std::make_shared<QLabel>(this);
        text->setGeometry(140, 24, 280, 212);
        text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);


        text->setOpenExternalLinks(false);
        text->setText(tr(
            "<h2 style='margin-bottom:2px'>Kalorite</h2>"
            "<p style='color:gray;margin-top:0'>"
            "<a href='kalorite:reset-greeting' style='color:gray;text-decoration:none'>Version %1</a></p>"
            "<p>Kalorite is a lightweight audio player. It supports all modern "
            "codecs and have simple and elegant design.</p>"
            "<p>by monsler<br>"
            "<a href='https://github.com/Monsler/Kalorite'>github.com/Monsler/Kalorite</a></p>")
            .arg("3.5.0"));

        this->badge = std::make_shared<BouncingLogo>(this, logo);
        this->invalidateLogo();
    }
    
    void AboutDialog::invalidateLogo() {
        badge->move(16, 16);
        badge->raise();
    }
}