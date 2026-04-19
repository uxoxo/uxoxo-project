/*******************************************************************************
* uxoxo [component]                                        component_traits.hpp
*
* Shared component trait detectors and classification:
*   Previously, every component header defined its own trait namespace with
* identical SFINAE detectors for shared members (enabled, visible, value,
* label, default_value, etc.).  This header centralizes those detectors
* so they are written once and reused everywhere.
*
*   Component-specific detectors (has_cursor_member, has_lines_member,
* has_suggestions_member, etc.) remain in their respective per-component
* trait headers.  This header provides the common vocabulary that all
* components share.
*
*   The classification struct `component_class<T>` aggregates all common
* detections into a single query point, analogous to container_class<T>
* in the container module.
*
* Contents:
*   1.  Common member detectors
*   2.  Capability flag detectors
*   3.  Mixin detectors (label, clearable, undo, copyable)
*   4.  Composite traits (is_input_like, is_output_like)
*   5.  component_class<T>
*
*
* path:      /inc/uxoxo/templates/component/component_traits.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.26
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_TRAITS_
#define  UXOXO_COMPONENT_TRAITS_ 1

// std
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  COMMON MEMBER DETECTORS
// ===============================================================================
//   These detect the recurring state surface that most components
// share: value, enabled, visible, read_only, on_commit, on_change.
//
//   Each detector follows the standard uxoxo SFINAE pattern:
// false_type primary template, void_t specialization.

NS_INTERNAL

    // -- value --------------------------------------------------------
    template <typename, typename = void>
    struct has_value_member : std::false_type {};
    template <typename _Type>
    struct has_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().value)
    >> : std::true_type {};

    // -- enabled ------------------------------------------------------
    template <typename, typename = void>
    struct has_enabled_member : std::false_type {};
    template <typename _Type>
    struct has_enabled_member<_Type, std::void_t<
        decltype(std::declval<_Type>().enabled)
    >> : std::true_type {};

    // -- visible ------------------------------------------------------
    template <typename, typename = void>
    struct has_visible_member : std::false_type {};
    template <typename _Type>
    struct has_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type>().visible)
    >> : std::true_type {};

    // -- read_only ----------------------------------------------------
    template <typename, typename = void>
    struct has_read_only_member : std::false_type {};
    template <typename _Type>
    struct has_read_only_member<_Type, std::void_t<
        decltype(std::declval<_Type>().read_only)
    >> : std::true_type {};

    // -- active -------------------------------------------------------
    template <typename, typename = void>
    struct has_active_member : std::false_type {};
    template <typename _Type>
    struct has_active_member<_Type, std::void_t<
        decltype(std::declval<_Type>().active)
    >> : std::true_type {};

    // -- on_commit ----------------------------------------------------
    template <typename, typename = void>
    struct has_on_commit_member : std::false_type {};
    template <typename _Type>
    struct has_on_commit_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_commit)
    >> : std::true_type {};

    // -- on_change ----------------------------------------------------
    template <typename, typename = void>
    struct has_on_change_member : std::false_type {};
    template <typename _Type>
    struct has_on_change_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_change)
    >> : std::true_type {};

// ===============================================================================
//  2.  CAPABILITY FLAG DETECTORS
// ===============================================================================
//   These detect static constexpr capability flags that components
// declare: focusable, scrollable, features.

    // -- focusable (static constexpr bool) ----------------------------
    template <typename, typename = void>
    struct has_focusable_flag : std::false_type 
    {};

    template <typename _Type>
    struct has_focusable_flag<_Type,
                              std::void_t<decltype(_Type::focusable)>>
        : std::true_type 
    {};

    // -- scrollable (static constexpr bool) ---------------------------
    template <typename, typename = void>
    struct has_scrollable_flag : std::false_type {};
    template <typename _Type>
    struct has_scrollable_flag<_Type, std::void_t<
        decltype(_Type::scrollable)
    >> : std::true_type {};

    // -- features (static constexpr unsigned) -------------------------
    template <typename, typename = void>
    struct has_features_field : std::false_type {};
    template <typename _Type>
    struct has_features_field<_Type, std::void_t<
        decltype(_Type::features)
    >> : std::true_type {};

    // -- focusable value (true vs false) ------------------------------
    //   These detect not just that the flag exists, but that it is
    // true.  Used for classification (inputs are focusable, outputs
    // are not).
    template <typename _Type, bool = has_focusable_flag<_Type>::value>
    struct is_focusable_impl : std::false_type {};
    template <typename _Type>
    struct is_focusable_impl<_Type, true>
        : std::bool_constant<_Type::focusable> {};

    template <typename _Type, bool = has_scrollable_flag<_Type>::value>
    struct is_scrollable_impl : std::false_type {};
    template <typename _Type>
    struct is_scrollable_impl<_Type, true>
        : std::bool_constant<_Type::scrollable> {};




// ===============================================================================
//  3,  MIXIN DETECTORS
// ===============================================================================
//   These detect the data members injected by the EBO mixins in
// component_mixin.hpp: label, default_value, previous_value,
// copy_requested.

    // -- label --------------------------------------------------------
    template <typename, typename = void>
    struct has_label_member : std::false_type {};
    template <typename _Type>
    struct has_label_member<_Type, std::void_t<
        decltype(std::declval<_Type>().label)
    >> : std::true_type {};

    // -- default_value ------------------------------------------------
    template <typename, typename = void>
    struct has_default_value_member : std::false_type {};
    template <typename _Type>
    struct has_default_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().default_value)
    >> : std::true_type {};

    // -- previous_value -----------------------------------------------
    template <typename, typename = void>
    struct has_previous_value_member : std::false_type {};
    template <typename _Type>
    struct has_previous_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().previous_value)
    >> : std::true_type {};

    // -- has_previous (flag for undo state) ---------------------------
    template <typename, typename = void>
    struct has_has_previous_member : std::false_type {};
    template <typename _Type>
    struct has_has_previous_member<_Type, std::void_t<
        decltype(std::declval<_Type>().has_previous)
    >> : std::true_type {};

    // -- copy_requested -----------------------------------------------
    template <typename, typename = void>
    struct has_copy_requested_member : std::false_type {};
    template <typename _Type>
    struct has_copy_requested_member<_Type, std::void_t<
        decltype(std::declval<_Type>().copy_requested)
    >> : std::true_type {};

}   // NS_INTERNAL




// ===============================================================================
//  VALUE ALIASES
// ===============================================================================
//   Convenience `_v` aliases for all detectors.

// -- core members ---------------------------------------------------------
template <typename _Type>
inline constexpr bool has_value_v =
    internal::has_value_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_enabled_v =
    internal::has_enabled_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_visible_v =
    internal::has_visible_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_read_only_v =
    internal::has_read_only_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_active_v =
    internal::has_active_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_on_commit_v =
    internal::has_on_commit_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_on_change_v =
    internal::has_on_change_member<_Type>::value;

// -- capability flags -----------------------------------------------------
template <typename _Type>
inline constexpr bool has_focusable_v =
    internal::has_focusable_flag<_Type>::value;
template <typename _Type>
inline constexpr bool has_scrollable_v =
    internal::has_scrollable_flag<_Type>::value;
template <typename _Type>
inline constexpr bool has_features_v =
    internal::has_features_field<_Type>::value;
template <typename _Type>
inline constexpr bool is_focusable_v =
    internal::is_focusable_impl<_Type>::value;
template <typename _Type>
inline constexpr bool is_scrollable_v =
    internal::is_scrollable_impl<_Type>::value;

// -- mixin members --------------------------------------------------------
template <typename _Type>
inline constexpr bool has_label_v =
    internal::has_label_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_default_value_v =
    internal::has_default_value_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_previous_value_v =
    internal::has_previous_value_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_copy_requested_v =
    internal::has_copy_requested_member<_Type>::value;




// ===============================================================================
//  4.  COMPOSITE TRAITS
// ===============================================================================

// is_component
//   trait: minimal component detection — has at least one of the
// core state members (enabled or visible) plus a value or features
// field.  This is deliberately loose: the point is to detect
// "something that looks like a uxoxo component" without requiring
// a specific base class.
template <typename _Type>
struct is_component : std::bool_constant<
    ( internal::has_enabled_member<_Type>::value ||
      internal::has_visible_member<_Type>::value )  &&
    ( internal::has_value_member<_Type>::value   ||
      internal::has_features_field<_Type>::value )
>
{};

template <typename _Type>
inline constexpr bool is_component_v =
    is_component<_Type>::value;

// is_input_like
//   trait: has value + enabled + read_only + focusable(true).
// Matches input_control and text_input.
template <typename _Type>
struct is_input_like : std::conjunction<
    internal::has_value_member<_Type>,
    internal::has_enabled_member<_Type>,
    internal::has_read_only_member<_Type>,
    internal::is_focusable_impl<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_input_like_v =
    is_input_like<_Type>::value;

// is_output_like
//   trait: has value + enabled + visible + on_change.
// Matches output_control.
template <typename _Type>
struct is_output_like : std::conjunction<
    internal::has_value_member<_Type>,
    internal::has_enabled_member<_Type>,
    internal::has_visible_member<_Type>,
    internal::has_on_change_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_output_like_v =
    is_output_like<_Type>::value;

// is_labeled
//   trait: is a component with a label.
template <typename _Type>
struct is_labeled : std::conjunction<
    is_component<_Type>,
    internal::has_label_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_labeled_v =
    is_labeled<_Type>::value;

// is_clearable
//   trait: is a component with a default_value.
template <typename _Type>
struct is_clearable : std::conjunction<
    is_component<_Type>,
    internal::has_default_value_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_clearable_v =
    is_clearable<_Type>::value;

// is_undoable
//   trait: is a component with previous_value + has_previous.
template <typename _Type>
struct is_undoable : std::conjunction<
    is_component<_Type>,
    internal::has_previous_value_member<_Type>,
    internal::has_has_previous_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_undoable_v =
    is_undoable<_Type>::value;

// is_copyable
//   trait: is a component with copy_requested.
template <typename _Type>
struct is_copyable : std::conjunction<
    is_component<_Type>,
    internal::has_copy_requested_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_copyable_v =
    is_copyable<_Type>::value;




// ===============================================================================
//  5  COMPONENT CLASS
// ===============================================================================
//   Aggregate classification struct.  Query this instead of
// individual `_v` traits when you need the full picture of a
// component's structural capabilities.
//
//   Usage:
//     using cls = component_class<my_widget>;
//     if constexpr (cls::has_value) { ... }
//     if constexpr (cls::is_input) { ... }

template <typename _Type>
struct component_class
{
    // -- core state ---------------------------------------------------
    static constexpr bool has_value     =
        internal::has_value_member<_Type>::value;
    static constexpr bool has_enabled   =
        internal::has_enabled_member<_Type>::value;
    static constexpr bool has_visible   =
        internal::has_visible_member<_Type>::value;
    static constexpr bool has_read_only =
        internal::has_read_only_member<_Type>::value;
    static constexpr bool has_active    =
        internal::has_active_member<_Type>::value;

    // -- callbacks ----------------------------------------------------
    static constexpr bool has_on_commit =
        internal::has_on_commit_member<_Type>::value;
    static constexpr bool has_on_change =
        internal::has_on_change_member<_Type>::value;

    // -- capability flags ---------------------------------------------
    static constexpr bool has_focusable  =
        internal::has_focusable_flag<_Type>::value;
    static constexpr bool has_scrollable =
        internal::has_scrollable_flag<_Type>::value;
    static constexpr bool has_features   =
        internal::has_features_field<_Type>::value;
    static constexpr bool focusable      =
        internal::is_focusable_impl<_Type>::value;
    static constexpr bool scrollable     =
        internal::is_scrollable_impl<_Type>::value;

    // -- mixin capabilities -------------------------------------------
    static constexpr bool has_label          =
        internal::has_label_member<_Type>::value;
    static constexpr bool has_default_value  =
        internal::has_default_value_member<_Type>::value;
    static constexpr bool has_previous_value =
        internal::has_previous_value_member<_Type>::value;
    static constexpr bool has_copy_requested =
        internal::has_copy_requested_member<_Type>::value;

    // -- classifications ----------------------------------------------
    static constexpr bool is_component = is_component_v<_Type>;
    static constexpr bool is_input     = is_input_like_v<_Type>;
    static constexpr bool is_output    = is_output_like_v<_Type>;
    static constexpr bool is_labeled   = is_labeled_v<_Type>;
    static constexpr bool is_clearable = is_clearable_v<_Type>;
    static constexpr bool is_undoable  = is_undoable_v<_Type>;
    static constexpr bool is_copyable  = is_copyable_v<_Type>;
};


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_TRAITS_