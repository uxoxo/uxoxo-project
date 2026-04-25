/*******************************************************************************
* uxoxo [component]                                             color_slider.hpp
*
* 1D color channel slider:
*   A bounded-range scalar control, decorated with gradient metadata
* so the renderer can draw a colour track behind the handle.  Used to
* edit a single channel of any colour value — RED of an rgb, hue of an
* hsl, lightness of a cie_lab, or a channel of a colour space that
* does not yet exist in this codebase.
*
*   The slider is strictly colour-space agnostic.  It does not access
* any channel members of _Color and performs no colour arithmetic; it
* only stores two endpoint colours (and optionally a sampler function)
* for the renderer to draw the track.  Wiring a slider to a particular
* channel of a host colour value is the consumer's responsibility, via
* the on_change callback.
*
*   Structural participation:
*     - has `value`, `min_value`, `max_value` so all of
*       bounded_range_common.hpp applies: set_range, normalized,
*       set_normalized, is_at_min, is_at_max, in_range, span,
*       clamp_to_range.
*     - has `enabled`, `visible`, `read_only`, `on_change`, `on_commit`,
*       and `value` so all of component_common.hpp applies:
*       enable / disable / show / hide / get_value / set_value / clear
*       (when _Clearable) / undo (when _Undoable) / request_copy (when
*       _Copyable) / commit / set_read_only / is_enabled / is_visible.
*
*   Slider-specific operations live here with the csl_ prefix.
*
* Contents:
*   1  DChannelFormat       — enum: hint for numeric readout formatting
*   2  color_slider<>       — struct template: the slider control
*   3  Operations
*       csl_set_step          — set step quantization
*       csl_snap_to_step      — snap value to nearest step multiple
*       csl_increment         — step value up by `step`, clamped
*       csl_decrement         — step value down by `step`, clamped
*       csl_set_gradient      — set both gradient endpoints at once
*       csl_set_sampler       — install a non-linear gradient sampler
*       csl_clear_sampler     — drop the sampler (revert to linear)
*       csl_has_sampler       — query whether a sampler is installed
*       csl_set_format        — set the numeric-readout format hint
*       csl_set_orientation   — set horizontal / vertical layout
*
*
* path:      /inc/uxoxo/templates/component/color_slider.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COLOR_SLIDER_
#define  UXOXO_COLOR_SLIDER_ 1

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
#include "./component_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  CHANNEL FORMAT
// ===============================================================================

// DChannelFormat
//   enum: a hint to the renderer about how to format the numeric
// readout shown next to the slider track.  The slider itself stores
// the channel value as a raw _Scalar and does no formatting; this
// enum exists so consumers can express channel conventions (8-bit
// integer, percentage, degrees, etc.) without subclassing.
//
//     raw          — display value verbatim.  Renderer's choice of
//                    precision / locale.
//     integer      — round to the nearest integer and display.  No
//                    rescaling.  Useful when min_value / max_value
//                    are already integers (e.g. 0..255).
//     integer_8bit — interpret value as a [0, 1] normalized channel
//                    and display as round(value * 255), in [0, 255].
//                    Matches the convention of the iOS / macOS
//                    colour picker RGB readouts.
//     percentage   — interpret value as a [0, 1] normalized channel
//                    and display as round(value * 100) with a "%"
//                    suffix.  Used for opacity, saturation,
//                    lightness, etc.
//     degrees      — display value with a "°" suffix.  Used for hue
//                    channels in HSL / HSV.
enum class DChannelFormat : std::uint8_t
{
    raw          = 0,
    integer      = 1,
    integer_8bit = 2,
    percentage   = 3,
    degrees      = 4
};


/*****************************************************************************/

// ===============================================================================
//  2  COLOR SLIDER
// ===============================================================================

