/*******************************************************************************
* uxoxo [component]                                          history_view.hpp
*
* Abstract history view:
*   A framework-agnostic, presentation-neutral history component template.
* history_view connects a history<> container (the data) to an arbitrary
* UI surface (the presentation) without prescribing how the history is
* displayed, persisted, or interacted with.
*
*   The template captures the structural invariants shared by all history
* presentations:
*     - A backing history<> container holding recorded entries
*     - A cursor position for navigation (current browse index)
*     - An active/inactive toggle (history may be disabled at runtime)
*     - A visibility policy (always visible, togglable, popup, etc.)
*     - An optional filter predicate over entries
*
*   What history_view deliberately does NOT prescribe:
*     - Rendering: could be a scrollable list, a popup overlay, ghost
*       text, a file log, a separate pane, or nothing visible at all.
*     - Persistence: could be in-memory only, written to a file, synced
*       to a database, or serialized to a config.
*     - Interaction: could be up/down arrows, click-to-select, search,
*       drag-and-drop reorder, or purely programmatic access.
*     - Layout: no position, size, or coordinate system.
*
*   A framework adapter reads the history_view state and maps it to
* concrete widgets.  The trait system in 5 detects conforming types
* structurally, so any type exposing the right members qualifies.
*
*   The _Policy template parameter is a tag type or struct that the
* framework uses to carry presentation hints.  The default is
* history_view_default_policy, which is an empty struct — meaning
* the framework gets no hints and must decide everything itself.
* A framework can define its own policy with static constexpr fields,
* type aliases, or methods to express preferences like:
*   - whether the view auto-scrolls to the newest entry
*   - whether entries are selectable
*   - whether the view supports search/filter UI
*   - maximum visible entries
*
* Contents:
*   1  Display mode enum
*   2  Default policy
*   3  EBO mixins
*   4  history_view struct
*   5  Free functions
*   6  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/history_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.09
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_HISTORY_VIEW_
#define  UXOXO_COMPONENT_HISTORY_VIEW_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../view_common.hpp"
#include "./history.hpp"


NS_UXOXO
NS_COMPONENT

// ===============================================================================
//  1  DISPLAY MODE
// ===============================================================================
//   Hints to the framework about how the history should be presented.
// The framework may ignore these entirely if the modality doesn't
// support them.

enum class history_display_mode : std::uint8_t
{
    // the framework decides — no preference expressed.
    automatic,

    // always visible inline (e.g. a scrollable list below input).
    inline_list,

    // shown on demand (e.g. a popup/dropdown triggered by a key).
    popup,

    // invisible — history is maintained but never rendered.
    // Useful for programmatic-only history (undo buffers, etc.).
    hidden,

    // ghost text overlay (e.g. dimmed previous command in the
    // input field itself, like fish shell).
    ghost
};


/*****************************************************************************/

// ===============================================================================
//  2  DEFAULT POLICY
// ===============================================================================
//   An empty policy — the framework gets no hints.  Framework-
// specific policies can define any combination of:
//   static constexpr history_display_mode display_mode;
//   static constexpr bool   auto_scroll;
//   static constexpr bool   selectable;
//   static constexpr bool   searchable;
//   static constexpr bool   persistent;
//   using  persistence_type = ...;
//   using  formatter_type   = ...;

struct history_view_default_policy
{};


/*****************************************************************************/

// ===============================================================================
//  3  EBO MIXINS
// ===============================================================================

namespace history_view_mixin {

    // -- policy carrier -----------------------------------------------
    //   Inherits from _Policy for EBO when the policy is empty.
    template <typename _Policy,
              bool _Empty = std::is_empty<_Policy>::value>
    struct policy_carrier
    {
        _Policy policy {};
    };

    template <typename _Policy>
    struct policy_carrier<_Policy, true> : _Policy
    {};

}   // namespace history_view_mixin


/*****************************************************************************/

// ===============================================================================
//  4  HISTORY VIEW
// ===============================================================================
//   _Type        element type stored in the history
//   _Container   backing sequential container (default: sequence<_Type>)
//   _SizeType    size type (default: std::size_t)
//   _MaxSize     compile-time capacity bound
//   _Policy      framework-supplied presentation policy
//
//   The history_view owns a history<> and adds presentation-neutral
// navigation state on top.

