/******************************************************************************
* uxoxo [component]                                          element_button.hpp
*
* The `button` element: an interactive surface wrapping one child.
*   A self-contained element module, owning the button descriptor and builder. It
* depends only on the open element core and knows nothing of label, row, or any
* other element type. Including this header is what makes `button` available.
*
*   Render role: interactive. Attribute keys: "enabled" (bool). Arity: 1.
*
* USAGE:
*   using namespace uxoxo;
*   element_template t = component::button(component::label("OK"));
*
* path:      /inc/uxoxo/element_button.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/


#ifndef UXOXO_ELEMENT_BUTTON_
#define UXOXO_ELEMENT_BUTTON_ 1

// uxoxo
#include "./element.hpp"


NS_UXOXO

NS_COMPONENT

    // button_type
    //   the descriptor for the button element, owned by this module.
    D_NODISCARD
    inline const element_type* button_type()
    {
        static const element_type descriptor =
            element_type{
                "button",
                archetype_interactive,
                option_set{ { { "enabled", ::djinterp::option_value(true) } } },
                1,
                1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    button
      Builds a `button` template wrapping a single content child.

    Parameter(s):
      _content: the template placed inside the button (e.g. a label).
      _enabled: whether the button is interactive; defaults to true ("enabled").
      _attrs:   optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a button node over _content.
    */
    D_NODISCARD
    inline element_template button(
        const element_template& _content,
        bool                    _enabled = true,
        option_set              _attrs   = option_set{}
    )
    {
        option_set resolved = button_type()->defaults;
        resolved.set("enabled", _enabled);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(button_type(), resolved, { _content });
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_BUTTON_
