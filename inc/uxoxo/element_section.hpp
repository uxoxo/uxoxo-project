/*******************************************************************************
* uxoxo [core]                                               element_section.hpp
*
* The section element: a titled, collapsible group of children.
*   A header the user opens and closes; the children are drawn while it is
* open. The element does not own the state -- the backend keeps it, keyed by
* the title -- and "open" is only how it starts.
*
* path:      /inc/uxoxo/element_section.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_SECTION_HPP
#define UXOXO_ELEMENT_SECTION_HPP 1

// std
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_section
//   constant: the render archetype for a collapsible titled group.
inline constexpr const char* archetype_section = "section";


NS_COMPONENT

    // section_type
    //   the descriptor for the section element, owned by this module.
    D_NODISCARD
    inline const element_type* section_type()
    {
        static const element_type descriptor =
            element_type{
                "section",
                archetype_section,
                option_set{ { { "text", ::djinterp::option_value(
                                            std::string()) },
                              { "open", ::djinterp::option_value(true) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    section
      Builds a `section` template over its children.

    Parameter(s):
      _text:  the header's title ("text").
      _items: the content, top to bottom.
      _open:  whether it starts open ("open").
      _attrs: optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a section node over _items.
    */
    D_NODISCARD
    inline element_template section(
        const std::string&            _text,
        std::vector<element_template> _items,
        bool                          _open  = true,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = section_type()->defaults;
        resolved.set("text", _text);
        resolved.set("open", _open);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(section_type(), resolved, std::move(_items));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_SECTION_HPP
