/*******************************************************************************
* uxoxo [core]                                                 element_popup.hpp
*
* The popup element: a menu that opens where the mouse is, over everything.
*   A popup is the context menu an application shows on a right-click: it
* holds menu items, menus and separators, as a menu does, and is drawn in a
* window of its own at the mouse. The application decides when it opens:
* "open" opens it in the frame it is drawn with that attribute set, and the
* application clears the attribute afterwards; it then stays open until an
* item is chosen or a click lands elsewhere, as ImGui's popups do. Elements
* report a right-click with their "context_action", which is how an
* application learns to open one.
*   "id" names the popup. It is drawn, and opened, in one place, so the two
* agree on the identity ImGui keys a popup by.
*
* path:      /inc/uxoxo/element_popup.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.09
*                                                            revised: 2026.10.09
*******************************************************************************/

#ifndef UXOXO_ELEMENT_POPUP_HPP
#define UXOXO_ELEMENT_POPUP_HPP 1

// std
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_popup
//   constant: the render archetype for a popup menu.
inline constexpr const char* archetype_popup = "popup";


NS_COMPONENT

    // popup_type
    //   the descriptor for the popup element, owned by this module.
    // Unbounded arity: the children are the popup's items.
    D_NODISCARD
    inline const element_type* popup_type()
    {
        static const element_type descriptor =
            element_type{
                "popup",
                archetype_popup,
                option_set{ { { "id",   ::djinterp::option_value(
                                            std::string("popup")) },
                              { "open", ::djinterp::option_value(false) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    popup
      Builds a `popup` template over its items.

    Parameter(s):
      _id:    the popup's name ("id"), unique where it is drawn.
      _items: its menu items, menus and separators, top to bottom.
      _attrs: optional extra attributes (open), overlaid on top (caller
              wins).
    Return:
      An element_template wrapping a popup node over _items.
    */
    D_NODISCARD
    inline element_template popup(
        const std::string&            _id,
        std::vector<element_template> _items,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = popup_type()->defaults;
        resolved.set("id", _id);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(popup_type(), resolved, std::move(_items));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_POPUP_HPP
