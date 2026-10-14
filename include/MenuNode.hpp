#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace Kalorite {

    // Declarative menu tree rendered by QML (MenuBar and popups). Leaves emit triggered().
    class MenuNode : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY changed)
        Q_PROPERTY(QString shortcut READ shortcut WRITE setShortcut NOTIFY changed)
        Q_PROPERTY(QString icon READ icon WRITE setIcon NOTIFY changed)
        Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable NOTIFY changed)
        Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY changed)
        Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)
        Q_PROPERTY(bool separator READ isSeparator CONSTANT)
        Q_PROPERTY(QList<MenuNode*> items READ items NOTIFY itemsChanged)
        Q_PROPERTY(QList<MenuNode*> shortcuts READ shortcuts NOTIFY itemsChanged)

    public:
        explicit MenuNode(QObject* parent = nullptr);

        QString title() const { return m_title; }
        QString shortcut() const { return m_shortcut; }
        QString icon() const { return m_icon; }
        bool isCheckable() const { return m_checkable; }
        bool isChecked() const { return m_checked; }
        bool isEnabled() const { return m_enabled; }
        bool isSeparator() const { return m_separator; }
        const QList<MenuNode*>& items() const { return m_items; }
        QList<MenuNode*> shortcuts() const;

        void setTitle(const QString& value);
        void setShortcut(const QString& value);
        void setIcon(const QString& value);
        void setCheckable(bool value);
        void setChecked(bool value);
        void setEnabled(bool value);

        MenuNode* addItem(const QString& title, const QString& shortcut = {}, const QString& icon = {});
        MenuNode* addSeparator();
        void clear();

        // Toggles checkable nodes first, then emits triggered() with the resulting state.
        Q_INVOKABLE void trigger();

    signals:
        void triggered(bool checked);
        void changed();
        void itemsChanged();

    private:
        void append(MenuNode* node);

        QString m_title;
        QString m_shortcut;
        QString m_icon;
        bool m_checkable = false;
        bool m_checked = false;
        bool m_enabled = true;
        bool m_separator = false;
        QList<MenuNode*> m_items;
    };

} // namespace Kalorite
