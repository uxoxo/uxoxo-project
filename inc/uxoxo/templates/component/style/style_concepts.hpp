/*******************************************************************************
* uxoxo [component]                                          style_concepts.hpp
*
* C++20 concepts for UI style detection
*   Concept wrappers over the SFINAE traits in ui_style_traits.hpp. These
* provide constraint-based interfaces for use in requires-clauses and
* template parameter constraints.
*
* Contents:
*   1.  label_control struct
*   2.  label-specific free functions (lb_append)
*
* path:      /inc/uxoxo/templates/component/style/style_concepts.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.04.18
*******************************************************************************/

#ifndef UXOXO_COMPONENT_STYLE_CONCEPTS_
#define UXOXO_COMPONENT_STYLE_CONCEPTS_

// std
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT

// __cpp_concepts == 201507L  // older Concepts TS / experimental concepts
// __cpp_concepts == 201907L  // C++20 concepts
#if defined(D_ENV_CPP_FEATURE_LANG_CONCEPTS) && __cpp_concepts >= 201907L


// =============================================================================
// 1.   PROTOCOL CONCEPTS
// =============================================================================

// style_typed
//   concept: constrains types that expose a `style_type` alias.
template<typename _Type>
concept style_typed =
    has_style_type_v<_Type>;

// style_applicable
//   concept: constrains types with an `apply_style(style_type)` method.
template<typename _Type>
concept style_applicable =
    has_apply_style_v<_Type>;

// style_exportable
//   concept: constrains types with a const `get_style()` accessor.
template<typename _Type>
concept style_exportable =
    has_get_style_v<_Type>;


// =============================================================================
// 2.   PER-PROPERTY CONCEPTS
// =============================================================================

// style_property_readable
//   concept: constrains types that expose a const `get<_Tag>()` accessor
// for the given property tag.
template<typename _Type,
         typename _Tag>
concept style_property_readable =
    has_style_property_v<_Type, _Tag>;

// style_property_writable
//   concept: constrains types that expose a `set<_Tag>(value)` mutator
// for the given property tag.
template<typename _Type,
         typename _Tag>
concept style_property_writable =
    has_mutable_style_property_v<_Type, _Tag>;


// =============================================================================
// 3.   AGGREGATE CONCEPTS
// =============================================================================

// stylable_ui
//   concept: constrains types that are stylable (expose style_type).
template<typename _Type>
concept stylable_ui =
    is_stylable_v<_Type>;

// readable_style_ui
//   concept: constrains types that can export their current style.
template<typename _Type>
concept readable_style_ui =
    is_style_readable_v<_Type>;

// writable_style_ui
//   concept: constrains types that can receive an external style.
template<typename _Type>
concept writable_style_ui =
    is_style_writable_v<_Type>;

// configurable_style_ui
//   concept: constrains types supporting round-trip style
// serialization (both apply_style and get_style).
template<typename _Type>
concept configurable_style_ui =
    is_style_configurable_v<_Type>;


// =============================================================================
// 4.   VARIADIC PROPERTY CONCEPTS
// =============================================================================

// has_all_properties
//   concept: constrains types that expose get<_Tag>() for every tag in
// the parameter pack.
template<typename   _Type,
         typename... _Tags>
concept has_all_properties =
    ( has_style_property_v<_Type, _Tags> && ... );

// has_any_property
//   concept: constrains types that expose get<_Tag>() for at least one
// tag in the parameter pack.
template<typename   _Type,
         typename... _Tags>
concept has_any_property =
    ( has_style_property_v<_Type, _Tags> || ... );

// has_all_mutable_properties
//   concept: constrains types that expose set<_Tag>() for every tag in
// the parameter pack.
template<typename   _Type,
         typename... _Tags>
concept has_all_mutable_properties =
    ( has_mutable_style_property_v<_Type, _Tags> && ... );


#endif  // __cpp_concepts


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_STYLE_CONCEPTS_