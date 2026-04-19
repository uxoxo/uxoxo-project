/*******************************************************************************
* uxoxo [component]                                           drop_container.hpp
*
* Generic drop-down / drop-up container:
*   A flat, ordered list of items that anchors to some parent position and
* expands downward (classic dropdown), upward (dropup), or in a direction
* the renderer chooses based on available space (auto).  The container
* itself is content-agnostic — it stores `_Item` values, tracks a
* highlighted index, and exposes open/close/navigate callbacks.
*
*   Target use cases:
*     - Autocomplete suggestion popovers
*     - Command-line / text-input history
*     - Select / combobox option lists
*     - Context and action menus
*     - Tag / mention pickers (`@user`, `#channel`)
*     - Any "list that appears next to something else"
*
*   Non-goals (by design):
*     - Nested / submenu behavior.  Compose two drop_containers if needed;
*       a single drop_container is flat.
*     - Filtering logic.  Consumers filter externally and replace `items`
*       via drop_set_items().  This keeps the container reusable for
*       history (no filter) and autocomplete (external filter) alike.
*     - Item styling.  The renderer resolves how an `_Item` becomes a
*       drawn row.
*
*   The container follows the standard uxoxo component protocol:
*     - Plain struct, no base class, no vtable.
*     - Common state members (enabled, visible, active) so shared free
*       functions in component_common.hpp (enable, show, activate, ...)
*       dispatch structurally without extra wiring.
*     - Optional capabilities via feature bits + EBO mixins from
*       component_mixin.hpp.
*     - Component-specific verbs live below as drop_* free functions.
*
* Contents:
*   1  DDropDirection                  — expansion axis enum
*   2  drop_container feature flags    — dcf_* bitmask
*   3  drop_container<>                — the component struct
*   4  drop_* free functions           — open, close, navigate, select
*
*
* path:      /inc/uxoxo/templates/component/drop_container.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_DROP_CONTAINER_
#define  UXOXO_COMPONENT_DROP_CONTAINER_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "./component_mixin.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  DROP DIRECTION
// ===============================================================================

// DDropDirection
//   enum: expansion direction of a drop_container relative to its anchor.
// `down` and `up` are explicit requests; `automatic` defers to the
// renderer, which picks whichever fits the remaining space best.  The
// renderer writes the resolved choice back into `resolved_direction`
// so the rest of the frame sees a concrete value.
enum class DDropDirection : std::uint8_t
{
    down      = 0,
    up        = 1,
    automatic = 2
};




// ===============================================================================
//  2  DROP CONTAINER FEATURE FLAGS
// ===============================================================================
//   Bitmask opt-in for drop_container capabilities.  Combine with
// bitwise-or when instantiating, e.g.:
//
//     drop_container<std::string, dcf_labeled | dcf_wraps> history;

enum : std::uint32_t
{
    dcf_none    = 0u,
    dcf_labeled = 1u << 0,  // adds component_mixin::label_data (header text)
    dcf_wraps   = 1u << 1   // navigation wraps past first / last
};




// ===============================================================================
//  3  DROP CONTAINER
// ===============================================================================

// drop_container
//   class: flat, anchored list of `_Item` values that expands up or down
// from an anchor.  Tracks a highlighted index, scroll offset, and
// standard component state (enabled / visible / active).  Optional
// capabilities are gated on `_Features` bits and injected via EBO
// mixins; see dcf_* above.
//
//   `_Item` may be anything copyable / movable — std::string for
// simple history, a custom struct with display text + payload for
// autocomplete, a function pointer for context menus, etc.  The
// renderer is responsible for turning an `_Item` into a drawn row.
template <typename      _Item,
          std::uint32_t _Features = dcf_none>
class drop_container
    : public component_mixin::label_data<(_Features & dcf_labeled) != 0>
{
public:
    using item_type  = _Item;
    using items_type = std::vector<_Item>;
    using size_type  = std::size_t;

    static constexpr std::uint32_t features   = _Features;
    static constexpr bool          focusable  = true;
    static constexpr bool          scrollable = true;

    // -- items --------------------------------------------------------
    //   Consumers may assign directly or go through drop_set_items to
    // also reset the highlight and scroll offset.
    items_type items;

    // -- core state ---------------------------------------------------
    bool enabled = true;   // interaction permitted
    bool visible = false;  // currently shown on screen
    bool active  = false;  // currently receiving navigation input

    // -- navigation ---------------------------------------------------
    size_type highlighted      = 0;   // index of highlighted item
    size_type scroll_offset    = 0;   // index of first visible row
    size_type max_visible_rows = 10;  // 0 means "unlimited / renderer decides"

    // -- layout -------------------------------------------------------
    //   `direction` is the consumer's request; `resolved_direction` is
    // what the renderer ultimately chose (equal to `direction` unless
    // `direction == automatic`).
    DDropDirection direction          = DDropDirection::down;
    DDropDirection resolved_direction = DDropDirection::down;
    size_type      desired_width      = 0;  // 0 means "auto from content"

    // -- callbacks ----------------------------------------------------
    //   All callbacks are optional; empty std::function is skipped.
    // `on_select` fires when the user confirms the highlighted item
    // (Enter / click).  `on_highlight` fires when the highlighted
    // index changes.  `on_open` / `on_close` bracket visibility
    // transitions triggered through drop_open / drop_close.
    std::function<void(const _Item&, size_type)> on_select;
    std::function<void(const _Item&, size_type)> on_highlight;
    std::function<void()>                        on_open;
    std::function<void()>                        on_close;
};




// ===============================================================================
//  4  DROP CONTAINER FREE FUNCTIONS
// ===============================================================================
//   Component-specific verbs.  Shared verbs (enable, disable, show,
// hide, activate, deactivate, is_enabled, is_visible) are inherited
// for free from component_common.hpp via structural detection — no
// additional plumbing required here.

// ---- 4.1  visibility lifecycle ------------------------------------------

// drop_open
//   function: makes the container visible, marks it active, and fires
// on_open if present.  No-op if already visible.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_open(
    drop_container<_Item, _Features>& _dc
)
{
    // guard against redundant open to avoid double callback fires
    if (_dc.visible)
    {
        return;
    }

    _dc.visible = true;
    _dc.active  = true;

    if (_dc.on_open)
    {
        _dc.on_open();
    }

    return;
}

// drop_close
//   function: hides the container, clears active state, and fires
// on_close if present.  Highlight and scroll state are preserved so
// a subsequent drop_open restores the prior navigation position.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_close(
    drop_container<_Item, _Features>& _dc
)
{
    if (!_dc.visible)
    {
        return;
    }

    _dc.visible = false;
    _dc.active  = false;

    if (_dc.on_close)
    {
        _dc.on_close();
    }

    return;
}

// drop_toggle
//   function: flips visibility — drop_open if hidden, drop_close if
// shown.  Fires the corresponding lifecycle callback.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_toggle(
    drop_container<_Item, _Features>& _dc
)
{
    if (_dc.visible)
    {
        drop_close(_dc);
    }
    else
    {
        drop_open(_dc);
    }

    return;
}




// ---- 4.2  item management -----------------------------------------------

// drop_set_items
//   function: replaces the item list and resets highlight + scroll
// offset to 0.  Preferred over direct assignment to `items` when
// the new list is unrelated to the old one (e.g. fresh autocomplete
// results for a new query).
template <typename      _Item,
          std::uint32_t _Features>
void
drop_set_items(
    drop_container<_Item, _Features>&                 _dc,
    typename drop_container<_Item, _Features>::items_type _new_items
)
{
    _dc.items         = std::move(_new_items);
    _dc.highlighted   = 0;
    _dc.scroll_offset = 0;

    return;
}

// drop_clear_items
//   function: empties the item list and resets navigation state.
// Does not alter visibility — use drop_close if the empty container
// should also be hidden.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_clear_items(
    drop_container<_Item, _Features>& _dc
)
{
    _dc.items.clear();
    _dc.highlighted   = 0;
    _dc.scroll_offset = 0;

    return;
}

// drop_is_empty
//   function: true when the container has no items.
template <typename      _Item,
          std::uint32_t _Features>
[[nodiscard]] bool
drop_is_empty(
    const drop_container<_Item, _Features>& _dc
) noexcept
{
    return _dc.items.empty();
}

// drop_item_count
//   function: number of items currently stored.
template <typename      _Item,
          std::uint32_t _Features>
[[nodiscard]] typename drop_container<_Item, _Features>::size_type
drop_item_count(
    const drop_container<_Item, _Features>& _dc
) noexcept
{
    return _dc.items.size();
}

// drop_highlighted_item
//   function: pointer to the currently highlighted item, or nullptr
// if the container is empty.  Returned by pointer rather than
// reference so callers can distinguish "empty" without exceptions.
template <typename      _Item,
          std::uint32_t _Features>
[[nodiscard]] const _Item*
drop_highlighted_item(
    const drop_container<_Item, _Features>& _dc
) noexcept
{
    if (_dc.items.empty() || (_dc.highlighted >= _dc.items.size()))
    {
        return nullptr;
    }

    return &_dc.items[_dc.highlighted];
}




// ---- 4.3  navigation ----------------------------------------------------

// drop_highlight_set
//   function: sets the highlighted index to `_index`, clamped to the
// valid range [0, items.size()).  Fires on_highlight if the index
// actually changed.  No-op on an empty container.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_highlight_set(
    drop_container<_Item, _Features>&                  _dc,
    typename drop_container<_Item, _Features>::size_type _index
)
{
    if (_dc.items.empty())
    {
        return;
    }

    // clamp to last valid index
    if (_index >= _dc.items.size())
    {
        _index = _dc.items.size() - 1;
    }

    if (_index == _dc.highlighted)
    {
        return;
    }

    _dc.highlighted = _index;

    if (_dc.on_highlight)
    {
        _dc.on_highlight(_dc.items[_index], _index);
    }

    return;
}

// drop_highlight_next
//   function: advances highlight by one.  At the end, wraps to 0 if
// dcf_wraps is set, otherwise stays put.  No-op on empty container.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_highlight_next(
    drop_container<_Item, _Features>& _dc
)
{
    if (_dc.items.empty())
    {
        return;
    }

    const auto last = _dc.items.size() - 1;

    if (_dc.highlighted < last)
    {
        drop_highlight_set(_dc, _dc.highlighted + 1);
    }
    else if constexpr ((_Features & dcf_wraps) != 0)
    {
        drop_highlight_set(_dc, 0);
    }

    return;
}

// drop_highlight_prev
//   function: moves highlight back by one.  At index 0, wraps to
// the last item if dcf_wraps is set, otherwise stays put.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_highlight_prev(
    drop_container<_Item, _Features>& _dc
)
{
    if (_dc.items.empty())
    {
        return;
    }

    if (_dc.highlighted > 0)
    {
        drop_highlight_set(_dc, _dc.highlighted - 1);
    }
    else if constexpr ((_Features & dcf_wraps) != 0)
    {
        drop_highlight_set(_dc, _dc.items.size() - 1);
    }

    return;
}

// drop_highlight_first
//   function: jumps highlight to index 0.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_highlight_first(
    drop_container<_Item, _Features>& _dc
)
{
    drop_highlight_set(_dc, 0);

    return;
}

// drop_highlight_last
//   function: jumps highlight to the final item.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_highlight_last(
    drop_container<_Item, _Features>& _dc
)
{
    if (_dc.items.empty())
    {
        return;
    }

    drop_highlight_set(_dc, _dc.items.size() - 1);

    return;
}

// drop_page_down
//   function: advances highlight by max_visible_rows (or 1 if that
// field is 0).  Saturates at the last item; does not wrap even
// when dcf_wraps is set (page motion is intentional long-range
// travel, wrap would be disorienting).
template <typename      _Item,
          std::uint32_t _Features>
void
drop_page_down(
    drop_container<_Item, _Features>& _dc
)
{
    if (_dc.items.empty())
    {
        return;
    }

    const auto step = (_dc.max_visible_rows > 0)
                      ? _dc.max_visible_rows
                      : 1u;
    const auto last = _dc.items.size() - 1;
    const auto next = ((_dc.highlighted + step) < last)
                      ? (_dc.highlighted + step)
                      : last;

    drop_highlight_set(_dc, next);

    return;
}

// drop_page_up
//   function: counterpart to drop_page_down, moving toward index 0.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_page_up(
    drop_container<_Item, _Features>& _dc
)
{
    if (_dc.items.empty())
    {
        return;
    }

    const auto step = (_dc.max_visible_rows > 0)
                      ? _dc.max_visible_rows
                      : 1u;
    const auto next = (_dc.highlighted > step)
                      ? (_dc.highlighted - step)
                      : 0u;

    drop_highlight_set(_dc, next);

    return;
}




// ---- 4.4  scrolling -----------------------------------------------------

// drop_scroll_to_highlight
//   function: adjusts scroll_offset so the highlighted row is within
// the visible window [scroll_offset, scroll_offset + max_visible_rows).
// No-op when max_visible_rows is 0 (renderer handles its own
// windowing) or when the container is empty.
template <typename      _Item,
          std::uint32_t _Features>
void
drop_scroll_to_highlight(
    drop_container<_Item, _Features>& _dc
)
{
    if (_dc.items.empty() || (_dc.max_visible_rows == 0))
    {
        return;
    }

    // scrolled past the top of the window -> pull it down
    if (_dc.highlighted < _dc.scroll_offset)
    {
        _dc.scroll_offset = _dc.highlighted;

        return;
    }

    // scrolled past the bottom of the window -> push it up
    const auto window_end = _dc.scroll_offset + _dc.max_visible_rows;

    if (_dc.highlighted >= window_end)
    {
        _dc.scroll_offset = _dc.highlighted - _dc.max_visible_rows + 1;
    }

    return;
}




// ---- 4.5  selection -----------------------------------------------------

// drop_select
//   function: confirms the highlighted item by invoking on_select
// with its value and index.  Returns true if a callback fired,
// false if the container is empty, disabled, or has no on_select
// installed.  Does NOT implicitly close the container — the
// consumer decides whether selection should dismiss (typical for
// autocomplete) or keep it open (typical for multi-select menus).
template <typename      _Item,
          std::uint32_t _Features>
bool
drop_select(
    drop_container<_Item, _Features>& _dc
)
{
    if ( (!_dc.enabled)  ||
         (_dc.items.empty()) ||
         (_dc.highlighted >= _dc.items.size()) )
    {
        return false;
    }

    if (!_dc.on_select)
    {
        return false;
    }

    _dc.on_select(_dc.items[_dc.highlighted], _dc.highlighted);

    return true;
}

// drop_select_at
//   function: variant of drop_select that first moves the highlight
// to `_index` (clamped), then fires on_select.  Useful for mouse
// clicks where the click row is not the current highlight.
template <typename      _Item,
          std::uint32_t _Features>
bool
drop_select_at(
    drop_container<_Item, _Features>&                    _dc,
    typename drop_container<_Item, _Features>::size_type _index
)
{
    drop_highlight_set(_dc, _index);

    return drop_select(_dc);
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DROP_CONTAINER_
