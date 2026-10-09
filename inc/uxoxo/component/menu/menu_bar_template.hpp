/******************************************************************************
* uxoxo [component]                                      menu_bar_template.hpp
*
* The `menu_bar` component: a horizontal bar of top-level menus.
*   A self-contained module owning one component type -- the bar container --
* and ignorant of every other component. A menu_bar is an ordered row of
* top-level entries, each of which opens a drop-down; since a labelled entry
* that opens a menu is exactly a `submenu` (menu.hpp), the bar needs no entry
* type of its own -- it just lays out children, and the caller supplies
* submenus. That keeps menu_bar decoupled from menu even though a bar is, in
* use, built from menus.
*
*   Render archetype: menu_bar (declared here). A backend that does not know it
* degrades; a real bar is a per-(type, platform) override.
*
*   Attribute keys: (none by default). Arity: 0..*.
*
*   Declarative form: the old bar's navigation and drop-down state (active,
* next/prev, open/close, cursor) are gone -- which entry is open and how focus
* moves are the behaviour layer's concern, over the live tree. The bar template
* is just the structure.
*
* USAGE:
*   using namespace uxoxo;
*   component_template bar = component::menu_bar(
*       { component::submenu("File", component::menu({ component::menu_item("New"),
*                                                      component::menu_item("Open") })),
*         component::submenu("Edit", component::menu({ component::menu_item("Undo") })) });
*
* path:      /inc/uxoxo/menu_bar_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_MENU_BAR_TEMPLATE_
#define UXOXO_MENU_BAR_TEMPLATE_ 1

// std
#include <utility>
#include <vector>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_menu_bar
//   constant: the render archetype for a top-level horizontal menu bar.
// Declared here (open-archetype pattern), at uxoxo:: root.
inline constexpr const char* archetype_menu_bar = "menu_bar";


NS_COMPONENT

    // menu_bar_type
    //   the descriptor for the menu bar container, owned by this module.
    D_NODISCARD
    inline const component_type* menu_bar_type()
    {
        static const component_type descriptor =
            component_type{
                "menu_bar",
                archetype_menu_bar,
                option_set{},   // no attributes
                0,
                -1
            };

        return &descriptor;
    }

    /*
    menu_bar
      Builds a `menu_bar` template laying out its top-level entries in order.

    Parameter(s):
      _entries: the top-level entry templates, in order (typically submenu
                nodes, or any component).
      _attrs:   optional extra attributes, overlaid on top (caller wins).
    Return:
      A component_template wrapping a menu_bar node over _entries. Closed iff
      every entry is closed.
    */
    D_NODISCARD
    inline component_template menu_bar(
        std::vector<component_template> _entries,
        option_set                      _attrs = option_set{}
    )
    {
        option_set resolved = menu_bar_type()->defaults;
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(menu_bar_type(), resolved, std::move(_entries));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_MENU_BAR_TEMPLATE_
