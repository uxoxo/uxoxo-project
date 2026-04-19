/*******************************************************************************
* uxoxo [component]                                     bounded_range_common.hpp
*
* Shared bounded-range component operations:
*   Generic free functions and traits for any component whose value lies
* within a closed numeric range [min_value, max_value].  Originally these
* operations were duplicated across slider (sl_set_range, sl_normalized,
* sl_is_at_min, etc.) and progress_bar (pb_set_range, pb_normalized,
* pb_is_at_min, etc.).  Factoring them here gives three benefits:
*
*     1. One implementation per operation instead of per-component-per-op.
*     2. A uniform call surface: `normalized(slider)`, `normalized(pb)`,
*        and future range-components all respond to the same verbs.
*     3. Type-safe participation: has_bounded_range_v<T> classifies any
*        structurally-conforming component, so generic code (renderers,
*        serializers, test harnesses) can statically dispatch.
*
*   Component-specific behaviors that need to decorate the generic ops —
* e.g. progress_bar firing on_complete on the underrun-to-complete
* transition — continue to live in their own headers as thin wrappers
* that call the generic op and handle their additional semantics
* around it.
*
*   The detection pattern mirrors component_traits.hpp: SFINAE detectors
* in an anonymous detail namespace, inline constexpr `_v` aliases in the
* component namespace.  Functions are gated via std::enable_if_t on the
* aggregate trait has_bounded_range_v<T>.
*
* Contents:
*   1  Member detectors (min_value, max_value)
*   2  Aggregate trait (has_bounded_range)
*   3  Value-type alias (range_value_t)
*   4  Query operations (span, is_at_min, is_at_max, normalized, in_range)
*   5  Mutation operations (set_range, clamp_to_range, set_normalized)
*
*
* path:      /inc/uxoxo/templates/component/bounded_range_common.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_BOUNDED_RANGE_COMMON_
#define  UXOXO_BOUNDED_RANGE_COMMON_ 1

// std
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "./component_traits.hpp"
#include "./component_common.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  MEMBER DETECTORS
// ===============================================================================

namespace detail {

    // has_min_value_member
    //   trait: structural detector for a `.min_value` data member.
    template <typename, typename = void>
    struct has_min_value_member : std::false_type
    {};

    template <typename _Type>
    struct has_min_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().min_value)
    >> : std::true_type
    {};

    // has_max_value_member
    //   trait: structural detector for a `.max_value` data member.
    template <typename, typename = void>
    struct has_max_value_member : std::false_type
    {};

    template <typename _Type>
    struct has_max_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().max_value)
    >> : std::true_type
    {};

}   // namespace detail


/*****************************************************************************/

// ===============================================================================
//  2  PUBLIC TRAITS
// ===============================================================================

// has_min_value_v
//   trait: true iff _Type exposes a `.min_value` data member.
template <typename _Type>
inline constexpr bool has_min_value_v =
    detail::has_min_value_member<_Type>::value;

// has_max_value_v
//   trait: true iff _Type exposes a `.max_value` data member.
template <typename _Type>
inline constexpr bool has_max_value_v =
    detail::has_max_value_member<_Type>::value;

// has_bounded_range_v
//   trait: true iff _Type has all three of .value, .min_value, .max_value.
// This is the gate for all generic bounded-range operations below.
template <typename _Type>
inline constexpr bool has_bounded_range_v =
    ( component_traits::has_value_v<_Type> &&
      has_min_value_v<_Type>               &&
      has_max_value_v<_Type> );


/*****************************************************************************/

// ===============================================================================
//  3  VALUE TYPE ALIAS
// ===============================================================================

// range_value_t
//   type: resolves to the decayed type of a range component's min_value
// member.  Used to type the min/max parameters of set_range without
// forcing callers to spell out the component's value_type.
template <typename _Type>
using range_value_t = std::decay_t<
    decltype(std::declval<_Type&>().min_value)
>;


/*****************************************************************************/

// ===============================================================================
//  4  QUERY OPERATIONS
// ===============================================================================

/*
span
  Returns `max_value - min_value`.  The result type is the range's value
type.  No check is performed for inverted ranges; callers who need to
guard against that should test via is_at_min / is_at_max first.

Parameter(s):
  _c: the range component to query.
Return:
  The span of the range as a value of range_value_t<_Type>.
*/
template <typename _Type,
          std::enable_if_t<
              has_bounded_range_v<_Type>,
              int> = 0>
[[nodiscard]] auto
span(
    const _Type& _c
) noexcept -> range_value_t<_Type>
{
    return (_c.max_value - _c.min_value);
}


/*
is_at_min
  Query whether the component's current value equals min_value.

Parameter(s):
  _c: the range component to query.
Return:
  true if value == min_value, false otherwise.
*/
template <typename _Type,
          std::enable_if_t<
              has_bounded_range_v<_Type>,
              int> = 0>
[[nodiscard]] bool
is_at_min(
    const _Type& _c
) noexcept
{
    return (_c.value == _c.min_value);
}


