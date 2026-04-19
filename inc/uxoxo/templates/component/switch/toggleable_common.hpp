/*******************************************************************************
* uxoxo [component]                                         toggleable_common.hpp
*
* Shared toggleable component operations:
*   Generic free functions and traits for any component whose value is
* drawn from a small discrete set with a clear on/off axis — currently
* bool (toggle_switch, any future pure-binary control) and DCheckState
* (checkbox, any future tri-state master control).  Factoring these here
* gives one unified verb set (`is_on`, `toggle`, `turn_on`, `turn_off`)
* that works across all such components, parallel to how component_common
* provides one unified `enable`/`disable`/`set_value` surface regardless
* of the specific component type.
*
*   The operations are SFINAE-gated by value type rather than by
* component identity.  A bool-valued component need not "opt in" — any
* struct with a bool `value` member, `enabled`, and (for mutations) the
* set_value preconditions from component_common.hpp, automatically
* participates.  Same for DCheckState-valued components.
*
*   Future multi-position selectors (3+ ordered states) do NOT fit here.
* "Toggleable" specifically means binary-axis semantics: on or off, with
* `mixed` as a derived indeterminate state for master controls.  Ordinal
* selection across N named states is a different abstraction and will
* live in a future selection_common.hpp providing `select_next`,
* `select_prev`, `select_at`, `state_count`, etc.
*
*   Component-specific helpers with the old cb_* / sw_* prefixes remain
* in their respective headers for now, but they are aliases for the
* generic verbs — prefer the generic names in new code.
*
* Contents:
*   1  Value-type detectors (has_boolean_value, has_check_state_value)
*   2  Aggregate trait (is_toggleable)
*   3  Boolean-valued overloads (is_on, is_off, turn_on, turn_off, toggle)
*   4  DCheckState-valued overloads (is_on, is_off, is_mixed, turn_on,
*      turn_off, set_mixed, toggle)
*
*
* path:      /inc/uxoxo/templates/component/toggleable_common.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_TOGGLEABLE_COMMON_
#define  UXOXO_TOGGLEABLE_COMMON_ 1

// std
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "./component_traits.hpp"
#include "./component_common.hpp"
#include "./component_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  VALUE-TYPE DETECTORS
// ===============================================================================

namespace detail {

    // has_boolean_value
    //   trait: structural detector for a `.value` data member of type bool.
    // Stricter than "convertible to bool" on purpose — we do not want
    // sliders with floating-point value members to accidentally match
    // the toggleable overload set.
    template <typename, typename = void>
    struct has_boolean_value : std::false_type
    {};

    template <typename _Type>
    struct has_boolean_value<_Type, std::enable_if_t<
        std::is_same_v<
            std::remove_cv_t<std::remove_reference_t<
                decltype(std::declval<_Type&>().value)>>,
            bool>
    >> : std::true_type
    {};

    // has_check_state_value
    //   trait: structural detector for a `.value` data member of type
    // DCheckState.
    template <typename, typename = void>
    struct has_check_state_value : std::false_type
    {};

    template <typename _Type>
    struct has_check_state_value<_Type, std::enable_if_t<
        std::is_same_v<
            std::remove_cv_t<std::remove_reference_t<
                decltype(std::declval<_Type&>().value)>>,
            DCheckState>
    >> : std::true_type
    {};

}   // namespace detail


/*****************************************************************************/

// ===============================================================================
//  2  PUBLIC TRAITS
// ===============================================================================

// has_boolean_value_v
//   trait: true iff _Type has a `.value` data member of exactly type bool.
template <typename _Type>
inline constexpr bool has_boolean_value_v =
    detail::has_boolean_value<_Type>::value;

// has_check_state_value_v
//   trait: true iff _Type has a `.value` data member of exactly type
// DCheckState.
template <typename _Type>
inline constexpr bool has_check_state_value_v =
    detail::has_check_state_value<_Type>::value;

// is_toggleable_v
//   trait: true iff _Type is classified as a toggleable component —
// either boolean-valued or check-state-valued.  Future binary-axis
// state types would add their disjunct here.
template <typename _Type>
inline constexpr bool is_toggleable_v =
    ( has_boolean_value_v<_Type>      ||
      has_check_state_value_v<_Type> );


/*****************************************************************************/

// ===============================================================================
//  3  BOOLEAN-VALUED OVERLOADS
// ===============================================================================
//   These match components whose value member is exactly bool —
// toggle_switch today, and any structurally-conforming future binary
// component.

/*
is_on  (bool-valued)
  Query whether the component is in the on state.

Parameter(s):
  _c: the toggleable component to query.
Return:
  true if value is true, false otherwise.
*/
template <typename _Type,
          std::enable_if_t<
              has_boolean_value_v<_Type>,
              int> = 0>
[[nodiscard]] bool
is_on(
    const _Type& _c
) noexcept
{
    return _c.value;
}


/*
is_off  (bool-valued)
  Query whether the component is in the off state.

Parameter(s):
  _c: the toggleable component to query.
Return:
  true if value is false, false otherwise.
*/
template <typename _Type,
          std::enable_if_t<
              has_boolean_value_v<_Type>,
              int> = 0>
