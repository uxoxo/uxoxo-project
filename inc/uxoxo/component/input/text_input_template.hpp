/******************************************************************************
* uxoxo [component]                                    text_input_template.hpp
*
* The `text_input` component: an editable single-line text field.
*   A self-contained component module -- descriptor plus builder, ignorant of
* every other component. The first *value-bearing* leaf: unlike a label (which
* displays text it does not own for editing), a text_input's "value" is the
* editable payload, and "placeholder" is the ghost text shown when empty.
*
*   Render archetype: text_field (declared here -- the open-archetype pattern).
* A backend draws an editable box from "value" / "placeholder" / "read_only"; a
* backend that does not know "text_field" degrades, and a real native field is
* a per-(type, platform) override.
*
*   Attribute keys: "value" (string), "placeholder" (string), "enabled" (bool),
* "read_only" (bool). Arity: 0.
*
*   Declarative form: this is a *description*, so the old text_input's runtime
* apparatus -- the on_commit callback, and the multiline / history / validation
* / masked capability mixins -- is not here. Those act on a live field and ride
* the behaviour layer; richer facets (max length, masking, multiline) attach as
* extra attribute keys when a backend conventionalizes them.
*
* USAGE:
*   using namespace uxoxo;
*   component_template a = component::text_input();                    // empty
*   component_template b = component::text_input("hello");             // pre-filled
*   component_template c = component::text_input("", "search...");     // placeholder
*
* path:      /inc/uxoxo/text_input_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_TEXT_INPUT_TEMPLATE_
#define UXOXO_TEXT_INPUT_TEMPLATE_ 1

// std
#include <string>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_text_field
//   constant: the render archetype for an editable single-line text field.
// Declared beside the component that needs it; any string is a valid archetype,
// so a new one needs no core edit. At uxoxo:: root, like the bundled tokens.
inline constexpr const char* archetype_text_field = "text_field";


NS_COMPONENT

    // text_input_type
    //   the descriptor for the text_input component, owned by this module.
    D_NODISCARD
    inline const component_type* text_input_type()
    {
        static const component_type descriptor =
            component_type{
                "text_input",
                archetype_text_field,
                option_set{ { { "value",       ::djinterp::option_value(std::string()) },
                              { "placeholder", ::djinterp::option_value(std::string()) },
                              { "enabled",     ::djinterp::option_value(true) },
                              { "read_only",   ::djinterp::option_value(false) } } },
                0,
                0
            };

        return &descriptor;
    }

    /*
    text_input
      Builds a leaf `text_input` template carrying an editable value.

    Parameter(s):
      _value:       the initial editable text (stored under "value").
      _placeholder: ghost text shown when the value is empty ("placeholder").
      _attrs:       optional extra attributes, overlaid on top (caller wins).
    Return:
      A closed (hole-free) component_template wrapping a single text_input node.
    */
    D_NODISCARD
    inline component_template text_input(
        const std::string& _value       = std::string(),
        const std::string& _placeholder = std::string(),
        option_set         _attrs        = option_set{}
    )
    {
        option_set resolved = text_input_type()->defaults;
        resolved.set("value",       _value);
        resolved.set("placeholder", _placeholder);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(text_input_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_TEXT_INPUT_TEMPLATE_