// color_slider
//   struct template: a 1D bounded-range control for editing a single
// channel of a colour value.  The slider stores the channel scalar
// directly; the colour space and the channel-of-interest are
// inferred by the consumer from how the on_change callback is
// wired.
//
//   Template parameters:
//     _Color      — type used for the gradient endpoint storage.
//                   Defaults to djinterp::color::rgb.  The slider
//                   never reads any members of _Color; any type
//                   default-constructible and copyable works,
//                   including custom or future colour types.
//     _Scalar     — numeric type for the channel value.  Defaults
//                   to djinterp::color::channel_t (typically
//                   double) so a slider's range matches the
//                   precision of djinterp colour channels.  Pass
//                   `int` for slider-of-8-bit-channels workflows.
//     _Labeled    — if true, mixes in a `label` string ("RED",
//                   "Hue", "Cyan", or whatever the consumer
//                   wants).  Defaults to true since labelled
//                   sliders are by far the more common case.
//     _Clearable  — if true, mixes in a `default_value` of type
//                   _Scalar; clear() resets `value` to it.
//     _Undoable   — if true, mixes in `previous_value` +
//                   `has_previous` so undo() can revert the last
//                   set_value.
//     _Copyable   — if true, mixes in `copy_requested` so
//                   request_copy() can flag the channel value
//                   for clipboard export.
//
//   Default-constructed instance has range [0, 1], value 0, no
// gradient (both endpoints default-constructed _Color), step 0
// (continuous), horizontal orientation, and `raw` format.  The
// consumer customizes by direct member assignment for static
// state and by csl_* / shared free functions for runtime state.
template <typename _Color     = djinterp::color::rgb,
          typename _Scalar    = djinterp::color::channel_t,
          bool     _Labeled   = true,
          bool     _Clearable = false,
          bool     _Undoable  = false,
          bool     _Copyable  = false>
struct color_slider
    : component_mixin::label_data<_Labeled>
    , component_mixin::clearable_data<_Clearable, _Scalar>
    , component_mixin::undo_data<_Undoable, _Scalar>
    , component_mixin::copyable_data<_Copyable>
{
    using color_type  = _Color;
    using scalar_type = _Scalar;
    using value_type  = _Scalar;

    // value
    //   member: the channel's current scalar value.  Read by the
    // shared get_value, written by the shared set_value (which
    // additionally fires on_change and stores undo state if
    // _Undoable).
    _Scalar value { _Scalar(0) };

    // min_value, max_value
    //   members: closed bounds of the channel range.  Together with
    // `value`, these satisfy the bounded-range protocol and so
    // bounded_range_common.hpp's set_range / normalized /
    // set_normalized / clamp_to_range / is_at_min / is_at_max /
    // in_range / span all work on this slider.  Defaults are
    // [0, 1] to match djinterp's normalized channel convention.
    _Scalar min_value { _Scalar(0) };
    _Scalar max_value { _Scalar(1) };

    // step
    //   member: quantization step for keyboard / arrow-key
    // increments.  A step of zero means "continuous" — no
    // snapping is performed.  The shared set_value does not
    // snap; csl_snap_to_step / csl_increment / csl_decrement
    // do.
    _Scalar step { _Scalar(0) };

    // gradient_start, gradient_end
    //   members: colours at the min_value and max_value ends of
    // the track, respectively.  The renderer draws a gradient
    // between them (linearly, unless gradient_sampler is set).
    // For a red channel of an rgb colour, typical values are
    // black at min_value and pure red at max_value.  These
    // are routinely updated as the host colour changes (e.g.
    // raising the red channel shifts the green slider's
    // endpoints away from black-to-green toward red-to-yellow).
    _Color gradient_start {};
    _Color gradient_end   {};

    // gradient_sampler
    //   callback: optional non-linear gradient evaluator.  When
    // set, the renderer should call gradient_sampler(t) for t
    // in [0, 1] to obtain the track colour at that normalized
    // position, instead of linearly interpolating between
    // gradient_start and gradient_end.  Required for channels
    // whose linear endpoint interpolation would mis-represent
    // the colour path — most notably hue, where gradient_start
    // and gradient_end may be the same colour but the gradient
    // should walk the entire hue wheel.
    std::function<_Color(double)> gradient_sampler;

    // format
    //   member: hint for how the renderer should format the
    // numeric readout.  See DChannelFormat.  The slider does
    // not consume this itself.
    DChannelFormat format { DChannelFormat::raw };

    // orientation
    //   member: layout axis.  Horizontal sliders grow value
    // left-to-right; vertical sliders grow bottom-to-top by
    // convention (the renderer may invert).
    DOrientation orientation { DOrientation::horizontal };

    // enabled
    //   member: when false, set_value still mutates the value
    // but the renderer should ignore user input and the
    // shared commit() will be a no-op.
    bool enabled = true;

    // visible
    //   member: whether the slider's UI is shown at all.
    bool visible = true;

    // read_only
    //   member: when true, the renderer should still draw the
    // slider but block user input; the shared commit() will be
    // a no-op.  Distinct from `enabled`: a read-only slider
    // typically renders normally, an `!enabled` slider renders
    // dimmed.
    bool read_only = false;

    // on_change
    //   callback: invoked by the shared set_value (and therefore
    // by bounded_range_common.hpp's set_normalized,
    // set_range-clamp, clamp_to_range) whenever `value`
    // changes.  This is where the consumer pushes the new
    // channel value back into the host colour.
    std::function<void(const _Scalar&)> on_change;

    // on_commit
    //   callback: invoked by the shared commit() when the user
    // finalizes a slider value (mouse up, keyboard Enter).
    // Most consumers route on_change to the live colour and
    // on_commit to higher-level "value chosen" logic
    // (history, autosave, swatch capture).
    std::function<void(const _Scalar&)> on_commit;
};


