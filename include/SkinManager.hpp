#pragma once

#include <QObject>
#include <QColor>
#include <QPalette>
#include <QString>

class QMenu;
class QWidget;

namespace Kalorite {

    class Settings;

    // QSS skins and the dark palette. Visualizer theming is announced via themeChanged.
    class SkinManager : public QObject {
        Q_OBJECT

    public:
        SkinManager(QWidget* window, Settings* settings, QObject* parent = nullptr);

        QString currentSkin() const { return m_currentSkinName; }
        void applyFromSettings();
        void populateMenu(QMenu* menu);
        void addSkinFromDialog();

    signals:
        void themeChanged(bool system, QColor accent);

    private:
        void apply(const QString& skinName);
        void applyDarkPalette(const QColor& accentColor, const QColor& bgColor, const QColor& surfaceColor);

        QWidget* m_window;
        Settings* m_settings;
        QString m_currentSkinName = "system";
        QPalette m_defaultPalette;
    };

} // namespace Kalorite
