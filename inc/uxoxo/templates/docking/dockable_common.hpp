/*******************************************************************************
* uxoxo [component]                                          dockable_common.hpp
*
* Shared dockable operations:
*   ADL-dispatched free functions that operate on any structurally
* conforming dockable component.  These are the canonical verbs for
* dock state mutation and inspection - dock, undock, dock_to_nearest,
* dock_to_center, set_dock_policy, set_dock_presentation, get_current_zone,
* is_docked, confirm_dock - plus the popup-only verbs open_dock_popup /
* close_dock_popup / toggle_dock_popup / is_dock_popup_open which gate
* themselves on the popup-state mixin.
*
*     dock(my_panel, DDockZone::left);
*     dock_to_nearest(my_panel);
*     dock_to_center(my_panel);
*     undock(my_panel);
*     set_dock_presentation(my_panel, DDockPresentation::popup);
*     toggle_dock_popup(my_panel);
*
*   Each verb is constrained via SFINAE so it only matches types that
* expose the required members.  No base class, no tags - just
* structural conformance, mirroring the dispatch model used by
* component_common.hpp.
*
*   The verbs are intentionally thin: they mutate state and raise the
* dock_requested edge trigger.  The actual reconciliation - moving a
* panel from its current_zone into its target_zone, allocating space,
* invoking the renderer - is the host's responsibility.  After the host
* has performed the layout pass, it calls confirm_dock(c) to bring
* current_zone into agreement with target_zone and clear the trigger.
*
*   Sample component opting in:
*
*     struct my_panel
*         : component_mixin::dockable_data       <true>
*         , component_mixin::dockable_popup_data <true>
*     {
*         std::string content;
*         bool        enabled = true;
*         bool        visible = true;
*     };
*
*     my_panel p;
*     dock(p, DDockZone::left);                 // request fixed dock
*     dock_to_nearest(p);                        // request closest side
*     set_dock_presentation(p, DDockPresentation::popup);
*     open_dock_popup(p);                        // host shows the picker
*
* Contents:
*   1   Dock requests (dock, dock_to_nearest, dock_to_center, undock)
*   2   Configuration (set_dock_policy, set_dock_presentation)
*   3   Queries (get_current_zone, get_target_zone, is_docked,
*       is_dock_pending)
*   4   Host reconciliation (confirm_dock)
*   5   Popup verbs (open / close / toggle / is_dock_popup_open)
*
*
* path:      /inc/uxoxo/templates/component/dockable/dockable_common.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCKABLE_COMMON_
#define  UXOXO_DOCKABLE_COMMON_ 1

// std
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./dockable_traits.hpp"
#include "./dockable_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  1   DOCK REQUESTS
// ===========================================================================
//   These verbs all set target_zone (and policy where relevant) and
// raise the dock_requested edge trigger.  The host observes the
// trigger on its next reconciliation tick, performs whatever layout
// work is required, and calls confirm_dock(c) to finalize.

// dock
//   function: requests that the component dock to the supplied zone
// using the component's currently configured policy.  Sets
// target_zone, raises dock_requested.
template<typename _Type,
         std::enable_if_t<
             is_dockable_v<_Type>,
             int> = 0>
void dock(_Type&    _c,
          DDockZone _zone)
{
    _c.target_zone    = _zone;
    _c.dock_requested = true;

    return;
}

// dock_to_nearest
//   function: requests that the host place the component in the
// closest available cardinal side.  Sets policy to nearest and
// clears any specific target zone, leaving the host free to pick.
template<typename _Type,
         std::enable_if_t<
             is_dockable_v<_Type>,
             int> = 0>
void dock_to_nearest(_Type& _c)
{
    _c.policy         = DDockPolicy::nearest;
    _c.target_zone    = DDockZone::none;
    _c.dock_requested = true;

    return;
}

// dock_to_nearest (with preferred zone)
//   function: requests nearest-side resolution biased toward a
// preferred zone - the host honors the preference if available and
// otherwise falls back to whichever cardinal side is closest.
template<typename _Type,
         std::enable_if_t<
             is_dockable_v<_Type>,
             int> = 0>
void dock_to_nearest(_Type&    _c,
                     DDockZone _preferred)
{
    _c.policy         = DDockPolicy::nearest;
    _c.target_zone    = _preferred;
    _c.dock_requested = true;

    return;
}

// dock_to_center
//   function: requests that the component dock to the center,
// taking all remaining interior space.  Convenience for the common
// "fill the available area" case.
template<typename _Type,
         std::enable_if_t<
             is_dockable_v<_Type>,
             int> = 0>
void dock_to_center(_Type& _c)
{
    _c.target_zone    = DDockZone::center;
    _c.dock_requested = true;

    return;
}

// undock
//   function: requests that the component leave its current zone
// and become floating / detached.  Sets target_zone to none and
// raises the trigger.
template<typename _Type,
         std::enable_if_t<
             is_dockable_v<_Type>,
             int> = 0>
void undock(_Type& _c)
{
    _c.target_zone    = DDockZone::none;
    _c.dock_requested = true;

    return;
}




// ===========================================================================
//  2   CONFIGURATION
// ===========================================================================
//   Sticky knobs.  These do not raise dock_requested - they only
// change how subsequent dock operations on this component are
// resolved or presented.

// set_dock_policy
//   function: replaces the component's resolution policy.
template<typename _Type,
         std::enable_if_t<
             has_dock_policy_v<_Type>,
             int> = 0>
void set_dock_policy(_Type&      _c,
                     DDockPolicy _policy)
{
    _c.policy = _policy;

    return;
}

// set_dock_presentation
//   function: switches the dock affordance between behavior-only and
// popup-driven.  The renderer observes this on its next pass.
template<typename _Type,
         std::enable_if_t<
             has_dock_presentation_v<_Type>,
             int> = 0>
void set_dock_presentation(_Type&            _c,
                           DDockPresentation _presentation)
{
    _c.presentation = _presentation;

    return;
}




// ===========================================================================
//  3   QUERIES
// ===========================================================================

// get_current_zone
//   function: returns the component's actual docked location, as
// last written by the host.
template<typename _Type,
         std::enable_if_t<
             has_current_zone_v<_Type>,
             int> = 0>
[[nodiscard]] DDockZone
get_current_zone(const _Type& _c) noexcept
{
    return _c.current_zone;
}

// get_target_zone
//   function: returns the component's pending dock destination.
template<typename _Type,
         std::enable_if_t<
             has_target_zone_v<_Type>,
             int> = 0>
[[nodiscard]] DDockZone
get_target_zone(const _Type& _c) noexcept
{
    return _c.target_zone;
}

// is_docked
//   function: returns true when the component currently occupies a
// concrete zone (cardinal side or center).
template<typename _Type,
         std::enable_if_t<
             has_current_zone_v<_Type>,
             int> = 0>
[[nodiscard]] bool
is_docked(const _Type& _c) noexcept
{
    return _c.current_zone != DDockZone::none;
}

// is_dock_pending
//   function: returns true when a dock request is awaiting the host's
// reconciliation pass.
template<typename _Type,
         std::enable_if_t<
             has_dock_requested_v<_Type>,
             int> = 0>
[[nodiscard]] bool
is_dock_pending(const _Type& _c) noexcept
{
    return _c.dock_requested;
}




// ===========================================================================
//  4   HOST RECONCILIATION
// ===========================================================================
//   confirm_dock is the single verb the host calls after applying a
// pending dock request.  It brings current_zone into agreement with
// target_zone and clears the edge trigger.  Components do not call
// this directly - dock-side verbs (dock, undock, ...) raise the
// trigger; host-side verbs lower it.

// confirm_dock
//   function: writes target_zone into current_zone and clears
// dock_requested.  The host calls this once it has finished the
// layout work implied by a dock request.
template<typename _Type,
         std::enable_if_t<
             is_dockable_v<_Type>,
             int> = 0>
void confirm_dock(_Type& _c)
{
    _c.current_zone   = _c.target_zone;
    _c.dock_requested = false;

    return;
}




// ===========================================================================
//  5   POPUP VERBS
// ===========================================================================
//   These are gated on has_dock_popup_v - i.e. the component must
// carry the popup_open flag.  They are no-ops at compile time on
// behavior-only dockables, so generic code that calls them through
// `if constexpr` does not need parallel manual gating.

// open_dock_popup
//   function: raises the popup-open flag.  The renderer observes the
// flag on its next pass and shows the dock picker.
template<typename _Type,
         std::enable_if_t<
             has_dock_popup_v<_Type>,
             int> = 0>
void open_dock_popup(_Type& _c)
{
    _c.popup_open = true;

    return;
}

// close_dock_popup
//   function: lowers the popup-open flag, dismissing the picker on
// the next render pass.
template<typename _Type,
         std::enable_if_t<
             has_dock_popup_v<_Type>,
             int> = 0>
void close_dock_popup(_Type& _c)
{
    _c.popup_open = false;

    return;
}

// toggle_dock_popup
//   function: flips the popup-open flag.
template<typename _Type,
         std::enable_if_t<
             has_dock_popup_v<_Type>,
             int> = 0>
void toggle_dock_popup(_Type& _c)
{
    _c.popup_open = !_c.popup_open;

    return;
}

// is_dock_popup_open
//   function: returns true when the dock picker is currently
// requested to be visible.
template<typename _Type,
         std::enable_if_t<
             has_dock_popup_v<_Type>,
             int> = 0>
[[nodiscard]] bool
is_dock_popup_open(const _Type& _c) noexcept
{
    return _c.popup_open;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCKABLE_COMMON_
