/*******************************************************************************
* uxoxo [component]                                                 checkbox.hpp
*
* Generic tri-state checkbox component template:
*   A structurally-conforming uxoxo component representing a boolean toggle
* that may additionally enter a `mixed` (indeterminate) state.  The `mixed`
* state models the canonical "check all" parent whose governed children
* are in heterogeneous states — e.g. a table header checkbox whose rows
* are not uniformly selected.
*
*   Like the rest of the component namespace, the checkbox is a plain
* aggregate with no base class overhead.  Capability mixins (label,
* clearable, undoable) are engaged per-template-instance and contribute
* zero bytes when disabled thanks to EBO.  All operations in
* component_common.hpp (enable, disable, show, hide, set_value, clear,
* undo, commit) work on this type through structural conformance — the
* free functions below only add behaviors specific to the three-valued
* state space.
*
*   The `_TriState` template flag controls whether the `mixed` state is
* user-reachable via the checkbox-specific helpers.  When `_TriState` is
* false, `cb_set_mixed` is unavailable (static_assert); `mixed` remains
* representable in `DCheckState` but is treated as a non-terminal state
* by `cb_toggle`.
*
* Contents:
*   1  DCheckState enum
*   2  checkbox template
*   3  Common configurations (type aliases)
*   4  Checkbox-specific free functions
*
*
* path:      /inc/uxoxo/templates/component/checkbox.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_CHECKBOX_
#define  UXOXO_COMPONENT_CHECKBOX_ 1

// std
#include <cstdint>
#include <functional>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "./component_mixin.hpp"
#include "./component_common.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  CHECK STATE ENUM
// ===============================================================================

// DCheckState
//   enum: tri-state value type for checkbox components.  `mixed` represents
// the indeterminate state used by "check all" controls whose governed
// children are in heterogeneous states.
enum class DCheckState : std::uint8_t
{
    unchecked = 0,
    checked   = 1,
    mixed     = 2
};


/*****************************************************************************/

// ===============================================================================
//  2  CHECKBOX TEMPLATE
// ===============================================================================
//   Plain aggregate with optional EBO mixins.  All template parameters
// default to false, yielding a minimal focusable binary checkbox.  Each
// enabled mixin injects exactly the members its corresponding trait
// detectors in component_traits.hpp look for, so shared operations
// (clear, undo, set_value, commit) light up automatically.

// checkbox
//   struct: generic checkbox component template with tri-state support and
// optional label, clearable, and undoable capability mixins.
template <bool _HasLabel  = false,
          bool _Clearable = false,
          bool _Undoable  = false,
          bool _TriState  = false>
struct checkbox
    : public component_mixin::label_data<_HasLabel>,
      public component_mixin::clearable_data<_Clearable, DCheckState>,
      public component_mixin::undo_data<_Undoable, DCheckState>
{
    static constexpr bool focusable = true;
    static constexpr bool tri_state = _TriState;

    DCheckState                      value     = DCheckState::unchecked;
    bool                             enabled   = true;
    bool                             visible   = true;
    bool                             read_only = false;

    std::function<void(DCheckState)> on_commit;
    std::function<void(DCheckState)> on_change;
};


/*****************************************************************************/

// ===============================================================================
//  3  COMMON CONFIGURATIONS
// ===============================================================================
//   Convenience aliases for the most frequent checkbox shapes.  Clients
// are free to instantiate the primary template directly for arbitrary
// capability combinations.

// simple_checkbox
//   type: minimal binary checkbox — no label, no reset, no undo, no mixed.
using simple_checkbox   = checkbox<false, false, false, false>;

// labeled_checkbox
//   type: binary checkbox with a string label.
using labeled_checkbox  = checkbox<true,  false, false, false>;

// tri_checkbox
//   type: tri-state checkbox suitable for "check all" / master controls.
using tri_checkbox      = checkbox<false, false, false, true>;

// full_checkbox
//   type: labeled, clearable, undoable, tri-state checkbox.
using full_checkbox     = checkbox<true,  true,  true,  true>;


/*****************************************************************************/

// ===============================================================================
//  4  CHECKBOX-SPECIFIC FREE FUNCTIONS
// ===============================================================================
//   These operate on concrete checkbox instances and provide the
// three-valued semantics that component_common.hpp cannot express
// generically.  State transitions route through set_value so that
// undo snapshots and on_change callbacks fire correctly.

/*
cb_check
  Sets the checkbox to the `checked` state.  Undo state is captured and
on_change is invoked per set_value semantics.

Parameter(s):
  _c: the checkbox to modify.
Return:
  none.
*/
template <bool _HasLabel,
          bool _Clearable,
          bool _Undoable,
          bool _TriState>
void
cb_check(
    checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _c
)
{
    set_value(_c, DCheckState::checked);

    return;
}


/*
cb_uncheck
  Sets the checkbox to the `unchecked` state.  Undo state is captured and
on_change is invoked per set_value semantics.

Parameter(s):
  _c: the checkbox to modify.
Return:
  none.
*/
template <bool _HasLabel,
          bool _Clearable,
          bool _Undoable,
          bool _TriState>
void
cb_uncheck(
    checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _c
)
{
    set_value(_c, DCheckState::unchecked);

    return;
}


/*
cb_set_mixed
  Sets the checkbox to the `mixed` (indeterminate) state.  Only available
for tri-state checkboxes; attempting to instantiate this template with a
non-tri-state checkbox yields a compile-time error.

Parameter(s):
  _c: the checkbox to modify.  Must be instantiated with _TriState == true.
Return:
  none.
*/
template <bool _HasLabel,
          bool _Clearable,
          bool _Undoable,
          bool _TriState>
