/******************************************************************************
* uxoxo [component]                                         button_template.hpp
*
* The `button` component: an interactive surface wrapping one child.
*   A self-contained component module, owning the button descriptor and builder.
* It depends only on the open component foundation and knows nothing of label,
* check_box, or any other component type. Including this header is what makes
* `button` available.
*
*   Render archetype: interactive. Attribute keys: "enabled" (bool). Arity: 1.
*
*   Compositional by design: a button wraps a *content child*, so a labelled
* button is `button(label("OK"))` and the button stays ignorant of what it
* wraps. This replaces the old button's direct `label` string -- the content is
* now whatever component the caller nests. The old feature flags (icon,
* tooltip, shape, badge, colour, ...) become attribute keys overlaid via
* `_attrs` or added to the descriptor defaults when a backend conventionalizes
* them; only "enabled" is core here. Runtime behaviour (what a press does, the
* pressed/hovered/focused flags a backend sets) is not here -- it rides the
* behaviour layer that annotates the live tree.
*
* USAGE:
*   using namespace uxoxo;
*   component_template t = component::button(component::label("OK"));
*   component_template d = component::button(component::label("Off"), false);  // disabled
*
* path:      /inc/uxoxo/button_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_BUTTON_TEMPLATE_
#define UXOXO_BUTTON_TEMPLATE_ 1

// uxoxo
#include "./component_template.hpp"


NS_UXOXO

NS_COMPONENT

    // button_type
    //   the descriptor for the button component, owned by this module.
    D_NODISCARD
    inline const component_type* button_type()
    {
        static const component_type descriptor =
            component_type{
                "button",
                archetype_interactive,
                option_set{ { { "enabled", ::djinterp::option_value(true) } } },
                1,
                1
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
      A component_template wrapping a button node over _content. Closed iff
      _content is closed.
    */
    D_NODISCARD
    inline component_template button(
        const component_template& _content,
        bool                      _enabled = true,
        option_set                _attrs   = option_set{}
    )
    {
        option_set resolved = button_type()->defaults;
        resolved.set("enabled", _enabled);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(button_type(), resolved, { _content });
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_BUTTON_TEMPLATE_