template <typename  _Type,
          typename  _Container = history<_Type>::container_type,
          typename  _SizeType  = std::size_t,
          _SizeType _MaxSize   = std::numeric_limits<
                                     _SizeType>::max(),
          typename  _Policy    = history_view_default_policy>
struct history_view
    : history_view_mixin::policy_carrier<_Policy>
{
    using value_type     = _Type;
    using container_type = _Container;
    using size_type      = _SizeType;
    using history_type   = history<_Type, _Container, _SizeType, _MaxSize>;
    using policy_type    = _Policy;
    using filter_fn      = std::function<
                               bool(const _Type&)>;

    static constexpr size_type max_capacity = _MaxSize;
    static constexpr bool focusable         = false;

    // -- data ---------------------------------------------------------
    history_type          data;

    // -- navigation ---------------------------------------------------
    //   cursor_pos tracks the browse position within the history.
    // A value of data.size() means "no selection / past the end"
    // (i.e. the user has navigated back to live input).
    size_type             cursor_pos    = 0;

    // -- display ------------------------------------------------------
    history_display_mode  display_mode  = history_display_mode::automatic;
    bool                  active        = true;
    bool                  visible       = true;

    // -- optional filter ----------------------------------------------
    //   When set, only entries satisfying the predicate are
    // considered "visible" to the framework.
    filter_fn             filter;

    // -- scroll -------------------------------------------------------
    size_type             scroll_offset = 0;
    size_type             page_size     = 0;

    // -- construction -------------------------------------------------
    history_view() = default;

    explicit history_view(
        size_type _max
    )
        : data(_max)
    {}

    history_view(
        size_type            _max,
        history_display_mode _mode
    )
        : data(_max),
            display_mode(_mode)
    {}

    // -- queries ------------------------------------------------------
    [[nodiscard]] bool
    empty() const noexcept
    {
        return data.empty();
    }

    [[nodiscard]] size_type
    size() const noexcept
    {
        return data.size();
    }

    [[nodiscard]] bool
    at_live_position() const noexcept
    {
        return cursor_pos >= data.size();
    }

    [[nodiscard]] bool
    has_filter() const noexcept
    {
        return static_cast<bool>(filter);
    }
};


/*****************************************************************************/

// ===============================================================================
//  5  FREE FUNCTIONS
// ===============================================================================

// hv_record
//   appends an entry to the history and resets the cursor
// to the live position.
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_record(history_view<_Type, _C, _S, _M, _P>& hv,
               const _Type&                          value)
{
    hv.data.record(value);
    hv.cursor_pos = hv.data.size();

    return;
}

// hv_record (move)
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_record(history_view<_Type, _C, _S, _M, _P>& hv,
               _Type&&                               value)
{
    hv.data.record(static_cast<_Type&&>(value));
    hv.cursor_pos = hv.data.size();

    return;
}

// hv_prev
//   navigates to the previous (older) entry.
// Returns true if the cursor moved.
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
bool hv_prev(history_view<_Type, _C, _S, _M, _P>& hv)
{
    if (hv.data.empty() || hv.cursor_pos == 0)
    {
        return false;
    }

    --hv.cursor_pos;

    return true;
}

// hv_next
//   navigates to the next (newer) entry, or back to the
// live position.  Returns true if the cursor moved.
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
bool hv_next(history_view<_Type, _C, _S, _M, _P>& hv)
{
    if (hv.cursor_pos >= hv.data.size())
    {
        return false;
    }

    ++hv.cursor_pos;

    return true;
}

// hv_go_to_live
//   resets the cursor to the live (past-end) position.
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_go_to_live(
    history_view<_Type, _C, _S, _M, _P>& hv)
{
    hv.cursor_pos = hv.data.size();

    return;
}

// hv_current
//   returns a pointer to the entry at the current cursor
// position, or nullptr if at the live position.
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
const _Type* hv_current(
    const history_view<_Type, _C, _S, _M, _P>& hv)
{
    if (hv.at_live_position())
    {
        return nullptr;
    }

    return &(hv.data[hv.cursor_pos]);
}