/*
is_at_max
  Query whether the component's current value equals max_value.

Parameter(s):
  _c: the range component to query.
Return:
  true if value == max_value, false otherwise.
*/
template <typename _Type,
          std::enable_if_t<
              has_bounded_range_v<_Type>,
              int> = 0>
[[nodiscard]] bool
is_at_max(
    const _Type& _c
) noexcept
{
    return (_c.value == _c.max_value);
}


/*
in_range
  Query whether a candidate value lies within the component's current
[min_value, max_value] range.  Inclusive on both ends.

Parameter(s):
  _c: the range component to query.
  _v: the candidate value.
Return:
  true iff min_value <= _v <= max_value.
*/
template <typename _Type,
          std::enable_if_t<
              has_bounded_range_v<_Type>,
              int> = 0>
[[nodiscard]] bool
in_range(
    const _Type&          _c,
    range_value_t<_Type>  _v
) noexcept
{
    return ( (_v >= _c.min_value) &&
             (_v <= _c.max_value) );
}


/*
normalized
  Returns the current value expressed as a position in [0.0, 1.0] along
the component's range.  Degenerate ranges (max_value == min_value)
collapse to 0.0 to avoid a divide-by-zero.  Out-of-range values are
clamped to [0, 1] in the return — this is defensive formatting, not a
mutation of the component's stored value.

Parameter(s):
  _c: the range component to query.
Return:
  A double in [0.0, 1.0] representing the current value's position.
*/
template <typename _Type,
          std::enable_if_t<
              has_bounded_range_v<_Type>,
              int> = 0>
[[nodiscard]] double
normalized(
    const _Type& _c
) noexcept
{
    double total;
    double offset;
    double result;

    if (_c.max_value == _c.min_value)
    {
        return 0.0;
    }

    total  = static_cast<double>(_c.max_value - _c.min_value);
    offset = static_cast<double>(_c.value     - _c.min_value);
    result = (offset / total);

    if (result < 0.0)
    {
        return 0.0;
    }

    if (result > 1.0)
    {
        return 1.0;
    }

    return result;
}


/*****************************************************************************/

// ===============================================================================
//  5  MUTATION OPERATIONS
// ===============================================================================

/*
set_range
  Replaces the component's [min, max] range.  Arguments are silently
swapped if _min > _max.  If the current value falls outside the new
range, it is clamped via set_value so that on_change and undo semantics
fire correctly.

Parameter(s):
  _c:   the range component to modify.
  _min: the new minimum value (inclusive).
  _max: the new maximum value (inclusive).
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_bounded_range_v<_Type>,
              int> = 0>
void
set_range(
    _Type&                _c,
    range_value_t<_Type>  _min,
    range_value_t<_Type>  _max
)
{
    range_value_t<_Type> lo;
    range_value_t<_Type> hi;

    // normalize ordering
    if (_min <= _max)
    {
        lo = _min;
        hi = _max;
    }
    else
    {
        lo = _max;
        hi = _min;
    }

    _c.min_value = lo;
    _c.max_value = hi;

    // clamp through the set_value channel so on_change / undo fire
    if (_c.value < lo)
    {
        set_value(_c, lo);
    }
    else if (_c.value > hi)
    {
        set_value(_c, hi);
    }

    return;
}


/*
clamp_to_range
  Clamps the current value to the [min_value, max_value] range.  No-op
if already in range.  Fires on_change / undo only if the value actually
changes.

Parameter(s):
  _c: the range component to clamp.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_bounded_range_v<_Type>,
              int> = 0>
void
clamp_to_range(
    _Type& _c
)
{
    if (_c.value < _c.min_value)
    {
        set_value(_c, _c.min_value);
    }
    else if (_c.value > _c.max_value)
    {
        set_value(_c, _c.max_value);
    }

    return;
}


/*
set_normalized
  Sets the component's value from a normalized [0.0, 1.0] position
along its range.  Out-of-range inputs are clamped to [0, 1] before
mapping.  Component-specific quantization (e.g. slider step snapping)
is NOT applied here — that is the caller's responsibility via the
component's own post-processing (sl_snap_to_step, or the component's
own set_normalized wrapper).

Parameter(s):
  _c: the range component to modify.
  _t: the normalized position.  Clamped to [0, 1] before mapping.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_bounded_range_v<_Type>,
              int> = 0>
void
set_normalized(
    _Type&  _c,
    double  _t
)
{
    double                total;
    double                raw;
    range_value_t<_Type>  next;

    // clamp the normalized input
    if (_t < 0.0)
    {
        _t = 0.0;
    }
    else if (_t > 1.0)
    {
        _t = 1.0;
    }

    total = static_cast<double>(_c.max_value - _c.min_value);
    raw   = static_cast<double>(_c.min_value) + (_t * total);
    next  = static_cast<range_value_t<_Type>>(raw);

    set_value(_c, next);

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_BOUNDED_RANGE_COMMON_
