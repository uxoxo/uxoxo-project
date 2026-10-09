/******************************************************************************
* uxoxo [component]                                             element_row.hpp
*
* The `row` element: a container laying its children out in order.
*   A self-contained element module, owning the row descriptor and builder. It
* depends only on the open element core and knows nothing of label, button, or
* any other element type. Including this header is what makes `row` available.
*
*   Render role: container. Attribute keys: "spacing" (long). Arity: 0..*.
*
* USAGE:
*   using namespace uxoxo;
*   element_template t = component::row({ component::label("A"),
*                                         component::label("B") }, 2);
*
* path:      /inc/uxoxo/element_row.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/


#ifndef UXOXO_ELEMENT_ROW_
#define UXOXO_ELEMENT_ROW_ 1

// std
#include <utility>
#include <vector>
// uxoxo
#include "./element.hpp"


NS_UXOXO

NS_COMPONENT

    // row_type
    //   the descriptor for the row element, owned by this module. Unbounded
    // arity (max_children -1).
    D_NODISCARD
    inline const element_type* row_type()
    {
        static const element_type descriptor =
            element_type{
                "row",
                archetype_container,
                option_set{ { { "spacing", ::djinterp::option_value(1L) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    row
      Builds a `row` template laying out a list of children in order.

    Parameter(s):
      _items:   the child templates, left to right.
      _spacing: the inter-child spacing; defaults to 1 ("spacing").
      _attrs:   optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a row node over _items.
    */
    D_NODISCARD
    inline element_template row(
        std::vector<element_template> _items,
        int                           _spacing = 1,
        option_set                    _attrs   = option_set{}
    )
    {
        option_set resolved = row_type()->defaults;
        resolved.set("spacing", static_cast<long>(_spacing));
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(row_type(), resolved, std::move(_items));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_ROW_