void
cb_set_mixed(
    checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _c
)
{
    static_assert(_TriState,
                  "cb_set_mixed requires a tri-state checkbox "
                  "(_TriState template parameter must be true).");

    set_value(_c, DCheckState::mixed);

    return;
}


/*
cb_toggle
  Toggles the checkbox.  When the current state is `checked`, transitions
to `unchecked`; from any other state (`unchecked` or `mixed`), transitions
to `checked`.  This matches conventional "check all" semantics where a
mixed master checkbox, when clicked, becomes fully checked.

Parameter(s):
  _c: the checkbox to toggle.
Return:
  none.
*/
template <bool _HasLabel,
          bool _Clearable,
          bool _Undoable,
          bool _TriState>
void
cb_toggle(
    checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _c
)
{
    DCheckState next;

    // `checked` is the only state that toggles back to `unchecked`;
    // `unchecked` and `mixed` both advance to `checked`.
    if (_c.value == DCheckState::checked)
    {
        next = DCheckState::unchecked;
    }
    else
    {
        next = DCheckState::checked;
    }

    set_value(_c, next);

    return;
}


/*
cb_cycle
  Cycles through all three states in order: unchecked -> checked -> mixed
-> unchecked.  Only available for tri-state checkboxes.  Useful for
controls that expose the indeterminate state as a directly-selectable
value rather than a derived one.

Parameter(s):
  _c: the checkbox to cycle.  Must be instantiated with _TriState == true.
Return:
  none.
*/
template <bool _HasLabel,
          bool _Clearable,
          bool _Undoable,
          bool _TriState>
void
cb_cycle(
    checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _c
)
{
    static_assert(_TriState,
                  "cb_cycle requires a tri-state checkbox "
                  "(_TriState template parameter must be true).");

    DCheckState next;

    switch (_c.value)
    {
        case DCheckState::unchecked:
            next = DCheckState::checked;
            break;

        case DCheckState::checked:
            next = DCheckState::mixed;
            break;

        case DCheckState::mixed:
        default:
            next = DCheckState::unchecked;
            break;
    }

    set_value(_c, next);

    return;
}


/*
cb_is_checked
  Query whether the checkbox is in the `checked` state.

Parameter(s):
  _c: the checkbox to query.
Return:
  true if value == DCheckState::checked, false otherwise.
*/
template <bool _HasLabel,
          bool _Clearable,
          bool _Undoable,
          bool _TriState>
[[nodiscard]] bool
cb_is_checked(
    const checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _c
) noexcept
{
    return (_c.value == DCheckState::checked);
}


/*
cb_is_unchecked
  Query whether the checkbox is in the `unchecked` state.

Parameter(s):
  _c: the checkbox to query.
Return:
  true if value == DCheckState::unchecked, false otherwise.
*/
template <bool _HasLabel,
          bool _Clearable,
          bool _Undoable,
          bool _TriState>
[[nodiscard]] bool
cb_is_unchecked(
    const checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _c
) noexcept
{
    return (_c.value == DCheckState::unchecked);
}


/*
cb_is_mixed
  Query whether the checkbox is in the `mixed` (indeterminate) state.
Always returns false for non-tri-state checkboxes (statically folded).

Parameter(s):
  _c: the checkbox to query.
Return:
  true if value == DCheckState::mixed, false otherwise.
*/
template <bool _HasLabel,
          bool _Clearable,
          bool _Undoable,
          bool _TriState>
[[nodiscard]] bool
cb_is_mixed(
    const checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _c
) noexcept
{
    if constexpr (!_TriState)
    {
        return false;
    }
    else
    {
        return (_c.value == DCheckState::mixed);
    }
}


/*
cb_sync_from_children
  Updates a tri-state master checkbox to reflect the aggregate state of
a range of child booleans.  All-true yields `checked`, all-false yields
`unchecked`, and any mix yields `mixed`.  This is the primary integration
point for "check all" behavior.

Parameter(s):
  _master:    the master checkbox to update.  Must be _TriState == true.
  _first:     iterator to the first child value.
  _last:      iterator one past the last child value.  Dereferencing any
              iterator in [_first, _last) must yield a type convertible
              to bool.
Return:
  none.
*/
template <bool        _HasLabel,
          bool        _Clearable,
          bool        _Undoable,
          bool        _TriState,
          typename    _InputIt>
void
cb_sync_from_children(
    checkbox<_HasLabel, _Clearable, _Undoable, _TriState>& _master,
    _InputIt                                               _first,
    _InputIt                                               _last
)
{
    static_assert(_TriState,
                  "cb_sync_from_children requires a tri-state master "
                  "checkbox (_TriState template parameter must be true).");

    bool any_true;
    bool any_false;

    any_true  = false;
    any_false = false;

    // single pass short-circuits as soon as heterogeneity is detected
    for (_InputIt it = _first; it != _last; ++it)
    {
        if (static_cast<bool>(*it))
        {
            any_true = true;
        }
        else
        {
            any_false = true;
        }

        if (any_true && any_false)
        {
            break;
        }
    }

    // derive target state: empty range collapses to unchecked
    DCheckState next;

    if (any_true && any_false)
    {
        next = DCheckState::mixed;
    }
    else if (any_true)
    {
        next = DCheckState::checked;
    }
    else
    {
        next = DCheckState::unchecked;
    }

    set_value(_master, next);

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_CHECKBOX_
