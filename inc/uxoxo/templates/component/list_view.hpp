/*******************************************************************************
* uxoxo [component]                                               list_view.hpp
*
*   Flat list component with the same zero-cost feature composition as
* tree_view.  Shares navigation, selection, and EBO mixins via view_common.hpp.
*
*   list_entry<_Data, _Feat, _Icon>  —  per-row data + optional features
*   list_view <_Data, _Feat, _Icon>  —  owns a vector of entries + view state
*
*   Features:
*     vf_checkable   tri-state checkbox per entry + check_all / uncheck_all
*     vf_icons       icon per entry
*     vf_renamable   per-entry renamable flag + inline edit state
*     vf_context     per-entry context action bitfield
*     (vf_collapsible is ignored — lists have no hierarchy)
*
*   Columns:
*     list_view optionally holds a vector<column_def> for multi-column display.
*   The _Data payload is opaque — a renderer or extractor function maps
*   _Data × column_index → display string.  This keeps the data model clean.
*
*   Filtering:
*     set_filter(predicate) hides entries that don't match.  Navigation,
*   selection, and checkbox operations work on visible entries only.
*
*
* path:      /inc/uxoxo/component/list_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.26
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_LIST_VIEW_
#define  UXOXO_COMPONENT_LIST_VIEW_ 1

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <uxoxo>
#include <component/view_common.hpp>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  LIST ENTRY
// ═══════════════════════════════════════════════════════════════════════════════
//   Per-row data + optional feature mixins.  Like tree_node but without
// children or collapse.

template <typename _Data,
          unsigned _Feat = vf_none,
          typename _Icon = int>
struct list_entry
    : entry_mixin::checkable_data <has_feat(_Feat, vf_checkable)>
    , entry_mixin::icon_data      <has_feat(_Feat, vf_icons), _Icon>
    , entry_mixin::rename_data    <has_feat(_Feat, vf_renamable)>
    , entry_mixin::context_data   <has_feat(_Feat, vf_context)>
{
    using data_type = _Data;
    using icon_type = _Icon;

    static constexpr unsigned features = _Feat;
    static constexpr bool is_checkable = has_feat(_Feat, vf_checkable);
    static constexpr bool has_icons    = has_feat(_Feat, vf_icons);
    static constexpr bool is_renamable = has_feat(_Feat, vf_renamable);
    static constexpr bool has_context  = has_feat(_Feat, vf_context);

    _Data data;

    list_entry() = default;
    explicit list_entry(_Data d) : data(std::move(d)) {}
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  LIST VIEW
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _Data,
          unsigned _Feat = vf_none,
          typename _Icon = int>
struct list_view
    : view_mixin::rename_state    <has_feat(_Feat, vf_renamable)>
    , view_mixin::context_state   <has_feat(_Feat, vf_context)>
    , view_mixin::check_view_state<has_feat(_Feat, vf_checkable)>
{
    using entry_type = list_entry<_Data, _Feat, _Icon>;
    using data_type  = _Data;
    using icon_type  = _Icon;

    static constexpr unsigned features = _Feat;
    static constexpr bool is_checkable = has_feat(_Feat, vf_checkable);
    static constexpr bool has_icons    = has_feat(_Feat, vf_icons);
    static constexpr bool is_renamable = has_feat(_Feat, vf_renamable);
    static constexpr bool has_context  = has_feat(_Feat, vf_context);
    static constexpr bool focusable  = true;
    static constexpr bool scrollable = true;

    // ── data ─────────────────────────────────────────────────────────────
    std::vector<entry_type>       items;
    std::vector<column_def>       columns;

    // navigation
    std::size_t                   cursor        = 0;
    std::size_t                   scroll_offset = 0;
    std::size_t                   page_size     = 20;

    // selection
    selection_mode                sel_mode = selection_mode::single;
    std::vector<std::size_t>      selected;

    // filter / visible cache
    //   visible_ holds indices into items_ that pass the current filter.
    //   If no filter is set, visible_ == {0, 1, 2, ... items.size()-1}.
    std::vector<std::size_t>      visible_;
    bool                          visible_dirty_ = true;
    std::function<bool(const _Data&)> filter_;

    // ── visible cache ────────────────────────────────────────────────────

    void rebuild_visible()
    {
        visible_.clear();
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (!filter_ || filter_(items[i].data))
            {
                visible_.push_back(i);
            }
        }
        visible_dirty_ = false;
        if (!visible_.empty()
        {
            && cursor >= visible_.size())
        }
            cursor = visible_.size() - 1;
    }

    void mark_dirty() { visible_dirty_ = true; }

    [[nodiscard]] std::size_t visible_count()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        return visible_.size();
    }

    // visible_entry
    //   Returns a pointer to the entry at the given visible index.
    entry_type* visible_entry(std::size_t vis_idx)
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        if (vis_idx >= visible_.size())
        {
            return nullptr;
        }
        return &items[visible_[vis_idx]];
    }

    // item_index
    //   Maps a visible index back to the original items[] index.
    [[nodiscard]] std::size_t item_index(std::size_t vis_idx)
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        if (vis_idx >= visible_.size())
        {
            return static_cast<std::size_t>(-1);
        }
        return visible_[vis_idx];
    }

    // cursor_entry
    entry_type* cursor_entry()      { return visible_entry(cursor); }
    const entry_type* cursor_entry() const 
    { 
        return const_cast<list_view*>(this)->cursor_entry(); 
    }

    // ── add / remove ─────────────────────────────────────────────────────

    entry_type& add(entry_type e)
    {
        items.push_back(std::move(e));
        visible_dirty_ = true;
        return items.back();
    }

    entry_type& emplace(non_deduced<_Data> d)
    {
        items.emplace_back(std::move(d));
        visible_dirty_ = true;
        return items.back();
    }

    bool remove_at_cursor()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        if (cursor >= visible_.size())
        {
            return false;
        }
        auto idx = visible_[cursor];
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(idx));
        visible_dirty_ = true;
        return true;
    }

    void clear()
    {
        items.clear();
        cursor = 0;
        scroll_offset = 0;
        selected.clear();
        visible_dirty_ = true;
    }

    // ── filter ───────────────────────────────────────────────────────────

    template <typename _Pred>
    void set_filter(_Pred pred)
    {
        filter_ = std::move(pred);
        visible_dirty_ = true;
        cursor = 0;
        scroll_offset = 0;
        selected.clear();
    }

    void clear_filter()
    {
        filter_ = nullptr;
        visible_dirty_ = true;
        cursor = 0;
        scroll_offset = 0;
    }

    [[nodiscard]] bool has_filter() const { return static_cast<bool>(filter_); }

    // ── navigation (delegates to nav::) ──────────────────────────────────

    bool cursor_up()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        return nav::up(cursor, scroll_offset, page_size);
    }

    bool cursor_down()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        return nav::down(cursor, scroll_offset, page_size, visible_.size());
    }

    bool cursor_home()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        return nav::home(cursor, scroll_offset, page_size);
    }

    bool cursor_end()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        return nav::end(cursor, scroll_offset, page_size, visible_.size());
    }

    bool page_up()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        return nav::page_up(cursor, scroll_offset, page_size);
    }

    bool page_down()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        return nav::page_down(cursor, scroll_offset, page_size, visible_.size());
    }

    // ── selection (delegates to sel::) ────────────────────────────────────

    void select_at_cursor()
    {
        if (sel_mode == selection_mode::none)
        {
            return;
        }
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        if (cursor >= visible_.size())
        {
            return;
        }
        if (sel_mode == selection_mode::single)
        {
            sel::select_single(selected, cursor);
        }
    }

    void toggle_select_at_cursor()
    {
        if (sel_mode != selection_mode::multi)
        {
            return;
        }
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        if (cursor >= visible_.size())
        {
            return;
        }
        sel::toggle_multi(selected, cursor);
    }

    void select_range(std::size_t from, std::size_t to)
    {
        if (sel_mode != selection_mode::multi)
        {
            return;
        }
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        sel::select_range(selected, from, to, visible_.size());
    }

    void clear_selection() { selected.clear(); }

    [[nodiscard]] bool is_selected(std::size_t idx) const
    {
        return sel::is_selected(selected, idx);
    }

    void select_all()
    {
        if (sel_mode != selection_mode::multi)
        {
            return;
        }
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        selected.clear();
        for (std::size_t i = 0; i < visible_.size(); ++i)
        {
            selected.push_back(i);
        }
    }

    std::vector<entry_type*> selected_entries()
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        std::vector<entry_type*> r;
        r.reserve(selected.size());
        for (auto i : selected)
        {
            if (i < visible_.size())
        }
                r.push_back(&items[visible_[i]]);
        return r;
    }

    // ── checkbox operations ──────────────────────────────────────────────

    void toggle_check_at_cursor()
    {
        static_assert(is_checkable, "requires vf_checkable");
        auto* e = cursor_entry();
        if (!e)
        {
            return;
        }
        e->checked = (e->checked == check_state::checked)
                     ? check_state::unchecked : check_state::checked;
    }

    void check_all()
    {
        static_assert(is_checkable, "requires vf_checkable");
        for (auto& e : items)
        {
            e.checked = check_state::checked;
        }
    }

    void uncheck_all()
    {
        static_assert(is_checkable, "requires vf_checkable");
        for (auto& e : items)
        {
            e.checked = check_state::unchecked;
        }
    }

    void toggle_check_all()
    {
        static_assert(is_checkable, "requires vf_checkable");
        bool all_c = true;
        for (const auto& e : items)
        {
            if (e.checked != check_state::checked) { all_c = false; break; }
        }
        if (all_c)
        {
            uncheck_all(); else check_all();
        }
    }

    // check_visible_all / uncheck_visible_all
    //   Operates on filtered entries only.
    void check_visible_all()
    {
        static_assert(is_checkable, "requires vf_checkable");
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        for (auto idx : visible_)
        {
            items[idx].checked = check_state::checked;
        }
    }

    void uncheck_visible_all()
    {
        static_assert(is_checkable, "requires vf_checkable");
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        for (auto idx : visible_)
        {
            items[idx].checked = check_state::unchecked;
        }
    }

    std::vector<entry_type*> checked_entries()
    {
        static_assert(is_checkable, "requires vf_checkable");
        std::vector<entry_type*> r;
        for (auto& e : items)
        {
            if (e.checked == check_state::checked)
            {
                r.push_back(&e);
            }
        }
        return r;
    }

    [[nodiscard]] std::size_t checked_count() const
    {
        static_assert(is_checkable, "requires vf_checkable");
        std::size_t n = 0;
        for (const auto& e : items)
        {
            if (e.checked == check_state::checked) ++n;
        }
        return n;
    }

    // ── rename (view-level) ──────────────────────────────────────────────

    bool begin_edit()
    {
        static_assert(is_renamable, "requires vf_renamable");
        auto* e = cursor_entry();
        if (!e || !e->renamable)
        {
            return false;
        }
        this->editing = true;
        this->edit_index = cursor;
        this->edit_buffer.clear();
        this->edit_cursor = 0;
        return true;
    }

    bool begin_edit_with(const std::string& name)
    {
        if (!begin_edit())
        {
            return false;
        }
        this->edit_buffer = name;
        this->edit_cursor = name.size();
        return true;
    }

    bool commit_edit()
    {
        static_assert(is_renamable, "requires vf_renamable");
        if (!this->editing)
        {
            return false;
        }
        this->editing = false;
        return true;
    }

    void cancel_edit()
    {
        static_assert(is_renamable, "requires vf_renamable");
        this->editing = false;
        this->edit_buffer.clear();
    }

    // ── context menu (view-level) ────────────────────────────────────────

    bool open_context(int x = 0, int y = 0)
    {
        static_assert(has_context, "requires vf_context");
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        if (cursor >= visible_.size())
        {
            return false;
        }
        this->context_open = true;
        this->context_index = cursor;
        this->context_x = x;
        this->context_y = y;
        return true;
    }

    void close_context()
    {
        static_assert(has_context, "requires vf_context");
        this->context_open = false;
    }

    entry_type* context_entry()
    {
        static_assert(has_context, "requires vf_context");
        if (!this->context_open)
        {
            return nullptr;
        }
        return visible_entry(this->context_index);
    }

    // ── sort ─────────────────────────────────────────────────────────────
    //   The comparator receives (const entry_type&, const entry_type&).

    template <typename _Cmp>
    void sort(_Cmp cmp)
    {
        std::stable_sort(items.begin(), items.end(), cmp);
        visible_dirty_ = true;
        selected.clear();
    }

    // sort_by
    //   Convenience: provide a key extractor and sort direction.
    //   _Key: (const _Data&) → comparable
    template <typename _Key>
    void sort_by(_Key key, sort_order order = sort_order::ascending)
    {
        sort([&key, order](const entry_type& a, const entry_type& b) {
            if (order == sort_order::descending)
            {
                return key(b.data) < key(a.data);
            }
            return key(a.data) < key(b.data);
        });
    }

    // sort_by_column
    //   Sorts and updates the column's sort indicator.
    //   _Extract: (const _Data&, std::size_t col) → comparable
    template <typename _Extract>
    void sort_by_column(std::size_t col, sort_order order, _Extract extract)
    {
        // clear all column sort indicators
        for (auto& c : columns)
        {
            c.sort = sort_order::none;
        }
        if (col < columns.size())
        {
            columns[col].sort = order;
        }

        sort([&extract, col, order](const entry_type& a, const entry_type& b) {
            if (order == sort_order::descending)
            {
                return extract(b.data, col) < extract(a.data, col);
            }
            return extract(a.data, col) < extract(b.data, col);
        });
    }

    // ── search ───────────────────────────────────────────────────────────

    template <typename _Match>
    bool search_next(_Match match)
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        if (visible_.empty())
        {
            return false;
        }
        for (std::size_t i = 1; i <= visible_.size(); ++i)
        {
            std::size_t idx = (cursor + i) % visible_.size();
            if (match(items[visible_[idx]].data))
            {
                cursor = idx;
                nav::ensure_visible(cursor, scroll_offset, page_size);
                return true;
            }
        }
        return false;
    }

    template <typename _Match>
    bool search_prev(_Match match)
    {
        if (visible_dirty_)
        {
            rebuild_visible();
        }
        if (visible_.empty())
        {
            return false;
        }
        for (std::size_t i = 1; i <= visible_.size(); ++i)
        {
            std::size_t idx = (cursor + visible_.size() - i) % visible_.size();
            if (match(items[visible_[idx]].data))
            {
                cursor = idx;
                nav::ensure_visible(cursor, scroll_offset, page_size);
                return true;
            }
        }
        return false;
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  LIST ENTRY HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

// set_icon  (list_entry)
template <typename _D,
          unsigned _F,
          typename _I>
void set_icon(list_entry<_D, _F, _I>& e, non_deduced<_I> icon)
{
    static_assert(has_feat(_F, vf_icons), "requires vf_icons");
    e.icon = std::move(icon);
}

// set_icons  (list_entry)
template <typename _D,
          unsigned _F,
          typename _I>
void set_icons(list_entry<_D, _F, _I>& e,
               non_deduced<_I> normal, non_deduced<_I> expanded)
{
    static_assert(has_feat(_F, vf_icons), "requires vf_icons");
    e.icon = std::move(normal);
    e.expanded_icon = std::move(expanded);
    e.use_expanded = true;
}

// has_action  (list_entry)
template <typename _D,
          unsigned _F,
          typename _I>
bool has_action(const list_entry<_D, _F, _I>& e, context_action a)
{
    static_assert(has_feat(_F, vf_context), "requires vf_context");
    return (e.context_actions & static_cast<unsigned>(a)) != 0;
}

// set_actions  (list_entry)
template <typename _D,
          unsigned _F,
          typename _I>
void set_actions(list_entry<_D, _F, _I>& e, unsigned actions)
{
    static_assert(has_feat(_F, vf_context), "requires vf_context");
    e.context_actions = actions;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  LIST TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace list_traits {
namespace detail {
    template <typename,
              typename = void>
    struct has_items_member : std::false_type {};
    template <typename _T>
    struct has_items_member<_T, std::void_t<
        decltype(std::declval<_T>().items)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_columns_member : std::false_type {};
    template <typename _T>
    struct has_columns_member<_T, std::void_t<
        decltype(std::declval<_T>().columns)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_filter_member : std::false_type {};
    template <typename _T>
    struct has_filter_member<_T, std::void_t<
        decltype(std::declval<_T>().filter_)
    >> : std::true_type {};
}

template <typename _T> inline constexpr bool has_items_v   = detail::has_items_member<_T>::value;
template <typename _T> inline constexpr bool has_columns_v = detail::has_columns_member<_T>::value;
template <typename _T> inline constexpr bool has_filter_v  = detail::has_filter_member<_T>::value;

// is_list_entry
template <typename _Type>
struct is_list_entry : std::conjunction<
    view_traits::detail::has_data_member<_Type>,
    view_traits::detail::has_features_constant<_Type>,
    std::negation<tree_traits::detail::has_children_member<_Type>>
> {};
template <typename _T> inline constexpr bool is_list_entry_v = is_list_entry<_T>::value;

// is_list_view
template <typename _Type>
struct is_list_view : std::conjunction<
    detail::has_items_member<_Type>,
    view_traits::detail::has_cursor_member<_Type>,
    view_traits::detail::has_focusable_flag<_Type>,
    std::negation<tree_traits::detail::has_roots_member<_Type>>
> {};
template <typename _T> inline constexpr bool is_list_view_v = is_list_view<_T>::value;

}   // namespace list_traits


NS_END  // component
NS_END  // uxoxo

#endif  // UXOXO_COMPONENT_LIST_VIEW_
