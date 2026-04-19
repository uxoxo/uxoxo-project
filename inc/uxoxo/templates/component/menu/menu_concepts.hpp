/*******************************************************************************
* uxoxo [component]                                           menu_concepts.hpp
*
*   C++20 concepts layered over menu_traits.hpp. These concepts provide
* readable constraints for menu-like types without replacing the existing
* structural SFINAE trait surface.
*
*   The concepts mirror the public classification axes from menu_traits.hpp:
*     1.  Menu identity
*     2.  Item-level capabilities
*     3.  Separator support
*     4.  Shortcut / accelerator support
*     5.  Submenu / hierarchy support
*     6.  Aggregate menu profiles
*
*
* file:      /inc/uxoxo/templates/component/menu/menu_concepts.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.15
*******************************************************************************/

#ifndef UXOXO_COMPONENT_MENU_CONCEPTS_
#define UXOXO_COMPONENT_MENU_CONCEPTS_ 1

// uxoxo
#include "../../../uxoxo.hpp"
#include "./menu_traits.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  MENU IDENTITY CONCEPTS
// ===============================================================================

// menu_type
//   concept: the absolute minimum structural menu requirement - public
// value_type plus iterability.
template <typename _Type>
concept menu_type =
    is_menu<_Type>::value;

// sized_menu_type
//   concept: a menu that also exposes size().
template <typename _Type>
concept sized_menu_type =
    is_sized_menu<_Type>::value;


// ===============================================================================
//  2.  ITEM-LEVEL CAPABILITY CONCEPTS
// ===============================================================================

// labelled_menu_type
//   concept: the menu's items expose some display label surface.
template <typename _Type>
concept labelled_menu_type =
    menu_type<_Type> &&
    items_have_labels<_Type>::value;

// activatable_menu_type
//   concept: the menu's items expose an action callback or on_select hook.
template <typename _Type>
concept activatable_menu_type =
    menu_type<_Type> &&
    items_are_activatable<_Type>::value;

// enabled_state_menu_type
//   concept: the menu's items expose an enabled field.
template <typename _Type>
concept enabled_state_menu_type =
    menu_type<_Type> &&
    detail::has_item_enabled<_Type>::value;

// visible_state_menu_type
//   concept: the menu's items expose a visible field.
template <typename _Type>
concept visible_state_menu_type =
    menu_type<_Type> &&
    detail::has_item_visible<_Type>::value;

// checkable_menu_type
//   concept: the menu's items expose a checked field.
template <typename _Type>
concept checkable_menu_type =
    menu_type<_Type> &&
    detail::has_item_checked<_Type>::value;

// icon_menu_type
//   concept: the menu's items expose an icon field.
template <typename _Type>
concept icon_menu_type =
    menu_type<_Type> &&
    detail::has_item_icon<_Type>::value;


// ===============================================================================
//  3.  SEPARATOR SUPPORT CONCEPTS
// ===============================================================================

// separator_capable_menu_type
//   concept: the menu supports separators through any recognised strategy.
template <typename _Type>
concept separator_capable_menu_type =
    menu_type<_Type> &&
    supports_separators<_Type>::value;

// separator_type_menu
//   concept: the menu exposes separator_type.
template <typename _Type>
concept separator_type_menu =
    menu_type<_Type> &&
    detail::has_separator_type_alias<_Type>::value;

// separator_method_menu
//   concept: the menu exposes is_separator(item).
template <typename _Type>
concept separator_method_menu =
    menu_type<_Type> &&
    detail::has_is_separator_method<_Type>::value;

// separator_flag_menu
//   concept: the menu's items expose an is_separator flag.
template <typename _Type>
concept separator_flag_menu =
    menu_type<_Type> &&
    detail::has_separator_flag<_Type>::value;

// add_separator_menu
//   concept: the menu exposes add_separator().
template <typename _Type>
concept add_separator_menu =
    menu_type<_Type> &&
    detail::has_add_separator_method<_Type>::value;


// ===============================================================================
//  4.  SHORTCUT / ACCELERATOR CONCEPTS
// ===============================================================================

