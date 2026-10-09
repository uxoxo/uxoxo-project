/******************************************************************************
* uxoxo [component]                                   radio_group_template.hpp
*
* The `radio_group` component: a one-of-N selector, and its option leaf.
*   A self-contained module owning two component types -- the option leaf and
* the group container -- and ignorant of every other component. The first
* *selection container*: a group lays out a list of option children and names
* which index is selected, so "exactly one is chosen" is a property of the
* group's "selected" attribute rather than of shared mutable state across the
* options.
*
*   Render archetypes (declared here): radio -- a single option glyph + label;
* radio_group -- the container that lays options out. Backends that do not know
* them degrade; real controls are per-(type, platform) overrides.
*
*   Attribute keys:
*     radio_option : "text" (string), "enabled" (bool).            Arity: 0.
*     radio_group  : "selected" (long, 0-based index),
*                    "orientation" (string: "vertical"/"horizontal"). Arity: 0..*.
*
*   Declarative form: the old group's cursor/navigation state (focused index,
* wrap), its selected_option() accessors, and the mutation helpers are not here
* -- moving the selection is the behaviour layer's job, and "which option"
* rides in "selected". Options are supplied as children; the group stays
* ignorant of what an option is, so any component may serve as one.
*
* USAGE:
*   using namespace uxoxo;
*   component_template g = component::radio_group(
*       { component::radio_option("Small"),
*         component::radio_option("Medium"),
*         component::radio_option("Large") },
*       1);                                      // "Medium" selected
*
* path:      /inc/uxoxo/radio_group_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_RADIO_GROUP_TEMPLATE_
#define UXOXO_RADIO_GROUP_TEMPLATE_ 1

// std
#include <string>
#include <vector>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_radio / archetype_radio_group
//   constants: the render archetypes for a radio option and the group that
// holds them. Declared here (open-archetype pattern), at uxoxo:: root.
inline constexpr const char* archetype_radio       = "radio";
inline constexpr const char* archetype_radio_group = "radio_group";


NS_COMPONENT

    // radio_option_type
    //   the descriptor for a single radio option, owned by this module.
    D_NODISCARD
    inline const component_type* radio_option_type()
    {
        static const component_type descriptor =
            component_type{
                "radio_option",
                archetype_radio,
                option_set{ { { "text",    ::djinterp::option_value(std::string()) },
                              { "enabled", ::djinterp::option_value(true) } } },
                0,
                0
            };

        return &descriptor;
    }

    /*
    radio_option
      Builds a leaf `radio_option` template carrying a label.

    Parameter(s):
      _label: the option's display text (stored under "text").
      _attrs: optional extra attributes, overlaid on top (caller wins).
    Return:
      A closed component_template wrapping a single radio_option node.
    */
    D_NODISCARD
    inline component_template radio_option(
        const std::string& _label,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = radio_option_type()->defaults;
        resolved.set("text", _label);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(radio_option_type(), resolved, {});
    }


    // radio_group_type
    //   the descriptor for the radio group container, owned by this module.
    D_NODISCARD
    inline const component_type* radio_group_type()
    {
        static const component_type descriptor =
            component_type{
                "radio_group",
                archetype_radio_group,
                option_set{ { { "selected",    ::djinterp::option_value(0L) },
                              { "orientation", ::djinterp::option_value(std::string("vertical")) } } },
                0,
                -1
            };

        return &descriptor;
    }

    /*
    radio_group
      Builds a `radio_group` template laying out its option children, with one
    marked selected.

    Parameter(s):
      _options:  the option templates, in order (typically radio_option nodes).
      _selected: the 0-based index of the selected option ("selected").
      _attrs:    optional extra attributes, overlaid on top (caller wins).
    Return:
      A component_template wrapping a radio_group node over _options. Closed iff
      every option is closed.
    */
    D_NODISCARD
    inline component_template radio_group(
        std::vector<component_template> _options,
        long                            _selected = 0,
        option_set                      _attrs    = option_set{}
    )
    {
        option_set resolved = radio_group_type()->defaults;
        resolved.set("selected", _selected);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(radio_group_type(), resolved, std::move(_options));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_RADIO_GROUP_TEMPLATE_
