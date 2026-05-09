/*******************************************************************************
* uxoxo [component]                                                dock_host.hpp
*
* Generic dock-host composite template:
*   The container side of the dockable subsystem - a composite component
* that owns a configurable set of dock zones (left, right, top, bottom,
* center), accepts heterogeneous dockable children via type-erased
* dock_handle objects, and surfaces the runtime state needed by a
* renderer to draw the cross-shaped dock picker when any child has
* requested it.
*
*   The host derives from `composite<_Feat, _Slots...>` with one slot
* per supported zone, each slot's value type being dock_zone_state.
* Disabled zones (their bit not set in _Feat) inherit the empty
* primary template and EBO-collapse to zero bytes; enabled zones
* contribute one dock_zone_state each.
*
*   Children are stored in a flat vector of dock_handle on the host.
* The handle's get_current_zone() determines which zone a child
* visually belongs to; for_each_in_zone (in dock_host_common.hpp)
* filters by this on demand.  The flat-storage choice avoids
* per-zone bucket thrashing during reconcile - the only state
* mutation is the panel's own current_zone field, performed inside
* its confirm() trampoline.
*
*   Picker integration is observation-only: reconcile() walks all
* handles and sets host.picker_open = true plus host.picker_owner
* to the first handle reporting popup_open == true.  A renderer that
* wants to draw the picker reads those two fields and consults
* host.has_field<dock_zone_X_tag>() (or supports(zone)) to know which
* arms of the cross to show.
*
*   Selecting host capabilities at instantiation:
*
*     using full_host       = dock_host<D_DOCK_HOST_ALL>;
*     using sides_only_host = dock_host<D_DOCK_HOST_CARDINAL>;
*     using vertical_host   = dock_host<D_DOCK_HOST_LEFT |
*                                       D_DOCK_HOST_RIGHT>;
*
*   Hosts compose with everything else in the framework: the host
* satisfies is_component_v (it carries enabled / visible) and can
* itself be a child of another container.
*
* Contents:
*   1  dock_host primary class template
*   2  supports() static query
*   3  picker state (picker_open, picker_owner)
*
*
* path:      /inc/uxoxo/templates/component/dockable/dock_host.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCK_HOST_
#define  UXOXO_DOCK_HOST_ 1

// std
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../template_component.hpp"
#include "./dock_handle.hpp"
#include "./dock_host_types.hpp"
#include "./dockable_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  1  DOCK HOST
// ===========================================================================

// dock_host
//   class: generic container template owning a configurable set of
// dock zones plus a flat list of heterogeneous dockable children.
// The _Feat bitmask selects which zones the host supports; each
// supported zone contributes one dock_zone_state slot to the
// inherited composite base.  Disabled zones contribute zero bytes
// thanks to EBO of the empty composite slot specialization.
template<unsigned _Feat>
class dock_host
    : public composite<_Feat,
                       slot<D_DOCK_HOST_LEFT,
                            field<dock_zone_left_tag,   dock_zone_state>>,
                       slot<D_DOCK_HOST_RIGHT,
                            field<dock_zone_right_tag,  dock_zone_state>>,
                       slot<D_DOCK_HOST_TOP,
                            field<dock_zone_top_tag,    dock_zone_state>>,
                       slot<D_DOCK_HOST_BOTTOM,
                            field<dock_zone_bottom_tag, dock_zone_state>>,
                       slot<D_DOCK_HOST_CENTER,
                            field<dock_zone_center_tag, dock_zone_state>>>
{
public:
    static constexpr unsigned features = _Feat;

    dock_host() = default;

    // ===================================================================
    //  2  SUPPORTS
    // ===================================================================

    // supports
    //   function: compile-time query reporting whether this host
    // instantiation includes the given zone.  Equivalent to checking
    // the corresponding has_field<_Tag>() but expressed in zone terms
    // for runtime callers.
    [[nodiscard]] static constexpr bool
    supports(DDockZone _zone) noexcept
    {
        return ( (_zone != DDockZone::none) &&
                 ((features & dock_zone_bit(_zone)) != 0u) );
    }

    // ===================================================================
    //  3  COMPONENT SURFACE
    // ===================================================================
    //   The host participates in the standard component contract so
    // it can itself be a dockable child of another host (a
    // sub-document panel inside a workspace, for example) and so
    // generic verbs like enable / show apply.

    bool enabled = true;
    bool visible = true;

    // ===================================================================
    //  4  CHILDREN
    // ===================================================================

    std::vector<dock_handle> items;

    // ===================================================================
    //  5  PICKER STATE
    // ===================================================================
    //   These fields are written by reconcile() and read by the
    // renderer.  picker_owner identifies which child requested the
    // picker so the renderer knows whose dock target it is choosing
    // when the user clicks one of the cross arms.

    bool        picker_open  = false;
    dock_handle picker_owner {};
};


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCK_HOST_
