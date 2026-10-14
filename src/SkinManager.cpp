#include "SkinManager.hpp"
#include "Settings.hpp"
#include <QApplication>
#include <QColorDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QWidget>

namespace Kalorite {

SkinManager::SkinManager(QWidget* window, Settings* settings, QObject* parent)
    : QObject(parent), m_window(window), m_settings(settings),
      m_defaultPalette(QApplication::palette()) {}

void SkinManager::applyFromSettings() {
    m_currentSkinName = QString::fromStdString(m_settings->get<std::string>("skin", "system"));
    apply(m_currentSkinName);
}

void SkinManager::apply(const QString& skinName) {
    if (skinName == "system") {
        QApplication::setPalette(m_defaultPalette);
        m_window->setStyleSheet("");
        emit themeChanged(true, QColor());
        return;
    }

    QString customPath = Settings::configDir() + "/skins/" + skinName + ".qss";

    QString qss;
    QFile file(customPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qss = QString::fromUtf8(file.readAll());
    }

    QString accentStr = "#00dcff";
    nlohmann::json& accents = m_settings->data();
    if (accents.contains("skin_accents") && accents["skin_accents"].is_object()) {
        std::string key = skinName.toStdString();
        if (accents["skin_accents"].contains(key)) {
            accentStr = QString::fromStdString(accents["skin_accents"][key].get<std::string>());
        }
    }

    QColor accent(accentStr);
    if (!accent.isValid()) accent = QColor(0, 220, 255);

    applyDarkPalette(accent, QColor("#121212"), QColor("#1e1e1e"));
    m_window->setStyleSheet(qss);

    emit themeChanged(false, accent);
}

void SkinManager::applyDarkPalette(const QColor& accentColor, const QColor& bgColor, const QColor& surfaceColor) {
    QPalette palette;
    palette.setColor(QPalette::Window, bgColor);
    palette.setColor(QPalette::WindowText, QColor(220, 220, 220));
    palette.setColor(QPalette::Base, surfaceColor);
    palette.setColor(QPalette::AlternateBase, bgColor);
    palette.setColor(QPalette::ToolTipBase, QColor(30, 30, 30));
    palette.setColor(QPalette::ToolTipText, QColor(220, 220, 220));
    palette.setColor(QPalette::Text, QColor(220, 220, 220));
    palette.setColor(QPalette::Button, surfaceColor);
    palette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, accentColor);
    palette.setColor(QPalette::Highlight, accentColor);
    palette.setColor(QPalette::HighlightedText, Qt::white);

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(100, 100, 100));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 100, 100));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(20, 20, 20));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(20, 20, 20));

    QApplication::setPalette(palette);
}

void SkinManager::populateMenu(QMenu* menu) {
    menu->clear();

    QAction* sysAct = menu->addAction(tr("System"));
    sysAct->setCheckable(true);
    sysAct->setChecked(m_currentSkinName == "system");
    connect(sysAct, &QAction::triggered, this, [this]() {
        m_currentSkinName = "system";
        apply(m_currentSkinName);
        m_settings->set("skin", "system");
    });

    QString skinsDir = Settings::configDir() + "/skins";
    QDir dir(skinsDir);
    if (dir.exists()) {
        QStringList fileList = dir.entryList(QStringList() << "*.qss", QDir::Files);
        if (!fileList.isEmpty()) {
            menu->addSeparator();
            for (const QString& fileName : fileList) {
                QString baseName = QFileInfo(fileName).baseName();

                QString displayName = baseName;
                displayName.replace('_', ' ').replace('-', ' ');
                if (!displayName.isEmpty()) {
                    displayName[0] = displayName[0].toUpper();
                }

                QAction* act = menu->addAction(displayName);
                act->setCheckable(true);
                act->setChecked(m_currentSkinName == baseName);
                connect(act, &QAction::triggered, this, [this, baseName]() {
                    m_currentSkinName = baseName;
                    apply(m_currentSkinName);
                    m_settings->set("skin", baseName.toStdString());
                });
            }
        }
    }
}

void SkinManager::addSkinFromDialog() {
    QString fileName = QFileDialog::getOpenFileName(m_window,
        tr("Open QSS Skin File"),
        QDir::homePath(),
        tr("Stylesheet Files (*.qss);;All Files (*.*)"));

    if (fileName.isEmpty()) return;

    QString baseName = QFileInfo(fileName).baseName();
    if (baseName.isEmpty() || baseName.toLower() == "system") {
        QMessageBox::warning(m_window, tr("Add Skin..."), tr("Invalid skin name."));
        return;
    }

    QColor accent = QColorDialog::getColor(Qt::cyan, m_window, tr("Select Accent Color for Custom Skin"));
    if (!accent.isValid()) return;

    QString skinsDir = Settings::configDir() + "/skins";
    QDir().mkpath(skinsDir);

    QString destPath = skinsDir + "/" + baseName + ".qss";
    if (QFile::exists(destPath)) {
        auto button = QMessageBox::question(m_window, tr("Add Skin..."),
            tr("A skin named \"%1\" already exists. Overwrite?").arg(baseName),
            QMessageBox::Yes | QMessageBox::No);
        if (button == QMessageBox::No) return;
        QFile::remove(destPath);
    }

    if (QFile::copy(fileName, destPath)) {
        m_settings->set("skin", baseName.toStdString());
        nlohmann::json& accents = m_settings->data();
        if (!accents.contains("skin_accents") || !accents["skin_accents"].is_object()) {
            accents["skin_accents"] = nlohmann::json::object();
        }
        accents["skin_accents"][baseName.toStdString()] = accent.name().toStdString();
        m_settings->save();

        m_currentSkinName = baseName;
        apply(baseName);

        QMessageBox::information(m_window, tr("Add Skin..."), tr("Skin added successfully."));
    } else {
        QMessageBox::warning(m_window, tr("Add Skin..."), tr("Failed to copy skin file."));
    }
}

} // namespace Kalorite
