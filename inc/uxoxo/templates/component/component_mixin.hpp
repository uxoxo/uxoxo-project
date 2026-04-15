/*******************************************************************************
* uxoxo [component]                                        component_mixin.hpp
*
* Unified component mixin registry:
*   Deduplicated EBO mixins shared across all component types.  Previously,
* label_data, clearable_data, and similar mixins were defined independently
* in input_mixin, output_mixin, and other per-component namespaces — often
* identical in structure, differing only in namespace.  This header
* consolidates them into a single namespace (component_mixin) so that
* any component can inherit from the same mixin definitions.
*
*   Every mixin follows the same pattern: a primary template that is an
* empty struct (EBO-eligible), and a `true` specialization that carries
* the actual data.  The primary template takes a bool `_Enable` as its
* first parameter; additional type parameters follow where needed.
*
*   These mixins prescribe no behavior — they are pure data.  Free
* functions in component_common.hpp operate on components that inherit
* these mixins via structural detection.
*
* Contents:
*   1  label_data       — optional string label
*   2  clearable_data   — default value for reset-to-default
*   3  undo_data        — single-level undo (previous value)
*   4  copyable_data    — clipboard copy request flag
*
*
* path:      /inc/uxoxo/component/component_mixin.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.15
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_MIXIN_
#define  UXOXO_COMPONENT_MIXIN_ 1

// std
#include <string>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  COMPONENT MIXIN NAMESPACE
// ===============================================================================
//   All shared EBO mixins live here.  Component structs inherit from
// these directly, e.g.:
//
//     struct my_control
//         : component_mixin::label_data<has_label_flag>
//         , component_mixin::clearable_data<has_clear_flag, _Value>
//     { ... };

namespace component_mixin {

// ===============================================================================
//  1  LABEL
// ===============================================================================
//   An optional human-readable label associated with the component.
// Used by input_control, output_control, and any future labeled
// component.

template <bool _Enable>
struct label_data
{};

template <>
struct label_data<true>
{
    std::string label;
};


/*****************************************************************************/

// ===============================================================================
//  2  CLEARABLE
// ===============================================================================
//   Stores a default value that the component can be reset to.
// Used by input_control (icf_clearable), output_control
// (ocf_clearable), and any future resettable component.

template <bool _Enable, typename _Value = void>
struct clearable_data
{};

template <typename _Value>
struct clearable_data<true, _Value>
{
    _Value default_value {};
};


/*****************************************************************************/

// ===============================================================================
//  3  UNDO
// ===============================================================================
//   Single-level undo: stores the previous value so it can be
// restored.  Used by input_control (icf_undoable).

template <bool _Enable, typename _Value = void>
struct undo_data
{};

template <typename _Value>
struct undo_data<true, _Value>
{
    _Value  previous_value {};
    bool    has_previous = false;
};


/*****************************************************************************/

// ===============================================================================
//  4  COPYABLE
// ===============================================================================
//   Signals the renderer to copy the component's value to the
// clipboard.  Used by output_control (ocf_copyable).

template <bool _Enable>
struct copyable_data
{};

template <>
struct copyable_data<true>
{
    bool copy_requested = false;
};


}   // namespace component_mixin


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MIXIN_
