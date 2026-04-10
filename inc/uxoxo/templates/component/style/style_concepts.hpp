// [uxoxo] ui_style_concepts.hpp — C++20 concepts for UI style detection
//
//   Concept wrappers over the SFINAE traits in ui_style_traits.hpp. These
// provide constraint-based interfaces for use in requires-clauses and
// template parameter constraints.
//
//   Requires __cpp_concepts >= 201907L (C++20). The entire file is
// feature-gated and produces no definitions on older compilers.
//
// path:      /inc/uxoxo/ui_style_concepts.hpp
// link(s):   TBA
// author(s): teer                                          date: 2025.06.08

#ifndef UXOXO_UI_STYLE_CONCEPTS_HPP
#define UXOXO_UI_STYLE_CONCEPTS_HPP

#include <type_traits>

#include "uxoxo.hpp"
#include "ui_style_traits.hpp"


NS_UXOXO
NS_UI
NS_TRAITS


#if defined(__cpp_concepts) && __cpp_concepts >= 201907L


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


NS_END  // traits
NS_END  // ui
NS_END  // uxoxo


#endif  // UXOXO_UI_STYLE_CONCEPTS_HPP
