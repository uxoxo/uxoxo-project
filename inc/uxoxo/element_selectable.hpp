/*******************************************************************************
* uxoxo [core]                                            element_selectable.hpp
*
* The selectable element: a row of text that can be picked from a list.
*   Picking it reports an event whose kind is the element's action and whose
* payload carries the element's value attribute through under "value", so
* one action can serve every row of a list. A right-click reports
* "context_action", and a double-click "double_action", with the same
* value.
*
* path:      /inc/uxoxo/element_selectable.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.10.09
*******************************************************************************/

#ifndef UXOXO_ELEMENT_SELECTABLE_HPP
#define UXOXO_ELEMENT_SELECTABLE_HPP 1

// std
#include <string>  // std::string
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_selectable
//   constant: the render archetype for a pickable row.
inline constexpr const char* archetype_selectable = "selectable";


NS_COMPONENT

    // selectable_type
    //   the descriptor for the selectable element, owned by this module.
    D_NODISCARD
    inline const element_type* selectable_type()
    {
        static const element_type descriptor =
            element_type{
                "selectable",
                archetype_selectable,
                option_set{ { { "text",     ::djinterp::option_value(
                                                std::string()) },
                              { "selected", ::djinterp::option_value(false) },
                              { "enabled",  ::djinterp::option_value(true) },
                              { "action",   ::djinterp::option_value(
                                                std::string()) },
                              { "value",    ::djinterp::option_value(0L) },
                              { "context_action", ::djinterp::option_value(
                                                      std::string()) },
                              { "double_action",  ::djinterp::option_value(
                                                      std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    selectable
      Builds a leaf `selectable` template.

    Parameter(s):
      _text:     the row's text ("text").
      _selected: whether the row is the current pick ("selected").
      _action:   the event kind reported when it is picked ("action").
      _value:    passed back in the event payload ("value").
      _attrs:    optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a single selectable node.
    */
    D_NODISCARD
    inline element_template selectable(
        const std::string& _text,
        bool               _selected,
        const std::string& _action,
        long               _value,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = selectable_type()->defaults;
        resolved.set("text", _text);
        resolved.set("selected", _selected);
        resolved.set("action", _action);
        resolved.set("value", _value);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(selectable_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_SELECTABLE_HPP
