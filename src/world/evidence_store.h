#pragma once

#include "akashi_core_export.h"
#include "proto/evidence.h"

#include <QList>
#include <QString>

namespace akashi {

// The court record of one area: the ordered items people can present.
// Today an item is the evidence triple the AO2 protocol knows (name,
// description, image); this store is the seam a richer item system -
// character profiles, roleplay items - replaces later. Everything outside
// talks to the store, never to the item layout, so that swap stays local.
class AKASHI_CORE_EXPORT EvidenceStore
{
  public:
    // Who may change the items.
    enum class Access
    {
        FreeForAll,
        Mod,
        Cm,
        // Like Cm, but an item whose description carries an <owner=...> tag
        // is shown only to the listed sides; a case manager sees everything.
        HiddenCm,
    };

    Access access() const { return m_access; }
    void setAccess(Access f_access) { m_access = f_access; }

    QList<Evidence> items() const { return m_items; }
    int itemCount() const { return m_items.size(); }

    void append(const Evidence &f_item);

    // The mutations ignore indexes that do not exist.
    void remove(int f_index);
    void replace(int f_index, const Evidence &f_item);
    void swap(int f_first, int f_second);

    // Rewrites the item's owner tag to <owner=all>, revealing it to everyone.
    void revealToAll(int f_index);

    // The items as one viewer sees them: everything, unless hidden mode is
    // on and the viewer cannot see hidden items - then only the items owned
    // by their side (or by all).
    QList<Evidence> visibleItems(bool f_can_see_hidden, const QString &f_side) const;

    // Translate between an item's place in the full record and its place in
    // one viewer's (1-based) view of it. -1 and 0 mean "not visible".
    int itemIndexByVisibleIndex(int f_visible_index, bool f_can_see_hidden, const QString &f_side) const;
    int visibleIndexByItemIndex(int f_item_index, bool f_can_see_hidden, const QString &f_side) const;

  private:
    bool isVisible(const Evidence &f_item, bool f_can_see_hidden, const QString &f_side) const;

    QList<Evidence> m_items;
    Access m_access = Access::FreeForAll;
};

} // namespace akashi

