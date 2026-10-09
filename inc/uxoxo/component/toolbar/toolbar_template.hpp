/******************************************************************************
* uxoxo [component]                                          toolbar_template.hpp
*
* The `toolbar` component: a docked sequence of buttons, separators, spacers.
*   A self-contained module owning the bar container and its two chrome leaves
* -- separator and spacer -- and ignorant of every other component. A toolbar is
* an ordered list of entry children anchored to an edge; buttons come from
* button_template.hpp (the caller supplies them), so the bar stays decoupled
* from button and merely lays children out.
*
*   This is where the old design's type erasure disappears entirely. The old
* toolbar held buttons of differing feature sets behind a polymorphic
* toolbar_button_iface / toolbar_button_helper, because each button<_F,_I> was a
* distinct type. Here every entry is a component_template already -- one type --
* so there is nothing to erase: the vtable, the raw_ptr escape hatch, and the
* per-frame reset all fall away. What a button does on click, and its
* pressed/hovered flags, are behaviour-layer concerns over the live tree.
*
*   Render archetypes (declared here): toolbar, toolbar_separator,
* toolbar_spacer. Backends that do not know them degrade; real chrome is a
* per-(type, platform) override.
*
*   Attribute keys:
*     toolbar_separator : (none).                                    Arity: 0.
*     toolbar_spacer    : (none).                                    Arity: 0.
*     toolbar           : "dock" (string: "top"/"bottom"/"left"/
*                         "right"/"floating").                       Arity: 0..*.
*
* USAGE:
*   using namespace uxoxo;
*   component_template bar = component::toolbar(
*       { component::button(component::label("Save")),
*         component::button(component::label("Undo")),
*         component::toolbar_separator(),
*         component::toolbar_spacer(),
*         component::button(component::label("Settings")) },
*       "top");
*
* path:      /inc/uxoxo/toolbar_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_TOOLBAR_TEMPLATE_
#define UXOXO_TOOLBAR_TEMPLATE_ 1

// std
#include <string>
#include <utility>
#include <vector>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_toolbar / archetype_toolbar_separator / archetype_toolbar_spacer
//   constants: the render archetypes for the toolbar family. Declared here (the
// open-archetype pattern), at uxoxo:: root.
inline constexpr const char* archetype_toolbar           = "toolbar";
inline constexpr const char* archetype_toolbar_separator = "toolbar_separator";
inline constexpr const char* archetype_toolbar_spacer    = "toolbar_spacer";


NS_COMPONENT

    // toolbar_separator_type
    //   the descriptor for a toolbar divider, owned by this module.
    D_NODISCARD
    inline const component_type* toolbar_separator_type()
    {
        static const component_type descriptor =
            component_type{
                "toolbar_separator",
                archetype_toolbar_separator,
                option_set{},   // no attributes
                0,
                0
            };

        return &descriptor;
    }

    /*
    toolbar_separator
      Builds a leaf `toolbar_separator` template -- a fixed visual divider.

    Return:
      A closed component_template wrapping a single toolbar_separator node.
    */
    D_NODISCARD
    inline component_template toolbar_separator()
    {
        return make_component(toolbar_separator_type(),
                              toolbar_separator_type()->defaults,
                              {});
    }


    // toolbar_spacer_type
    //   the descriptor for a toolbar flexible spacer, owned by this module.
    D_NODISCARD
    inline const component_type* toolbar_spacer_type()
    {
        static const component_type descriptor =
            component_type{
                "toolbar_spacer",
                archetype_toolbar_spacer,
                option_set{},   // no attributes
                0,
                0
            };

        return &descriptor;
    }

    /*
    toolbar_spacer
      Builds a leaf `toolbar_spacer` template -- a flexible gap that pushes the
    entries after it toward the opposite end.

    Return:
      A closed component_template wrapping a single toolbar_spacer node.
    */
    D_NODISCARD
    inline component_template toolbar_spacer()
    {
        return make_component(toolbar_spacer_type(),
                              toolbar_spacer_type()->defaults,
                              {});
    }


    // toolbar_type
    //   the descriptor for the toolbar container, owned by this module.
    D_NODISCARD
    inline const component_type* toolbar_type()
    {
        static const component_type descriptor =
            component_type{
                "toolbar",
                archetype_toolbar,
                option_set{ { { "dock", ::djinterp::option_value(std::string("bottom")) } } },
                0,
                -1
            };

        return &descriptor;
    }

    /*
    toolbar
      Builds a `toolbar` template laying out its entry children in order.

    Parameter(s):
      _entries: the entry templates, in order (button / toolbar_separator /
                toolbar_spacer nodes, or any component).
      _dock:    the edge the bar anchors to ("dock"); defaults to "bottom".
      _attrs:   optional extra attributes, overlaid on top (caller wins).
    Return:
      A component_template wrapping a toolbar node over _entries. Closed iff
      every entry is closed.
    */
    D_NODISCARD
    inline component_template toolbar(
        std::vector<component_template> _entries,
        const std::string&              _dock  = std::string("bottom"),
        option_set                      _attrs = option_set{}
    )
    {
        option_set resolved = toolbar_type()->defaults;
        resolved.set("dock", _dock);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(toolbar_type(), resolved, std::move(_entries));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_TOOLBAR_TEMPLATE_
