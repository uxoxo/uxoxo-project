/*******************************************************************************
* uxoxo [component]                                             color_sample.hpp
*
* Color sampling meta-control:
*   An eyedropper that acts on other UI components.  Captures a color
* value from a designated target component and exposes it as its own
* `value` for downstream consumers (color pickers, palettes, pipette
* previews, contrast checkers).
*
*   Dual to color_fill: color_sample READS FROM a target component.
* The render layer is responsible for translating `target`, `sample_x`,
* `sample_y`, `mode`, and `region_radius` into an actual color and
* feeding that color back in via `cs_capture`.  The sampler itself
* holds no rendering logic — only state.
*
*   Standard component operations (enable, disable, show, hide,
* activate, deactivate, get_value, set_value, clear, commit) are
* inherited from component_common.hpp via structural detection.
* Only sampler-specific operations live here.
*
* Contents:
*   1  DSampleMode      — enum: single-point / averaged / area sampling
*   2  color_sample<>   — struct template: the sampling control
*   3  Operations
*       cs_begin         — arm the sampler
*       cs_end           — disarm the sampler
*       cs_capture       — record a freshly sampled color as value
*       cs_retarget      — point at a different UI element
*       cs_set_position  — set sample position within target
*       cs_set_mode      — switch sampling mode
*       cs_set_radius    — set area/average region radius
*
*
* path:      /inc/uxoxo/templates/component/color_sample.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COLOR_SAMPLE_
#define  UXOXO_COLOR_SAMPLE_ 1

// std
#include <cstdint>
#include <functional>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/util/color/color.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../component/component_mixin.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  SAMPLE MODE
// ===============================================================================

// DSampleMode
//   enum: sampling strategy.
//     point    — read a single pixel / element color at (sample_x,
//                sample_y).  region_radius is ignored.
//     average  — read all pixels within region_radius of the sample
//                point and return their arithmetic mean.
//     area     — renderer-defined aggregate over the region (e.g. mode
//                colour, dominant colour, median).  Intended for
//                palette-extraction workflows.
enum class DSampleMode : std::uint8_t
{
    point   = 0,
    average = 1,
    area    = 2
};


/*****************************************************************************/

// ===============================================================================
//  2  COLOR SAMPLE
// ===============================================================================

// color_sample
//   struct template: eyedropper-style meta control that captures a
// colour value from a target UI component.
//
//   Template parameters:
//     _Color      — color model type.  Must be a djinterp color model
//                   (satisfies djinterp::color::is_color_model_v).
//                   Defaults to rgb.
//     _TargetRef  — type used to reference the target UI component.
//                   Defaults to void* so the template is usable
//                   without a framework handle type, but a typed
//                   handle is preferred in practice.
//     _Labeled    — if true, mixes in a `label` string.
//     _Clearable  — if true, mixes in a `default_value` of type
//                   _Color which component_common::clear resets
//                   `value` to.
//
//   The sampler's inherited shared operations behave as follows:
//     enable / disable — toggle `enabled`
//     show / hide      — toggle `visible` (the reticle / cursor)
//     activate         — set `active` (equivalent to cs_begin)
//     deactivate       — clear `active` (equivalent to cs_end); also
//                        hides the reticle because has_visible_v is
//                        true
//     get_value        — returns the most recent captured colour
//     set_value        — forcibly set the captured colour (bypasses
//                        the normal capture pipeline; fires on_change)
//     clear            — reset to default_value (requires _Clearable)
//     commit           — invoke on_commit with current value
template <typename _Color     = djinterp::color::rgb,
          typename _TargetRef = void*,
          bool     _Labeled   = false,
          bool     _Clearable = false>
