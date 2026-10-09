/*******************************************************************************
* uxoxo [core]                                                 element_combo.hpp
*
* The combo element: one choice from a drop-down list.
*   The choices travel as one string under "items", one per line, because an
* option record holds scalars. The current choice is the index under
* "selected"; picking another reports an event whose kind is the element's
* action and whose payload holds the new index under "value".
*
* path:      /inc/uxoxo/element_combo.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_COMBO_HPP
#define UXOXO_ELEMENT_COMBO_HPP 1

// std
#include <cstddef>  // std::size_t
#include <string>   // std::string
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_combo
//   constant: the render archetype for a drop-down choice.
inline constexpr const char* archetype_combo = "combo";


NS_COMPONENT

    // combo_type
    //   the descriptor for the combo element, owned by this module.
    D_NODISCARD
    inline const element_type* combo_type()
    {
        static const element_type descriptor =
            element_type{
                "combo",
                archetype_combo,
                option_set{ { { "items",    ::djinterp::option_value(
                                                std::string()) },
                              { "selected", ::djinterp::option_value(0L) },
                              { "action",   ::djinterp::option_value(
                                                std::string()) },
                              { "enabled",  ::djinterp::option_value(true) },
                              { "width",    ::djinterp::option_value(0.0) },
                              { "label",    ::djinterp::option_value(
                                                std::string()) } } },
                0,
                0,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    combo
      Builds a leaf `combo` template.

    Parameter(s):
      _items:    the choices, in order ("items", one per line).
      _selected: the index of the current choice ("selected").
      _action:   the event kind reported when another is picked ("action").
      _attrs:    optional extra attributes (width, label, enabled), overlaid
                 on top (caller wins).
    Return:
      An element_template wrapping a single combo node.
    */
    D_NODISCARD
    inline element_template combo(
        const std::vector<std::string>& _items,
        long                            _selected,
        const std::string&              _action,
        option_set                      _attrs = option_set{}
    )
    {
        std::string joined;

        // one choice per line
        for (std::size_t i = 0; i < _items.size(); ++i)
        {
            if (i != 0)
            {
                joined += '\n';
            }

            joined += _items[i];
        }

        option_set resolved = combo_type()->defaults;
        resolved.set("items", joined);
        resolved.set("selected", _selected);
        resolved.set("action", _action);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(combo_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_COMBO_HPP
