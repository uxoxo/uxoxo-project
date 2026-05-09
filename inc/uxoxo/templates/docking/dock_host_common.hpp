/*******************************************************************************
* uxoxo [component]                                         dock_host_common.hpp
*
* Shared dock-host operations:
*   Free-function verbs that operate on dock_host instances - the host-side
* counterpart to dockable_common.hpp.  Keeps the host class itself
* thin (data only) and follows the framework convention of expressing
* container behavior as ADL-dispatched free functions parameterized
* over the _Feat mask.
*
*   The host's job is registration and reconciliation.  Children are
* registered once via register_dockable, then drive their own state
* through the dockable verbs (dock, dock_to_nearest, undock, ...).
* Each tick the host calls reconcile, which sweeps every registered
* handle and:
*
*     1.  applies any pending dock request by calling the handle's
*         confirm() trampoline (which writes target_zone into
*         current_zone and clears dock_requested on the child);
*     2.  recomputes picker_open and picker_owner so the renderer
*         can draw or hide the cross-shaped picker on the next pass.
*
*   Policy-driven resolution (DDockPolicy::nearest / flow) needs
* layout information - screen positions, zone bounds, available
* sides - that this header has no business reaching for.  When a
* policy decision is required, the renderer or host implementation
* substitutes the resolved zone via dockable_common's set_target_zone
* equivalent (or directly mutates target_zone) before reconcile()
* runs; reconcile itself is layout-agnostic and just confirms.
*
* Contents:
*   1   Registration (register_dockable, unregister_dockable)
*   2   Reconciliation (reconcile)
*   3   Zone iteration (for_each_in_zone, count_in_zone)
*   4   Picker queries (is_picker_open, picker_owner)
*
*
* path:      /inc/uxoxo/templates/component/dockable/dock_host_common.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCK_HOST_COMMON_
#define  UXOXO_DOCK_HOST_COMMON_ 1

// std
#include <algorithm>
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./dock_handle.hpp"
#include "./dock_host.hpp"
#include "./dock_host_types.hpp"
#include "./dockable_traits.hpp"
#include "./dockable_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  1   REGISTRATION
// ===========================================================================

// register_dockable
//   function: adds a handle wrapping the supplied dockable component
// to the host's child list.  The component must outlive the host (or
// be unregistered before destruction).  Constrained to types
// satisfying is_dockable_v so the call site is rejected at compile
// time when the supplied component is missing the dockable surface.
template<unsigned _Feat,
         typename _Type,
         std::enable_if_t<
             is_dockable_v<_Type>,
             int> = 0>
void register_dockable(dock_host<_Feat>& _host,
                       _Type&            _dockable)
{
    _host.items.emplace_back(_dockable);

    return;
}

// unregister_dockable
//   function: removes the handle whose underlying pointer matches
// the supplied component.  Safe to call when no matching handle is
// registered (no-op).
template<unsigned _Feat,
         typename _Type,
         std::enable_if_t<
             is_dockable_v<_Type>,
             int> = 0>
void unregister_dockable(dock_host<_Feat>& _host,
                         _Type&            _dockable)
{
    void* const target = static_cast<void*>(&_dockable);

    auto it = std::remove_if(_host.items.begin(),
                             _host.items.end(),
                             [target](const dock_handle& _h)
                             {
                                 return (_h.raw() == target);
                             });

    _host.items.erase(it, _host.items.end());

    return;
}




// ===========================================================================
//  2   RECONCILIATION
// ===========================================================================

// reconcile
//   function: applies every pending dock request and refreshes the
// picker state.  Runs in two passes - first confirming dock requests
// so each handle's current_zone matches its target_zone, then
// scanning for any handle reporting popup_open to drive the host's
// picker_open / picker_owner fields.  The renderer is expected to
// call this once per layout tick before consulting the host.
//
// Parameter(s):
//   _host: the host whose registered children are reconciled.
// Return:
//   none.
template<unsigned _Feat>
void reconcile(dock_host<_Feat>& _host)
{
    // pass 1: apply pending dock requests
    for (auto& h : _host.items)
    {
        if (h.is_dock_pending())
        {
            // a host implementation that wants policy-driven zone
            // resolution (DDockPolicy::nearest / flow) would slot
            // its resolver call here, before confirm(), to rewrite
            // the child's target_zone into a concrete supported one.
            h.confirm();
        }
    }

    // pass 2: refresh picker state
    _host.picker_open  = false;
    _host.picker_owner = dock_handle{};

    for (const auto& h : _host.items)
    {
        if (h.is_popup_open())
        {
            _host.picker_open  = true;
            _host.picker_owner = h;
            break;
        }
    }

    return;
}




// ===========================================================================
//  3   ZONE ITERATION
// ===========================================================================

// for_each_in_zone
//   function: invokes _fn(handle&) on every registered handle whose
// current_zone matches _zone.  Iteration order matches insertion
// order.  Allocation-free.
template<unsigned _Feat,
         typename _Fn>
void for_each_in_zone(dock_host<_Feat>& _host,
                      DDockZone         _zone,
                      _Fn&&             _fn)
{
    for (auto& h : _host.items)
    {
        if (h.get_current_zone() == _zone)
        {
            _fn(h);
        }
    }

    return;
}

// for_each_in_zone (const overload)
//   function: const-iterating sibling of for_each_in_zone.
template<unsigned _Feat,
         typename _Fn>
void for_each_in_zone(const dock_host<_Feat>& _host,
                      DDockZone               _zone,
                      _Fn&&                   _fn)
{
    for (const auto& h : _host.items)
    {
        if (h.get_current_zone() == _zone)
        {
            _fn(h);
        }
    }

    return;
}

// count_in_zone
//   function: returns the number of registered handles whose
// current_zone matches _zone.
template<unsigned _Feat>
[[nodiscard]] std::size_t
count_in_zone(const dock_host<_Feat>& _host,
              DDockZone               _zone) noexcept
{
    std::size_t n = 0;

    for (const auto& h : _host.items)
    {
        if (h.get_current_zone() == _zone)
        {
            ++n;
        }
    }

    return n;
}




// ===========================================================================
//  4   PICKER QUERIES
// ===========================================================================

// is_picker_open
//   function: returns true when the host's last reconcile() pass
// found at least one child with popup_open == true.  Renderers
// consult this to decide whether to draw the cross-shaped dock
// picker.
template<unsigned _Feat>
[[nodiscard]] bool
is_picker_open(const dock_host<_Feat>& _host) noexcept
{
    return _host.picker_open;
}

// get_picker_owner
//   function: returns the handle whose popup-open request triggered
// the picker.  Valid only when is_picker_open() returns true.
// Renderers use the returned handle to identify the panel whose
// dock target the user is choosing, and dispatch dock(...) onto
// the corresponding component when the user picks an arm.
template<unsigned _Feat>
[[nodiscard]] dock_handle
get_picker_owner(const dock_host<_Feat>& _host) noexcept
{
    return _host.picker_owner;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCK_HOST_COMMON_
