/*******************************************************************************
* uxoxo [core]                                             element_separator.hpp
*
* The separator element: a horizontal rule, optionally titled.
*   With text, the rule carries it as a section title.
*
* path:      /inc/uxoxo/element_separator.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.09.22
*******************************************************************************/

#ifndef UXOXO_ELEMENT_SEPARATOR_HPP
#define UXOXO_ELEMENT_SEPARATOR_HPP 1

// std
#include <string>  // std::string
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_separator
//   constant: the render archetype for a horizontal rule.
inline constexpr const char* archetype_separator = "separator";


NS_COMPONENT

    // separator_type
    //   the descriptor for the separator element, owned by this module.
    D_NODISCARD
    inline const element_type* separator_type()
    {
        static const element_type descriptor =
            element_type{
                "separator",
                archetype_separator,
                option_set{ { { "text", ::djinterp::option_value(
                                            std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    separator
      Builds a leaf `separator` template.

    Parameter(s):
      _text:  an optional title carried on the rule ("text").
      _attrs: optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a single separator node.
    */
    D_NODISCARD
    inline element_template separator(
        const std::string& _text  = std::string(),
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = separator_type()->defaults;
        resolved.set("text", _text);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(separator_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_SEPARATOR_HPP
