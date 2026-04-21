/*******************************************************************************
* uxoxo [component]                                               color_fill.hpp
*
* Color fill meta-control:
*   A paint-bucket that acts on other UI components.  Holds a chosen
* colour and, on commit, asks the renderer to apply it to a designated
* target component.  Supports optional undo (so a fill can be reverted
* to the previous colour), optional clearability (reset the fill to a
* default), and optional copyable state (expose the applied colour to
* the clipboard).
*
*   Dual to color_sample: color_fill WRITES TO a target component.
* The struct itself never touches the target — it is a data-and-
* callback record.  The renderer observes commit (via the on_commit
* callback) and performs the actual paint operation.
*
*   Standard component operations (enable, disable, show, hide,
* set_read_only, get_value, set_value, clear, undo, request_copy,
* commit) are inherited from component_common.hpp via structural
* detection.  Only fill-specific operations live here.
*
* Contents:
*   1  DFillMode         — enum: solid / gradient / overlay / replace
*   2  color_fill<>      — struct template: the fill control
*   3  Operations
*       cf_apply          — convenience wrapper around commit
*       cf_retarget       — point at a different UI element
*       cf_set_mode       — switch fill mode
*       cf_set_opacity    — adjust overlay opacity
*
*
* path:      /inc/uxoxo/templates/component/color_fill.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COLOR_FILL_
#define  UXOXO_COLOR_FILL_ 1

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
//  1  FILL MODE
// ===============================================================================

// DFillMode
//   enum: how `value` should be applied to the target.
//     solid    — replace the target's colour with `value` exactly.
//     gradient — use `value` as one stop of a renderer-defined
//                gradient (the other stop is the target's existing
//                colour).  Useful for highlight / selection effects.
//     overlay  — alpha-composite `value` at `opacity` over the
//                target's existing colour.  Leaves the underlying
//                colour recoverable.
//     replace  — like `solid`, but instructs the renderer to treat
//                this as a destructive overwrite (no compositing
//                buffers, no undo at the renderer level).  Use
//                sparingly.
enum class DFillMode : std::uint8_t
{
    solid    = 0,
    gradient = 1,
    overlay  = 2,
    replace  = 3
};


/*****************************************************************************/

// ===============================================================================
//  2  COLOR FILL
// ===============================================================================

// color_fill
//   struct template: paint-bucket meta control that applies a
// colour value to a target UI component.
//
//   Template parameters:
//     _Color      — color model type (defaults to rgb).
//     _TargetRef  — type used to reference the target UI component.
//     _Labeled    — if true, mixes in a `label` string.
//     _Clearable  — if true, mixes in a `default_value` of type
//                   _Color for reset-to-default via clear().
//     _Undoable   — if true, mixes in `previous_value` + `has_previous`
//                   so the shared undo() can revert the last fill.
//     _Copyable   — if true, mixes in `copy_requested` so the shared
//                   request_copy() can flag the fill colour for
//                   clipboard export.
//
//   Interaction with shared operations:
//     set_value      — updates `value`; if undoable, stores the prior
//                      value; fires on_change.  (Just sets the colour;
//                      does NOT apply it.)
//     commit         — asks the renderer to apply `value` to `target`
//                      via on_commit.  This is the operation that
//                      actually paints.
//     undo           — restores `value` to `previous_value`.  Whether
//                      the target is repainted depends on whether
//                      the caller invokes commit() afterwards.
//     clear          — resets `value` to `default_value`.
//     request_copy   — flags `value` for copy-to-clipboard.
//     set_read_only  — blocks commit() even if `enabled` is true.
template <typename _Color     = djinterp::color::rgb,
          typename _TargetRef = void*,
          bool     _Labeled   = false,
          bool     _Clearable = false,
          bool     _Undoable  = false,
          bool     _Copyable  = false>
struct color_fill
    : component_mixin::label_data<_Labeled>
    , component_mixin::clearable_data<_Clearable, _Color>
    , component_mixin::undo_data<_Undoable, _Color>
    , component_mixin::copyable_data<_Copyable>
{
    using color_type  = _Color;
    using target_type = _TargetRef;
    using value_type  = _Color;

    // value
    //   member: the colour that will be applied to `target` on commit.
    _Color value {};

    // target
    //   member: reference to the UI component being filled.
    _TargetRef target {};

    // mode
    //   member: how to apply `value`.  See DFillMode.
    DFillMode mode { DFillMode::solid };

    // opacity
    //   member: alpha used when mode == overlay, in [0, 1].  Ignored
    // in other modes.  Kept as djinterp::color::channel_t so it has
    // the same precision as a colour channel.
    djinterp::color::channel_t opacity =
        djinterp::color::channel_t(1);

    // enabled
    //   member: when false, commit() is a no-op.
    bool enabled = true;

    // visible
    //   member: whether the fill control's own UI (swatch preview,
    // apply button) is shown.
    bool visible = true;

    // read_only
    //   member: when true, commit() is a no-op even if enabled.
    // Useful for "preview the chosen colour but don't let the user
    // apply it yet".
    bool read_only = false;

    // on_change
    //   callback: invoked when `value` is updated via set_value.
    std::function<void(const _Color&)> on_change;

    // on_commit
    //   callback: the renderer's handler that actually paints `value`
    // onto `target`.  Invoked by the shared commit().
    std::function<void(const _Color&)> on_commit;
};


/*****************************************************************************/

// ===============================================================================
//  3  OPERATIONS
// ===============================================================================
//   Fill-specific operations.  Shared operations are inherited.

// cf_apply
//   function: convenience alias for commit().  Named to match the
// paint-bucket mental model.  Returns true if the fill callback
// fired, false if the control was disabled, read-only, or had
// no callback registered.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
bool cf_apply(color_fill<_Color,
                         _TargetRef,
                         _Labeled,
                         _Clearable,
                         _Undoable,
                         _Copyable>& _f)
{
    if (!_f.enabled || _f.read_only)
    {
        return false;
    }

    if (_f.on_commit)
    {
        _f.on_commit(_f.value);

        return true;
    }

    return false;
}

// cf_retarget
//   function: points the fill at a different UI component.
// Does not fire any callbacks.  Does not commit the fill.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void cf_retarget(color_fill<_Color,
                            _TargetRef,
                            _Labeled,
                            _Clearable,
                            _Undoable,
                            _Copyable>& _f,
                 _TargetRef              _new_target)
{
    _f.target = std::move(_new_target);

    return;
}

// cf_set_mode
//   function: switches fill strategy.
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void cf_set_mode(color_fill<_Color,
                            _TargetRef,
                            _Labeled,
                            _Clearable,
                            _Undoable,
                            _Copyable>& _f,
                 DFillMode                _mode)
{
    _f.mode = _mode;

    return;
}

// cf_set_opacity
//   function: sets overlay opacity, clamped to [0, 1].
template <typename _Color,
          typename _TargetRef,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void cf_set_opacity(color_fill<_Color,
                               _TargetRef,
                               _Labeled,
                               _Clearable,
                               _Undoable,
                               _Copyable>&        _f,
                    djinterp::color::channel_t   _op)
{
    using ch = djinterp::color::channel_t;

    _f.opacity = djinterp::color::clamp_channel(
                     _op, ch(0), ch(1));

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COLOR_FILL_
