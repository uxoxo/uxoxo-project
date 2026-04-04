/*******************************************************************************
* uxoxo [component]                                             menu_traits.hpp
*
*   Tagless SFINAE detection for menu-like types.  This header answers the
* question "does type T look like a menu?" without requiring T to inherit
* from anything, tag itself, or include this header.  Any user type that
* happens to have the right surface - iterable, has value_type, etc. - will
* be recognised.
*
*   A `std::vector<std::string>` satisfies `is_menu`.  So does a hand-rolled
* fixed-size array with begin()/end().  The detection is structural, never
* nominal.
*
*   Structure:
*     1.  Iterability detection (self-contained - no external deps)
*     2.  Menu identity (is_menu, is_sized_menu)
*     3.  Item-level capability detection (label, enabled, checked, action, ...)
*     4.  Separator support (5 detection strategies + priority resolver)
*     5.  Shortcut / accelerator support (multiple strategies + resolver)
*     6.  Submenu / hierarchy support
*     7.  Convenience _v aliases
*
*
* file:      /inc/uxoxo/component/menu/menu_bar.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.31
*******************************************************************************/

#ifndef UXOXO_COMPONENT_MENU_TRAITS_
#define UXOXO_COMPONENT_MENU_TRAITS_ 1

#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>

//#include <djinterp>


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  ITERABILITY DETECTION
// ===============================================================================
//   Self-contained - does not depend on container/iterator_traits.hpp.  We
// detect begin()/end() via both member and ADL forms.

namespace detail
{
    // has_begin_end
    //   type trait: T has usable begin() and end() (member or ADL).
    template <typename,
              typename = void>
    struct has_begin_end : std::false_type {};

    template <typename _Type>
    struct has_begin_end<_Type, std::void_t<
        decltype(std::begin(std::declval<_Type&>())),
        decltype(std::end(std::declval<_Type&>()))
    >> : std::true_type {};

    // has_value_type
    //   type trait: T exposes a public value_type alias.
    template <typename,
              typename = void>
    struct has_value_type : std::false_type {};

    template <typename _Type>
    struct has_value_type<_Type, std::void_t<
        typename _Type::value_type
    >> : std::true_type {};

    // has_size
    //   type trait: T has a .size() method.
    template <typename,
              typename = void>
    struct has_size : std::false_type {};

    template <typename _Type>
    struct has_size<_Type, std::void_t<
        decltype(std::declval<const _Type>().size())
    >> : std::true_type {};

}   // namespace detail


// ===============================================================================
//  2.  MENU IDENTITY
// ===============================================================================

// is_menu
//   type trait: the absolute minimum requirement for a menu - a publicly
// accessible value_type and iterability (begin/end).
//   This deliberately casts a wide net: std::vector<std::string> satisfies it.
// Subsequent capability traits narrow what the menu supports.
template <typename _Type>
struct is_menu : std::conjunction<
    detail::has_value_type<_Type>,
    detail::has_begin_end<_Type>
> {};

// is_sized_menu
//   type trait: a menu that also knows its own size.
template <typename _Type>
struct is_sized_menu : std::conjunction<
    is_menu<_Type>,
    detail::has_size<_Type>
> {};


// ===============================================================================
//  3.  ITEM-LEVEL CAPABILITY DETECTION
// ===============================================================================
//   These probe the menu's value_type to discover per-item capabilities.
//   Guards against value_type not existing via nested enable_if in 7.

namespace detail
{
    // -- label / text -----------------------------------------------------

    // has_label_member
    //   type trait: value_type has a .label member.
    template <typename,
              typename = void>
    struct has_label_member : std::false_type {};

