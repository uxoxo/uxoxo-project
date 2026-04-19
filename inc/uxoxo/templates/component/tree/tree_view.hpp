/*******************************************************************************
* uxoxo [component]                                               tree_view.hpp
*
*   The tree_view is a component that owns a forest of tree_nodes and manages
* navigation state: cursor, scroll, selection, edit mode, context menu.
*
*   Like all uxoxo components, it is a pure data aggregate.  It has no
* render() method, no base class, no vtable.  A renderer discovers its
* capabilities via tree_traits:: and dispatches with if constexpr / std::visit.
*
*   The observer pattern (pattern/observer.hpp) is NOT included here.  The
* component describes WHAT the UI state is; the pattern is attached externally
* by whoever wires the application.
*
*   Template parameters match tree_node:
*     _Data:   user payload type
*     _Feat:   bitwise OR of tree_feat flags
*     _Icon:   icon type  (default: int)
*
*
* path:      /inc/uxoxo/component/tree_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.26
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_TREE_VIEW_
#define  UXOXO_COMPONENT_TREE_VIEW_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// imgui
#include "imgui.h"
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./tree_node.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  ENUMERATIONS
// ===============================================================================
//   selection_mode is defined in view_common.hpp and reused here.


// ===============================================================================
//  2.  VIEW-LEVEL STATE MIXINS  (EBO)
// ===============================================================================
//   rename_state, context_state, and check_view_state are defined in
// view_common.hpp under namespace view_mixin and reused here.
//
//   NOTE: the shared check_view_state defaults `policy` to
// check_policy::independent.  Tree views that want the previous
// cascade_both default should set it explicitly at construction
// or via a tree_view factory.


// ===============================================================================
//  3.  TREE VIEW
// ===============================================================================

template <typename _Data,
    unsigned _Feat = tf_none,
    typename _Icon = int>
struct tree_view
    : view_mixin::rename_state  <has_feat(_Feat, tf_renamable)>
    , view_mixin::context_state <has_feat(_Feat, tf_context)>
    , view_mixin::check_view_state<has_feat(_Feat, tf_checkable)>
{
    // -- type aliases -----------------------------------------------------
    using node_type = tree_node<_Data, _Feat, _Icon>;
    using data_type = _Data;
    using icon_type = _Icon;
    using entry_type = flat_entry<node_type>;

    static constexpr unsigned features = _Feat;

    // compile-time feature queries (mirroring node)
    static constexpr bool is_checkable = has_feat(_Feat, tf_checkable);
    static constexpr bool has_icons = has_feat(_Feat, tf_icons);
    static constexpr bool is_collapsible = has_feat(_Feat, tf_collapsible);
    static constexpr bool is_renamable = has_feat(_Feat, tf_renamable);
    static constexpr bool has_context = has_feat(_Feat, tf_context);

    // component identity (for trait detection by renderers)
    static constexpr bool focusable = true;
    static constexpr bool scrollable = true;

    // -- data -------------------------------------------------------------
    std::vector<node_type>   roots;

    // navigation
    std::size_t              cursor = 0;     // flat index in visible list
    std::size_t              scroll_offset = 0;
    std::size_t              page_size = 20;    // visible rows (set by renderer)

    // selection
    selection_mode           sel_mode = selection_mode::single;
    std::vector<std::size_t> selected;              // flat indices of selected nodes

    // search / filter
    std::string              search_query;
    bool                     search_active = false;

    // -- visible entries cache --------------------------------------------
    //   Call rebuild_visible() after any structural mutation.
    //   Navigation methods call it automatically when needed.

    std::vector<entry_type>  visible;
    bool                     visible_dirty = true;

    void rebuild_visible()
    {
        visible = flatten_roots_visible(roots);
        visible_dirty = false;

        // clamp cursor
        if ((!visible.empty()) &&
            (cursor >= visible.size()))
        {
            cursor = visible.size() - 1;
        }
    }

    const std::vector<entry_type>& entries()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        return visible;
    }

    // -- cursor queries ---------------------------------------------------

    [[nodiscard]] node_type* cursor_node()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor < visible.size())
        {
            return visible[cursor].node;
        }
        return nullptr;
    }

    [[nodiscard]] const node_type* cursor_node() const
    {
        // const_cast for lazy rebuild — logically const
        return const_cast<tree_view*>(this)->cursor_node();
    }

    [[nodiscard]] entry_type* cursor_entry()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor < visible.size())
        {
            return &visible[cursor];
        }
        return nullptr;
    }

    [[nodiscard]] std::size_t visible_count()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        return visible.size();
    }

    // -- navigation -------------------------------------------------------

    bool cursor_up()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor > 0)
        {
            --cursor; ensure_visible(); return true;
        }
        return false;
    }

    bool cursor_down()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor + 1 < visible.size())
        {
            ++cursor; ensure_visible(); return true;
        }
        return false;
    }

    bool cursor_home()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor != 0)
        {
            cursor = 0; ensure_visible(); return true;
        }
        return false;
    }

    bool cursor_end()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        auto last = visible.empty() ? 0 : visible.size() - 1;
        if (cursor != last)
        {
            cursor = last; ensure_visible(); return true;
        }
        return false;
    }

    bool page_up()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor == 0)
        {
            return false;
        }
        cursor = (cursor > page_size) ? cursor - page_size : 0;
        ensure_visible();
        return true;
    }

    bool page_down()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (visible.empty())
        {
            return false;
        }
        auto last = visible.size() - 1;
        if (cursor >= last)
        {
            return false;
        }
        cursor = std::min(cursor + page_size, last);
        ensure_visible();
        return true;
    }

    // cursor_left
    //   If node is expanded, collapse it.
    //   If node is collapsed or a leaf, move to parent.
    bool cursor_left()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor >= visible.size())
        {
            return false;
        }

        auto& entry = visible[cursor];
        auto* node = entry.node;

        if constexpr (is_collapsible)
        {
            if (!node->is_leaf() && node->expanded)
            {
                node->expanded = false;
                visible_dirty = true;
                rebuild_visible();
                return true;
            }
        }

        // move to parent: find closest entry at depth-1 before cursor
        if (entry.depth > 0)
        {
            for (std::size_t i = cursor; i > 0; --i)
            {
                if (visible[i - 1].depth < entry.depth)
                {
                    cursor = i - 1;
                    ensure_visible();
                    return true;
                }
            }
        }
        return false;
    }

    // cursor_right
    //   If node is collapsed, expand it.
    //   If node is expanded, move to first child.
    bool cursor_right()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor >= visible.size())
        {
            return false;
        }

        auto* node = visible[cursor].node;

        if (node->is_leaf())
        {
            return false;
        }

        if constexpr (is_collapsible)
        {
            if (!node->expanded)
            {
                node->expanded = true;
                visible_dirty = true;
                rebuild_visible();
                // cursor stays on same node; children now visible below
                return true;
            }
        }

        // already expanded: move to first child
        if ((cursor + 1 < visible.size()) &&
            (visible[cursor + 1].depth > visible[cursor].depth))
        {
            ++cursor;
            ensure_visible();
            return true;
        }
        return false;
    }

    // ensure_visible
    //   Adjusts scroll_offset so cursor is within the viewport.
    void ensure_visible()
    {
        if (cursor < scroll_offset)
        {
            scroll_offset = cursor;
        }
        else if (cursor >= scroll_offset + page_size)
        {
            scroll_offset = cursor - page_size + 1;
        }
    }

    // -- collapse operations (view-level) ---------------------------------

    void expand_all_nodes()
    {
        static_assert(is_collapsible, "expand_all_nodes requires tf_collapsible");
        for (auto& r : roots)
        {
            expand_all(r);
        }
        visible_dirty = true;
    }

    void collapse_all_nodes()
    {
        static_assert(is_collapsible, "collapse_all_nodes requires tf_collapsible");
        for (auto& r : roots)
        {
            collapse_all(r);
        }
        visible_dirty = true;
        cursor = 0;
        scroll_offset = 0;
    }

    void toggle_at_cursor()
    {
        static_assert(is_collapsible, "toggle_at_cursor requires tf_collapsible");
        if (auto* node = cursor_node())
        {
            if (!node->is_leaf())
            {
                toggle_expanded(*node);
                visible_dirty = true;
            }
        }
    }

    // -- checkbox operations (view-level) ---------------------------------

    void toggle_check_at_cursor()
    {
        static_assert(is_checkable, "toggle_check_at_cursor requires tf_checkable");
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor >= visible.size())
        {
            return;
        }

        auto* node = visible[cursor].node;
        toggle_check(*node, this->policy);

        // cascade_both: walk ancestors upward and recalculate
        if (this->policy == check_policy::cascade_both)
        {
            // find which root this node belongs to and sync the whole tree
            // (more efficient ancestor-walk requires path tracking)
            for (auto& r : roots)
            {
                sync_check_tree(r);
            }
        }
    }

    // -- selection --------------------------------------------------------

    void select_at_cursor()
    {
        if (sel_mode == selection_mode::none)
        {
            return;
        }
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor >= visible.size())
        {
            return;
        }

        if (sel_mode == selection_mode::single)
        {
            selected.clear();
            selected.push_back(cursor);
        }
    }

    void toggle_select_at_cursor()
    {
        if (sel_mode != selection_mode::multi)
        {
            return;
        }
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (cursor >= visible.size())
        {
            return;
        }

        auto it = std::find(selected.begin(), selected.end(), cursor);
        if (it != selected.end())
        {
            selected.erase(it);
        }
        else
        {
            selected.push_back(cursor);
        }
    }

    void select_range(std::size_t from, std::size_t to)
    {
        if (sel_mode != selection_mode::multi)
        {
            return;
        }
        if (from > to)
        {
            std::swap(from, to);
        }
        if (visible_dirty)
        {
            rebuild_visible();
        }
        to = std::min(to, visible.size() - 1);

        selected.clear();
        for (std::size_t i = from; i <= to; ++i)
        {
            selected.push_back(i);
        }
    }

    void clear_selection() { selected.clear(); }

    [[nodiscard]] bool is_selected(std::size_t flat_index) const
    {
        return std::find(selected.begin(), selected.end(), flat_index)
            != selected.end();
    }

    // -- rename operations (view-level) -----------------------------------
    bool begin_edit()
    {
        static_assert(is_renamable, "begin_edit requires tf_renamable");
        if (visible_dirty)
        {
            rebuild_visible();
        }

        if (cursor >= visible.size())
        {
            return false;
        }

        auto* node = visible[cursor].node;
        if (!node->renamable)
        {
            return false;
        }

        this->editing = true;
        this->edit_index = cursor;
        // the caller must populate edit_buffer with current name
        // (we don't know which member of _Data holds the name)
        this->edit_buffer.clear();
        this->edit_cursor = 0;

        return true;
    }

    // begin_edit_with
    //   Begins editing and pre-populates the buffer (e.g. with node->data
    // or whatever the caller considers the "name").
    bool begin_edit_with(const std::string& current_name)
    {
        if (!begin_edit())
        {
            return false;
        }

        this->edit_buffer = current_name;
        this->edit_cursor = current_name.size();

        return true;
    }

    bool commit_edit()
    {
        static_assert(is_renamable, "commit_edit requires tf_renamable");

        if (!this->editing)
        {
            return false;
        }

        this->editing = false;
        // the caller reads edit_buffer and applies it to the node's data
        return true;
    }

    void cancel_edit()
    {
        static_assert(is_renamable, "cancel_edit requires tf_renamable");
        this->editing = false;
        this->edit_buffer.clear();
    }

    // -- context menu operations (view-level) -----------------------------

    bool open_context(int x = 0, int y = 0)
    {
        static_assert(has_context, "open_context requires tf_context");
        if (visible_dirty)
        {
            rebuild_visible();
        }

        if (cursor >= visible.size())
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
        static_assert(has_context, "close_context requires tf_context");
        this->context_open = false;
    }

    node_type* context_node()
    {
        static_assert(has_context, "context_node requires tf_context");
        if (!this->context_open)
        {
            return nullptr;
        }

        if (visible_dirty)
        {
            rebuild_visible();
        }

        if (this->context_index < visible.size())
        {
            return visible[this->context_index].node;
        }

        return nullptr;
    }

    // -- search -----------------------------------------------------------
    //   Moves cursor to next node matching the query.  The match predicate
    // is provided by the caller because we don't know _Data's shape.

    template <typename _Match>
    bool search_next(_Match match)
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (visible.empty())
        {
            return false;
        }

        // search forward from cursor+1, wrapping around
        for (std::size_t i = 1; i <= visible.size(); ++i)
        {
            std::size_t idx = (cursor + i) % visible.size();
            if (match(visible[idx].node->data))
            {
                cursor = idx;
                ensure_visible();
                return true;
            }
        }
        return false;
    }

    template <typename _Match>
    bool search_prev(_Match match)
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        if (visible.empty())
        {
            return false;
        }

        for (std::size_t i = 1; i <= visible.size(); ++i)
        {
            std::size_t idx = (cursor + visible.size() - i) % visible.size();
            if (match(visible[idx].node->data))
            {
                cursor = idx;
                ensure_visible();
                return true;
            }
        }
        return false;
    }

    // -- bulk access ------------------------------------------------------

    // selected_nodes
    //   Returns pointers to all selected nodes.
    std::vector<node_type*> selected_nodes()
    {
        if (visible_dirty)
        {
            rebuild_visible();
        }
        std::vector<node_type*> result;
        result.reserve(selected.size());
        for (auto idx : selected)
        {
            if (idx < visible.size())
            {
                result.push_back(visible[idx].node);
            }
        }
        return result;
    }

    // checked_nodes  (tf_checkable)
    //   Returns pointers to all checked nodes in the tree.
    std::vector<node_type*> checked_nodes()
    {
        static_assert(is_checkable, "checked_nodes requires tf_checkable");
        std::vector<node_type*> result;
        for (auto& r : roots)
        {
            walk(r, [&result](auto& n, std::size_t) {
                if (n.checked == check_state::checked)
                {
                    result.push_back(&n);
                }
                });
        }
        return result;
    }
};


// ===============================================================================
//  4  TREE TRAITS  (SFINAE detection)
// ===============================================================================

namespace tree_traits {
    NS_INTERNAL
        // -- node-level detectors ---------------------------------------------

        // has_data_member
        template <typename,
            typename = void>
        struct has_data_member : std::false_type {};
        template <typename _Type>
        struct has_data_member<_Type, std::void_t<
            decltype(std::declval<_Type>().data)
            >> : std::true_type {};

        // has_children_member
        template <typename,
            typename = void>
        struct has_children_member : std::false_type {};
        template <typename _Type>
        struct has_children_member<_Type, std::void_t<
            decltype(std::declval<_Type>().children)
            >> : std::true_type {};

        // has_is_leaf_method
        template <typename,
            typename = void>
        struct has_is_leaf_method : std::false_type {};
        template <typename _Type>
        struct has_is_leaf_method<_Type, std::void_t<
            decltype(std::declval<_Type>().is_leaf())
            >> : std::true_type {};

        // has_features_constant
        template <typename,
            typename = void>
        struct has_features_constant : std::false_type {};
        template <typename _Type>
        struct has_features_constant<_Type, std::void_t<
            decltype(_Type::features)
            >> : std::true_type {};

        // has_data_type_alias
        template <typename,
            typename = void>
        struct has_data_type_alias : std::false_type {};
        template <typename _Type>
        struct has_data_type_alias<_Type, std::void_t<
            typename _Type::data_type
            >> : std::true_type {};

        // -- feature detectors ------------------------------------------------
        //   These detect whether specific mixin data is present.

        // has_checked_member
        template <typename,
            typename = void>
        struct has_checked_member : std::false_type {};
        template <typename _Type>
        struct has_checked_member<_Type, std::void_t<
            decltype(std::declval<_Type>().checked)
            >> : std::true_type {};

        // has_icon_member
        template <typename,
            typename = void>
        struct has_icon_member : std::false_type {};
        template <typename _Type>
        struct has_icon_member<_Type, std::void_t<
            decltype(std::declval<_Type>().icon)
            >> : std::true_type {};

        // has_expanded_icon_member
        template <typename,
            typename = void>
        struct has_expanded_icon_member : std::false_type {};
        template <typename _Type>
        struct has_expanded_icon_member<_Type, std::void_t<
            decltype(std::declval<_Type>().expanded_icon)
            >> : std::true_type {};

        // has_expanded_member
        template <typename,
            typename = void>
        struct has_expanded_member : std::false_type {};
        template <typename _Type>
        struct has_expanded_member<_Type, std::void_t<
            decltype(std::declval<_Type>().expanded)
            >> : std::true_type {};

        // has_renamable_member
        template <typename,
            typename = void>
        struct has_renamable_member : std::false_type {};
        template <typename _Type>
        struct has_renamable_member<_Type, std::void_t<
            decltype(std::declval<_Type>().renamable)
            >> : std::true_type {};

        // has_context_actions_member
        template <typename,
            typename = void>
        struct has_context_actions_member : std::false_type {};
        template <typename _Type>
        struct has_context_actions_member<_Type, std::void_t<
            decltype(std::declval<_Type>().context_actions)
            >> : std::true_type {};

        /***********************************************************************/

        // -- view-level detectors ---------------------------------------------

        // has_roots_member
        template <typename,
            typename = void>
        struct has_roots_member : std::false_type {};
        template <typename _Type>
        struct has_roots_member<_Type, std::void_t<
            decltype(std::declval<_Type>().roots)
            >> : std::true_type {};

        // has_cursor_member
        template <typename,
            typename = void>
        struct has_cursor_member : std::false_type {};
        template <typename _Type>
        struct has_cursor_member<_Type, std::void_t<
            decltype(std::declval<_Type>().cursor)
            >> : std::true_type {};

        // has_scroll_offset_member
        template <typename,
            typename = void>
        struct has_scroll_offset_member : std::false_type {};
        template <typename _Type>
        struct has_scroll_offset_member<_Type, std::void_t<
            decltype(std::declval<_Type>().scroll_offset)
            >> : std::true_type {};

        // has_selected_member
        template <typename,
            typename = void>
        struct has_selected_member : std::false_type {};
        template <typename _Type>
        struct has_selected_member<_Type, std::void_t<
            decltype(std::declval<_Type>().selected)
            >> : std::true_type {};

        // has_editing_member (rename view state)
        template <typename,
            typename = void>
        struct has_editing_member : std::false_type {};
        template <typename _Type>
        struct has_editing_member<_Type, std::void_t<
            decltype(std::declval<_Type>().editing)
            >> : std::true_type {};

        // has_context_open_member
        template <typename,
            typename = void>
        struct has_context_open_member : std::false_type {};
        template <typename _Type>
        struct has_context_open_member<_Type, std::void_t<
            decltype(std::declval<_Type>().context_open)
            >> : std::true_type {};

        // has_policy_member (check view state)
        template <typename,
            typename = void>
        struct has_policy_member : std::false_type {};
        template <typename _Type>
        struct has_policy_member<_Type, std::void_t<
            decltype(std::declval<_Type>().policy)
            >> : std::true_type {};

        // has_focusable_flag
        template <typename,
            typename = void>
        struct has_focusable_flag : std::false_type {};
        template <typename _Type>
        struct has_focusable_flag<_Type, std::enable_if_t<_Type::focusable>>
            : std::true_type {
        };

        // has_scrollable_flag
        template <typename,
            typename = void>
        struct has_scrollable_flag : std::false_type {};
        template <typename _Type>
        struct has_scrollable_flag<_Type, std::enable_if_t<_Type::scrollable>>
            : std::true_type {
        };

    }   // internal


    

    // -- convenience aliases --------------------------------------------------

    // node detectors
    template <typename _Type> inline constexpr bool has_data_v = internal::has_data_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_children_v = internal::has_children_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_is_leaf_v = internal::has_is_leaf_method<_Type>::value;
    template <typename _Type> inline constexpr bool has_features_v = internal::has_features_constant<_Type>::value;
    template <typename _Type> inline constexpr bool has_data_type_v = internal::has_data_type_alias<_Type>::value;

    // feature detectors
    template <typename _Type> inline constexpr bool has_checked_v = internal::has_checked_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_icon_v = internal::has_icon_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_expanded_icon_v = internal::has_expanded_icon_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_expanded_v = internal::has_expanded_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_renamable_v = internal::has_renamable_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_context_actions_v = internal::has_context_actions_member<_Type>::value;

    // view detectors
    template <typename _Type> inline constexpr bool has_roots_v = internal::has_roots_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_cursor_v = internal::has_cursor_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_scroll_offset_v = internal::has_scroll_offset_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_selected_v = internal::has_selected_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_editing_v = internal::has_editing_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_context_open_v = internal::has_context_open_member<_Type>::value;
    template <typename _Type> inline constexpr bool has_policy_v = internal::has_policy_member<_Type>::value;
    template <typename _Type> inline constexpr bool is_focusable_v = internal::has_focusable_flag<_Type>::value;
    template <typename _Type> inline constexpr bool is_scrollable_v = internal::has_scrollable_flag<_Type>::value;


    

    // ===============================================================================
    //  COMPOSITE IDENTITY TRAITS
    // ===============================================================================

    // is_tree_node
    //   type trait: has data + children + is_leaf.  The structural minimum.
    template <typename _Type>
    struct is_tree_node : std::conjunction<
        internal::has_data_member<_Type>,
        internal::has_children_member<_Type>,
        internal::has_is_leaf_method<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_tree_node_v = is_tree_node<_Type>::value;

    

    // is_tree_view
    //   type trait: has roots + cursor + scroll_offset + focusable.
    template <typename _Type>
    struct is_tree_view : std::conjunction<
        internal::has_roots_member<_Type>,
        internal::has_cursor_member<_Type>,
        internal::has_scroll_offset_member<_Type>,
        internal::has_focusable_flag<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_tree_view_v = is_tree_view<_Type>::value;

    

    // node feature composites

    // is_checkable_node
    template <typename _Type>
    struct is_checkable_node : std::conjunction<
        is_tree_node<_Type>,
        internal::has_checked_member<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_checkable_node_v = is_checkable_node<_Type>::value;

    

    // is_icon_node
    template <typename _Type>
    struct is_icon_node : std::conjunction<
        is_tree_node<_Type>,
        internal::has_icon_member<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_icon_node_v = is_icon_node<_Type>::value;

    

    // is_collapsible_node
    template <typename _Type>
    struct is_collapsible_node : std::conjunction<
        is_tree_node<_Type>,
        internal::has_expanded_member<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_collapsible_node_v = is_collapsible_node<_Type>::value;

    

    // is_renamable_node
    template <typename _Type>
    struct is_renamable_node : std::conjunction<
        is_tree_node<_Type>,
        internal::has_renamable_member<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_renamable_node_v = is_renamable_node<_Type>::value;

    

    // is_context_node
    template <typename _Type>
    struct is_context_node : std::conjunction<
        is_tree_node<_Type>,
        internal::has_context_actions_member<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_context_node_v = is_context_node<_Type>::value;

    

    // view feature composites

    // is_editable_view
    template <typename _Type>
    struct is_editable_view : std::conjunction<
        is_tree_view<_Type>,
        internal::has_editing_member<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_editable_view_v = is_editable_view<_Type>::value;

    

    // is_context_view
    template <typename _Type>
    struct is_context_view : std::conjunction<
        is_tree_view<_Type>,
        internal::has_context_open_member<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_context_view_v = is_context_view<_Type>::value;

    

    // is_checkable_view
    template <typename _Type>
    struct is_checkable_view : std::conjunction<
        is_tree_view<_Type>,
        internal::has_policy_member<_Type>
    > {
    };

    template <typename _Type>
    inline constexpr bool is_checkable_view_v = is_checkable_view<_Type>::value;


    // -- data type extraction -------------------------------------------------

    // tree_node_data
    //   Extracts the _Data type from a tree_node.
    template <typename _Type,
        typename = void>
    struct tree_node_data { using type = void; };

    template <typename _Type>
    struct tree_node_data<_Type, std::enable_if_t<has_data_type_v<_Type>>>
    {
        using type = typename _Type::data_type;
    };

    template <typename _Type>
    using tree_node_data_t = typename tree_node_data<_Type>::type;


}   // namespace tree_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_TREE_VIEW_