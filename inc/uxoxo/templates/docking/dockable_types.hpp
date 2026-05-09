/*******************************************************************************
* uxoxo [component]                                          dockable_types.hpp
*
* Shared dockable type vocabulary:
*   Plain enums and value types describing where a dockable component can be
* placed, how a requested zone is resolved against the host's available
* sides, and how the dockability affordance is exposed to the user.  This
* header carries no traits, mixins, or functions - only the value
* vocabulary that the trait, mixin, concept, and free-function layers
* downstream all share.
*
*   The types here are deliberately UI-framework- and docking-strategy-
* agnostic: they describe intent ("where", "how-to-resolve",
* "how-presented") rather than mechanism.  Concrete renderers (ImGui,
* native, web) and concrete dock layouts (split, accordion, tabbed,
* floating) interpret these values according to their own conventions -
* a `DDockZone::left` request becomes a left-anchored split panel in
* one renderer and a left-aligned tab strip in another.
*
*   `DDockPolicy::nearest` covers the "closest available side" case:
* the zone field carries the request, the policy controls how the host
* maps that request to a final concrete zone when sides are constrained
* or unavailable.  `DDockPresentation` covers the "button popup vs
* pure functionality" axis.
*
* Contents:
*   1.  DDockZone         - concrete docking destinations
*   2.  DDockPolicy       - target-zone resolution strategy
*   3.  DDockPresentation - how the dock affordance is exposed
*
* path:      /inc/uxoxo/templates/component/dockable/dockable_types.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCKABLE_TYPES_
#define  UXOXO_DOCKABLE_TYPES_ 1

// std
#include <cstdint>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  1.  DOCK ZONE
// ===========================================================================

// DDockZone
//   enum: concrete docking destinations a dockable component may occupy.
// The four cardinal values represent the "far sides" - dock targets pinned
// to an edge of the host.  `center` fills the host's remaining interior
// area (the "take all available space" target), and is mutually exclusive
// with the cardinal sides on a per-host basis.  `none` indicates the
// component is not currently docked (floating, hidden, or unattached).
//
//   Resolution-time sentinels (e.g. "closest side") are NOT zones - they
// are policies (see DDockPolicy).  Once resolved, the result is always
// one of {left, right, top, bottom, center, none}.
enum class DDockZone : std::uint8_t
{
    none   = 0,  // not docked / floating / detached
    left   = 1,  // pinned to the host's left edge
    right  = 2,  // pinned to the host's right edge
    top    = 3,  // pinned to the host's top edge
    bottom = 4,  // pinned to the host's bottom edge
    center = 5   // fills the host's remaining interior area
};


// ===========================================================================
//  2.  DOCK POLICY
// ===========================================================================

// DDockPolicy
//   enum: strategy a host uses to map a requested zone onto a concrete
// final zone.  `fixed` honors the requested zone exactly and refuses if
// it is unavailable; `nearest` picks the closest available cardinal side
// to the component's current screen position (or to the requested zone
// when no position is known); `flow` walks a host-defined preference
// order and accepts the first available zone.
//
//   Renderers and host containers are free to add private resolution
// behaviors on top of these (snapping, animation, etc.); the policy
// only governs the logical outcome, not the visual transition.
enum class DDockPolicy : std::uint8_t
{
    fixed   = 0,  // honor the requested zone exactly
    nearest = 1,  // resolve to the closest available cardinal side
    flow    = 2   // first-available across a host-defined preference list
};


// ===========================================================================
//  3.  DOCK PRESENTATION
// ===========================================================================

// DDockPresentation
//   enum: how the dock-target affordance is exposed to the user.
// `behavior` makes dockability a pure programmatic capability - state
// transitions are driven by API calls or drag-and-drop gestures, with
// no dedicated control rendered.  `popup` adds a button-triggered
// popover that reveals the host's available zones (typically rendered
// as the cross-shaped picker) and dispatches the user's choice through
// the same state surface.
//
//   `popup` does not imply any particular widget vocabulary - the
// renderer chooses whether the trigger is a corner glyph, a context
// menu entry, or a toolbar button.  The contract is only that the
// renderer observes `popup_open` (see dockable_mixin.hpp) and toggles
// the picker accordingly.
enum class DDockPresentation : std::uint8_t
{
    behavior = 0,  // pure functionality; no dedicated UI affordance
    popup    = 1   // button-triggered popover exposes the dock zones
};


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCKABLE_TYPES_