/*****************************************************************************/

// ===============================================================================
//  3  OPERATIONS
// ===============================================================================
//   Slider-specific operations.  Shared bounded-range and component
// operations come from bounded_range_common.hpp and component_common.hpp
// respectively, via structural conformance.

namespace detail {

    // clamp_scalar
    //   helper: clamp `_v` into [_lo, _hi].  Local rather than
    // pulled from djinterp::color::clamp_channel so slider code
    // stays independent of the colour module's clamp utility.
    template <typename _Scalar>
    inline _Scalar
    clamp_scalar(_Scalar _v,
                 _Scalar _lo,
                 _Scalar _hi)
    {
        return (_v < _lo) ? _lo
             : (_v > _hi) ? _hi
             :              _v;
    }

}   // namespace detail


// csl_set_step
//   function: sets the quantization step.  Negative steps are
// treated as zero (continuous).  Does not re-snap the current
// `value`; call csl_snap_to_step explicitly if that's wanted.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_set_step(color_slider<_Color,
                               _Scalar,
                               _Labeled,
                               _Clearable,
                               _Undoable,
                               _Copyable>& _s,
                  _Scalar                  _step)
{
    _s.step = (_step < _Scalar(0)) ? _Scalar(0) : _step;

    return;
}

// csl_snap_to_step
//   function: snaps `value` to the nearest multiple of `step`
// measured from `min_value`, clamped to [min_value, max_value].
// No-op if step <= 0.  Routes through the shared set_value so
// on_change and undo semantics fire if the snap moves the value.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_snap_to_step(color_slider<_Color,
                                   _Scalar,
                                   _Labeled,
                                   _Clearable,
                                   _Undoable,
                                   _Copyable>& _s)
{
    if (_s.step <= _Scalar(0))
    {
        return;
    }

    const double offset =
        static_cast<double>(_s.value - _s.min_value);
    const double quantum =
        static_cast<double>(_s.step);

    // round to nearest step
    const double n_steps =
        (offset / quantum) + 0.5;

    const _Scalar snapped =
        _s.min_value + static_cast<_Scalar>(
            static_cast<long long>(n_steps)) * _s.step;

    const _Scalar clamped = detail::clamp_scalar(
        snapped, _s.min_value, _s.max_value);

    if (clamped != _s.value)
    {
        set_value(_s, clamped);
    }

    return;
}