struct color_sample
    : component_mixin::label_data<_Labeled>
    , component_mixin::clearable_data<_Clearable, _Color>
{
    using color_type  = _Color;
    using target_type = _TargetRef;
    using value_type  = _Color;

    // value
    //   member: the most recently captured colour.  Read/written by
    // the shared get_value / set_value; written by cs_capture.
    _Color value {};

    // target
    //   member: reference to the UI component being sampled from.
    // Interpreted by the renderer.
    _TargetRef target {};

    // mode
    //   member: current sampling strategy.  See DSampleMode.
    DSampleMode mode { DSampleMode::point };

    // sample_x, sample_y
    //   members: sample position within the target.  Units are
    // renderer-defined (pixels for GUI, cells for TUI, etc.).
    int sample_x = 0;
    int sample_y = 0;

    // region_radius
    //   member: radius in target units used by `average` and `area`
    // modes.  Ignored in `point` mode.  Zero means "single pixel",
    // which is equivalent to `point` mode.
    int region_radius = 0;

    // enabled
    //   member: when false, cs_capture is a no-op.
    bool enabled = true;

    // visible
    //   member: whether the sampler's own UI representation (reticle,
    // crosshair, magnified preview) is shown.
    bool visible = true;

    // active
    //   member: whether sampling is currently armed.  A disarmed
    // sampler ignores cs_capture calls.
    bool active = false;

    // on_change
    //   callback: invoked whenever `value` changes (either through
    // cs_capture or the shared set_value).
    std::function<void(const _Color&)> on_change;

    // on_commit
    //   callback: invoked by the shared commit() when the user
    // finalizes a sample (click, press Enter, etc.).
    std::function<void(const _Color&)> on_commit;
};


/*****************************************************************************/

// ===============================================================================
//  3  OPERATIONS
// ===============================================================================
//   Sampler-specific operations.  Shared operations are inherited
// from component_common.hpp via structural detection.

// cs_begin
//   function: arms the sampler.  Semantically equivalent to the
// shared activate(), but named for the sampling mental model.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable>
void cs_begin(color_sample<_Color,
                           _TargetRef,
                           _Labeled,
                           _Clearable>& _s)
{
    _s.active = true;

    return;
}

// cs_end
//   function: disarms the sampler without changing `value`.
// Note: unlike the shared deactivate(), cs_end does NOT hide
// the reticle, because ending a sample is expected to leave
// the captured preview visible.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable>
void cs_end(color_sample<_Color,
                         _TargetRef,
                         _Labeled,
                         _Clearable>& _s)
{
    _s.active = false;

    return;
}

// cs_capture
//   function: records a freshly sampled colour as the sampler's
// value and fires on_change.  No-op if the sampler is disabled
// or not armed.  Does not fire on_commit — that is the
// finalization step, handled by the shared commit().
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable>
void cs_capture(color_sample<_Color,
                             _TargetRef,
                             _Labeled,
                             _Clearable>& _s,
                _Color                    _captured)
{
    if (!_s.enabled || !_s.active)
    {
        return;
    }

    _s.value = std::move(_captured);

    if (_s.on_change)
    {
        _s.on_change(_s.value);
    }

    return;
}

// cs_retarget
//   function: points the sampler at a different UI component.
// Leaves mode, position, and captured value unchanged.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable>
void cs_retarget(color_sample<_Color,
                              _TargetRef,
                              _Labeled,
                              _Clearable>& _s,
                 _TargetRef                _new_target)
{
    _s.target = std::move(_new_target);

    return;
}

// cs_set_position
//   function: moves the sample point within the target.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable>
void cs_set_position(color_sample<_Color,
                                  _TargetRef,
                                  _Labeled,
                                  _Clearable>& _s,
                     int                       _x,
                     int                       _y)
{
    _s.sample_x = _x;
    _s.sample_y = _y;

    return;
}

// cs_set_mode
//   function: switches sampling strategy.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable>
void cs_set_mode(color_sample<_Color,
                              _TargetRef,
                              _Labeled,
                              _Clearable>& _s,
                 DSampleMode               _mode)
{
    _s.mode = _mode;

    return;
}

// cs_set_radius
//   function: sets the region radius used by average and area
// modes.  Negative values are treated as zero.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable>
void cs_set_radius(color_sample<_Color,
                                _TargetRef,
                                _Labeled,
                                _Clearable>& _s,
                   int                       _radius)
{
    _s.region_radius = (_radius < 0) ? 0 : _radius;

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COLOR_SAMPLE_
