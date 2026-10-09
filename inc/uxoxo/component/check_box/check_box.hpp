/******************************************************************************
* uxoxo [component]                                                check_box.hpp
*
* The `check_box` component: a checkable toggle, binary or tri-state.
*   A self-contained component module, owning the check_box descriptor and
* builder. It depends only on the open component foundation and knows nothing of
* label, button, or any other component type.
*
*   Render archetype: toggle -- declared *here*, not in the core. This is the
* open-archetype pattern in practice: a new archetype is just a new string, so
* adding one touches no core file. A backend draws the check glyph from the
* "state" attribute; a backend that does not know "toggle" degrades gracefully,
* and a real native control is a per-(type, platform) override.
*
*   Attribute keys: "state" (string -- one of "unchecked" / "checked" /
* "mixed"), "enabled" (bool). Arity: 0. The tri-state `mixed` (indeterminate)
* models the canonical "check all" parent whose governed children are in
* heterogeneous states -- e.g. a table header whose rows are not uniformly
* selected.
*
*   Declarative form: a check_box is an immutable description. The old
* checkbox's runtime operations (cb_cycle, cb_sync_from_children, on_commit /
* on_change callbacks, and the clearable / undoable mixins) are not here --
* those act on a *live* control and belong to the behaviour layer. `state` is
* carried as a legible string so the tri-state reads cleanly in the option_set
* and in a serialized template.
*
* USAGE:
*   using namespace uxoxo;
*   component_template a = component::check_box();                             // unchecked
*   component_template b = component::check_box(component::check_state::checked);
*   component_template c = component::check_box(component::check_state::mixed, false);
*
* path:      /inc/uxoxo/check_box.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_CHECK_BOX_
#define UXOXO_CHECK_BOX_ 1

// std
#include <string>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_toggle
//   constant: the render archetype for a checkable box -- an atomic control
// whose glyph reflects a tri-state "state". Declared beside the component that
// needs it rather than in the core, since any string is a valid archetype and a
// new one needs no central edit. Lives at uxoxo:: root, like the bundled
// archetype_* tokens, so modules and backends spell it identically.
inline constexpr const char* archetype_toggle = "toggle";


NS_COMPONENT

    // check_state
    //   enum: the three values a check_box can hold. `mixed` (indeterminate)
    // models the "check all" parent over heterogeneous children.
    enum class check_state
    {
        unchecked = 0,
        checked   = 1,
        mixed     = 2
    };

    // check_state_token
    //   the attribute spelling of a check_state -- what rides in the "state"
    // key and what a backend reads back. Total over the enum.
    D_NODISCARD
    inline const char* check_state_token(
        check_state _state
    )
    {
        switch (_state)
        {
            case check_state::checked: return "checked";
            case check_state::mixed:   return "mixed";
            case check_state::unchecked:
            default:                   return "unchecked";
        }
    }

    // check_box_type
    //   the descriptor for the check_box component, owned by this module.
    D_NODISCARD
    inline const component_type* check_box_type()
    {
        static const component_type descriptor =
            component_type{
                "check_box",
                archetype_toggle,
                option_set{ { { "state",   ::djinterp::option_value(std::string("unchecked")) },
                              { "enabled", ::djinterp::option_value(true) } } },
                0,
                0
            };

        return &descriptor;
    }

    /*
    check_box
      Builds a leaf `check_box` template in the given state.

    Parameter(s):
      _state:   unchecked / checked / mixed; defaults to unchecked ("state").
      _enabled: whether the box is interactive; defaults to true ("enabled").
      _attrs:   optional extra attributes, overlaid on top (caller wins).
    Return:
      A closed (hole-free) component_template wrapping a single check_box node.
    */
    D_NODISCARD
    inline component_template check_box(
        check_state _state   = check_state::unchecked,
        bool        _enabled = true,
        option_set  _attrs   = option_set{}
    )
    {
        option_set resolved = check_box_type()->defaults;
        resolved.set("state",   std::string(check_state_token(_state)));
        resolved.set("enabled", _enabled);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(check_box_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_CHECK_BOX_
