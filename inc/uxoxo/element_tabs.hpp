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
* content of the front page alone.
*
* path:      /inc/uxoxo/element_tabs.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.10.04
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

// archetype_tab_bar / archetype_tab
//   constants: the render archetypes for a bar of pages and for one page.
inline constexpr const char* archetype_tab_bar = "tab_bar";
inline constexpr const char* archetype_tab     = "tab";


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

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_TABS_HPP
