/*******************************************************************************
* uxoxo [core]                                           element_color_field.hpp
*
* The color_field element: a colour swatch that opens a picker.
*   The colour is four reals in [0, 1] under "r", "g", "b" and "a" --
* not a packed integer, which a 32-bit long could not hold unsigned. An edit
* reports an event whose kind is the element's action and whose payload
* carries the new colour under the same four keys. "alpha" offers the
* alpha channel; "inputs" shows the numeric fields beside the swatch.
*
* path:      /inc/uxoxo/element_color_field.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_COLOR_FIELD_HPP
#define UXOXO_ELEMENT_COLOR_FIELD_HPP 1

// std
#include <string>  // std::string
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_color_field
//   constant: the render archetype for a colour swatch and picker.
inline constexpr const char* archetype_color_field = "color_field";


NS_COMPONENT

    // color_field_type
    //   the descriptor for the color_field element, owned by this module.
    D_NODISCARD
    inline const element_type* color_field_type()
    {
        static const element_type descriptor =
            element_type{
                "color_field",
                archetype_color_field,
                option_set{ { { "r",       ::djinterp::option_value(1.0) },
                              { "g",       ::djinterp::option_value(1.0) },
                              { "b",       ::djinterp::option_value(1.0) },
                              { "a",       ::djinterp::option_value(1.0) },
                              { "alpha",   ::djinterp::option_value(true) },
                              { "inputs",  ::djinterp::option_value(false) },
                              { "action",  ::djinterp::option_value(
                                               std::string()) },
                              { "enabled", ::djinterp::option_value(true) },
                              { "label",   ::djinterp::option_value(
                                               std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    color_field
      Builds a leaf `color_field` template.

    Parameter(s):
      _r:      red, in [0, 1] ("r").
      _g:      green, in [0, 1] ("g").
      _b:      blue, in [0, 1] ("b").
      _a:      alpha, in [0, 1] ("a").
      _action: the event kind reported when the colour changes ("action").
      _attrs:  optional extra attributes (alpha, inputs, label), overlaid on
               top (caller wins).
    Return:
      An element_template wrapping a single color_field node.
    */
    D_NODISCARD
    inline element_template color_field(
        double             _r,
        double             _g,
        double             _b,
        double             _a,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = color_field_type()->defaults;
        resolved.set("r", _r);
        resolved.set("g", _g);
        resolved.set("b", _b);
        resolved.set("a", _a);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(color_field_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_COLOR_FIELD_HPP