    template <typename _Type>
    struct has_label_member<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().label)
    >> : std::true_type {};

    // has_text_member
    //   type trait: value_type has a .text member.
    template <typename,
              typename = void>
    struct has_text_member : std::false_type {};

    template <typename _Type>
    struct has_text_member<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().text)
    >> : std::true_type {};

    // has_name_member
    //   type trait: value_type has a .name member.
    template <typename,
              typename = void>
    struct has_name_member : std::false_type {};

    template <typename _Type>
    struct has_name_member<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().name)
    >> : std::true_type {};

    // -- enabled / visible / checked --------------------------------------

    // has_enabled_member (on value_type)
    template <typename,
              typename = void>
    struct has_item_enabled : std::false_type {};

    template <typename _Type>
    struct has_item_enabled<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().enabled)
    >> : std::true_type {};

    // has_visible_member (on value_type)
    template <typename,
              typename = void>
    struct has_item_visible : std::false_type {};

    template <typename _Type>
    struct has_item_visible<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().visible)
    >> : std::true_type {};

    // has_checked_member (on value_type) - checkmark menus
    template <typename,
              typename = void>
    struct has_item_checked : std::false_type {};

    template <typename _Type>
    struct has_item_checked<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().checked)
    >> : std::true_type {};

    // -- action / callback ------------------------------------------------

    // has_action_member (on value_type)
    template <typename,
              typename = void>
    struct has_item_action : std::false_type {};

    template <typename _Type>
    struct has_item_action<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().action)
    >> : std::true_type {};

    // has_on_select_method (on value_type)
    template <typename,
              typename = void>
    struct has_item_on_select : std::false_type {};

    template <typename _Type>
    struct has_item_on_select<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().on_select())
    >> : std::true_type {};

    // -- icon (on value_type) ---------------------------------------------

    template <typename,
              typename = void>
    struct has_item_icon : std::false_type {};

    template <typename _Type>
    struct has_item_icon<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().icon)
    >> : std::true_type {};

}   // namespace detail

// items_have_labels
//   type trait: the menu's items carry a display string via .label, .text,
// or .name.  If none of these exist, the items are their own display type
// (e.g. std::string).
template <typename _Type>
struct items_have_labels : std::disjunction<
    detail::has_label_member<_Type>,
    detail::has_text_member<_Type>,
    detail::has_name_member<_Type>
> {};

// items_are_activatable
//   type trait: items carry an action callback or on_select method.
template <typename _Type>
struct items_are_activatable : std::disjunction<
    detail::has_item_action<_Type>,
    detail::has_item_on_select<_Type>
> {};


// ===============================================================================
//  4.  SEPARATOR SUPPORT
// ===============================================================================
//   Menus may support separators in many ways.  We detect five distinct
// mechanisms and expose a single `supports_separators` disjunction.

namespace detail
{
    // has_separator_type_alias
    //   type trait: T has a public `separator_type` type alias.
    template <typename _Type,
              typename = void>
    struct has_separator_type_alias : std::false_type 
	{};

    template <typename _Type>
    struct has_separator_type_alias<_Type, std::void_t<
        typename _Type::separator_type
    >> : std::true_type 
	{};

    // has_is_separator_method
    //   type trait: T has .is_separator(value_type) --> bool.
    template <typename,
              typename = void>
    struct has_is_separator_method : std::false_type {};

    template <typename _Type>
    struct has_is_separator_method<_Type, std::void_t<
        decltype(std::declval<const _Type>().is_separator(
            std::declval<typename _Type::value_type>()))
    >> : std::true_type {};

    // has_separator_flag
    //   type trait: value_type has a .is_separator member.
    template <typename,
              typename = void>
    struct has_separator_flag : std::false_type {};

    template <typename _Type>
    struct has_separator_flag<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().is_separator)
    >> : std::true_type {};

    // has_add_separator_method
    //   type trait: T has .add_separator().
    template <typename,
              typename = void>
    struct has_add_separator_method : std::false_type {};

    template <typename _Type>
    struct has_add_separator_method<_Type, std::void_t<
        decltype(std::declval<_Type>().add_separator())
    >> : std::true_type {};

    // has_separator_constant
    //   type trait: T has a static SEPARATOR constant.
    template <typename,
              typename = void>
    struct has_separator_constant : std::false_type {};

    template <typename _Type>
    struct has_separator_constant<_Type, std::void_t<
        decltype(_Type::SEPARATOR)
    >> : std::true_type {};

}   // namespace detail

// supports_separators
//   type trait: T supports separators via any of the five mechanisms.
template <typename _Type>
struct supports_separators : std::disjunction<
    detail::has_separator_type_alias<_Type>,
    detail::has_is_separator_method<_Type>,
    detail::has_separator_flag<_Type>,
    detail::has_add_separator_method<_Type>,
    detail::has_separator_constant<_Type>
> {};

// separator_indicator
//   Priority-ordered type resolver: determines which separator mechanism a
// type uses and exposes the indicator type + a constexpr descriptor string.
// Specialisations are ordered by priority (most specific first).

template <typename,
          typename = void>
struct separator_indicator
{
    using type = void;
    static constexpr auto get_indicator() { return "No separator support"; }
};

template <typename _Type>
struct separator_indicator<_Type, std::enable_if_t<
    detail::has_separator_type_alias<_Type>::value
