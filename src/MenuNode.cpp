#include "MenuNode.hpp"

namespace Kalorite {

MenuNode::MenuNode(QObject* parent) : QObject(parent) {}

QList<MenuNode*> MenuNode::shortcuts() const {
    QList<MenuNode*> out;
    for (MenuNode* node : m_items) {
        if (!node->m_shortcut.isEmpty()) out << node;
        out << node->shortcuts();
    }
    return out;
}

void MenuNode::setTitle(const QString& value) {
    if (m_title == value) return;
    m_title = value;
    emit changed();
}

void MenuNode::setShortcut(const QString& value) {
    if (m_shortcut == value) return;
    m_shortcut = value;
    emit changed();
}

void MenuNode::setIcon(const QString& value) {
    if (m_icon == value) return;
    m_icon = value;
    emit changed();
}

void MenuNode::setCheckable(bool value) {
    if (m_checkable == value) return;
    m_checkable = value;
    emit changed();
}

void MenuNode::setChecked(bool value) {
    if (m_checked == value) return;
    m_checked = value;
    emit changed();
}

void MenuNode::setEnabled(bool value) {
    if (m_enabled == value) return;
    m_enabled = value;
    emit changed();
}

MenuNode* MenuNode::addItem(const QString& title, const QString& shortcut, const QString& icon) {
    auto* node = new MenuNode(this);
    node->m_title = title;
    node->m_shortcut = shortcut;
    node->m_icon = icon;
    append(node);
    return node;
}

MenuNode* MenuNode::addSeparator() {
    auto* node = new MenuNode(this);
    node->m_separator = true;
    append(node);
    return node;
}

void MenuNode::clear() {
    for (MenuNode* node : m_items) node->deleteLater();
    m_items.clear();
    emit itemsChanged();
}

void MenuNode::trigger() {
    if (m_checkable) {
        m_checked = !m_checked;
        emit changed();
    }
    emit triggered(m_checked);
}

void MenuNode::append(MenuNode* node) {
    m_items.append(node);
    emit itemsChanged();
}

} // namespace Kalorite