[[nodiscard]] bool
is_off(
    const _Type& _c
) noexcept
{
    return !_c.value;
}


/*
turn_on  (bool-valued)
  Sets the component's value to true.  Fires on_change via set_value.

Parameter(s):
  _c: the toggleable component to modify.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_boolean_value_v<_Type>,
              int> = 0>
void
turn_on(
    _Type& _c
)
{
    set_value(_c, true);

    return;
}


/*
turn_off  (bool-valued)
  Sets the component's value to false.  Fires on_change via set_value.

Parameter(s):
  _c: the toggleable component to modify.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_boolean_value_v<_Type>,
              int> = 0>
void
turn_off(
    _Type& _c
)
{
    set_value(_c, false);

    return;
}


/*
toggle  (bool-valued)
  Flips the component's value.  Fires on_change via set_value.

Parameter(s):
  _c: the toggleable component to flip.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_boolean_value_v<_Type>,
              int> = 0>
void
toggle(
    _Type& _c
)
{
    set_value(_c, !_c.value);

    return;
}


/*****************************************************************************/

// ===============================================================================
//  4  DCHECKSTATE-VALUED OVERLOADS
// ===============================================================================
//   These match components whose value member is DCheckState — checkbox
// today, and any future tri-state control.  Semantic choices:
//
//     is_on    → value == checked        (mixed is neither on nor off)
//     is_off   → value == unchecked
//     is_mixed → value == mixed
//     toggle   → checked -> unchecked, anything else -> checked
//                (mirrors the conventional "check all" click behavior
//                 where mixed becomes checked on user interaction)
//     set_mixed is only available for tri-state contexts; non-tri-state
//     checkboxes can still call it but the mixed state should be
//     externally-derived rather than user-reachable.

/*
is_on  (DCheckState-valued)
  Query whether the component is in the checked state.

Parameter(s):
  _c: the toggleable component to query.
Return:
  true if value == DCheckState::checked, false otherwise (including
  the mixed state).
*/
template <typename _Type,
          std::enable_if_t<
              has_check_state_value_v<_Type>,
              int> = 0>
[[nodiscard]] bool
is_on(
    const _Type& _c
) noexcept
{
    return (_c.value == DCheckState::checked);
}


/*
is_off  (DCheckState-valued)
  Query whether the component is in the unchecked state.

Parameter(s):
  _c: the toggleable component to query.
Return:
  true if value == DCheckState::unchecked, false otherwise (including
  the mixed state).
*/
template <typename _Type,
          std::enable_if_t<
              has_check_state_value_v<_Type>,
              int> = 0>
[[nodiscard]] bool
is_off(
    const _Type& _c
) noexcept
{
    return (_c.value == DCheckState::unchecked);
}


/*
is_mixed  (DCheckState-valued)
  Query whether the component is in the mixed / indeterminate state.

Parameter(s):
  _c: the toggleable component to query.
Return:
  true if value == DCheckState::mixed, false otherwise.
*/
template <typename _Type,
          std::enable_if_t<
              has_check_state_value_v<_Type>,
              int> = 0>
[[nodiscard]] bool
is_mixed(
    const _Type& _c
) noexcept
{
    return (_c.value == DCheckState::mixed);
}


/*
turn_on  (DCheckState-valued)
  Sets the component to DCheckState::checked.  Fires on_change via
set_value.

Parameter(s):
  _c: the toggleable component to modify.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_check_state_value_v<_Type>,
              int> = 0>
void
turn_on(
    _Type& _c
)
{
    set_value(_c, DCheckState::checked);

    return;
}


/*
turn_off  (DCheckState-valued)
  Sets the component to DCheckState::unchecked.  Fires on_change via
set_value.

Parameter(s):
  _c: the toggleable component to modify.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_check_state_value_v<_Type>,
              int> = 0>
void
turn_off(
    _Type& _c
)
{
    set_value(_c, DCheckState::unchecked);

    return;
}


/*
set_mixed  (DCheckState-valued)
  Sets the component to DCheckState::mixed.  This is typically used by
aggregate logic (e.g. a master checkbox reflecting heterogeneous
children) rather than by direct user interaction.  Fires on_change
via set_value.

Parameter(s):
  _c: the toggleable component to modify.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_check_state_value_v<_Type>,
              int> = 0>
void
set_mixed(
    _Type& _c
)
{
    set_value(_c, DCheckState::mixed);

    return;
}


/*
toggle  (DCheckState-valued)
  Flips the component between its on and non-on states.  Specifically:
from DCheckState::checked, transitions to DCheckState::unchecked; from
any other state (unchecked or mixed), transitions to
DCheckState::checked.  This matches conventional "check all" click
semantics where a mixed master, when clicked, becomes fully checked.

Parameter(s):
  _c: the toggleable component to toggle.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_check_state_value_v<_Type>,
              int> = 0>
void
toggle(
    _Type& _c
)
{
    DCheckState next;

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


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_TOGGLEABLE_COMMON_
