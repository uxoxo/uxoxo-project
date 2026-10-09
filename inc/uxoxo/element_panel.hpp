/*******************************************************************************
* uxoxo [core]                                                 element_panel.hpp
*
* The panel element: a sized, scrolling region holding a column of children.
*   width and height are in pixels, or a fraction of the space available when
* they lie in (0, 1]; 0 takes all remaining space and a negative value leaves
* that many pixels free. resize ("x", "y" or "") lets the user drag the
* panel's edge, and scroll_x gives it a horizontal scroll bar for content
* wider than it is.
*
* path:      /inc/uxoxo/element_panel.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_PANEL_HPP
#define UXOXO_ELEMENT_PANEL_HPP 1

// std
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_panel
//   constant: the render archetype for a sized, scrolling region.
inline constexpr const char* archetype_panel = "panel";


NS_COMPONENT

    // panel_type
    //   the descriptor for the panel element, owned by this module.
    D_NODISCARD
    inline const element_type* panel_type()
    {
        static const element_type descriptor =
            element_type{
                "panel",
                archetype_panel,
                option_set{ { { "id",     ::djinterp::option_value(
                                              std::string()) },
                              { "width",  ::djinterp::option_value(0.0) },
                              { "height", ::djinterp::option_value(0.0) },
                              { "border", ::djinterp::option_value(false) },
                              { "resize", ::djinterp::option_value(
                                              std::string()) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    panel
      Builds a `panel` template: a region of the given size whose children are
    laid out top to bottom and scroll when they overflow it.

    Parameter(s):
      _id:     a name unique among sibling panels ("id").
      _items:  the child templates, top to bottom.
      _attrs:  optional extra attributes (width, height, border, resize),
               overlaid on top (caller wins).
    Return:
      An element_template wrapping a panel node over _items.
    */
    D_NODISCARD
    inline element_template panel(
        const std::string&            _id,
        std::vector<element_template> _items,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = panel_type()->defaults;
        resolved.set("id", _id);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(panel_type(), resolved, std::move(_items));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_PANEL_HPP
