/*******************************************************************************
* uxoxo [component]                                             view_common.hpp
*
*   Shared infrastructure for tree_view and list_view (and any future view
* component).  This module owns:
*     1.  Deduction helpers
*     2.  Feature flags (vf_checkable, vf_icons, vf_renamable, vf_context)
*     3.  Common enumerations (check_state, selection_mode, sort_order, ...)
*     4.  Entry-level EBO mixins (checkable, icon, rename, context)
*     5.  View-level EBO mixins (rename_state, context_state, check_view_state)
*     6.  Navigation free functions (nav::)
*     7.  Selection free functions (sel::)
*     8.  Common view traits (view_traits::)
*
*   Neither tree_node nor list_entry appears here.  This module is strictly
* below both — they include it, never the reverse.
*
* 
* path:      /inc/uxoxo/templates/component/view_common.hpp 
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.26
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_VIEW_COMMON_
#define  UXOXO_COMPONENT_VIEW_COMMON_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/text/text_align.hpp>
#include <djinterp/core/util/sort/sort.hpp>
// uxoxo
#include "../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  DEDUCTION HELPERS
// ===============================================================================

// non_deduced
//   Prevents template argument deduction on a parameter.  Allows
// emplace(container, "hello") when _Data = std::string, because _Data
// is deduced only from the container, not from the const char* literal.
template <typename _Type>
struct type_identity
{
    using type = _Type;
};

template <typename _Type>
using non_deduced = typename type_identity<_Type>::type;


// ===============================================================================
//  2.  FEATURE FLAGS
// ===============================================================================
//   Combine with bitwise OR:
//     tree_node<string, vf_collapsible | vf_checkable | vf_icons>
//     list_entry<string, vf_checkable | vf_icons>
//
//   vf_collapsible is meaningful only for tree_node (a list has no children
// to collapse).  Enabling it on a list_entry is harmless — the collapse
// mixin is defined in tree_node.hpp, not here, so nothing happens.

enum view_feat : unsigned
{
    vf_none        = 0,
    vf_checkable   = 1u << 0,     // tri-state checkbox per entry
    vf_icons       = 1u << 1,     // icon + optional expanded_icon per entry
    vf_collapsible = 1u << 2,     // expanded/collapsed state  (tree only)
    vf_renamable   = 1u << 3,     // per-entry renamable flag
    vf_context     = 1u << 4,     // per-entry context-action descriptor

    vf_list_all    = vf_checkable | vf_icons | vf_renamable | vf_context,
    vf_tree_all    = vf_list_all  | vf_collapsible
};

constexpr view_feat operator|(view_feat a, view_feat b) noexcept
{
    return static_cast<view_feat>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr view_feat operator&(view_feat a, view_feat b) noexcept
{
    return static_cast<view_feat>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}

constexpr bool has_feat(unsigned f, view_feat bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}

// ===============================================================================
//  3.  COMMON ENUMERATIONS
// ===============================================================================

// check_state
//   Tri-state for checkbox entries.
enum class check_state : std::uint8_t
{
    unchecked,
    checked,
    indeterminate       // some children checked, some not (tree only)
};

// check_policy
//   How checkbox changes propagate.  independent is the only one that
// makes sense for flat lists; cascade variants are for tree hierarchies.
enum class check_policy : std::uint8_t
{
    independent,        // no propagation
    cascade_down,       // parent → all descendants
    cascade_both        // parent ↔ children (full sync + indeterminate)
};

// context_action
//   Bitfield of available context-menu actions for an entry.
// Application-specific actions start at ctx_user.
enum context_action : unsigned
{
    ctx_none      = 0,
    ctx_open      = 1u << 0,
    ctx_rename    = 1u << 1,
    ctx_delete    = 1u << 2,
    ctx_copy      = 1u << 3,
    ctx_cut       = 1u << 4,
    ctx_paste     = 1u << 5,
    ctx_new_child = 1u << 6,
    ctx_properties= 1u << 7,
    ctx_user      = 1u << 16,    // user-defined actions start here

    ctx_file_ops  = ctx_open | ctx_rename | ctx_delete | ctx_copy | ctx_cut | ctx_paste,
    ctx_all       = ctx_file_ops | ctx_new_child | ctx_properties
};

constexpr context_action operator|(context_action a, context_action b) noexcept
{
    return static_cast<context_action>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

// selection_mode
enum class selection_mode : std::uint8_t
{
    none,           // cursor only, no selection tracking
    single,         // at most one entry selected (click to replace)
    multi           // multi-select (Ctrl+click to toggle, Shift for range)
};

// ===============================================================================
//  4.  ENTRY-LEVEL EBO MIXINS
// ===============================================================================
//   Each mixin is a struct template parameterised on a bool.  The `false`
// specialisation is empty; the `true` specialisation holds per-entry data.
// When a node/entry inherits from a disabled mixin, EBO guarantees zero
// storage overhead.

namespace entry_mixin 
{

    // -- checkable --------------------------------------------------------

    template <bool _Enable>
    struct checkable_data {};

    template <>
    struct checkable_data<true>
    {
        check_state checked = check_state::unchecked;
    };

    // -- icons ------------------------------------------------------------

    template <bool _Enable,
              typename _Icon = int>
    struct icon_data {};

    template <typename _Icon>
    struct icon_data<true, _Icon>
    {
        _Icon icon{};
        _Icon expanded_icon{};
        bool  use_expanded = false;
    };

    // -- renamable --------------------------------------------------------

    template <bool _Enable>
    struct rename_data {};

    template <>
    struct rename_data<true>
    {
        bool renamable = true;
    };

    // -- context menu -----------------------------------------------------

    template <bool _Enable>
    struct context_data {};

    template <>
    struct context_data<true>
    {
        unsigned context_actions = ctx_all;
    };

}   // namespace entry_mixin

// ===============================================================================
//  5.  VIEW-LEVEL EBO MIXINS
// ===============================================================================
//   Per-view state that only exists when the corresponding feature is enabled.

namespace view_mixin {

    // -- rename state -----------------------------------------------------
    template <bool _Enable>
    struct rename_state {};

    template <>
    struct rename_state<true>
    {
        bool            editing        = false;
        std::size_t     edit_index     = 0;
        std::string     edit_buffer;
        std::size_t     edit_cursor    = 0;
    };

    // -- context menu state -----------------------------------------------
    template <bool _Enable>
    struct context_state {};

    template <>
    struct context_state<true>
    {
        bool            context_open   = false;
        std::size_t     context_index  = 0;
        int             context_x      = 0;
        int             context_y      = 0;
    };

    // -- checkbox view state ----------------------------------------------
    template <bool _Enable>
    struct check_view_state {};

    template <>
    struct check_view_state<true>
    {
        check_policy    policy = check_policy::independent;
    };

}   // namespace view_mixin

// ===============================================================================
//  6.  COLUMN DEFINITION
// ===============================================================================

struct column_def
{
    std::string    name;
    int            width = 0;       // 0 = flex
    float          flex  = 1.0f;    // proportional weight when width == 0
    text_alignment align = text_alignment::left;
    sort_order     sort  = sort_order::none;
};

// ===============================================================================
//  7.  NAVIGATION FREE FUNCTIONS
// ===============================================================================
//   Pure functions operating on cursor/scroll state.  Both tree_view and
// list_view delegate to these — no code duplication.

namespace nav {

    inline void ensure_visible(
        std::size_t  cursor,
        std::size_t& scroll,
        std::size_t  page)
    {
        if (cursor < scroll)
        {
            scroll = cursor;
        }
        else if (page > 0 && cursor >= scroll + page)
        {
            scroll = cursor - page + 1;
        }
    }

    inline bool up(
        std::size_t& cursor,
        std::size_t& scroll,
        std::size_t  page)
    {
        if (cursor > 0)
        {
            --cursor;
            ensure_visible(cursor, scroll, page);
            return true;
        }
        return false;
    }

    inline bool down(
        std::size_t& cursor,
        std::size_t& scroll,
        std::size_t  page,
        std::size_t  total)
    {
        if (total > 0 && cursor + 1 < total)
        {
            ++cursor;
            ensure_visible(cursor, scroll, page);
            return true;
        }
        return false;
    }

    inline bool home(
        std::size_t& cursor,
        std::size_t& scroll,
        std::size_t  page
    )
    {
        if (cursor != 0)
        {
            cursor = 0;
            ensure_visible(cursor, scroll, page);
            return true;
        }
        return false;
    }

    inline bool end(
        std::size_t& cursor,
        std::size_t& scroll,
        std::size_t  page,
        std::size_t  total)
    {
        if (total == 0)
        {
            return false;
        }
        auto last = total - 1;
        if (cursor != last)
        {
            cursor = last;
            ensure_visible(cursor, scroll, page);
            return true;
        }
        return false;
    }

    inline bool page_up(
        std::size_t& cursor,
        std::size_t& scroll,
        std::size_t  page)
    {
        if (cursor == 0)
        {
            return false;
        }
        cursor = (cursor > page) ? cursor - page : 0;
        ensure_visible(cursor, scroll, page);
        return true;
    }

    inline bool page_down(
        std::size_t& cursor,
        std::size_t& scroll,
        std::size_t  page,
        std::size_t  total)
    {
        if (total == 0)
        {
            return false;
        }
        auto last = total - 1;
        if (cursor >= last)
        {
            return false;
        }
        cursor = std::min(cursor + page, last);
        ensure_visible(cursor, scroll, page);
        return true;
    }

}   // namespace nav


// ===============================================================================
//  8.  SELECTION FREE FUNCTIONS
// ===============================================================================

namespace sel {

    inline void select_single(
        std::vector<std::size_t>& selected,
        std::size_t idx)
    {
        selected.clear();
        selected.push_back(idx);
    }

    inline void toggle_multi(
        std::vector<std::size_t>& selected,
        std::size_t idx)
    {
        auto it = std::find(selected.begin(), selected.end(), idx);
        if (it != selected.end())
        {
            selected.erase(it);
        }
        else
        {
            selected.push_back(idx);
        }
    }

    inline void select_range(
        std::vector<std::size_t>& selected,
        std::size_t from,
        std::size_t to,
        std::size_t total)
    {
        if (from > to)
        {
            std::swap(from, to);
        }
        if (total > 0)
        {
            to = std::min(to, total - 1);
        }
        selected.clear();
        for (std::size_t i = from; i <= to; ++i)
        {
            selected.push_back(i);
        }
    }

    inline bool is_selected(
        const std::vector<std::size_t>& selected,
        std::size_t idx)
    {
        return std::find(selected.begin(), selected.end(), idx) 
               != selected.end();
    }

}   // namespace sel

// ===============================================================================
//  9.  COMMON VIEW TRAITS  (SFINAE detection)
// ===============================================================================
//   Detectors that work identically for tree_view and list_view.  Component-
// specific traits live in their own headers (tree_traits, list_traits).

namespace view_traits {
namespace detail 
{
    // -- entry-level feature detectors ------------------------------------

    template <typename,
              typename = void>
    struct has_checked_member : std::false_type {};
    template <typename _Type>
    struct has_checked_member<_Type, std::void_t<
        decltype(std::declval<_Type>().checked)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_icon_member : std::false_type {};
    template <typename _Type>
    struct has_icon_member<_Type, std::void_t<
        decltype(std::declval<_Type>().icon)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_expanded_icon_member : std::false_type {};
    template <typename _Type>
    struct has_expanded_icon_member<_Type, std::void_t<
        decltype(std::declval<_Type>().expanded_icon)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_renamable_member : std::false_type {};
    template <typename _Type>
    struct has_renamable_member<_Type, std::void_t<
        decltype(std::declval<_Type>().renamable)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_context_actions_member : std::false_type {};
    template <typename _Type>
    struct has_context_actions_member<_Type, std::void_t<
        decltype(std::declval<_Type>().context_actions)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_data_member : std::false_type {};
    template <typename _Type>
    struct has_data_member<_Type, std::void_t<
        decltype(std::declval<_Type>().data)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_data_type_alias : std::false_type {};
    template <typename _Type>
    struct has_data_type_alias<_Type, std::void_t<
        typename _Type::data_type
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_features_constant : std::false_type {};
    template <typename _Type>
    struct has_features_constant<_Type, std::void_t<
        decltype(_Type::features)
    >> : std::true_type {};

    // -- view-level detectors ---------------------------------------------

    template <typename,
              typename = void>
    struct has_cursor_member : std::false_type {};
    template <typename _Type>
    struct has_cursor_member<_Type, std::void_t<
        decltype(std::declval<_Type>().cursor)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_scroll_offset_member : std::false_type {};
    template <typename _Type>
    struct has_scroll_offset_member<_Type, std::void_t<
        decltype(std::declval<_Type>().scroll_offset)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_selected_member : std::false_type {};
    template <typename _Type>
    struct has_selected_member<_Type, std::void_t<
        decltype(std::declval<_Type>().selected)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_editing_member : std::false_type {};
    template <typename _Type>
    struct has_editing_member<_Type, std::void_t<
        decltype(std::declval<_Type>().editing)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_context_open_member : std::false_type {};
    template <typename _Type>
    struct has_context_open_member<_Type, std::void_t<
        decltype(std::declval<_Type>().context_open)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_policy_member : std::false_type {};
    template <typename _Type>
    struct has_policy_member<_Type, std::void_t<
        decltype(std::declval<_Type>().policy)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_focusable_flag : std::false_type {};
    template <typename _Type>
    struct has_focusable_flag<_Type, std::enable_if_t<_Type::focusable>>
        : std::true_type {};

    template <typename,
              typename = void>
    struct has_scrollable_flag : std::false_type {};
    template <typename _Type>
    struct has_scrollable_flag<_Type, std::enable_if_t<_Type::scrollable>>
        : std::true_type {};

}   // namespace detail

// convenience aliases
template <typename _Type> inline constexpr bool has_checked_v          = detail::has_checked_member<_Type>::value;
template <typename _Type> inline constexpr bool has_icon_v             = detail::has_icon_member<_Type>::value;
template <typename _Type> inline constexpr bool has_expanded_icon_v    = detail::has_expanded_icon_member<_Type>::value;
template <typename _Type> inline constexpr bool has_renamable_v        = detail::has_renamable_member<_Type>::value;
template <typename _Type> inline constexpr bool has_context_actions_v  = detail::has_context_actions_member<_Type>::value;
template <typename _Type> inline constexpr bool has_data_v             = detail::has_data_member<_Type>::value;
template <typename _Type> inline constexpr bool has_data_type_v        = detail::has_data_type_alias<_Type>::value;
template <typename _Type> inline constexpr bool has_features_v         = detail::has_features_constant<_Type>::value;
template <typename _Type> inline constexpr bool has_cursor_v           = detail::has_cursor_member<_Type>::value;
template <typename _Type> inline constexpr bool has_scroll_offset_v    = detail::has_scroll_offset_member<_Type>::value;
template <typename _Type> inline constexpr bool has_selected_v         = detail::has_selected_member<_Type>::value;
template <typename _Type> inline constexpr bool has_editing_v          = detail::has_editing_member<_Type>::value;
template <typename _Type> inline constexpr bool has_context_open_v     = detail::has_context_open_member<_Type>::value;
template <typename _Type> inline constexpr bool has_policy_v           = detail::has_policy_member<_Type>::value;
template <typename _Type> inline constexpr bool is_focusable_v         = detail::has_focusable_flag<_Type>::value;
template <typename _Type> inline constexpr bool is_scrollable_v        = detail::has_scrollable_flag<_Type>::value;

// is_navigable_view
//   type trait: has cursor + scroll_offset + focusable.  The minimum for
// any view component that supports keyboard navigation.
template <typename _Type>
struct is_navigable_view : std::conjunction<
    detail::has_cursor_member<_Type>,
    detail::has_scroll_offset_member<_Type>,
    detail::has_focusable_flag<_Type>
> {};

template <typename _Type>
inline constexpr bool is_navigable_view_v = is_navigable_view<_Type>::value;

// is_editable_view
template <typename _Type>
struct is_editable_view : std::conjunction<
    is_navigable_view<_Type>,
    detail::has_editing_member<_Type>
> {};
template <typename _Type>
inline constexpr bool is_editable_view_v = is_editable_view<_Type>::value;

// is_context_view
template <typename _Type>
struct is_context_view : std::conjunction<
    is_navigable_view<_Type>,
    detail::has_context_open_member<_Type>
> {};
template <typename _Type>
inline constexpr bool is_context_view_v = is_context_view<_Type>::value;

// is_checkable_view
template <typename _Type>
struct is_checkable_view : std::conjunction<
    is_navigable_view<_Type>,
    detail::has_policy_member<_Type>
> {};
template <typename _Type>
inline constexpr bool is_checkable_view_v = is_checkable_view<_Type>::value;

// data type extraction
template <typename _Type,
          typename = void>
struct view_data { using type = void; };

template <typename _Type>
struct view_data<_Type, std::enable_if_t<has_data_type_v<_Type>>>
{
    using type = typename _Type::data_type;
};

template <typename _Type>
using view_data_t = typename view_data<_Type>::type;


NS_END  // namespace view_traits
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_VIEW_COMMON_