/*******************************************************************************
* uxoxo [core]                                                element_toggle.hpp
*
* The toggle element: a labelled on/off switch.
*   Uses the toggle archetype and the checked key that component/check_box
* declares. A flip is reported as an event whose kind is the element's action
* and whose payload holds the new state under "value".
*
* path:      /inc/uxoxo/element_toggle.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.09.22
*******************************************************************************/

#ifndef UXOXO_ELEMENT_TOGGLE_HPP
#define UXOXO_ELEMENT_TOGGLE_HPP 1

// std
#include <string>  // std::string
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_toggle
//   constant: the render archetype for an on/off switch.
inline constexpr const char* archetype_toggle = "toggle";


NS_COMPONENT

    // toggle_type
    //   the descriptor for the toggle element, owned by this module.
    D_NODISCARD
    inline const element_type* toggle_type()
    {
        static const element_type descriptor =
            element_type{
                "toggle",
                archetype_toggle,
                option_set{ { { "text",    ::djinterp::option_value(
                                               std::string()) },
                              { "checked", ::djinterp::option_value(false) },
                              { "enabled", ::djinterp::option_value(true) },
                              { "action",  ::djinterp::option_value(
                                               std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    toggle
      Builds a leaf `toggle` template.

    Parameter(s):
      _text:    the label beside the switch ("text").
      _checked: whether the switch is on ("checked").
      _action:  the event kind reported when it is flipped ("action").
      _attrs:   optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a single toggle node.
    */
    D_NODISCARD
    inline element_template toggle(
        const std::string& _text,
        bool               _checked,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = toggle_type()->defaults;
        resolved.set("text", _text);
        resolved.set("checked", _checked);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(toggle_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_TOGGLE_HPP
