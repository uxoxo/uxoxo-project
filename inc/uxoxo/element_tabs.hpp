/*******************************************************************************
* uxoxo [core]                                                  element_tabs.hpp
*
* The tabs and tab elements: a bar of titled pages, one shown at a time.
*   tabs holds tab children; each tab holds its page's children. A tab whose
* selected attribute is true is brought to the front on the frame it is seen,
* so an application sets it for one frame to switch pages.
*   A tab with an "action" reports when it comes to the front: on a frame it
* is shown while its "active" attribute is false, it posts its action with
* its "value". An application that marks the page it believes is in front
* "active" therefore hears about every change of page, and can build the
* content of the front page alone. A tab's "click_action" reports every
* click on its title, the front page's too, and its "double_action" a
* double-click, each with the tab's "value".
*   A tab_button is a small button at the bar's right end, after the pages:
* its text -- or an "icon" drawn in lines, one of "minimize", "restore",
* "down", "float", "dock", "pin" and "close" -- a "tooltip", and the
* "action" it reports, with its "value", when it is pressed. "on" shows it
* held down.
*
* path:      /inc/uxoxo/element_tabs.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.10.11
*******************************************************************************/

#ifndef UXOXO_ELEMENT_TABS_HPP
#define UXOXO_ELEMENT_TABS_HPP 1

// std
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_tab_bar / archetype_tab / archetype_tab_button
//   constants: the render archetypes for a bar of pages, for one page, and
// for a button at the bar's end.
inline constexpr const char* archetype_tab_bar    = "tab_bar";
inline constexpr const char* archetype_tab        = "tab";
inline constexpr const char* archetype_tab_button = "tab_button";


NS_COMPONENT

    // tabs_type
    //   the descriptor for the tab bar, owned by this module.
    D_NODISCARD
    inline const element_type* tabs_type()
    {
        static const element_type descriptor =
            element_type{
                "tabs",
                archetype_tab_bar,
                option_set{ { { "id", ::djinterp::option_value(
                                          std::string()) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    // tab_type
    //   the descriptor for one page of a tab bar, owned by this module.
    D_NODISCARD
    inline const element_type* tab_type()
    {
        static const element_type descriptor =
            element_type{
                "tab",
                archetype_tab,
                option_set{ { { "text",     ::djinterp::option_value(
                                                std::string()) },
                              { "selected", ::djinterp::option_value(
                                                false) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    // tab_button_type
    //   the descriptor for a button at a tab bar's end, owned by this module.
    D_NODISCARD
    inline const element_type* tab_button_type()
    {
        static const element_type descriptor =
            element_type{
                "tab_button",
                archetype_tab_button,
                option_set{ { { "text",   ::djinterp::option_value(
                                              std::string()) },
                              { "action", ::djinterp::option_value(
                                              std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    tabs
      Builds a `tabs` template over its pages.

    Parameter(s):
      _id:    a name unique among sibling tab bars ("id").
      _pages: the tab templates, left to right.
      _attrs: optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a tabs node over _pages.
    */
    D_NODISCARD
    inline element_template tabs(
        const std::string&            _id,
        std::vector<element_template> _pages,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = tabs_type()->defaults;
        resolved.set("id", _id);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(tabs_type(), resolved, std::move(_pages));
    }

    /*
    tab
      Builds one `tab` page.

    Parameter(s):
      _title:    the page's title ("text").
      _items:    the page's content, top to bottom.
      _selected: bring this page to the front this frame ("selected").
      _attrs:    optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a tab node over _items.
    */
    D_NODISCARD
    inline element_template tab(
        const std::string&            _title,
        std::vector<element_template> _items,
        bool                          _selected = false,
        option_set                    _attrs    = option_set{}
    )
    {
        option_set resolved = tab_type()->defaults;
        resolved.set("text", _title);
        resolved.set("selected", _selected);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(tab_type(), resolved, std::move(_items));
    }

    /*
    tab_button
      Builds a button at a tab bar's right end.

    Parameter(s):
      _text:   the button's face ("text").
      _action: the event kind it reports when pressed ("action").
      _attrs:  optional extra attributes (value, tooltip), overlaid on top
               (caller wins).
    Return:
      An element_template wrapping a single tab_button node.
    */
    D_NODISCARD
    inline element_template tab_button(
        const std::string& _text,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = tab_button_type()->defaults;
        resolved.set("text", _text);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(tab_button_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_TABS_HPP
