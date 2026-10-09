/*******************************************************************************
* uxoxo [core]                                              element_splitter.hpp
*
* The splitter element: a bar dragged to resize what lies either side of it.
*   A "vertical" splitter stands between side-by-side panels and is dragged
* left and right; a "horizontal" one lies between stacked panels and is
* dragged up and down. It owns no size: while it is dragged it reports an
* event whose kind is its action and whose payload holds the movement in
* pixels under "value", and the application resizes the panels.
* "thickness" is its width across the drag; "length" its extent along
* it, as a panel measures (0 for all of it).
*
* path:      /inc/uxoxo/element_splitter.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_SPLITTER_HPP
#define UXOXO_ELEMENT_SPLITTER_HPP 1

// std
#include <string>  // std::string
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_splitter
//   constant: the render archetype for a resizing bar.
inline constexpr const char* archetype_splitter = "splitter";


NS_COMPONENT

    // splitter_type
    //   the descriptor for the splitter element, owned by this module.
    D_NODISCARD
    inline const element_type* splitter_type()
    {
        static const element_type descriptor =
            element_type{
                "splitter",
                archetype_splitter,
                option_set{ { { "orientation", ::djinterp::option_value(
                                                   std::string("vertical")) },
                              { "thickness",   ::djinterp::option_value(6.0) },
                              { "length",      ::djinterp::option_value(0.0) },
                              { "action",      ::djinterp::option_value(
                                                   std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    splitter
      Builds a leaf `splitter` template.

    Parameter(s):
      _vertical: whether it stands between side-by-side panels
                 ("orientation").
      _action:   the event kind reported while it is dragged ("action").
      _attrs:    optional extra attributes (thickness, length), overlaid on
                 top (caller wins).
    Return:
      An element_template wrapping a single splitter node.
    */
    D_NODISCARD
    inline element_template splitter(
        bool               _vertical,
        const std::string& _action,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = splitter_type()->defaults;
        resolved.set("orientation", _vertical ? "vertical" : "horizontal");
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(splitter_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_SPLITTER_HPP
