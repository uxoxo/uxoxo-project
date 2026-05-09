/*******************************************************************************
* uxoxo [component]                                                 dockable.hpp
*
* Foundational dockable module:
*   The umbrella header for the dockable subsystem.  Includes every
* dockable header in dependency order, and defines - in this comment
* block - the abstract model that every other file in the subsystem
* implements.  Read this file before extending any other dockable
* header: the contracts and invariants stated here are the promises
* the rest of the subsystem keeps.
*
*
*   --------------------------------------------------------------------
*   ABSTRACT MODEL
*   --------------------------------------------------------------------
*
*   Dockability is the capability of a component to occupy a named
* region of a host's interior - a "zone" - and to migrate between
* zones in response to programmatic, gestural, or user-initiated
* events.  A dockable component does not perform docking itself.  It
* publishes its intent via a small, structurally detectable state
* surface, and a host realizes that intent through a layout pass.
*
*   The model factors into three orthogonal axes plus a contract.
*
*   1.  WHERE - the destination set (DDockZone)
*       The five concrete zones (left, right, top, bottom, center)
*       plus `none` for the floating / detached state.  Zones are
*       names, not positions: a renderer is free to lay out `top`
*       as a top-anchored strip in one host and as a top-aligned
*       tab row in another.  Zones are mutually compatible across
*       a host - a component may move between any two of them.
*
*   2.  HOW-RESOLVED - the policy axis (DDockPolicy)
*       `fixed` honors the requested zone exactly.  `nearest`
*       resolves to the closest available cardinal side, biased
*       toward an optionally-supplied preference.  `flow` walks a
*       host-defined preference order and accepts the first
*       available zone.  Resolution requires layout information
*       (positions, available sides) and is therefore performed
*       at the host or renderer level - never inside the component
*       itself.  The component carries the policy as a sticky
*       configuration knob; the host reads it during reconcile.
*
*   3.  HOW-PRESENTED - the affordance axis (DDockPresentation)
*       `behavior` exposes dockability as a pure programmatic
*       capability - state changes are driven by API calls and
*       drag-and-drop gestures.  `popup` overlays the same state
*       surface with a button-triggered picker (the cross-shaped
*       widget) that lets the user select a zone visually.  Both
*       presentations operate over identical state - the popup is
*       a UI affordance, not a separate state machine.
*
*   --------------------------------------------------------------------
*   STATE SURFACE
*   --------------------------------------------------------------------
*
*   A dockable component carries five fields by structural
* contract (see component_mixin::dockable_data):
*
*       current_zone   : DDockZone         ; written by the host
*       target_zone    : DDockZone         ; written by the API verbs
*       policy         : DDockPolicy       ; sticky configuration
*       presentation   : DDockPresentation ; sticky configuration
*       dock_requested : bool              ; edge trigger, raised by
*                                          ; the verbs, lowered by
*                                          ; the host on confirm
*
*   Components that opt into popup presentation additionally carry:
*
*       popup_open : bool ; renderer-driven picker visibility flag
*
*   The five-field surface is the minimum bar.  is_dockable_v<T>
* evaluates to true on any type structurally exposing all five core
* fields, regardless of inheritance, namespace, or origin.  This
* matches the framework's preference for structural conformance
* over tagged-base machinery.
*
*   --------------------------------------------------------------------
*   STATE MACHINE
*   --------------------------------------------------------------------
*
*   The component half of the protocol is a single edge-triggered
* request register:
*
*                        dock(c, z)
*           IDLE  -------------------------->  PENDING
*           ^                                     |
*           |                                     | confirm_dock(c)
*           |                                     V
*           +------------------------------------ +
*
*   - IDLE:    dock_requested == false; current_zone == target_zone.
*   - PENDING: dock_requested == true;  target_zone is the requested
*              destination (possibly different from current_zone).
*
*   API verbs (dock, dock_to_nearest, dock_to_center, undock) move
* the component from IDLE into PENDING by writing target_zone and
* raising dock_requested.  Hosts call confirm_dock during
* reconcile, which writes target_zone into current_zone and lowers
* dock_requested, returning to IDLE.  No other transitions exist.
*
*   The popup is a parallel boolean register, independent of the
* dock state machine.  open_dock_popup raises popup_open;
* close_dock_popup lowers it.  Hosts observe popup_open during
* reconcile to drive their picker_open / picker_owner fields.
*
*   --------------------------------------------------------------------
*   HOST CONTRACT
*   --------------------------------------------------------------------
*
*   The host accepts dockables through dock_handle - a 16-byte
* type-erased view that carries a per-type vtable of trampolines
* delegating back into the public free-function verbs.  This means
* the host is decoupled from any concrete dockable type: it sees
* only the contract surface, never the underlying class.
*
*   The host's per-tick obligations are:
*
*       1.  Resolve any pending policy-driven requests by rewriting
*           target_zone into a concrete supported zone using its
*           own layout information.
*       2.  For each handle whose dock_requested is true, call
*           confirm() on the handle.  This is the single
*           reconciliation handshake.
*       3.  Scan handles for popup_open == true and update its own
*           picker_open + picker_owner fields accordingly.
*
*   See dock_host.hpp and dock_host_common.hpp for the concrete
* generic host.  Renderers may build hosts of their own - the
* contract is that they consume dock_handle and respect the
* request/confirm handshake.
*
*   --------------------------------------------------------------------
*   ADAPTER PATHWAY
*   --------------------------------------------------------------------
*
*   Components that did not declare the dockable surface in their
* original definition can be lifted into the subsystem with no
* source modification via dockable_adapter.  The adapter inherits
* from the inner type and from the dockable mixins, preserving the
* inner's full structural surface (every existing free function
* still matches) while adding the dockable contract.
*
*       auto p = make_dockable<my_existing_panel>(args...);
*       dock(p, DDockZone::left);
*       set_value(p, ...);          // still works on inner's value
*
*   --------------------------------------------------------------------
*   INVARIANTS
*   --------------------------------------------------------------------
*
*   I1. After a successful confirm_dock, current_zone == target_zone
*       and dock_requested == false.
*   I2. dock_requested is monotone within a tick: it is raised only
*       by API verbs and lowered only by confirm_dock.  Renderers
*       never write to it.
*   I3. current_zone is mutated only by the host (via confirm_dock).
*       API verbs never touch current_zone directly.
*   I4. presentation and policy are sticky: they persist across
*       dock requests.  Switching them does not raise
*       dock_requested.
*   I5. popup_open is independent of the dock request register.
*       Opening or closing the picker does not raise
*       dock_requested.
*   I6. A dockable's outliveness is the user's responsibility:
*       dock_handle is non-owning and aliasing a destroyed
*       component through it is undefined behavior.  Hosts provide
*       unregister_dockable to break the link before destruction.
*
*   --------------------------------------------------------------------
*   LAYERING
*   --------------------------------------------------------------------
*
*   The subsystem is organized as a directed acyclic include graph:
*
*       dockable_types         (vocabulary; depends on uxoxo only)
*           |
*           |- dockable_mixin       (EBO data, depends on types)
*           |- dockable_traits      (SFINAE, depends on types)
*           |       |
*           |       |- dockable_concepts  (depends on traits +
*           |       |                      component_traits)
*           |       |- dockable_common    (verbs, depends on
*           |       |                      traits + types)
*           |       |- dockable_adapter   (depends on mixin +
*           |       |                      traits + types)
*           |       |
*           |       \- dock_handle        (depends on common +
*           |                              traits + types)
*           |
*           \- dock_host_types     (host vocabulary, depends on
*                                   types)
*                   |
*                   \- dock_host           (depends on handle +
*                                           host_types +
*                                           template_component)
*                           |
*                           \- dock_host_common (depends on host +
*                                                handle +
*                                                host_types +
*                                                traits + types)
*
*   Including this file pulls the entire subsystem in dependency
* order.  Downstream code that only needs a subset (e.g. just the
* verbs and types, no host) is free to include the subset headers
* directly.
*
*
* path:      /inc/uxoxo/templates/component/dockable/dockable.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCKABLE_
#define  UXOXO_DOCKABLE_ 1

// ---- vocabulary -----------------------------------------------------------
#include "./dockable_types.hpp"

// ---- per-component layer --------------------------------------------------
#include "./dockable_mixin.hpp"
#include "./dockable_traits.hpp"
#include "./dockable_concepts.hpp"
#include "./dockable_common.hpp"
#include "./dockable_adapter.hpp"

// ---- host layer -----------------------------------------------------------
#include "./dock_handle.hpp"
#include "./dock_host_types.hpp"
#include "./dock_host.hpp"
#include "./dock_host_common.hpp"


#endif  // UXOXO_DOCKABLE_
