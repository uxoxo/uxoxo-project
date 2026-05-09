/*******************************************************************************
* uxoxo [component]                                       dockable_concepts.hpp
*
* Shared dockable concepts layered over dockable_traits.hpp:
*   Readable C++20 concepts that wrap the structural predicates from
* dockable_traits.hpp.  Mirrors the relationship between
* component_concepts.hpp and component_traits.hpp - the concepts add no
* new semantics, they just give downstream templates and free functions
* a clearer constraint vocabulary than `std::enable_if_t<is_X_v<T>>`.
*
*   The concepts mirror the public classification axes from
* dockable_traits.hpp:
*     1.  Member-presence concepts
*     2.  Composite classification concepts
*     3.  Aggregate profile concepts
*
*
* path:      /inc/uxoxo/templates/component/dockable/dockable_concepts.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCKABLE_CONCEPTS_
#define  UXOXO_DOCKABLE_CONCEPTS_ 1

#include "../../../uxoxo.hpp"
#include "../component_traits.hpp"
#include "./dockable_traits.hpp"


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  1.  MEMBER-PRESENCE CONCEPTS
// ===========================================================================

// current_zone_component_type
//   concept: the type exposes a current_zone member.
template<typename _Type>
concept current_zone_component_type = has_current_zone_v<_Type>;

// target_zone_component_type
//   concept: the type exposes a target_zone member.
template<typename _Type>
concept target_zone_component_type = has_target_zone_v<_Type>;

// dock_policy_component_type
//   concept: the type exposes a policy member.
template<typename _Type>
concept dock_policy_component_type = has_dock_policy_v<_Type>;

// dock_presentation_component_type
//   concept: the type exposes a presentation member.
template<typename _Type>
concept dock_presentation_component_type = has_dock_presentation_v<_Type>;

// dock_requested_component_type
//   concept: the type exposes a dock_requested edge-trigger member.
template<typename _Type>
concept dock_requested_component_type = has_dock_requested_v<_Type>;

// dock_popup_open_component_type
//   concept: the type exposes a popup_open member.
template<typename _Type>
concept dock_popup_open_component_type = has_popup_open_v<_Type>;


// ===========================================================================
//  2.  COMPOSITE CLASSIFICATION CONCEPTS
// ===========================================================================

// dockable_component_type
//   concept: the type satisfies the structural dockable contract -
// current zone, target zone, policy, and a dock_requested trigger.
template<typename _Type>
concept dockable_component_type = is_dockable_v<_Type>;

// popup_dockable_component_type
//   concept: a dockable component that also carries the popup-open
// flag.  Required for verbs that open or close the dock picker.
template<typename _Type>
concept popup_dockable_component_type = has_dock_popup_v<_Type>;


// ===========================================================================
//  3.  AGGREGATE PROFILE CONCEPTS
// ===========================================================================

// configurable_dockable_component_type
//   concept: a dockable that exposes its presentation field, allowing
// callers to switch between behavior-only and popup-driven affordances
// at runtime.
template<typename _Type>
concept configurable_dockable_component_type =
    ( dockable_component_type<_Type> &&
      dock_presentation_component_type<_Type> );

// visible_dockable_component_type
//   concept: a dockable that also participates in the visibility
// surface.  Hosts that hide undocked components rely on this to
// reconcile visible/current_zone state in lockstep.
template<typename _Type>
concept visible_dockable_component_type =
    ( dockable_component_type<_Type> &&
      has_visible_v<_Type> );

// interactive_dockable_component_type
//   concept: a dockable, popup-capable component that also has its
// enabled flag exposed.  This is the surface a popup-driven dock
// picker actually needs - it has to gate its own activation on the
// component being enabled.
template<typename _Type>
concept interactive_dockable_component_type =
    ( popup_dockable_component_type<_Type> &&
      has_enabled_v<_Type> );


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCKABLE_CONCEPTS_
