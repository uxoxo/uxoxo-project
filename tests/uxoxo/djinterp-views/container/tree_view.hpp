/*******************************************************************************
* djinterp [container]                                           tree_view.hpp
*
*   View component for tree_node forests.  Owns navigation, selection, edit,
*   context, and checkbox state.  Navigation and selection delegate to the
*   shared free functions in view_common.hpp (nav::, sel::).
*
* author(s): Samuel 'teer' Neal-Blim
* file:   \inc\container\tree_view.hpp                         date: 2025.05.19
*******************************************************************************/

#ifndef  DJINTERP_CONTAINER_TREE_VIEW_
#define  DJINTERP_CONTAINER_TREE_VIEW_ 1

#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#include <djinterp>
#include <container/tree_node.hpp>

NS_DJINTERP
namespace container {

// ═══════════════════════════════════════════════════════════════════════════════
//  TREE VIEW
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _Data, unsigned _Feat = vf_none, typename _Icon = int>
struct tree_view
    : view_mixin::rename_state    <has_feat(_Feat, vf_renamable)>
    , view_mixin::context_state   <has_feat(_Feat, vf_context)>
    , view_mixin::check_view_state<has_feat(_Feat, vf_checkable)>
{
    using node_type  = tree_node<_Data, _Feat, _Icon>;
    using data_type  = _Data;
    using icon_type  = _Icon;
    using entry_type = flat_entry<node_type>;
    static constexpr unsigned features = _Feat;
    static constexpr bool is_checkable   = has_feat(_Feat, vf_checkable);
    static constexpr bool has_icons      = has_feat(_Feat, vf_icons);
    static constexpr bool is_collapsible = has_feat(_Feat, vf_collapsible);
    static constexpr bool is_renamable   = has_feat(_Feat, vf_renamable);
    static constexpr bool has_context    = has_feat(_Feat, vf_context);
    static constexpr bool focusable  = true;
    static constexpr bool scrollable = true;

    // ── data ─────────────────────────────────────────────────────────────
    std::vector<node_type>   roots;
    std::size_t              cursor        = 0;
    std::size_t              scroll_offset = 0;
    std::size_t              page_size     = 20;
    selection_mode           sel_mode = selection_mode::single;
    std::vector<std::size_t> selected;
    std::string              search_query;
    bool                     search_active = false;
    std::vector<entry_type>  visible;
    bool                     visible_dirty = true;

    // ── visible cache ────────────────────────────────────────────────────
    void rebuild_visible() {
        visible = flatten_roots_visible(roots);
        visible_dirty = false;
        if (!visible.empty() && cursor >= visible.size())
            cursor = visible.size() - 1;
    }
    const std::vector<entry_type>& entries() {
        if (visible_dirty) rebuild_visible();
        return visible;
    }
    [[nodiscard]] std::size_t visible_count() {
        if (visible_dirty) rebuild_visible();
        return visible.size();
    }
    [[nodiscard]] node_type* cursor_node() {
        if (visible_dirty) rebuild_visible();
        return (cursor < visible.size()) ? visible[cursor].node : nullptr;
    }
    [[nodiscard]] const node_type* cursor_node() const {
        return const_cast<tree_view*>(this)->cursor_node();
    }
    [[nodiscard]] entry_type* cursor_entry() {
        if (visible_dirty) rebuild_visible();
        return (cursor < visible.size()) ? &visible[cursor] : nullptr;
    }

    // ── navigation (delegates to nav::) ──────────────────────────────────
    bool cursor_up() {
        if (visible_dirty) rebuild_visible();
        return nav::up(cursor, scroll_offset, page_size);
    }
    bool cursor_down() {
        if (visible_dirty) rebuild_visible();
        return nav::down(cursor, scroll_offset, page_size, visible.size());
    }
    bool cursor_home() {
        if (visible_dirty) rebuild_visible();
        return nav::home(cursor, scroll_offset, page_size);
    }
    bool cursor_end() {
        if (visible_dirty) rebuild_visible();
        return nav::end(cursor, scroll_offset, page_size, visible.size());
    }
    bool page_up() {
        if (visible_dirty) rebuild_visible();
        return nav::page_up(cursor, scroll_offset, page_size);
    }
    bool page_down() {
        if (visible_dirty) rebuild_visible();
        return nav::page_down(cursor, scroll_offset, page_size, visible.size());
    }

    // cursor_left: collapse, then parent
    bool cursor_left() {
        if (visible_dirty) rebuild_visible();
        if (cursor >= visible.size()) return false;
        auto& entry = visible[cursor];
        auto* node  = entry.node;
        if constexpr (is_collapsible) {
            if (!node->is_leaf() && node->expanded) {
                node->expanded = false; visible_dirty = true; rebuild_visible();
                return true;
            }
        }
        if (entry.depth > 0) {
            for (std::size_t i = cursor; i > 0; --i)
                if (visible[i-1].depth < entry.depth) {
                    cursor = i - 1;
                    nav::ensure_visible(cursor, scroll_offset, page_size);
                    return true;
                }
        }
        return false;
    }

    // cursor_right: expand, then first child
    bool cursor_right() {
        if (visible_dirty) rebuild_visible();
        if (cursor >= visible.size()) return false;
        auto* node = visible[cursor].node;
        if (node->is_leaf()) return false;
        if constexpr (is_collapsible) {
            if (!node->expanded) {
                node->expanded = true; visible_dirty = true; rebuild_visible();
                return true;
            }
        }
        if (cursor + 1 < visible.size() && visible[cursor+1].depth > visible[cursor].depth) {
            ++cursor; nav::ensure_visible(cursor, scroll_offset, page_size);
            return true;
        }
        return false;
    }

    // ── collapse (view-level) ────────────────────────────────────────────
    void expand_all_nodes() {
        static_assert(is_collapsible, "requires vf_collapsible");
        for (auto& r : roots) expand_all(r);
        visible_dirty = true;
    }
    void collapse_all_nodes() {
        static_assert(is_collapsible, "requires vf_collapsible");
        for (auto& r : roots) collapse_all(r);
        visible_dirty = true; cursor = 0; scroll_offset = 0;
    }
    void toggle_at_cursor() {
        static_assert(is_collapsible, "requires vf_collapsible");
        if (auto* n = cursor_node()) if (!n->is_leaf()) { toggle_expanded(*n); visible_dirty = true; }
    }

    // ── checkbox (view-level) ────────────────────────────────────────────
    void toggle_check_at_cursor() {
        static_assert(is_checkable, "requires vf_checkable");
        if (visible_dirty) rebuild_visible();
        if (cursor >= visible.size()) return;
        toggle_check(*visible[cursor].node, this->policy);
        if (this->policy == check_policy::cascade_both)
            for (auto& r : roots) sync_check_tree(r);
    }

    // check_all / uncheck_all / toggle_check_all
    void check_all() {
        static_assert(is_checkable, "requires vf_checkable");
        for (auto& r : roots) propagate_check_down(r, check_state::checked);
    }
    void uncheck_all() {
        static_assert(is_checkable, "requires vf_checkable");
        for (auto& r : roots) propagate_check_down(r, check_state::unchecked);
    }
    void toggle_check_all() {
        static_assert(is_checkable, "requires vf_checkable");
        bool all_checked = true;
        for (auto& r : roots) {
            walk(r, [&](const auto& n, std::size_t) {
                if (n.checked != check_state::checked) all_checked = false;
            });
        }
        if (all_checked) uncheck_all(); else check_all();
    }
    std::vector<node_type*> checked_nodes() {
        static_assert(is_checkable, "requires vf_checkable");
        std::vector<node_type*> result;
        for (auto& r : roots) walk(r, [&](auto& n, std::size_t) {
            if (n.checked == check_state::checked) result.push_back(&n);
        });
        return result;
    }

    // ── selection (delegates to sel::) ────────────────────────────────────
    void select_at_cursor() {
        if (sel_mode == selection_mode::none) return;
        if (visible_dirty) rebuild_visible();
        if (cursor >= visible.size()) return;
        if (sel_mode == selection_mode::single) sel::select_single(selected, cursor);
    }
    void toggle_select_at_cursor() {
        if (sel_mode != selection_mode::multi) return;
        if (visible_dirty) rebuild_visible();
        if (cursor >= visible.size()) return;
        sel::toggle_multi(selected, cursor);
    }
    void select_range(std::size_t from, std::size_t to) {
        if (sel_mode != selection_mode::multi) return;
        if (visible_dirty) rebuild_visible();
        sel::select_range(selected, from, to, visible.size());
    }
    void clear_selection() { selected.clear(); }
    [[nodiscard]] bool is_selected(std::size_t idx) const {
        return sel::is_selected(selected, idx);
    }
    std::vector<node_type*> selected_nodes() {
        if (visible_dirty) rebuild_visible();
        std::vector<node_type*> r; r.reserve(selected.size());
        for (auto i : selected) if (i < visible.size()) r.push_back(visible[i].node);
        return r;
    }

    // ── rename (view-level) ──────────────────────────────────────────────
    bool begin_edit() {
        static_assert(is_renamable, "requires vf_renamable");
        if (visible_dirty) rebuild_visible();
        if (cursor >= visible.size()) return false;
        if (!visible[cursor].node->renamable) return false;
        this->editing = true; this->edit_index = cursor;
        this->edit_buffer.clear(); this->edit_cursor = 0;
        return true;
    }
    bool begin_edit_with(const std::string& name) {
        if (!begin_edit()) return false;
        this->edit_buffer = name; this->edit_cursor = name.size(); return true;
    }
    bool commit_edit() {
        static_assert(is_renamable, "requires vf_renamable");
        if (!this->editing) return false;
        this->editing = false; return true;
    }
    void cancel_edit() {
        static_assert(is_renamable, "requires vf_renamable");
        this->editing = false; this->edit_buffer.clear();
    }

    // ── context menu (view-level) ────────────────────────────────────────
    bool open_context(int x = 0, int y = 0) {
        static_assert(has_context, "requires vf_context");
        if (visible_dirty) rebuild_visible();
        if (cursor >= visible.size()) return false;
        this->context_open = true; this->context_index = cursor;
        this->context_x = x; this->context_y = y; return true;
    }
    void close_context() {
        static_assert(has_context, "requires vf_context");
        this->context_open = false;
    }
    node_type* context_node() {
        static_assert(has_context, "requires vf_context");
        if (!this->context_open) return nullptr;
        if (visible_dirty) rebuild_visible();
        return (this->context_index < visible.size()) ? visible[this->context_index].node : nullptr;
    }

    // ── search ───────────────────────────────────────────────────────────
    template <typename _Match>
    bool search_next(_Match match) {
        if (visible_dirty) rebuild_visible();
        if (visible.empty()) return false;
        for (std::size_t i = 1; i <= visible.size(); ++i) {
            std::size_t idx = (cursor + i) % visible.size();
            if (match(visible[idx].node->data)) {
                cursor = idx; nav::ensure_visible(cursor, scroll_offset, page_size); return true;
            }
        }
        return false;
    }
    template <typename _Match>
    bool search_prev(_Match match) {
        if (visible_dirty) rebuild_visible();
        if (visible.empty()) return false;
        for (std::size_t i = 1; i <= visible.size(); ++i) {
            std::size_t idx = (cursor + visible.size() - i) % visible.size();
            if (match(visible[idx].node->data)) {
                cursor = idx; nav::ensure_visible(cursor, scroll_offset, page_size); return true;
            }
        }
        return false;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
//  TREE TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace tree_traits {
namespace detail {
    template <typename, typename = void> struct has_children_member : std::false_type {};
    template <typename _T> struct has_children_member<_T, std::void_t<decltype(std::declval<_T>().children)>> : std::true_type {};
    template <typename, typename = void> struct has_is_leaf_method : std::false_type {};
    template <typename _T> struct has_is_leaf_method<_T, std::void_t<decltype(std::declval<_T>().is_leaf())>> : std::true_type {};
    template <typename, typename = void> struct has_expanded_member : std::false_type {};
    template <typename _T> struct has_expanded_member<_T, std::void_t<decltype(std::declval<_T>().expanded)>> : std::true_type {};
    template <typename, typename = void> struct has_roots_member : std::false_type {};
    template <typename _T> struct has_roots_member<_T, std::void_t<decltype(std::declval<_T>().roots)>> : std::true_type {};
}

template <typename _T> inline constexpr bool has_children_v = detail::has_children_member<_T>::value;
template <typename _T> inline constexpr bool has_is_leaf_v  = detail::has_is_leaf_method<_T>::value;
template <typename _T> inline constexpr bool has_expanded_v = detail::has_expanded_member<_T>::value;
template <typename _T> inline constexpr bool has_roots_v    = detail::has_roots_member<_T>::value;

template <typename _Type>
struct is_tree_node : std::conjunction<
    view_traits::detail::has_data_member<_Type>, detail::has_children_member<_Type>,
    detail::has_is_leaf_method<_Type>> {};
template <typename _T> inline constexpr bool is_tree_node_v = is_tree_node<_T>::value;

template <typename _Type>
struct is_tree_view : std::conjunction<
    detail::has_roots_member<_Type>, view_traits::detail::has_cursor_member<_Type>,
    view_traits::detail::has_focusable_flag<_Type>> {};
template <typename _T> inline constexpr bool is_tree_view_v = is_tree_view<_T>::value;

template <typename _T> struct is_collapsible_node : std::conjunction<is_tree_node<_T>, detail::has_expanded_member<_T>> {};
template <typename _T> inline constexpr bool is_collapsible_node_v = is_collapsible_node<_T>::value;
template <typename _T> struct is_checkable_node : std::conjunction<is_tree_node<_T>, view_traits::detail::has_checked_member<_T>> {};
template <typename _T> inline constexpr bool is_checkable_node_v = is_checkable_node<_T>::value;
template <typename _T> struct is_icon_node : std::conjunction<is_tree_node<_T>, view_traits::detail::has_icon_member<_T>> {};
template <typename _T> inline constexpr bool is_icon_node_v = is_icon_node<_T>::value;
template <typename _T> struct is_renamable_node : std::conjunction<is_tree_node<_T>, view_traits::detail::has_renamable_member<_T>> {};
template <typename _T> inline constexpr bool is_renamable_node_v = is_renamable_node<_T>::value;
template <typename _T> struct is_context_node : std::conjunction<is_tree_node<_T>, view_traits::detail::has_context_actions_member<_T>> {};
template <typename _T> inline constexpr bool is_context_node_v = is_context_node<_T>::value;

}   // namespace tree_traits

}   // namespace container
NS_END
#endif
