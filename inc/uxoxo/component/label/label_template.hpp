/******************************************************************************
* uxoxo [component]                                          label_template.hpp
*
* The `label` component: an atomic text leaf.
*   A self-contained component module. It defines and owns the label descriptor
* (a static, referenced by pointer from every label node) and the label builder.
* It depends only on the open component foundation; it has no knowledge of any
* other component type, and no other module has knowledge of it. Including this
* header is what makes `label` available -- the std::swap discipline: a type's
* signature lives in its own module, visible only when included.
*
*   Render archetype: text_leaf. Attribute keys: "text". Arity: 0.
*
*   This is the declarative form: a label is a *description* -- an immutable
* component_template -- not a mutable object. The old label's runtime verbs
* (append, set_value firing on_change, ...) are not here; mutation over a live
* label rides the runtime layer, and "changing the text" is rebuilding the
* template. Visual attributes beyond the text (alignment, emphasis) attach as
* extra keys in `_attrs` or, once conventionalized, in the descriptor defaults.
*
* USAGE:
*   using namespace uxoxo;
*   component_template t = component::label("OK");
*
* path:      /inc/uxoxo/label_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_LABEL_TEMPLATE_
#define UXOXO_LABEL_TEMPLATE_ 1

// std
#include <string>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO

NS_COMPONENT

    // label_type
    //   the descriptor for the label component, owned by this module. A static,
    // so every label node references one identity by pointer.
    D_NODISCARD
    inline const component_type* label_type()
    {
        static const component_type descriptor =
            component_type{
                "label",
                archetype_text_leaf,
                option_set{ { { "text", ::djinterp::option_value(std::string()) } } },
                0,
                0
            };

        return &descriptor;
    }

    /*
    label
      Builds a leaf `label` template carrying the given text.

    Parameter(s):
      _text:  the text the label displays (stored under the "text" key).
      _attrs: optional extra attributes, overlaid on top of "text" (caller wins).
    Return:
      A closed (hole-free) component_template wrapping a single label node.
    */
    D_NODISCARD
    inline component_template label(
        const std::string& _text,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = label_type()->defaults;
        resolved.set("text", _text);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(label_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_LABEL_TEMPLATE_