// shortcut_capable_menu_type
//   concept: the menu or its items support keyboard shortcuts through any
// recognised strategy.
template <typename _Type>
concept shortcut_capable_menu_type =
    menu_type<_Type> &&
    supports_shortcuts<_Type>::value;

// shortcut_member_menu
//   concept: the menu's items expose a shortcut member.
template <typename _Type>
concept shortcut_member_menu =
    menu_type<_Type> &&
    detail::has_shortcut_member<_Type>::value;

// get_shortcut_menu
//   concept: the menu's items expose get_shortcut().
template <typename _Type>
concept get_shortcut_menu =
    menu_type<_Type> &&
    detail::has_get_shortcut_method<_Type>::value;

// set_shortcut_menu
//   concept: the menu exposes set_shortcut(item, const char*).
template <typename _Type>
concept set_shortcut_menu =
    menu_type<_Type> &&
    detail::has_set_shortcut_method<_Type>::value;

// find_by_shortcut_menu
//   concept: the menu exposes find_by_shortcut(const char*).
template <typename _Type>
concept find_by_shortcut_menu =
    menu_type<_Type> &&
    detail::has_find_by_shortcut<_Type>::value;

// modifier_shortcut_menu
//   concept: the menu's items expose ctrl/alt/shift members.
template <typename _Type>
concept modifier_shortcut_menu =
    menu_type<_Type> &&
    detail::has_modifier_keys<_Type>::value;

// mnemonic_menu_type
//   concept: the menu's items expose a mnemonic member.
template <typename _Type>
concept mnemonic_menu_type =
    menu_type<_Type> &&
    detail::has_mnemonic<_Type>::value;

// accelerator_menu_type
//   concept: the menu's items expose an accelerator member.
template <typename _Type>
concept accelerator_menu_type =
    menu_type<_Type> &&
    detail::has_accelerator<_Type>::value;


// ===============================================================================
//  5.  SUBMENU / HIERARCHY CONCEPTS
// ===============================================================================

// hierarchical_menu_type
//   concept: the menu supports nested submenus.
template <typename _Type>
concept hierarchical_menu_type =
    menu_type<_Type> &&
    supports_submenus<_Type>::value;

// submenu_member_menu
//   concept: the menu's items expose a submenu member.
template <typename _Type>
concept submenu_member_menu =
    menu_type<_Type> &&
    detail::has_submenu_member<_Type>::value;

// child_items_menu
//   concept: the menu's items expose a children member.
template <typename _Type>
concept child_items_menu =
    menu_type<_Type> &&
    detail::has_item_children<_Type>::value;

// add_submenu_menu
//   concept: the menu exposes add_submenu(item).
template <typename _Type>
concept add_submenu_menu =
    menu_type<_Type> &&
    detail::has_add_submenu_method<_Type>::value;


// ===============================================================================
//  6.  AGGREGATE MENU PROFILE CONCEPTS
// ===============================================================================

// basic_action_menu
//   concept: a simple menu with labels and activatable items.
template <typename _Type>
concept basic_action_menu =
    labelled_menu_type<_Type> &&
    activatable_menu_type<_Type>;

// rich_menu_type
//   concept: a fuller menu surface with labels, actions, separators, and
// shortcut support.
template <typename _Type>
concept rich_menu_type =
    labelled_menu_type<_Type>           &&
    activatable_menu_type<_Type>        &&
    separator_capable_menu_type<_Type>  &&
    shortcut_capable_menu_type<_Type>;

// desktop_menu_type
//   concept: a rich hierarchical menu with common desktop-style features.
template <typename _Type>
concept desktop_menu_type =
    rich_menu_type<_Type>          &&
    hierarchical_menu_type<_Type>  &&
    enabled_state_menu_type<_Type>;

// command_palette_menu_type
//   concept: a menu especially suited to command-palette or keyboard-driven
// navigation patterns.
template <typename _Type>
concept command_palette_menu_type =
    labelled_menu_type<_Type>        &&
    activatable_menu_type<_Type>     &&
    shortcut_capable_menu_type<_Type>;


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MENU_CONCEPTS_