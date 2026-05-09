/*******************************************************************************
* uxoxo [component]                                          dockable_mixin.hpp
*
* Dockable EBO data mixin:
*   Shared data carried by any component that opts into dockability.  The
* mixin follows the same EBO-eligible primary-template / `<true>`-active-
* specialization pattern used by component_mixin.hpp - components that
* set the enable flag inherit storage; components that don't pay zero
* bytes for the disabled base.
*
*   The dockable_data mixin holds purely descriptive state.  It does not
* perform any docking, layout, or rendering: it is the contract surface
* that the renderer/host observes (current_zone, target_zone, policy,
* dock_requested, popup_open) and the API verbs in dockable_common.hpp
* mutate.  Concrete docking is the renderer's responsibility.
*
*   Components mix this in alongside their other capability mixins:
*
*     struct my_panel
*         : component_mixin::label_data    <has_label>
*         , component_mixin::dockable_data <has_dock>
*     {
*         std::string content;
*         bool        enabled  = true;
*         bool        visible  = true;
*     };
*
* Contents:
*   1  dockable_data       - core dockable state (zone, target, policy,
*                            presentation, dock_requested)
*   2  dockable_popup_data - popup-trigger state, separable for
*                            components that statically opt out of popup
*                            presentation
*
*
* path:      /inc/uxoxo/templates/component/dockable/dockable_mixin.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCKABLE_MIXIN_
#define  UXOXO_DOCKABLE_MIXIN_ 1

// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./dockable_types.hpp"


NS_UXOXO
NS_COMPONENT


namespace component_mixin {

// ===========================================================================
//  1  DOCKABLE
// ===========================================================================
//   Core dockable state.  `current_zone` is the component's actual
// docked location, written by the host/renderer after a layout pass.
// `target_zone` is the requested destination, written by the API verbs
// (dock, undock) and read by the host on the next reconciliation tick.
// `dock_requested` is the edge-trigger that tells the host a pending
// change exists; the host clears it once `current_zone` has been
// brought into agreement with `target_zone`.
//
//   `policy` and `presentation` are sticky configuration knobs - they
// describe how subsequent dock operations on this component should be
// resolved and how the affordance should be exposed.  Both are
// runtime-mutable so a component can switch from behavior-only to
// popup-driven (or vice versa) without changing its type.

// dockable_data
//   trait: EBO-eligible empty base used when a component does not opt
// into dockability.
template<bool _Enable>
struct dockable_data
{};

// dockable_data<true>
//   struct: active specialization carrying the dockable state surface.
template<>
struct dockable_data<true>
{
    DDockZone         current_zone   = DDockZone::none;
    DDockZone         target_zone    = DDockZone::none;
    DDockPolicy       policy         = DDockPolicy::fixed;
    DDockPresentation presentation   = DDockPresentation::behavior;
    bool              dock_requested = false;
};




// ===========================================================================
//  2  DOCKABLE POPUP
// ===========================================================================
//   Optional sibling mixin that supplies the runtime flag observed by
// renderers when `presentation == popup`.  Splitting this from
// dockable_data lets components statically opt out of popup support
// when a button affordance will never be needed (e.g. fixed-layout
// panels), saving the byte and signaling intent at the type level.
//
//   Components that want runtime switchability between behavior and
// popup presentations should still inherit dockable_popup_data<true>
// even when the initial presentation is `behavior` - otherwise the
// renderer will have nowhere to write back the open/close state.

// dockable_popup_data
//   trait: EBO-eligible empty base used when a component will never
// expose a popup dock picker.
template<bool _Enable>
struct dockable_popup_data
{};

// dockable_popup_data<true>
//   struct: active specialization carrying the popup-open flag.
template<>
struct dockable_popup_data<true>
{
    bool popup_open = false;
};


}   // namespace component_mixin


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCKABLE_MIXIN_
