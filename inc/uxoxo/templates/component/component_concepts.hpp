/*******************************************************************************
* uxoxo [component]                                      component_concepts.hpp
*
* Shared component concepts layered over component_traits.hpp:
*   This header provides readable C++20 concepts for the common component
* capability surface detected by component_traits.hpp. It does not replace
* the existing SFINAE traits; it simply wraps them in concept form.
*
*   The concepts mirror the public classification axes from
* component_traits.hpp:
*     1.  Common state members
*     2.  Callback and capability flags
*     3.  Mixin-style capabilities
*     4.  Composite component classifications
*     5.  Aggregate profile concepts
*
*
* path:      /inc/uxoxo/templates/component/component_concepts.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.15
*******************************************************************************/

#ifndef UXOXO_COMPONENT_CONCEPTS_
#define UXOXO_COMPONENT_CONCEPTS_ 1

#include "../../uxoxo.hpp"
#include "./component_traits.hpp"


NS_UXOXO
NS_COMPONENT

// ===============================================================================
//  1.  COMMON STATE MEMBER CONCEPTS
// ===============================================================================

// value_component_type
//   concept: the type exposes a value member.
template <typename _Type>
concept value_component_type = has_value_v<_Type>;

// enabled_component_type
//   concept: the type exposes an enabled member.
template <typename _Type>
concept enabled_component_type = has_enabled_v<_Type>;

// visible_component_type
//   concept: the type exposes a visible member.
template <typename _Type>
concept visible_component_type = has_visible_v<_Type>;

// read_only_component_type
//   concept: the type exposes a read_only member.
template <typename _Type>
concept read_only_component_type = has_read_only_v<_Type>;

// active_component_type
//   concept: the type exposes an active member.
template <typename _Type>
concept active_component_type = has_active_v<_Type>;


// ===============================================================================
//  2.  CALLBACK / CAPABILITY FLAG CONCEPTS
// ===============================================================================

// commit_callback_component_type
//   concept: the type exposes an on_commit member.
template <typename _Type>
concept commit_callback_component_type = has_on_commit_v<_Type>;

// change_callback_component_type
//   concept: the type exposes an on_change member.
template <typename _Type>
concept change_callback_component_type = has_on_change_v<_Type>;

// focusable_flag_component_type
//   concept: the type declares a focusable flag.
template <typename _Type>
concept focusable_flag_component_type = has_focusable_v<_Type>;

// scrollable_flag_component_type
//   concept: the type declares a scrollable flag.
template <typename _Type>
concept scrollable_flag_component_type = has_scrollable_v<_Type>;

// features_field_component_type
//   concept: the type declares a features field.
template <typename _Type>
concept features_field_component_type = has_features_v<_Type>;

// focusable_component_type
//   concept: the type is classified as focusable.
template <typename _Type>
concept focusable_component_type = is_focusable_v<_Type>;

// scrollable_component_type
//   concept: the type is classified as scrollable.
template <typename _Type>
concept scrollable_component_type = is_scrollable_v<_Type>;


// ===============================================================================
//  3.  MIXIN-STYLE CAPABILITY CONCEPTS
// ===============================================================================

// labeled_component_type
//   concept: the type exposes a label member.
template <typename _Type>
concept labeled_component_type = has_label_v<_Type>;

// defaultable_component_type
//   concept: the type exposes a default_value member.
template <typename _Type>
concept defaultable_component_type = has_default_value_v<_Type>;

// previous_value_component_type
//   concept: the type exposes a previous_value member.
template <typename _Type>
concept previous_value_component_type = has_previous_value_v<_Type>;

// copy_request_component_type
//   concept: the type exposes a copy_requested member.
template <typename _Type>
concept copy_request_component_type = has_copy_requested_v<_Type>;


// ===============================================================================
//  4.  COMPOSITE COMPONENT CLASSIFICATION CONCEPTS
// ===============================================================================

// component_type
//   concept: the type satisfies the minimum shared component protocol.
template <typename _Type>
concept component_type = is_component_v<_Type>;

// input_like_component_type
//   concept: the type matches the shared input-like profile.
template <typename _Type>
concept input_like_component_type = is_input_like_v<_Type>;

// output_like_component_type
//   concept: the type matches the shared output-like profile.
template <typename _Type>
concept output_like_component_type = is_output_like_v<_Type>;

// clearable_component_type
//   concept: the type is a component with default-value support.
template <typename _Type>
concept clearable_component_type = is_clearable_v<_Type>;

// undoable_component_type
//   concept: the type is a component with undo-state support.
template <typename _Type>
concept undoable_component_type = is_undoable_v<_Type>;

// copyable_component_type
//   concept: the type is a component with copy-request support.
template <typename _Type>
concept copyable_component_type = is_copyable_v<_Type>;


// ===============================================================================
//  5.  AGGREGATE PROFILE CONCEPTS
// ===============================================================================

// interactive_component_type
//   concept: a component with value, enabled state, and change or commit hooks.
template <typename _Type>
concept interactive_component_type =
    ( component_type<_Type>         &&
      value_component_type<_Type>   &&
      enabled_component_type<_Type> &&
      ( commit_callback_component_type<_Type> ||
        change_callback_component_type<_Type> ) );

// form_component_type
//   concept: a focusable, input-like component suited to form entry.
template <typename _Type>
concept form_component_type = 
    ( input_like_component_type<_Type> &&
      focusable_component_type<_Type> );

// presentation_component_type
//   concept: an output-like component that is visible and change-aware.
template <typename _Type>
concept presentation_component_type = 
    ( output_like_component_type<_Type> &&
      visible_component_type<_Type> );

// rich_component_type
//   concept: a component with labeling plus at least one secondary capability.
template <typename _Type>
concept rich_component_type = 
    ( component_type<_Type>         &&
     labeled_component_type<_Type>  &&
     ( clearable_component_type<_Type> ||
       undoable_component_type<_Type>  ||
       copyable_component_type<_Type> ) );

// stateful_component_type
//   concept: a component with both current value and active or visibility state.
template <typename _Type>
concept stateful_component_type = 
    ( component_type<_Type>        &&
      value_component_type<_Type>  &&
      ( active_component_type<_Type> ||
        visible_component_type<_Type> ) )


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_CONCEPTS_