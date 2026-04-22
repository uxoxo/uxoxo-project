/*******************************************************************************
* uxoxo [component]                                    magnification_control.hpp
*
* Magnification meta-control:
*   A zoom / magnifier that acts on other UI components.  Holds a
* scalar zoom factor and a focus region; the renderer observes the
* control and scales its target accordingly.  Typical consumers are
* accessibility features (low-vision magnifier, cursor-follow lens)
* and design tools (rubber-band zoom, pixel-precision inspection).
*
*   Unlike color_sample and color_fill, the magnification control's
* `value` is not a colour - it is a zoom factor.  The underlying
* scalar type is a template parameter (defaulting to double) so that
* fixed-point or lower-precision floats can be used on embedded
* targets.
*
*   Standard component operations (enable, disable, show, hide,
* activate, deactivate, get_value, set_value, clear, commit) are
* inherited from component_common.hpp via structural detection.
* Only magnifier-specific operations live here.  The shared
* set_value will accept any zoom factor; mc_set_zoom is provided
* alongside it and additionally clamps into [min_zoom, max_zoom].
*
* Contents:
*   1  DMagnificationMode  - enum: fullscreen / region / follow / fixed
*   2  magnification_control<>  - struct template: the zoom control
*   3  Operations
*       mc_zoom_in        - step up by `step`, clamped
*       mc_zoom_out       - step down by `step`, clamped
*       mc_set_zoom       - set zoom directly, clamped
*       mc_reset_zoom     - reset to 1.0 (no magnification)
*       mc_retarget       - point at a different UI element
*       mc_set_focus      - set focus point within target
*       mc_set_region     - set focus region dimensions
*       mc_set_bounds     - adjust min_zoom / max_zoom
*       mc_set_mode       - switch magnification mode
*
*
* path:      /inc/uxoxo/templates/component/magnification_control.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_MAGNIFICATION_CONTROL_
#define  UXOXO_MAGNIFICATION_CONTROL_ 1

// std
#include <cstdint>
#include <functional>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../component/component_mixin.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  MAGNIFICATION MODE
// ===============================================================================

// DMagnificationMode
//   enum: how the magnifier applies its zoom factor.
//     fullscreen - scale the entire target uniformly from its centre.
//     region     - scale only the rectangle (focus_x, focus_y,
//                  region_width, region_height) within the target.
//                  Remainder of the target is rendered unscaled.
//     follow     - scale a region around (focus_x, focus_y) whose
//                  dimensions are region_width x region_height, and
//                  move that region with the caller-supplied focus
//                  point (e.g. cursor, active text field).
//     fixed      - scale a fixed rectangle on-screen (renderer-
//                  defined anchor).  Focus coordinates describe what
//                  within the target is shown in that rectangle.
enum class DMagnificationMode : std::uint8_t
{
    fullscreen = 0,
    region     = 1,
    follow     = 2,
    fixed      = 3
};


/*****************************************************************************/

// ===============================================================================
//  2  MAGNIFICATION CONTROL
// ===============================================================================

// magnification_control
//   struct template: meta control that drives the renderer's zoom
// of a target UI component.
//
//   Template parameters:
//     _TargetRef  - type used to reference the target UI component.
//     _Scalar     - numeric type for the zoom factor.  Defaults to
//                   double.  Must be an arithmetic type with
//                   well-defined comparison and arithmetic.
//     _Labeled    - if true, mixes in a `label` string.
//     _Clearable  - if true, mixes in a `default_value` of type
//                   _Scalar.  clear() resets the zoom to that
//                   value.  (Most consumers set default_value to
//                   1.0 so that clear() = "no magnification".)
//
//   Interaction with shared operations:
//     get_value    - returns the current zoom factor.
//     set_value    - sets the zoom directly, WITHOUT clamping.  Use
//                    mc_set_zoom if you want min/max enforcement.
//                    Fires on_change.
//     activate     - sets `active` (magnifier is currently applied).
//     deactivate   - clears `active` (no zoom applied); also hides
//                    the magnifier chrome because has_visible_v is
//                    true.
//     clear        - resets to default_value (requires _Clearable).
//     commit       - invokes on_commit, typically used to "lock in"
//                    a chosen zoom level.
template <typename _TargetRef = void*,
          typename _Scalar    = double,
          bool     _Labeled   = false,
          bool     _Clearable = false>
struct magnification_control
    : component_mixin::label_data<_Labeled>
    , component_mixin::clearable_data<_Clearable, _Scalar>
{
    using target_type = _TargetRef;
    using scalar_type = _Scalar;
    using value_type  = _Scalar;

    // value
    //   member: current zoom factor.  1.0 = no magnification.
    // Values > 1.0 zoom in; values < 1.0 zoom out.  The renderer
    // may additionally interpret very small values (e.g. < 0.01)
    // as degenerate and skip rendering.
    _Scalar value { _Scalar(1) };

    // min_zoom, max_zoom
    //   members: bounds enforced by mc_set_zoom, mc_zoom_in,
    // mc_zoom_out.  Not enforced by the shared set_value.
    _Scalar min_zoom { _Scalar(1) };
    _Scalar max_zoom { _Scalar(10) };

    // step
    //   member: additive increment used by mc_zoom_in / mc_zoom_out.
    _Scalar step { _Scalar(0.25) };

    // target
    //   member: reference to the UI component being magnified.
    _TargetRef target {};

    // mode
    //   member: how the zoom is applied.  See DMagnificationMode.
    DMagnificationMode mode { DMagnificationMode::fullscreen };

    // focus_x, focus_y
    //   members: focus point within the target, used by region,
    // follow, and fixed modes.  Ignored by fullscreen mode.
    int focus_x = 0;
    int focus_y = 0;

    // region_width, region_height
    //   members: dimensions of the magnified region, used by region
    // and follow modes.  Ignored by fullscreen and fixed modes.
    int region_width  = 0;
    int region_height = 0;

    // enabled
    //   member: when false, none of the mc_* zoom operations apply
    // and commit() is a no-op.
    bool enabled = true;

    // visible
    //   member: whether the magnifier's own UI chrome (lens frame,
    // zoom-level readout) is shown.
    bool visible = true;

    // active
    //   member: whether magnification is currently being applied to
    // the target.  `false` means the target renders at 1.0 even if
    // `value` is set to something else - essentially a bypass
    // switch.
    bool active = false;

    // on_change
    //   callback: invoked whenever `value` changes, by any path.
    std::function<void(const _Scalar&)> on_change;

    // on_commit
    //   callback: invoked when the caller finalizes a zoom level,
    // via the shared commit().  Usually used to persist the choice.
    std::function<void(const _Scalar&)> on_commit;
};


