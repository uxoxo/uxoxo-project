/*******************************************************************************
* uxoxo [core]                                                  element_menu.hpp
*
* Menu elements: a menu bar, the menus on it, and the items in them.
*   A menu_bar holds menus; with "main" it is the application's bar across
* the top of the viewport, otherwise the bar of the window it is drawn in
* (which must have one). A menu holds items and further menus. An item
* reports an event whose kind is its action, passing any "value" through;
* "shortcut" is drawn beside it, and "checkable" shows "checked".
*
* path:      /inc/uxoxo/element_menu.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_MENU_HPP
#define UXOXO_ELEMENT_MENU_HPP 1

// std
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_menu_bar / archetype_menu / archetype_menu_item
//   constants: the render archetypes for a bar, a menu and an item.
inline constexpr const char* archetype_menu_bar  = "menu_bar";
inline constexpr const char* archetype_menu      = "menu";
inline constexpr const char* archetype_menu_item = "menu_item";


NS_COMPONENT

    // menu_bar_type
    //   the descriptor for the menu bar, owned by this module.
    D_NODISCARD
    inline const element_type* menu_bar_type()
    {
        static const element_type descriptor =
            element_type{
                "menu_bar",
                archetype_menu_bar,
                option_set{ { { "main", ::djinterp::option_value(false) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    // menu_type
    //   the descriptor for one menu, owned by this module.
    D_NODISCARD
    inline const element_type* menu_type()
    {
        static const element_type descriptor =
            element_type{
                "menu",
                archetype_menu,
                option_set{ { { "text",    ::djinterp::option_value(
                                               std::string()) },
                              { "enabled", ::djinterp::option_value(true) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    // menu_item_type
    //   the descriptor for one menu item, owned by this module.
    D_NODISCARD
    inline const element_type* menu_item_type()
    {
        static const element_type descriptor =
            element_type{
                "menu_item",
                archetype_menu_item,
                option_set{ { { "text",      ::djinterp::option_value(
                                                 std::string()) },
                              { "action",    ::djinterp::option_value(
                                                 std::string()) },
                              { "shortcut",  ::djinterp::option_value(
                                                 std::string()) },
                              { "checkable", ::djinterp::option_value(false) },
                              { "checked",   ::djinterp::option_value(false) },
                              { "enabled",   ::djinterp::option_value(
                                                 true) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    menu_bar
      Builds a `menu_bar` template over its menus.

    Parameter(s):
      _menus: the menus, left to right.
      _attrs: optional extra attributes (main), overlaid on top (caller wins).
    Return:
      An element_template wrapping a menu_bar node over _menus.
    */
    D_NODISCARD
    inline element_template menu_bar(
        std::vector<element_template> _menus,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = ::djinterp::overlay(menu_bar_type()->defaults,
                                                  _attrs);

        return make_element(menu_bar_type(), resolved, std::move(_menus));
    }

    /*
    menu
      Builds a `menu` template over its items.

    Parameter(s):
      _text:  the menu's title ("text").
      _items: its items and submenus, top to bottom.
      _attrs: optional extra attributes (enabled), overlaid on top.
    Return:
      An element_template wrapping a menu node over _items.
    */
    D_NODISCARD
    inline element_template menu(
        const std::string&            _text,
        std::vector<element_template> _items,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = menu_type()->defaults;
        resolved.set("text", _text);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(menu_type(), resolved, std::move(_items));
    }

    /*
    menu_item
      Builds a leaf `menu_item` template.

    Parameter(s):
      _text:   the item's label ("text").
      _action: the event kind reported when it is chosen ("action").
      _attrs:  optional extra attributes (shortcut, checkable, checked,
               enabled, value), overlaid on top (caller wins).
    Return:
      An element_template wrapping a single menu_item node.
    */
    D_NODISCARD
    inline element_template menu_item(
        const std::string& _text,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = menu_item_type()->defaults;
        resolved.set("text", _text);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(menu_item_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_MENU_HPP