// hv_clear
//   clears all history entries and resets navigation.
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_clear(history_view<_Type, _C, _S, _M, _P>& hv)
{
    hv.data.clear();
    hv.cursor_pos    = 0;
    hv.scroll_offset = 0;

    return;
}

// hv_set_display_mode
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_set_display_mode(
    history_view<_Type, _C, _S, _M, _P>& hv,
    history_display_mode               mode)
{
    hv.display_mode = mode;

    return;
}

// hv_show / hv_hide / hv_toggle
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_show(history_view<_Type, _C, _S, _M, _P>& hv)
{
    hv.visible = true;

    return;
}

template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_hide(history_view<_Type, _C, _S, _M, _P>& hv)
{
    hv.visible = false;

    return;
}

template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_toggle(history_view<_Type, _C, _S, _M, _P>& hv)
{
    hv.visible = !hv.visible;

    return;
}

// hv_set_filter
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_set_filter(
    history_view<_Type, _C, _S, _M, _P>&                hv,
    typename history_view<_Type, _C, _S, _M, _P>::filter_fn fn)
{
    hv.filter = std::move(fn);

    return;
}

// hv_clear_filter
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P>
void hv_clear_filter(
    history_view<_Type, _C, _S, _M, _P>& hv)
{
    hv.filter = nullptr;

    return;
}

// hv_search
//   searches backward from cursor_pos for an entry matching
// the predicate.  Returns true and updates cursor_pos if found.
template <typename _Type, typename _C,
          typename _S, _S _M, typename _P,
          typename _Pred>
bool hv_search(
    history_view<_Type, _C, _S, _M, _P>& hv,
    _Pred                              pred)
{
    if (hv.data.empty())
    {
        return false;
    }

    // start from one before current position
    _S start = (hv.cursor_pos > 0)
               ? hv.cursor_pos - 1
               : 0;

    for (_S i = start + 1; i > 0; --i)
    {
        _S idx = i - 1;

        if (pred(hv.data[idx]))
        {
            hv.cursor_pos = idx;

            return true;
        }
    }

    return false;
}


/*****************************************************************************/

// ===============================================================================
//  6  TRAITS
// ===============================================================================

namespace history_view_traits {
namespace detail {

    template <typename, typename = void>
    struct has_data_member : std::false_type {};
    template <typename _Type>
    struct has_data_member<_Type, std::void_t<
        decltype(std::declval<_Type>().data)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_cursor_pos_member : std::false_type {};
    template <typename _Type>
    struct has_cursor_pos_member<_Type, std::void_t<
        decltype(std::declval<_Type>().cursor_pos)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_display_mode_member : std::false_type {};
    template <typename _Type>
    struct has_display_mode_member<_Type, std::void_t<
        decltype(std::declval<_Type>().display_mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_active_member : std::false_type {};
    template <typename _Type>
    struct has_active_member<_Type, std::void_t<
        decltype(std::declval<_Type>().active)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_visible_member : std::false_type {};
    template <typename _Type>
    struct has_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type>().visible)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_filter_member : std::false_type {};
    template <typename _Type>
    struct has_filter_member<_Type, std::void_t<
        decltype(std::declval<_Type>().filter)
    >> : std::true_type {};

}   // namespace detail

template <typename _Type>
inline constexpr bool has_data_v =
    detail::has_data_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_cursor_pos_v =
    detail::has_cursor_pos_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_display_mode_v =
    detail::has_display_mode_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_active_v =
    detail::has_active_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_visible_v =
    detail::has_visible_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_filter_v =
    detail::has_filter_member<_Type>::value;

// is_history_view
//   type trait: has data + cursor_pos + display_mode +
// active + visible.
template <typename _Type>
struct is_history_view : std::conjunction<
    detail::has_data_member<_Type>,
    detail::has_cursor_pos_member<_Type>,
    detail::has_display_mode_member<_Type>,
    detail::has_active_member<_Type>,
    detail::has_visible_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_history_view_v =
    is_history_view<_Type>::value;

// is_filterable_history_view
template <typename _Type>
struct is_filterable_history_view : std::conjunction<
    is_history_view<_Type>,
    detail::has_filter_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_filterable_history_view_v =
    is_filterable_history_view<_Type>::value;

}   // namespace history_view_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_HISTORY_VIEW_
