/******************************************************************************
* uxoxo [component]                                        combo_box_template.hpp
*
* The `combo_box` component: a drop-down selector, and its option leaf.
*   A self-contained module owning two component types -- the option leaf and
* the combo container -- and ignorant of every other component. A combo_box is
* a collapsed display of the chosen option plus a deferred panel of option
* children; structurally it is a selection container like radio_group, differing
* in presentation (a drop-down rather than an inline set) and in the extra
* facets it admits: an editable text field, and multi-select.
*
*   Render archetypes (declared here): combo_box, combo_option. Backends that do
* not know them degrade; real controls are per-(type, platform) overrides.
*
*   Attribute keys:
*     combo_option : "text" (string), "enabled" (bool).             Arity: 0.
*     combo_box    : "selected" (long, 0-based index of the chosen option),
*                    "editable" (bool -- may the display be typed into),
*                    "placeholder" (string -- shown when nothing is chosen),
*                    "multi_select" (bool).                          Arity: 0..*.
*
*   ON MULTI-SELECT. option_set holds one scalar per key, so a *set* of chosen
* indices does not fit "selected". A single-select combo uses the group-level
* "selected" index; a multi-select combo sets "multi_select" true and marks each
* chosen option with its own "selected" bool (overlaid via that option's attrs).
* Single-select is the common path and reads directly off "selected".
*
*   Declarative form: the old combo's edit buffer, filter text / filtered-index
* cache, and the clearable / undoable mixins are gone -- typing, filtering, and
* commit act on a live control and ride the behaviour layer; the durable facts
* (which option, editable, placeholder, multi) are attributes.
*
* USAGE:
*   using namespace uxoxo;
*   component_template c = component::combo_box(
*       { component::combo_option("Red"),
*         component::combo_option("Green"),
*         component::combo_option("Blue") },
*       2);                                        // "Blue" chosen
*
* path:      /inc/uxoxo/combo_box_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_COMBO_BOX_TEMPLATE_
#define UXOXO_COMBO_BOX_TEMPLATE_ 1

// std
#include <string>
#include <utility>
#include <vector>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_combo_box / archetype_combo_option
//   constants: the render archetypes for the combo box and its options.
// Declared here (open-archetype pattern), at uxoxo:: root.
inline constexpr const char* archetype_combo_box    = "combo_box";
inline constexpr const char* archetype_combo_option = "combo_option";


NS_COMPONENT

    // combo_option_type
    //   the descriptor for a single combo option, owned by this module.
    D_NODISCARD
    inline const component_type* combo_option_type()
    {
        static const component_type descriptor =
            component_type{
                "combo_option",
                archetype_combo_option,
                option_set{ { { "text",    ::djinterp::option_value(std::string()) },
                              { "enabled", ::djinterp::option_value(true) } } },
                0,
                0
            };

        return &descriptor;
    }

    /*
    combo_option
      Builds a leaf `combo_option` template carrying a label.

    Parameter(s):
      _label: the option's display text (stored under "text").
      _attrs: optional extra attributes -- e.g. "selected" (bool) for a chosen
              option in a multi-select combo -- overlaid on top (caller wins).
    Return:
      A closed component_template wrapping a single combo_option node.
    */
    D_NODISCARD
    inline component_template combo_option(
        const std::string& _label,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = combo_option_type()->defaults;
        resolved.set("text", _label);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(combo_option_type(), resolved, {});
    }


    // combo_box_type
    //   the descriptor for the combo box container, owned by this module.
    D_NODISCARD
    inline const component_type* combo_box_type()
    {
        static const component_type descriptor =
            component_type{
                "combo_box",
                archetype_combo_box,
                option_set{ { { "selected",     ::djinterp::option_value(0L) },
                              { "editable",     ::djinterp::option_value(false) },
                              { "placeholder",  ::djinterp::option_value(std::string()) },
                              { "multi_select", ::djinterp::option_value(false) } } },
                0,
                -1
            };

        return &descriptor;
    }

    /*
    combo_box
      Builds a `combo_box` template over its option children, with one chosen.

    Parameter(s):
      _options:  the option templates, in order (typically combo_option nodes).
      _selected: the 0-based index of the chosen option ("selected").
      _attrs:    optional extra attributes -- "editable", "placeholder",
                 "multi_select" -- overlaid on top (caller wins).
    Return:
      A component_template wrapping a combo_box node over _options. Closed iff
      every option is closed.
    */
    D_NODISCARD
    inline component_template combo_box(
        std::vector<component_template> _options,
        long                            _selected = 0,
        option_set                      _attrs    = option_set{}
    )
    {
        option_set resolved = combo_box_type()->defaults;
        resolved.set("selected", _selected);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(combo_box_type(), resolved, std::move(_options));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_COMBO_BOX_TEMPLATE_
