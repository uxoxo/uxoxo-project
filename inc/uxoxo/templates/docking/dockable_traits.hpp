/*******************************************************************************
* uxoxo [component]                                         dockable_traits.hpp
*
* Dockable trait detectors and classification:
*   SFINAE detectors for the data members exposed by dockable_data and
* dockable_popup_data, plus the composite `is_dockable` predicate that
* aggregates them into a single query point.  Mirrors the structure of
* component_traits.hpp - one detector per member, an `_v` short form
* per detector, and a classification struct for full-picture queries.
*
*   The detectors are deliberately structural: they fire on any type
* exposing the right members, regardless of base class.  This keeps the
* dock subsystem free of any hard dependency on the EBO mixins - a
* component that hand-rolls equivalent fields (or wraps a third-party
* type) is just as dockable as one that inherits from
* component_mixin::dockable_data<true>.
*
* Contents:
*   1.  Member detectors (current_zone, target_zone, policy,
*       presentation, dock_requested, popup_open)
*   2.  Composite traits (is_dockable, has_dock_popup)
*   3.  dockable_class<T>
*
*
* path:      /inc/uxoxo/templates/component/dockable/dockable_traits.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCKABLE_TRAITS_
#define  UXOXO_DOCKABLE_TRAITS_ 1

// std
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./dockable_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  1.  MEMBER DETECTORS
// ===========================================================================
//   Each detector follows the standard uxoxo SFINAE pattern: a
// false_type primary template paired with a void_t-guarded
// specialization that succeeds when the member access is well-formed.

NS_INTERNAL

    // -- current_zone -------------------------------------------------
    template<typename _Type,
             typename = void>
    struct has_current_zone_member : std::false_type
    {};

    template<typename _Type>
    struct has_current_zone_member<_Type, std::void_t<
        decltype(std::declval<_Type>().current_zone)
    >> : std::true_type {};

    // -- target_zone --------------------------------------------------
    template<typename _Type,
             typename = void>
    struct has_target_zone_member : std::false_type
    {};

    template<typename _Type>
    struct has_target_zone_member<_Type, std::void_t<
        decltype(std::declval<_Type>().target_zone)
    >> : std::true_type {};

    // -- policy -------------------------------------------------------
    template<typename _Type,
             typename = void>
    struct has_dock_policy_member : std::false_type
    {};

    template<typename _Type>
    struct has_dock_policy_member<_Type, std::void_t<
        decltype(std::declval<_Type>().policy)
    >> : std::true_type {};

    // -- presentation -------------------------------------------------
    template<typename _Type,
             typename = void>
    struct has_dock_presentation_member : std::false_type
    {};

    template<typename _Type>
    struct has_dock_presentation_member<_Type, std::void_t<
        decltype(std::declval<_Type>().presentation)
    >> : std::true_type {};

    // -- dock_requested -----------------------------------------------
    template<typename _Type,
             typename = void>
    struct has_dock_requested_member : std::false_type
    {};

    template<typename _Type>
    struct has_dock_requested_member<_Type, std::void_t<
        decltype(std::declval<_Type>().dock_requested)
    >> : std::true_type {};

    // -- popup_open ---------------------------------------------------
    template<typename _Type,
             typename = void>
    struct has_popup_open_member : std::false_type
    {};

    template<typename _Type>
    struct has_popup_open_member<_Type, std::void_t<
        decltype(std::declval<_Type>().popup_open)
    >> : std::true_type {};

NS_END  // internal

// -- public _v aliases ----------------------------------------------------
template<typename _Type>
inline constexpr bool has_current_zone_v =
    internal::has_current_zone_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_target_zone_v =
    internal::has_target_zone_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_dock_policy_v =
    internal::has_dock_policy_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_dock_presentation_v =
    internal::has_dock_presentation_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_dock_requested_v =
    internal::has_dock_requested_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_popup_open_v =
    internal::has_popup_open_member<_Type>::value;




// ===========================================================================
//  2.  COMPOSITE TRAITS
// ===========================================================================

// is_dockable
//   trait: structural predicate for "looks like a dockable component".
// Requires the four core fields - current/target zone, policy, and the
// dock_requested edge trigger.  Presentation is excluded from the
// minimum bar because a component may participate in dockability with
// no popup picker at all.
template<typename _Type>
struct is_dockable : std::conjunction<
    internal::has_current_zone_member  <_Type>,
    internal::has_target_zone_member   <_Type>,
    internal::has_dock_policy_member   <_Type>,
    internal::has_dock_requested_member<_Type>
>
{};

template<typename _Type>
inline constexpr bool is_dockable_v =
    is_dockable<_Type>::value;

// has_dock_popup
//   trait: dockable component that additionally carries the popup-open
// flag.  Used to gate the open_dock_popup / close_dock_popup verbs and
// the popup-related concept in dockable_concepts.hpp.
template<typename _Type>
struct has_dock_popup : std::conjunction<
    is_dockable           <_Type>,
    internal::has_popup_open_member<_Type>
>
{};

template<typename _Type>
inline constexpr bool has_dock_popup_v =
    has_dock_popup<_Type>::value;




// ===========================================================================
//  3.  DOCKABLE CLASS
// ===========================================================================
//   Aggregate classification, analogous to component_class<T>.  Query
// this when a single check needs the full structural picture:
//
//     using cls = dockable_class<my_panel>;
//     if constexpr (cls::is_dockable) { ... }
//     if constexpr (cls::has_popup)   { ... }

// dockable_class
//   trait: aggregates every dockable detector into a single struct.
template<typename _Type>
struct dockable_class
{
    // -- raw member presence ------------------------------------------
    static constexpr bool has_current_zone     =
        internal::has_current_zone_member<_Type>::value;
    static constexpr bool has_target_zone      =
        internal::has_target_zone_member<_Type>::value;
    static constexpr bool has_policy           =
        internal::has_dock_policy_member<_Type>::value;
    static constexpr bool has_presentation     =
        internal::has_dock_presentation_member<_Type>::value;
    static constexpr bool has_dock_requested   =
        internal::has_dock_requested_member<_Type>::value;
    static constexpr bool has_popup_open       =
        internal::has_popup_open_member<_Type>::value;

    // -- classifications ----------------------------------------------
    static constexpr bool is_dockable = is_dockable_v<_Type>;
    static constexpr bool has_popup   = has_dock_popup_v<_Type>;
};


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCKABLE_TRAITS_
