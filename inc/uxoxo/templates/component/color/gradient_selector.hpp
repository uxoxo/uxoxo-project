/*******************************************************************************
* uxoxo [component]                                       gradient_selector.hpp
*
* 2D continuous gradient selector:
*   A picker that lets the user choose any point within a smooth 2D
* colour gradient — the typical "Spectrum" or "saturation × value"
* surface of a colour-picker dialog.  Conceptually this is the 2D
* continuous counterpart to color_slider (1D continuous) and the
* continuous counterpart to color_palette_selector (2D discrete).
*
*   Like the slider and palette, the gradient selector is colour-space
* agnostic.  It stores four corner colours (which the renderer may
* bilinearly interpolate to draw the gradient surface) plus an
* optional sampler callable
*       _Color(double tx, double ty)
* which, when present, fully describes the gradient — overriding
* corner-based interpolation for surfaces whose colour path is not
* bilinear (HSV cone slices, perceptual gradients, lookup-driven
* surfaces, anything).  The struct itself never reads a channel of
* _Color.
*
*   The selector's `value` is the user's pick position, stored as a
* small DGradientPosition { double tx; double ty; } in [0, 1]².  This
* makes the selector structurally conform to component_common.hpp:
* set_value sets a position, get_value returns it, clear() resets to
* default_value (when _Clearable), undo() reverts the last move
* (when _Undoable), commit() invokes on_commit with the position.
* request_copy (when _Copyable) flags the position for clipboard
* export — the renderer is responsible for resolving position to
* colour at copy time.
*
*   The position is the primary state.  The current colour is
* derivable from it via the sampler (when one is installed) and is
* exposed via gs_resolve_color; consumers who care about the colour
* per se typically wire on_change to call gs_resolve_color and
* forward the result.
*
* Contents:
*   1.  DGradientPosition          — struct: (tx, ty) in [0, 1]^2
*   2.  gradient_selector<>        — struct template: the selector
*   3.  Operations
*         Position
*           gs_set_position             — set both axes, clamped
*           gs_set_position_x           — set x axis, clamped
*           gs_set_position_y           — set y axis, clamped
*           gs_position_x               — query x axis
*           gs_position_y               — query y axis
*           gs_center                   — set position to (0.5, 0.5)
*           gs_set_step                 — set keyboard nudge step
*           gs_step_left                — nudge -x by step, clamped
*           gs_step_right               — nudge +x by step, clamped
*           gs_step_up                  — nudge -y by step, clamped
*           gs_step_down                — nudge +y by step, clamped
*         Colour
*           gs_resolve_color            — sample colour at current position
*           gs_resolve_color_at         — sample colour at arbitrary position
*         Gradient definition
*           gs_set_corners              — assign all four corner colours
*           gs_set_sampler              — install a non-bilinear sampler
*           gs_clear_sampler            — drop the sampler
*           gs_has_sampler              — query whether a sampler is set
*         Layout
*           gs_set_size                 — set width / height in renderer units
*         
*
* path:      /inc/uxoxo/templates/component/color/gradient_selector.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.25
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_GRADIENT_SELECTOR_
#define  UXOXO_COMPONENT_GRADIENT_SELECTOR_ 1

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
//  1.  GRADIENT POSITION
// ===============================================================================

// DGradientPosition
//   struct: a 2D position within a unit-square gradient surface.
// Both axes are normalized to [0, 1] inclusive.  By convention,
// (tx=0, ty=0) is the top-left of the surface and (tx=1, ty=1) is
// the bottom-right, matching screen-coordinate Y-down orientation
// and the "corner_top_left / corner_top_right / corner_bottom_left
// / corner_bottom_right" naming used elsewhere in this header.
//
//   The struct is deliberately public-data and constexpr-friendly so
// it can be copied freely, used as a value type by the framework's
// shared set_value / get_value / undo machinery, and inserted into
// containers without concern.
struct DGradientPosition
{
    double tx = 0.0;
    double ty = 0.0;

    // operator==
    //   function: exact equality.  Floating-point comparison is
    // exact, mirroring the convention in djinterp::color types.
    constexpr bool
    operator==(const DGradientPosition& _other) const noexcept
    {
        return ((tx == _other.tx) && (ty == _other.ty));
    }

    // operator!=
    constexpr bool
    operator!=(const DGradientPosition& _other) const noexcept
    {
        return !(*this == _other);
    }
};

// ===============================================================================
//  2.  GRADIENT SELECTOR
// ===============================================================================

// gradient_selector
//   struct template: a 2D continuous gradient picker.  The user's
// pick position is the primary state; the corresponding colour is
// derived on demand via the optional gradient_sampler (or by the
// renderer's bilinear interpolation of the four corner colours,
// when no sampler is installed).
//
//   Template parameters:
//     _Color      — type used for the four corner colours and the
//                   sampler's return type.  Defaults to
//                   djinterp::color::rgb.  Any default-
//                   constructible, copyable type works; the
//                   selector never reads any channel members of
//                   _Color.
//     _Labeled    — if true, mixes in a `label` string ("Spectrum",
//                   "HSV", whatever the consumer wants).  Defaults
//                   to true.
//     _Clearable  — if true, mixes in a `default_value` of type
//                   DGradientPosition; clear() resets `value`
//                   to it.  Common pattern: set default_value
//                   to {0.5, 0.5} so clear() means "back to
//                   centre".
//     _Undoable   — if true, mixes in `previous_value` +
//                   `has_previous` so undo() reverts the last
//                   set_value (i.e. the last move).
//     _Copyable   — if true, mixes in `copy_requested` so
//                   request_copy() flags the position for
//                   clipboard export.  The renderer is
//                   responsible for resolving the position to
//                   a colour at copy time.
//
//   Default-constructed instance has position (0, 0), all four
// corners default-constructed _Color (typically opaque black for
// djinterp::color types), no sampler, and step = 0.05.  The
// consumer customizes the gradient definition via gs_set_corners
// and / or gs_set_sampler before the surface is meaningful.
template <typename _Color     = djinterp::color::rgb,
          bool     _Labeled   = true,
          bool     _Clearable = false,
          bool     _Undoable  = false,
          bool     _Copyable  = false>
struct gradient_selector
    : component_mixin::label_data<_Labeled>,
      component_mixin::clearable_data<_Clearable, DGradientPosition>,
      component_mixin::undo_data<_Undoable, DGradientPosition>,
      component_mixin::copyable_data<_Copyable>
{
    using color_type = _Color;
    using value_type = DGradientPosition;

    // value
    //   member: the user's pick position within the gradient
    // surface.  Both axes are in [0, 1].  Read by the shared
    // get_value, written by the shared set_value (which also
    // fires on_change and stores undo state if _Undoable).
    DGradientPosition value {};

    // corner_top_left, corner_top_right,
    // corner_bottom_left, corner_bottom_right
    //   members: the four corner colours of the gradient surface,
    // at (tx=0, ty=0), (tx=1, ty=0), (tx=0, ty=1), (tx=1, ty=1)
    // respectively.  When no sampler is installed, the renderer
    // is expected to bilinearly interpolate between these to
    // draw the surface (and to resolve a colour at a given
    // position, if it is doing colour resolution itself).
    _Color corner_top_left     {};
    _Color corner_top_right    {};
    _Color corner_bottom_left  {};
    _Color corner_bottom_right {};

    // gradient_sampler
    //   callback: optional gradient evaluator.  When set, the
    // renderer should call gradient_sampler(tx, ty) for any
    // (tx, ty) in [0, 1]² to obtain the surface colour at that
    // position, instead of bilinearly interpolating the corners.
    // gs_resolve_color likewise prefers the sampler when present.
    // Required for surfaces whose colour path between corners is
    // not bilinear (HSV / HSL slices, CIE-space gradients,
    // table-driven surfaces, anything non-linear).
    std::function<_Color(double, double)> gradient_sampler;

    // width, height
    //   members: dimensions of the gradient surface in renderer
    // units.  Zero on either axis means "renderer chooses",
    // typically based on container size.
    int width  = 0;
    int height = 0;

    // step
    //   member: nudge amount used by gs_step_left / right / up /
    // down for keyboard-style navigation.  Applied uniformly on
    // both axes.  Defaults to 0.05 (5% per nudge) — fine for
    // keyboard arrow input, coarser for big-step nav.
    double step = 0.05;

    // enabled
    //   member: when false, the renderer should ignore user input
    // and the shared commit() is a no-op.
    bool enabled = true;

    // visible
    //   member: whether the surface UI is shown.
    bool visible = true;

    // read_only
    //   member: when true, the surface still renders but is not
    // interactable; commit() is a no-op.  Useful for "showing the
    // current colour position on a gradient without letting the
    // user pick" workflows.
    bool read_only = false;

    // on_change
    //   callback: invoked by the shared set_value (and by every
    // gs_* op that mutates `value`, since they all route through
    // set_value) whenever the position changes.  Receives the
    // new position; consumers wanting the colour typically
    // call gs_resolve_color from inside.
    std::function<void(const DGradientPosition&)> on_change;

    // on_commit
    //   callback: invoked by the shared commit() when the user
    // finalizes a position (mouse up, keyboard Enter).
    std::function<void(const DGradientPosition&)> on_commit;
};

// ===============================================================================
//  3.  OPERATIONS
// ===============================================================================
//   Selector-specific operations.  Shared component operations from
// component_common.hpp (enable / disable / show / hide / set_value /
// get_value / clear / undo / request_copy / commit / set_read_only)
// work on the selector via structural conformance.

NS_INTERNAL
    // clamp_unit
    //   helper: clamp `_v` into [0, 1].
    D_INLINE double
    clamp_unit(double _v) noexcept
    {
        return (_v < 0.0) ? 0.0
             : (_v > 1.0) ? 1.0
             :              _v;
    }

NS_END   // internal


// ----- POSITION -----------------------------------------------------------

// gs_set_position
//   function: sets both axes of the pick position, each clamped to
// [0, 1].  Routes through the shared set_value, so on_change fires
// and undo is recorded (when _Undoable).  Early-exits without
// firing on_change if the clamped position equals the current one.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_set_position(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g,
    double                        _tx,
    double                        _ty
)
{
    const DGradientPosition next 
    {
        internal::clamp_unit(_tx),
        internal::clamp_unit(_ty)
    };

    if (next == _g.value)
    {
        return;
    }

    set_value(_g, next);

    return;
}

// gs_set_position_x
//   function: sets the x axis only, clamped.  ty is preserved.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_set_position_x(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g,
    double                        _tx
)
{
    const DGradientPosition next 
    {
        internal::clamp_unit(_tx),
        _g.value.ty
    };

    if (next == _g.value)
    {
        return;
    }

    set_value(_g, next);

    return;
}

// gs_set_position_y
//   function: sets the y axis only, clamped.  tx is preserved.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_set_position_y(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g,
    double                        _ty
)
{
    const DGradientPosition next {
        _g.value.tx,
        internal::clamp_unit(_ty)
    };

    if (next == _g.value)
    {
        return;
    }

    set_value(_g, next);

    return;
}

// gs_position_x
//   function: returns the current x axis position.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD double
gs_position_x(
    const gradient_selector<_Color,
                            _Labeled,
                            _Clearable,
                            _Undoable,
                            _Copyable>& _g
) noexcept
{
    return _g.value.tx;
}

// gs_position_y
//   function: returns the current y axis position.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD double
gs_position_y(
    const gradient_selector<_Color,
                            _Labeled,
                            _Clearable,
                            _Undoable,
                            _Copyable>& _g
) noexcept
{
    return _g.value.ty;
}

// gs_center
//   function: convenience for setting the position to (0.5, 0.5).
// Routes through gs_set_position so all the usual side effects
// fire.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_center(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g
)
{
    gs_set_position(_g, 0.5, 0.5);

    return;
}

// gs_set_step
//   function: sets the per-nudge step amount.  Clamped to [0, 1];
// negative inputs become zero (effectively disabling nudge ops).
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_set_step(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g,
    double                        _step
)
{
    _g.step = (_step < 0.0) ? 0.0
            : (_step > 1.0) ? 1.0
            :                 _step;

    return;
}

// gs_step_left
//   function: nudges position -x by `step`, clamped.  No-op if
// disabled, read-only, step <= 0, or already at tx == 0.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_step_left(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g
)
{
    if ( (!_g.enabled)  || 
         (_g.read_only) || 
         (_g.step <= 0.0) )
    {
        return;
    }

    gs_set_position_x(_g, _g.value.tx - _g.step);

    return;
}

// gs_step_right
//   function: nudges position +x by `step`, clamped.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_step_right(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g
)
{
    if ( (!_g.enabled)  || 
         (_g.read_only) || 
         (_g.step <= 0.0) )
    {
        return;
    }

    gs_set_position_x(_g, _g.value.tx + _g.step);

    return;
}

// gs_step_up
//   function: nudges position -y by `step`, clamped.  Note that
// "up" means decreasing ty under the screen-Y-down convention
// (ty=0 is the top edge).
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_step_up(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g
)
{
    if ( (!_g.enabled)  || 
         (_g.read_only) || 
         (_g.step <= 0.0) )
    {
        return;
    }

    gs_set_position_y(_g, _g.value.ty - _g.step);

    return;
}

// gs_step_down
//   function: nudges position +y by `step`, clamped.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_step_down(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g
)
{
    if ( (!_g.enabled)  || 
         (_g.read_only) || 
         (_g.step <= 0.0) )
    {
        return;
    }

    gs_set_position_y(_g, _g.value.ty + _g.step);

    return;
}


// ----- COLOUR -------------------------------------------------------------

// gs_resolve_color
//   function: returns the colour at the current pick position by
// invoking the gradient_sampler.  Returns a default-constructed
// _Color if no sampler is installed — callers who care about the
// distinction should test gs_has_sampler first, or arrange for a
// sampler to always be present (e.g. installing a wrapper that
// bilinearly interpolates the corner members).
//
//   Note: the gradient struct intentionally does not perform
// corner-based bilinear interpolation itself; that would require
// colour arithmetic and break colour-space agnosticism.  The
// renderer, which knows the concrete _Color type, is the right
// place for corner interpolation when no sampler is installed.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD _Color
gs_resolve_color(
    const gradient_selector<_Color,
                            _Labeled,
                            _Clearable,
                            _Undoable,
                            _Copyable>& _g
)
{
    if (_g.gradient_sampler)
    {
        return _g.gradient_sampler(_g.value.tx, _g.value.ty);
    }

    return _Color {};
}

// gs_resolve_color_at
//   function: returns the colour at an arbitrary (tx, ty), each
// clamped to [0, 1].  Same fallback as gs_resolve_color.  Useful
// for renderers that need to query the gradient at handle-adjacent
// points (e.g. to compute a contrast colour for the cursor mark).
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD _Color
gs_resolve_color_at(
    const gradient_selector<_Color,
                            _Labeled,
                            _Clearable,
                            _Undoable,
                            _Copyable>& _g,
    double                              _tx,
    double                              _ty)
{
    if (_g.gradient_sampler)
    {
        return _g.gradient_sampler(internal::clamp_unit(_tx),
                                   internal::clamp_unit(_ty));
    }

    return _Color {};
}


// ----- GRADIENT DEFINITION ------------------------------------------------

// gs_set_corners
//   function: assigns all four corner colours in one call.  Order
// is top-left, top-right, bottom-left, bottom-right — matching
// (tx, ty) of (0,0), (1,0), (0,1), (1,1) respectively.  Does NOT
// install or clear gradient_sampler.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_set_corners(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g,
    _Color                         _top_left,
    _Color                         _top_right,
    _Color                         _bottom_left,
    _Color                         _bottom_right)
{
    _g.corner_top_left     = std::move(_top_left);
    _g.corner_top_right    = std::move(_top_right);
    _g.corner_bottom_left  = std::move(_bottom_left);
    _g.corner_bottom_right = std::move(_bottom_right);

    return;
}

// gs_set_sampler
//   function: installs a non-bilinear gradient sampler.  When set,
// the renderer should prefer the sampler over corner
// interpolation; gs_resolve_color likewise.  Required for
// gradients whose colour path is not bilinear in _Color space —
// HSV / HSL slices, CIE-space gradients, table-driven surfaces,
// anything non-linear.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
gs_set_sampler(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>&         _g,
    std::function<_Color(double, double)> _sampler
)
{
    _g.gradient_sampler = std::move(_sampler);

    return;
}

// gs_clear_sampler
//   function: drops any installed sampler.  After this call, the
// renderer should fall back to bilinear interpolation of the
// corner colours, and gs_resolve_color returns a default _Color.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
gs_clear_sampler(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g
)
{
    _g.gradient_sampler = nullptr;

    return;
}

// gs_has_sampler
//   function: query whether a sampler is currently installed.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD bool
gs_has_sampler(
    const gradient_selector<_Color,
                            _Labeled,
                            _Clearable,
                            _Undoable,
                            _Copyable>& _g
) noexcept
{
    return static_cast<bool>(_g.gradient_sampler);
}


// ----- LAYOUT -------------------------------------------------------------

// gs_set_size
//   function: sets the gradient surface dimensions in renderer
// units.  Negative inputs become zero (the "renderer chooses"
// sentinel).
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
gs_set_size(
    gradient_selector<_Color,
                      _Labeled,
                      _Clearable,
                      _Undoable,
                      _Copyable>& _g,
    int                           _w,
    int                           _h
)
{
    _g.width  = (_w < 0) ? 0 : _w;
    _g.height = (_h < 0) ? 0 : _h;

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_GRADIENT_SELECTOR_