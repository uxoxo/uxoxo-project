/*******************************************************************************
* uxoxo [core]                                          element_number_field.hpp
*
* Number elements: a field for an integer or a real, and a slider.
*   A number_field holds its value under "value" -- a long when "kind" is
* "int", a double when it is "real" -- and reports an edit as an event
* whose kind is its action and whose payload carries the new value under
* "value", of the same type. "min" and "max", where present, clamp it;
* "step" is the +/- increment, "drag" turns the field into a drag box with
* "speed", and "hex" edits an integer in hexadecimal. A slider is a real
* between "min" and "max". "label" is drawn beside either; "width" is
* in pixels, or a fraction of the row in (0, 1].
*
* path:      /inc/uxoxo/element_number_field.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_NUMBER_FIELD_HPP
#define UXOXO_ELEMENT_NUMBER_FIELD_HPP 1

// std
#include <string>  // std::string
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_number_field / archetype_slider
//   constants: the render archetypes for a numeric field and a slider.
inline constexpr const char* archetype_number_field = "number_field";
inline constexpr const char* archetype_slider       = "slider";


NS_COMPONENT

    // number_field_type
    //   the descriptor for the number field, owned by this module.
    D_NODISCARD
    inline const element_type* number_field_type()
    {
        static const element_type descriptor =
            element_type{
                "number_field",
                archetype_number_field,
                option_set{ { { "kind",    ::djinterp::option_value(
                                               std::string("int")) },
                              { "action",  ::djinterp::option_value(
                                               std::string()) },
                              { "enabled", ::djinterp::option_value(true) },
                              { "drag",    ::djinterp::option_value(false) },
                              { "hex",     ::djinterp::option_value(false) },
                              { "width",   ::djinterp::option_value(0.0) },
                              { "label",   ::djinterp::option_value(
                                               std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    // slider_type
    //   the descriptor for the slider, owned by this module.
    D_NODISCARD
    inline const element_type* slider_type()
    {
        static const element_type descriptor =
            element_type{
                "slider",
                archetype_slider,
                option_set{ { { "min",     ::djinterp::option_value(0.0) },
                              { "max",     ::djinterp::option_value(1.0) },
                              { "format",  ::djinterp::option_value(
                                               std::string("%.2f")) },
                              { "action",  ::djinterp::option_value(
                                               std::string()) },
                              { "enabled", ::djinterp::option_value(true) },
                              { "width",   ::djinterp::option_value(0.0) },
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
    int_field
      Builds a leaf `number_field` template over an integer.

    Parameter(s):
      _value:  the integer shown for editing ("value").
      _action: the event kind reported when it is edited ("action").
      _attrs:  optional extra attributes (min, max, step, drag, speed, hex,
               width, label), overlaid on top (caller wins).
    Return:
      An element_template wrapping a single number_field node.
    */
    D_NODISCARD
    inline element_template int_field(
        long               _value,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = number_field_type()->defaults;
        resolved.set("kind", "int");
        resolved.set("value", _value);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(number_field_type(), resolved, {});
    }

    /*
    real_field
      Builds a leaf `number_field` template over a real.

    Parameter(s):
      _value:  the real shown for editing ("value").
      _action: the event kind reported when it is edited ("action").
      _attrs:  optional extra attributes (min, max, step, drag, speed, format,
               width, label), overlaid on top (caller wins).
    Return:
      An element_template wrapping a single number_field node.
    */
    D_NODISCARD
    inline element_template real_field(
        double             _value,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = number_field_type()->defaults;
        resolved.set("kind", "real");
        resolved.set("value", _value);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(number_field_type(), resolved, {});
    }

    /*
    slider
      Builds a leaf `slider` template over a real between two bounds.

    Parameter(s):
      _value:  the position of the slider ("value").
      _min:    the left end ("min").
      _max:    the right end ("max").
      _action: the event kind reported when it moves ("action").
      _attrs:  optional extra attributes (format, width, label), overlaid on
               top (caller wins).
    Return:
      An element_template wrapping a single slider node.
    */
    D_NODISCARD
    inline element_template slider(
        double             _value,
        double             _min,
        double             _max,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = slider_type()->defaults;
        resolved.set("value", _value);
        resolved.set("min", _min);
        resolved.set("max", _max);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(slider_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_NUMBER_FIELD_HPP
