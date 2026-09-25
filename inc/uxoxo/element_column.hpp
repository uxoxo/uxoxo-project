/*******************************************************************************
* uxoxo [core]                                                element_column.hpp
*
* The column element: children laid out top to bottom.
*   A column is a container like row, with its orientation attribute set to
* "vertical". It reuses the container archetype, so a backend that ignores
* orientation still shows every child, just in a line.
*
* path:      /inc/uxoxo/element_column.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.09.22
*******************************************************************************/

#ifndef UXOXO_ELEMENT_COLUMN_HPP
#define UXOXO_ELEMENT_COLUMN_HPP 1

// std
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

NS_COMPONENT

    // column_type
    //   the descriptor for the column element, owned by this module.
    // Unbounded arity (max_children -1).
    D_NODISCARD
    inline const element_type* column_type()
    {
        static const element_type descriptor =
            element_type{
                "column",
                archetype_container,
                option_set{ { { "spacing",
                                ::djinterp::option_value(1L) },
                              { "orientation",
                                ::djinterp::option_value(
                                    std::string("vertical")) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    column
      Builds a `column` template laying out a list of children top to bottom.

    Parameter(s):
      _items:   the child templates, top to bottom.
      _spacing: the inter-child spacing; defaults to 1 ("spacing").
      _attrs:   optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a column node over _items.
    */
    D_NODISCARD
    inline element_template column(
        std::vector<element_template> _items,
        int                           _spacing = 1,
        option_set                    _attrs   = option_set{}
    )
    {
        option_set resolved = column_type()->defaults;
        resolved.set("spacing", static_cast<long>(_spacing));
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(column_type(), resolved, std::move(_items));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_COLUMN_HPP
