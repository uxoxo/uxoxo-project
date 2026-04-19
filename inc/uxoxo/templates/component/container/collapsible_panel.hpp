/*******************************************************************************
* uxoxo [component]                                          collapsible_panel.hpp
*
* Collapsible panel template component:
*   A bordered, optionally-titled container whose body can be toggled between
* an expanded state (body visible) and a collapsed state (body hidden, with
* an optional summary line shown in its place).  Common in preference panes,
* debug consoles, accordion sidebars, tree-outline nodes, and anywhere
* grouped content needs to stay out of the way until the user asks for it.
*
*   The template is parameterized on two axes:
*     _Body:      the payload type stored inside the panel.  Any type the
*                 caller wants (a single child node_ptr, a vector of
*                 children, a string, a nested component struct...).  The
*                 template does not look inside it — the renderer does.
*     _Features:  a bitmask of cpf_* flags selecting optional capabilities.
*                 Unselected features compile out to empty EBO bases with
*                 no runtime cost.
*
*   Structural capabilities exposed unconditionally (so the free functions
* in component_common.hpp discover them via SFINAE):
*     .enabled  — participates in enable() / disable() / is_enabled()
*     .visible  — participates in show() / hide() / is_visible() /
*                 toggle_visible()
*
*   Domain-specific state (the reason this is a *collapsible* panel):
*     .expanded — toggled by the cp_* free functions in this header.
*                 Orthogonal to .visible: a panel may be hidden entirely,
*                 or merely collapsed.
*
*   Feature-gated state (present only when the corresponding cpf_* bit
* is set in _Features):
*     .label              — cpf_labeled
*     .summary            — cpf_summary
*     .animation_progress — cpf_animated
*     .on_change          — cpf_change_callback
*
* Contents:
*   1  cpf_*                        — feature flag bits
*   2  collapsible_panel_mixin      — EBO data mixins for optional features
*   3  collapsible_panel<>          — primary struct template
*   4  Preset aliases               — named configurations for common uses
*   5  cp_* free functions          — expand / collapse / toggle / query
*
*
* path:      /inc/uxoxo/templates/component/collapsible_panel.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COLLAPSIBLE_PANEL_
#define  UXOXO_COLLAPSIBLE_PANEL_ 1

// std
#include <cstdint>
#include <functional>
#include <string>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../component_types.hpp"
#include "../../component/component_mixin.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  FEATURE FLAGS
// ===============================================================================
//   Bitflag constants selecting which optional capabilities a given
// collapsible_panel instantiation carries.  Pack into the _Features
// non-type template parameter.  Each flag maps to one EBO mixin base;
// unselected flags compile out to an empty base and contribute
// nothing to sizeof().

// cpf_none
//   constant: no optional features.  The panel carries only its core
// state (expanded / enabled / visible) and its body payload.
inline constexpr unsigned cpf_none            = 0u;

// cpf_labeled
//   constant: panel carries a `label` string (the header text shown
// in the title bar).  Pulls in component_mixin::label_data.
inline constexpr unsigned cpf_labeled         = 1u << 0;

// cpf_summary
//   constant: panel carries a `summary` string, rendered in place of
// the body while collapsed (e.g. "3 items" or "user@host").
inline constexpr unsigned cpf_summary         = 1u << 1;

// cpf_animated
//   constant: panel carries animation state (animation_progress,
// animation_duration_ms) so that renderers capable of animating can
// interpolate between the expanded and collapsed shapes.  Headless
// or cell-based (TUI) renderers may ignore this data entirely.
inline constexpr unsigned cpf_animated        = 1u << 2;

// cpf_focusable
//   constant: the panel header is itself keyboard-focusable.  When
// focused, Enter / Space toggles the panel.  Enables the
// `focusable = true` capability flag detected by component_traits.
inline constexpr unsigned cpf_focusable       = 1u << 3;

// cpf_change_callback
//   constant: panel carries an `on_change(bool expanded)` callable,
// invoked whenever the expanded state transitions.  The callback
// receives the NEW expanded state as its argument.
inline constexpr unsigned cpf_change_callback = 1u << 4;

// cpf_default
//   constant: the typical feature set — a labeled, keyboard-toggleable
// panel that fires change notifications.  Does not include animation
// or a summary line.
inline constexpr unsigned cpf_default         =
    ( cpf_labeled         |
      cpf_focusable       |
      cpf_change_callback );

// cpf_standard
//   constant: cpf_default plus a summary line and animation support —
// the full-featured configuration suitable for GUI renderers.
inline constexpr unsigned cpf_standard        =
    ( cpf_default |
      cpf_summary |
      cpf_animated );




// ===============================================================================
//  2  COLLAPSIBLE PANEL MIXINS
// ===============================================================================
//   EBO data mixins local to collapsible_panel.  Each follows the same
// primary-empty + true-specialization pattern as component_mixin.
// Shared mixins (label_data) are pulled from component_mixin; only
// the panel-specific ones live here.

namespace collapsible_panel_mixin {

// summary_data
//   mixin: an optional summary string shown in place of the body
// while the panel is collapsed.
template <bool _Enable>
struct summary_data
{};

template <>
struct summary_data<true>
{
    std::string summary;
};


// animation_data
//   mixin: visual interpolation state.  animation_progress tracks
// the renderer's current position in the collapse / expand transition
// (0.0 == fully collapsed, 1.0 == fully expanded).  The template does
// not drive the interpolation itself; it merely reserves the storage
// so the renderer has somewhere to park per-frame state.
template <bool _Enable>
struct animation_data
{};

template <>
struct animation_data<true>
{
    float         animation_progress    = 1.0f;
    std::uint16_t animation_duration_ms = 150;
};


// change_callback_data
//   mixin: an on_change(bool) callable invoked on every transition
// of the expanded flag.
template <bool _Enable>
struct change_callback_data
{};

template <>
struct change_callback_data<true>
{
    std::function<void(bool)> on_change;
};


}   // namespace collapsible_panel_mixin




// ===============================================================================
//  3  COLLAPSIBLE PANEL
// ===============================================================================

// collapsible_panel
//   struct template: a toggleable container carrying an arbitrary body
// payload and a bitmask of optional feature mixins.  Inherits from
// component_mixin::label_data (shared) and the three collapsible-
// -panel-specific EBO mixins above.  Base classes for unselected
// features degrade to empty structs and occupy no storage.
template <typename _Body,
          unsigned _Features = cpf_default>
struct collapsible_panel
    : component_mixin::label_data<
          bool(_Features & cpf_labeled)>
    , collapsible_panel_mixin::summary_data<
          bool(_Features & cpf_summary)>
    , collapsible_panel_mixin::animation_data<
          bool(_Features & cpf_animated)>
    , collapsible_panel_mixin::change_callback_data<
          bool(_Features & cpf_change_callback)>
{
    // -- capability flags ---------------------------------------------
    //   Published as static constexpr members so component_traits
    // can detect them via has_features_field_v / is_focusable_v.

    static constexpr unsigned features  = _Features;
    static constexpr bool     focusable =
        bool(_Features & cpf_focusable);

    // -- body payload -------------------------------------------------
    //   Whatever the caller wants nested inside.  Default-constructed
    // on panel construction; the renderer is responsible for drawing
    // (or skipping) it according to the expanded flag.

    _Body body {};

    // -- core state ---------------------------------------------------
    //   Exposed unconditionally so the ADL functions in
    // component_common.hpp (enable, disable, show, hide,
    // toggle_visible, is_enabled, is_visible) dispatch on us.

    bool expanded = true;
    bool enabled  = true;
    bool visible  = true;

    // -- styling ------------------------------------------------------
    //   DEmphasis drives the renderer's choice of border color /
    // weight / tone for the panel frame.

    DEmphasis emph = DEmphasis::normal;
};




// ===============================================================================
//  4  PRESET ALIASES
// ===============================================================================
//   Named aliases for common feature combinations.  The caller
// provides _Body; the preset fixes _Features.

// simple_collapsible_panel
//   alias template: the bare minimum — no label, no summary, no
// callback, no animation.  Useful when the panel is a pure grouping
// control and its header is drawn out-of-band by the surrounding UI.
template <typename _Body>
using simple_collapsible_panel =
    collapsible_panel<_Body, cpf_none>;

// titled_collapsible_panel
//   alias template: a labeled, keyboard-toggleable panel without
// change notifications.  Fits tree-outline-style UIs where the owner
// polls `expanded` rather than subscribing to transitions.
template <typename _Body>
using titled_collapsible_panel =
    collapsible_panel<_Body,
                      ( cpf_labeled |
                        cpf_focusable )>;

// reactive_collapsible_panel
//   alias template: the cpf_default configuration — labeled,
// focusable, and with a change callback.  The common case for
// interactive preference / debug panels.
template <typename _Body>
using reactive_collapsible_panel =
    collapsible_panel<_Body, cpf_default>;

// full_collapsible_panel
//   alias template: every optional feature enabled — summary line,
// animation, label, focus, and change callback.  The right choice
// for GUI renderers that want to animate collapse transitions and
// surface a summary while collapsed.
template <typename _Body>
using full_collapsible_panel =
    collapsible_panel<_Body, cpf_standard>;




// ===============================================================================
//  5  CP_* FREE FUNCTIONS
// ===============================================================================
//   Domain-specific operations on the expanded flag.  Prefixed `cp_`
// per the convention noted in component_common.hpp (ti_*, to_*,
// as_*, ...).  Each change-producing operation fires on_change iff
// (a) the state actually transitioned, and (b) the cpf_change_callback
// feature is enabled and a callable is installed.
//
//   Operations on the other state surface (enable, disable, show,
// hide, toggle_visible, is_enabled, is_visible) are inherited from
// component_common.hpp via structural SFINAE — there is no need to
// redeclare them here.

NS_INTERNAL

    // fire_change
    //   function: invokes the on_change callback when cpf_change_callback
    // is enabled and the callback is populated.  No-op otherwise.
    template <typename _Body,
              unsigned _Features>
    void fire_change(collapsible_panel<_Body, _Features>& _p)
    {
        if constexpr ((_Features & cpf_change_callback) != 0u)
        {
            if (_p.on_change)
            {
                _p.on_change(_p.expanded);
            }
        }

        return;
    }

}   // NS_INTERNAL


// cp_is_expanded
//   function: queries whether the panel is currently expanded.
template <typename _Body,
          unsigned _Features>
[[nodiscard]] bool
cp_is_expanded(const collapsible_panel<_Body, _Features>& _p) noexcept
{
    return _p.expanded;
}

// cp_is_collapsed
//   function: queries whether the panel is currently collapsed.
// Equivalent to !cp_is_expanded(_p); provided for symmetry.
template <typename _Body,
          unsigned _Features>
[[nodiscard]] bool
cp_is_collapsed(const collapsible_panel<_Body, _Features>& _p) noexcept
{
    return !_p.expanded;
}

// cp_expand
//   function: expands the panel.  No-op if already expanded or if
// the panel is disabled.  Fires on_change on transition.
template <typename _Body,
          unsigned _Features>
void cp_expand(collapsible_panel<_Body, _Features>& _p)
{
    // bail if the panel is disabled — no state changes allowed
    if (!_p.enabled)
    {
        return;
    }

    // bail if already in the target state — no-op, no notification
    if (_p.expanded)
    {
        return;
    }

    _p.expanded = true;

    internal::fire_change(_p);

    return;
}

// cp_collapse
//   function: collapses the panel.  No-op if already collapsed or
// if the panel is disabled.  Fires on_change on transition.
template <typename _Body,
          unsigned _Features>
void cp_collapse(collapsible_panel<_Body, _Features>& _p)
{
    // bail if the panel is disabled — no state changes allowed
    if (!_p.enabled)
    {
        return;
    }

    // bail if already in the target state
    if (!_p.expanded)
    {
        return;
    }

    _p.expanded = false;

    internal::fire_change(_p);

    return;
}

// cp_toggle
//   function: flips the expanded state.  No-op if the panel is
// disabled.  Always fires on_change when the panel is enabled
// (every enabled call is, by definition, a transition).
template <typename _Body,
          unsigned _Features>
void cp_toggle(collapsible_panel<_Body, _Features>& _p)
{
    // bail if the panel is disabled — toggling is blocked
    if (!_p.enabled)
    {
        return;
    }

    _p.expanded = !_p.expanded;

    internal::fire_change(_p);

    return;
}

// cp_set_expanded
//   function: sets the expanded flag to the caller-supplied value.
// No-op if the panel is disabled or if the flag is already in the
// requested state.  Fires on_change on transition.
template <typename _Body,
          unsigned _Features>
void cp_set_expanded(collapsible_panel<_Body, _Features>& _p,
                     bool                                 _new_expanded)
{
    // bail if the panel is disabled
    if (!_p.enabled)
    {
        return;
    }

    // bail if already in the requested state
    if (_p.expanded == _new_expanded)
    {
        return;
    }

    _p.expanded = _new_expanded;

    internal::fire_change(_p);

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COLLAPSIBLE_PANEL_