/*****************************************************************************/

// ===============================================================================
//  3  OPERATIONS
// ===============================================================================
//   Magnifier-specific operations.  Shared operations are inherited.

namespace detail {

    // clamp_zoom
    //   helper: clamp a candidate zoom value into
    // [min_zoom, max_zoom].  Local rather than pulled from
    // djinterp::color::clamp_channel so magnifier code stays
    // independent of the colour module.
    template <typename _Scalar>
    inline _Scalar
    clamp_zoom(_Scalar _v,
               _Scalar _lo,
               _Scalar _hi)
    {
        return (_v < _lo) ? _lo
             : (_v > _hi) ? _hi
             :              _v;
    }

}   // namespace detail


// mc_set_zoom
//   function: sets the zoom factor, clamped into [min_zoom,
// max_zoom], and fires on_change if the effective value changed.
// Prefer this to the shared set_value when you want the bounds
// enforced.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_set_zoom(magnification_control<_TargetRef,
                                       _Scalar,
                                       _Labeled,
                                       _Clearable>& _m,
                 _Scalar                            _z)
{
    if (!_m.enabled)
    {
        return;
    }

    const _Scalar clamped = detail::clamp_zoom(
        _z, _m.min_zoom, _m.max_zoom);

    if (clamped == _m.value)
    {
        return;
    }

    _m.value = clamped;

    if (_m.on_change)
    {
        _m.on_change(_m.value);
    }

    return;
}

// mc_zoom_in
//   function: increments `value` by `step`, clamped.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_zoom_in(magnification_control<_TargetRef,
                                      _Scalar,
                                      _Labeled,
                                      _Clearable>& _m)
{
    mc_set_zoom(_m, _m.value + _m.step);

    return;
}

// mc_zoom_out
//   function: decrements `value` by `step`, clamped.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_zoom_out(magnification_control<_TargetRef,
                                       _Scalar,
                                       _Labeled,
                                       _Clearable>& _m)
{
    mc_set_zoom(_m, _m.value - _m.step);

    return;
}

// mc_reset_zoom
//   function: sets the zoom factor to 1.0 (no magnification).
// Unlike the shared clear(), this does not require _Clearable -
// the identity zoom is well-defined for any magnifier.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_reset_zoom(magnification_control<_TargetRef,
                                         _Scalar,
                                         _Labeled,
                                         _Clearable>& _m)
{
    mc_set_zoom(_m, _Scalar(1));

    return;
}

// mc_retarget
//   function: points the magnifier at a different UI component.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_retarget(magnification_control<_TargetRef,
                                       _Scalar,
                                       _Labeled,
                                       _Clearable>& _m,
                 _TargetRef                         _new_target)
{
    _m.target = std::move(_new_target);

    return;
}

// mc_set_focus
//   function: sets the focus point within the target.  Relevant
// to region, follow, and fixed modes.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_set_focus(magnification_control<_TargetRef,
                                        _Scalar,
                                        _Labeled,
                                        _Clearable>& _m,
                  int                                _x,
                  int                                _y)
{
    _m.focus_x = _x;
    _m.focus_y = _y;

    return;
}

// mc_set_region
//   function: sets the dimensions of the magnified region.
// Negative values are treated as zero.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_set_region(magnification_control<_TargetRef,
                                         _Scalar,
                                         _Labeled,
                                         _Clearable>& _m,
                   int                                _w,
                   int                                _h)
{
    _m.region_width  = (_w < 0) ? 0 : _w;
    _m.region_height = (_h < 0) ? 0 : _h;

    return;
}

// mc_set_bounds
//   function: updates min_zoom and max_zoom.  If the current
// `value` falls outside the new bounds, it is clamped.  No-op
// if min > max.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_set_bounds(magnification_control<_TargetRef,
                                         _Scalar,
                                         _Labeled,
                                         _Clearable>& _m,
                   _Scalar                            _min,
                   _Scalar                            _max)
{
    if (_min > _max)
    {
        return;
    }

    _m.min_zoom = _min;
    _m.max_zoom = _max;

    const _Scalar clamped = detail::clamp_zoom(
        _m.value, _m.min_zoom, _m.max_zoom);

    if (clamped != _m.value)
    {
        _m.value = clamped;

        if (_m.on_change)
        {
            _m.on_change(_m.value);
        }
    }

    return;
}

// mc_set_mode
//   function: switches magnification strategy.
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
void mc_set_mode(magnification_control<_TargetRef,
                                       _Scalar,
                                       _Labeled,
                                       _Clearable>& _m,
                 DMagnificationMode                 _mode)
{
    _m.mode = _mode;

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_MAGNIFICATION_CONTROL_