// csl_increment
//   function: bumps `value` up by `step`, clamped to
// [min_value, max_value].  No-op if disabled, read-only, or
// step <= 0.  Routes through set_value, so on_change fires.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_increment(color_slider<_Color,
                                _Scalar,
                                _Labeled,
                                _Clearable,
                                _Undoable,
                                _Copyable>& _s)
{
    if (!_s.enabled || _s.read_only || _s.step <= _Scalar(0))
    {
        return;
    }

    const _Scalar next = detail::clamp_scalar(
        static_cast<_Scalar>(_s.value + _s.step),
        _s.min_value,
        _s.max_value);

    if (next != _s.value)
    {
        set_value(_s, next);
    }

    return;
}

// csl_decrement
//   function: bumps `value` down by `step`, clamped.  See
// csl_increment for semantics.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_decrement(color_slider<_Color,
                                _Scalar,
                                _Labeled,
                                _Clearable,
                                _Undoable,
                                _Copyable>& _s)
{
    if (!_s.enabled || _s.read_only || _s.step <= _Scalar(0))
    {
        return;
    }

    const _Scalar next = detail::clamp_scalar(
        static_cast<_Scalar>(_s.value - _s.step),
        _s.min_value,
        _s.max_value);

    if (next != _s.value)
    {
        set_value(_s, next);
    }

    return;
}

// csl_set_gradient
//   function: sets both gradient endpoints in one call.  Does not
// install or clear gradient_sampler — call csl_set_sampler /
// csl_clear_sampler for that.  Endpoint colours are stored
// verbatim; no validation is performed.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_set_gradient(color_slider<_Color,
                                   _Scalar,
                                   _Labeled,
                                   _Clearable,
                                   _Undoable,
                                   _Copyable>& _s,
                      _Color                   _start,
                      _Color                   _end)
{
    _s.gradient_start = std::move(_start);
    _s.gradient_end   = std::move(_end);

    return;
}

// csl_set_sampler
//   function: installs a non-linear gradient sampler.  When set,
// the renderer should prefer the sampler over linear
// interpolation between gradient_start / gradient_end.  Required
// for channels whose linear endpoint interpolation would not
// follow the perceptual or definitional path of the colour
// space (hue is the canonical example).
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_set_sampler(color_slider<_Color,
                                  _Scalar,
                                  _Labeled,
                                  _Clearable,
                                  _Undoable,
                                  _Copyable>&         _s,
                     std::function<_Color(double)>    _sampler)
{
    _s.gradient_sampler = std::move(_sampler);

    return;
}

// csl_clear_sampler
//   function: drops any installed sampler.  After this call, the
// renderer should revert to linear interpolation between
// gradient_start and gradient_end.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_clear_sampler(color_slider<_Color,
                                    _Scalar,
                                    _Labeled,
                                    _Clearable,
                                    _Undoable,
                                    _Copyable>& _s)
{
    _s.gradient_sampler = nullptr;

    return;
}

// csl_has_sampler
//   function: query whether a non-linear gradient sampler is
// currently installed.  Renderers can branch on this to choose
// between linear interpolation and sampled evaluation.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
[[nodiscard]] bool
csl_has_sampler(const color_slider<_Color,
                                   _Scalar,
                                   _Labeled,
                                   _Clearable,
                                   _Undoable,
                                   _Copyable>& _s) noexcept
{
    return static_cast<bool>(_s.gradient_sampler);
}

// csl_set_format
//   function: sets the renderer's numeric-readout format hint.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_set_format(color_slider<_Color,
                                 _Scalar,
                                 _Labeled,
                                 _Clearable,
                                 _Undoable,
                                 _Copyable>& _s,
                    DChannelFormat           _fmt)
{
    _s.format = _fmt;

    return;
}

// csl_set_orientation
//   function: switches between horizontal and vertical layout.
template <typename _Color,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void csl_set_orientation(color_slider<_Color,
                                      _Scalar,
                                      _Labeled,
                                      _Clearable,
                                      _Undoable,
                                      _Copyable>& _s,
                         DOrientation             _o)
{
    _s.orientation = _o;

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COLOR_SLIDER_
