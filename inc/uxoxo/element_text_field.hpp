/*******************************************************************************
* uxoxo [core]                                            element_text_field.hpp
*
* The text_field element: editable text, one line or many.
*   Uses the text_field archetype and the value / placeholder / enabled /
* read_only keys that component/input/text_input_template.hpp declares, so
* one backend serves both. An edit is reported as an event whose kind is the
* element's action and whose payload holds the new text under "value".
*
* path:      /inc/uxoxo/element_text_field.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.09.22
*******************************************************************************/

#ifndef UXOXO_ELEMENT_TEXT_FIELD_HPP
#define UXOXO_ELEMENT_TEXT_FIELD_HPP 1

// std
#include <string>  // std::string
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_text_field
//   constant: the render archetype for editable text.
inline constexpr const char* archetype_text_field = "text_field";


NS_COMPONENT

    // text_field_type
    //   the descriptor for the text_field element, owned by this module.
    D_NODISCARD
    inline const element_type* text_field_type()
    {
        static const element_type descriptor =
            element_type{
                "text_field",
                archetype_text_field,
                option_set{ { { "value",       ::djinterp::option_value(
                                                   std::string()) },
                              { "placeholder", ::djinterp::option_value(
                                                   std::string()) },
                              { "action",      ::djinterp::option_value(
                                                   std::string()) },
                              { "enabled",     ::djinterp::option_value(
                                                   true) },
                              { "read_only",   ::djinterp::option_value(
                                                   false) },
                              { "multiline",   ::djinterp::option_value(
                                                   false) },
                              { "height",      ::djinterp::option_value(
                                                   0.0) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    text_field
      Builds a leaf `text_field` template.

    Parameter(s):
      _value:  the text shown for editing ("value").
      _action: the event kind reported when the text is edited ("action").
      _attrs:  optional extra attributes (placeholder, multiline, height,
               read_only, enabled), overlaid on top (caller wins).
    Return:
      An element_template wrapping a single text_field node.
    */
    D_NODISCARD
    inline element_template text_field(
        const std::string& _value,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = text_field_type()->defaults;
        resolved.set("value", _value);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(text_field_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_TEXT_FIELD_HPP
