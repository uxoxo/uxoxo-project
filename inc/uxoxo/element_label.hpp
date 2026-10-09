/******************************************************************************
* uxoxo [component]                                           element_label.hpp
*
* The `label` element: an atomic text leaf.
*   A self-contained element module. It defines and owns the label descriptor (a
* static, referenced by pointer from every label node) and the label builder. It
* depends only on the open element core; it has no knowledge of any other element
* type, and no other module has knowledge of it. Including this header is what
* makes `label` available -- the std::swap discipline: a type's signature lives
* in its own module, visible only when included.
*
*   Render role: text_leaf. Attribute keys: "text". Arity: 0.
*
* USAGE:
*   using namespace uxoxo;
*   element_template t = component::label("OK");
*
* path:      /inc/uxoxo/element_label.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/


#ifndef UXOXO_ELEMENT_LABEL_
#define UXOXO_ELEMENT_LABEL_ 1

// std
#include <string>
// uxoxo
#include "./element.hpp"


NS_UXOXO

NS_COMPONENT

    // label_type
    //   the descriptor for the label element, owned by this module. A static, so
    // every label node references one identity by pointer.
    D_NODISCARD
    inline const element_type* label_type()
    {
        static const element_type descriptor =
            element_type{
                "label",
                archetype_text_leaf,
                option_set{ { { "text", ::djinterp::option_value(std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    label
      Builds a leaf `label` template carrying the given text.

    Parameter(s):
      _text:  the text the label displays (stored under "text").
      _attrs: optional extra attributes, overlaid on top (caller wins).
    Return:
      A closed element_template wrapping a single label node.
    */
    D_NODISCARD
    inline element_template label(
        const std::string& _text,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = label_type()->defaults;
        resolved.set("text", _text);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(label_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_LABEL_