>> {
    using type = typename _Type::separator_type;
    static constexpr auto get_indicator() { return "separator_type alias"; }
};

template <typename _Type>
struct separator_indicator<_Type, std::enable_if_t<
    !detail::has_separator_type_alias<_Type>::value &&
     detail::has_is_separator_method<_Type>::value
>> {
    using type = bool;
    static constexpr auto get_indicator() { return "is_separator() method"; }
};

template <typename _Type>
struct separator_indicator<_Type, std::enable_if_t<
    !detail::has_separator_type_alias<_Type>::value &&
    !detail::has_is_separator_method<_Type>::value &&
     detail::has_separator_flag<_Type>::value
>> {
    using type = decltype(std::declval<typename _Type::value_type>().is_separator);
    static constexpr auto get_indicator() { return "is_separator flag"; }
};

template <typename _Type>
struct separator_indicator<_Type, std::enable_if_t<
    !detail::has_separator_type_alias<_Type>::value &&
    !detail::has_is_separator_method<_Type>::value &&
    !detail::has_separator_flag<_Type>::value &&
     detail::has_add_separator_method<_Type>::value
>> {
    using type = decltype(std::declval<_Type>().add_separator());
    static constexpr auto get_indicator() { return "add_separator() method"; }
};

template <typename _Type>
struct separator_indicator<_Type, std::enable_if_t<
    !detail::has_separator_type_alias<_Type>::value &&
    !detail::has_is_separator_method<_Type>::value &&
    !detail::has_separator_flag<_Type>::value &&
    !detail::has_add_separator_method<_Type>::value &&
     detail::has_separator_constant<_Type>::value
>> {
    using type = decltype(_Type::SEPARATOR);
    static constexpr auto get_indicator() { return "SEPARATOR constant"; }
};


// ===============================================================================
//  5.  SHORTCUT / ACCELERATOR SUPPORT
// ===============================================================================
//   Detects how a menu's items express keyboard shortcuts.

namespace detail
{
    // -- on the menu itself -----------------------------------------------

    // has_shortcut_type
    //   type trait: T has a public shortcut_type alias.
    template <typename,
              typename = void>
    struct has_shortcut_type : std::false_type {};

    template <typename _Type>
    struct has_shortcut_type<_Type, std::void_t<
        typename _Type::shortcut_type
    >> : std::true_type {};

    // has_set_shortcut_method
    //   type trait: T has .set_shortcut(value_type&, const char*).
    template <typename,
              typename = void>
    struct has_set_shortcut_method : std::false_type {};

    template <typename _Type>
    struct has_set_shortcut_method<_Type, std::void_t<
        decltype(std::declval<_Type>().set_shortcut(
            std::declval<typename _Type::value_type&>(),
            std::declval<const char*>()))
    >> : std::true_type {};

    // has_find_by_shortcut
    //   type trait: T has .find_by_shortcut(const char*).
    template <typename,
              typename = void>
    struct has_find_by_shortcut : std::false_type {};

    template <typename _Type>
    struct has_find_by_shortcut<_Type, std::void_t<
        decltype(std::declval<const _Type>().find_by_shortcut(
            std::declval<const char*>()))
    >> : std::true_type {};

    // -- on the item (value_type) -----------------------------------------

    // has_shortcut_member
    //   type trait: value_type has a .shortcut member.
    template <typename,
              typename = void>
    struct has_shortcut_member : std::false_type {};

    template <typename _Type>
    struct has_shortcut_member<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().shortcut)
    >> : std::true_type {};

    // has_get_shortcut_method
    //   type trait: value_type has .get_shortcut().
    template <typename,
              typename = void>
    struct has_get_shortcut_method : std::false_type {};

    template <typename _Type>
    struct has_get_shortcut_method<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().get_shortcut())
    >> : std::true_type {};

    // has_key_member
    //   type trait: value_type has a .key member.
    template <typename,
              typename = void>
    struct has_key_member : std::false_type {};

    template <typename _Type>
    struct has_key_member<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().key)
    >> : std::true_type {};

    // has_keycode_member
    //   type trait: value_type has a .keycode member.
    template <typename,
              typename = void>
    struct has_keycode_member : std::false_type {};

    template <typename _Type>
    struct has_keycode_member<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().keycode)
    >> : std::true_type {};

    // has_modifier_keys
    //   type trait: value_type has .ctrl, .alt, .shift members.
    template <typename,
              typename = void>
    struct has_modifier_keys : std::false_type {};

    template <typename _Type>
    struct has_modifier_keys<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().ctrl),
        decltype(std::declval<typename _Type::value_type>().alt),
        decltype(std::declval<typename _Type::value_type>().shift)
    >> : std::true_type {};

    // has_accelerator
    //   type trait: value_type has .accelerator member.
    template <typename,
              typename = void>
    struct has_accelerator : std::false_type {};

    template <typename _Type>
    struct has_accelerator<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().accelerator)
    >> : std::true_type {};

    // has_mnemonic
    //   type trait: value_type has .mnemonic member.
    template <typename,
              typename = void>
    struct has_mnemonic : std::false_type {};

    template <typename _Type>
    struct has_mnemonic<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().mnemonic)
    >> : std::true_type {};

}   // namespace detail

