/*******************************************************************************
* uxoxo [component]                                          dock_host_types.hpp
*
* Shared dock-host type vocabulary:
*   The compile-time and value-type vocabulary used by dock_host:
*
*     - feature-flag macros (D_DOCK_HOST_LEFT, ..., D_DOCK_HOST_ALL)
*       that select which zones a host instantiation supports;
*     - empty tag types (dock_zone_left_tag, ...) that name each zone
*       at compile time so it can be queried via the existing slot /
*       composite machinery in template_component.hpp;
*     - dock_zone_state, the per-zone runtime metadata stored in each
*       slot's value (split ratio, collapsed flag).
*
*   The host's dynamic content (the vector of dock_handle children)
* lives separately on the host class itself - the slots carry only
* per-zone *layout* state, not per-zone item collections, because the
* items are heterogeneously typed and managed runtime-side via the
* type-erased dock_handle from dock_handle.hpp.
*
* Contents:
*   1.  Feature-flag constants (D_DOCK_HOST_*)
*   2.  Zone tag types
*   3.  dock_zone_state
*   4.  zone <-> tag mapping helpers
*
*
* path:      /inc/uxoxo/templates/component/dockable/dock_host_types.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCK_HOST_TYPES_
#define  UXOXO_DOCK_HOST_TYPES_ 1

// std
#include <cstdint>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./dockable_types.hpp"


// ===========================================================================
//  1.  FEATURE-FLAG CONSTANTS
// ===========================================================================
//   These compose into the unsigned _Feat parameter that selects which
// zones a dock_host instantiation supports.  Bit positions are stable
// and map directly onto DDockZone values for the purposes of the
// runtime-zone-to-bit lookup helper below.

// D_DOCK_HOST_LEFT
//   constant: feature bit enabling the left zone.
#define D_DOCK_HOST_LEFT     (1u << 0)

// D_DOCK_HOST_RIGHT
//   constant: feature bit enabling the right zone.
#define D_DOCK_HOST_RIGHT    (1u << 1)

// D_DOCK_HOST_TOP
//   constant: feature bit enabling the top zone.
#define D_DOCK_HOST_TOP      (1u << 2)

// D_DOCK_HOST_BOTTOM
//   constant: feature bit enabling the bottom zone.
#define D_DOCK_HOST_BOTTOM   (1u << 3)

// D_DOCK_HOST_CENTER
//   constant: feature bit enabling the center zone.
#define D_DOCK_HOST_CENTER   (1u << 4)

// D_DOCK_HOST_CARDINAL
//   constant: combined flags for the four cardinal sides only.
#define D_DOCK_HOST_CARDINAL ( D_DOCK_HOST_LEFT  | \
                               D_DOCK_HOST_RIGHT | \
                               D_DOCK_HOST_TOP   | \
                               D_DOCK_HOST_BOTTOM )

// D_DOCK_HOST_ALL
//   constant: every supported zone enabled.
#define D_DOCK_HOST_ALL      ( D_DOCK_HOST_CARDINAL | \
                               D_DOCK_HOST_CENTER )


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  2.  ZONE TAG TYPES
// ===========================================================================
//   Empty tag types used purely for compile-time slot lookup via
// tc_get<_Tag>() and composite::has_field<_Tag>().  Each tag
// corresponds 1:1 with one DDockZone value and one D_DOCK_HOST_*
// feature bit.

// dock_zone_left_tag
//   struct: compile-time tag identifying the left zone.
struct dock_zone_left_tag
{};

// dock_zone_right_tag
//   struct: compile-time tag identifying the right zone.
struct dock_zone_right_tag
{};

// dock_zone_top_tag
//   struct: compile-time tag identifying the top zone.
struct dock_zone_top_tag
{};

// dock_zone_bottom_tag
//   struct: compile-time tag identifying the bottom zone.
struct dock_zone_bottom_tag
{};

// dock_zone_center_tag
//   struct: compile-time tag identifying the center zone.
struct dock_zone_center_tag
{};




// ===========================================================================
//  3.  DOCK ZONE STATE
// ===========================================================================

// dock_zone_state
//   struct: per-zone runtime metadata held by each enabled host slot.
// Carries layout information that is meaningful regardless of which
// renderer eventually realizes the zone - split ratio relative to the
// host's primary axis, and a collapsed flag for hide-but-preserve.
//
//   Renderers may interpret split_ratio as "fraction of host width"
// for vertical sides, "fraction of host height" for horizontal sides,
// and ignore it entirely for center (which always fills remaining
// area).  Renderers that want additional state (custom titles,
// pinned-tab order, per-zone background color, ...) should compose
// dock_zone_state into a derived struct used as the slot value type
// in their own host instantiation rather than mutate this shared
// definition.
struct dock_zone_state
{
    float split_ratio = 0.25f;
    bool  collapsed   = false;
};




// ===========================================================================
//  4.  ZONE <-> TAG MAPPING HELPERS
// ===========================================================================
//   Compile-time and runtime helpers translating between DDockZone
// values and the equivalent tag / feature-bit / zone-bit triples.
// Callers prefer the runtime form when they have a DDockZone in hand
// (e.g., during reconcile), and the compile-time form when the zone
// is known statically.

// dock_zone_bit
//   function: maps a DDockZone value to the corresponding feature
// bit, or 0u for DDockZone::none.
[[nodiscard]] constexpr unsigned
dock_zone_bit(DDockZone _zone) noexcept
{
    switch (_zone)
    {
        case DDockZone::left:   return D_DOCK_HOST_LEFT;
        case DDockZone::right:  return D_DOCK_HOST_RIGHT;
        case DDockZone::top:    return D_DOCK_HOST_TOP;
        case DDockZone::bottom: return D_DOCK_HOST_BOTTOM;
        case DDockZone::center: return D_DOCK_HOST_CENTER;
        case DDockZone::none:   return 0u;
    }

    return 0u;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCK_HOST_TYPES_
