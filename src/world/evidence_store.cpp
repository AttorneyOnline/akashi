#include "world/evidence_store.h"

#include <QRegularExpression>

namespace akashi {

// The owner tag a case manager puts into a description to hide an item
// from everyone but the listed sides.
static const QRegularExpression OWNER_TAG("<owner=(.*?)>");

void EvidenceStore::append(const Evidence &f_item)
{
    m_items.append(f_item);
}

void EvidenceStore::remove(int f_index)
{
    if (f_index >= 0 && f_index < m_items.size()) {
        m_items.removeAt(f_index);
    }
}

void EvidenceStore::replace(int f_index, const Evidence &f_item)
{
    if (f_index >= 0 && f_index < m_items.size()) {
        m_items.replace(f_index, f_item);
    }
}

void EvidenceStore::swap(int f_first, int f_second)
{
    if (f_first >= 0 && f_first < m_items.size() && f_second >= 0 && f_second < m_items.size()) {
        m_items.swapItemsAt(f_first, f_second);
    }
}

void EvidenceStore::revealToAll(int f_index)
{
    if (f_index < 0 || f_index >= m_items.size()) {
        return;
    }

    Evidence &l_item = m_items[f_index];
    if (OWNER_TAG.match(l_item.description).hasMatch()) {
        l_item.description.replace(OWNER_TAG, "<owner=all>");
    }
    else {
        l_item.description = "<owner=all>\n" + l_item.description;
    }
}

bool EvidenceStore::isVisible(const Evidence &f_item, bool f_can_see_hidden, const QString &f_side) const
{
    if (f_can_see_hidden || m_access != Access::HiddenCm) {
        return true;
    }
    const QRegularExpressionMatch l_match = OWNER_TAG.match(f_item.description);
    if (!l_match.hasMatch()) {
        // No owner tag means everyone sees it.
        return true;
    }
    const QStringList l_owners = l_match.captured(1).split(",");
    return l_owners.contains("all", Qt::CaseSensitivity::CaseInsensitive) ||
           l_owners.contains(f_side, Qt::CaseSensitivity::CaseInsensitive);
}

QList<Evidence> EvidenceStore::visibleItems(bool f_can_see_hidden, const QString &f_side) const
{
    QList<Evidence> l_visible;
    for (const Evidence &l_item : m_items) {
        if (isVisible(l_item, f_can_see_hidden, f_side)) {
            l_visible.append(l_item);
        }
    }
    return l_visible;
}

int EvidenceStore::itemIndexByVisibleIndex(int f_visible_index, bool f_can_see_hidden, const QString &f_side) const
{
    if (f_visible_index <= 0) {
        return -1;
    }

    int l_visible_count = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!isVisible(m_items[i], f_can_see_hidden, f_side)) {
            continue;
        }
        ++l_visible_count;
        if (l_visible_count == f_visible_index) {
            return i;
        }
    }
    return -1;
}

int EvidenceStore::visibleIndexByItemIndex(int f_item_index, bool f_can_see_hidden, const QString &f_side) const
{
    if (f_item_index < 0 || f_item_index >= m_items.size()) {
        return 0;
    }

    int l_visible_count = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!isVisible(m_items[i], f_can_see_hidden, f_side)) {
            continue;
        }
        ++l_visible_count;
        if (i == f_item_index) {
            return l_visible_count;
        }
    }
    return 0;
}

} // namespace akashi