// supports_shortcuts
//   type trait: T or its value_type supports keyboard shortcuts via any
// of the detected mechanisms.
template <typename _Type>
struct supports_shortcuts : std::disjunction<
    detail::has_shortcut_type<_Type>,
    detail::has_shortcut_member<_Type>,
    detail::has_get_shortcut_method<_Type>,
    detail::has_set_shortcut_method<_Type>,
    detail::has_key_member<_Type>,
    detail::has_keycode_member<_Type>,
    detail::has_modifier_keys<_Type>,
    detail::has_find_by_shortcut<_Type>,
    detail::has_accelerator<_Type>,
    detail::has_mnemonic<_Type>
> {};


// ===============================================================================
//  6.  SUBMENU / HIERARCHY SUPPORT
// ===============================================================================

namespace detail
{
    // has_submenu_member
    //   type trait: value_type has a .submenu member (pointer/optional to child menu).
    template <typename,
              typename = void>
    struct has_submenu_member : std::false_type {};

    template <typename _Type>
    struct has_submenu_member<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().submenu)
    >> : std::true_type {};

    // has_children_member (on value_type - items that nest)
    template <typename,
              typename = void>
    struct has_item_children : std::false_type {};

    template <typename _Type>
    struct has_item_children<_Type, std::void_t<
        decltype(std::declval<typename _Type::value_type>().children)
    >> : std::true_type {};

    // has_add_submenu_method
    template <typename,
              typename = void>
    struct has_add_submenu_method : std::false_type {};

    template <typename _Type>
    struct has_add_submenu_method<_Type, std::void_t<
        decltype(std::declval<_Type>().add_submenu(
            std::declval<typename _Type::value_type&>()))
    >> : std::true_type {};

}   // namespace detail

// supports_submenus
//   type trait: T supports nested submenus.
template <typename _Type>
struct supports_submenus : std::disjunction<
    detail::has_submenu_member<_Type>,
    detail::has_item_children<_Type>,
    detail::has_add_submenu_method<_Type>
> {};


// ===============================================================================
//  7.  CONVENIENCE _v ALIASES
// ===============================================================================

// identity
template <typename _T> inline constexpr bool is_menu_v                = is_menu<_T>::value;
template <typename _T> inline constexpr bool is_sized_menu_v          = is_sized_menu<_T>::value;

// item capabilities
template <typename _T> inline constexpr bool items_have_labels_v      = items_have_labels<_T>::value;
template <typename _T> inline constexpr bool items_are_activatable_v  = items_are_activatable<_T>::value;
template <typename _T> inline constexpr bool has_item_enabled_v       = detail::has_item_enabled<_T>::value;
template <typename _T> inline constexpr bool has_item_visible_v       = detail::has_item_visible<_T>::value;
template <typename _T> inline constexpr bool has_item_checked_v       = detail::has_item_checked<_T>::value;
template <typename _T> inline constexpr bool has_item_action_v        = detail::has_item_action<_T>::value;
template <typename _T> inline constexpr bool has_item_icon_v          = detail::has_item_icon<_T>::value;

// separators
template <typename _T> inline constexpr bool supports_separators_v    = supports_separators<_T>::value;

// shortcuts
template <typename _T> inline constexpr bool supports_shortcuts_v     = supports_shortcuts<_T>::value;
template <typename _T> inline constexpr bool has_modifier_keys_v      = detail::has_modifier_keys<_T>::value;
template <typename _T> inline constexpr bool has_mnemonic_v           = detail::has_mnemonic<_T>::value;
template <typename _T> inline constexpr bool has_accelerator_v        = detail::has_accelerator<_T>::value;

// submenus
template <typename _T> inline constexpr bool supports_submenus_v      = supports_submenus<_T>::value;


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MENU_TRAITS_