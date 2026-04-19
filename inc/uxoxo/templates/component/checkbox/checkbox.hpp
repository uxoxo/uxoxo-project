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
*   DCheckState is defined in component_types.hpp (rather than here) so
* that toggleable_common.hpp can reference it for unified is_on / toggle /
* turn_on overloads without introducing a circular include.
*
*   Like the rest of the component namespace, the checkbox is a plain
* aggregate with no base class overhead.  Capability mixins (label,
* clearable, undoable) are engaged per-template-instance and contribute
* zero bytes when disabled thanks to EBO.  All operations in
* component_common.hpp (enable, disable, set_value, clear, undo,
* commit) and toggleable_common.hpp (is_on, is_off, is_mixed, turn_on,
* turn_off, set_mixed, toggle) work on this type through structural
* conformance.  The cb_* helpers below cover only the tri-state-specific
* operations (cb_cycle, cb_sync_from_children) that have no generic
* analogue; the binary ops previously prefixed cb_ are subsumed by the
* generic verbs from toggleable_common.hpp.
*
* Contents:
*   1  checkbox template
*   2  Common configurations (type aliases)
*   3  Checkbox-specific free functions (tri-state-only)
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
#include "./component_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  CHECKBOX TEMPLATE
// ===============================================================================

// checkbox
//   struct: generic checkbox component with optional label, clearable,
// undoable, and tri-state capability mixins.
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
//  2  COMMON CONFIGURATIONS
// ===============================================================================

// simple_checkbox
//   type: minimal binary checkbox — no label, no clearable, no undo, no
// mixed.
using simple_checkbox   = checkbox<false, false, false, false>;

// labeled_checkbox
//   type: binary checkbox with a label.
using labeled_checkbox  = checkbox<true,  false, false, false>;

// tri_checkbox
//   type: tri-state checkbox for "check all" / master controls.
using tri_checkbox      = checkbox<false, false, false, true>;

// full_checkbox
//   type: checkbox with all mixin capabilities enabled.
using full_checkbox     = checkbox<true,  true,  true,  true>;


/*****************************************************************************/

// ===============================================================================
//  3  CHECKBOX-SPECIFIC FREE FUNCTIONS
// ===============================================================================
//   Binary toggle operations (turn_on, turn_off, toggle, is_on, is_off,
// is_mixed, set_mixed) are provided by toggleable_common.hpp and work
// on checkbox directly via structural conformance.  The cb_* helpers
// below cover the two tri-state-only operations that have no generic
// analogue.

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

    bool        any_true;
    bool        any_false;
    DCheckState next;

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
