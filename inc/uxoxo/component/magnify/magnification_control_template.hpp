/******************************************************************************
* uxoxo [component]                         magnification_control_template.hpp
*
* The `magnifier` component: a zoom meta-control over another component.
*   A self-contained module owning one component type, ignorant of every other.
* Unlike the rest of the family, a magnifier is a *meta-control*: its job is to
* scale some OTHER component (an accessibility lens, a design-tool zoom), so its
* "value" is a zoom factor, not content of its own.
*
* THE TARGETING CAVEAT (read this).
*   The old magnification_control held its target as a live reference
* (_TargetRef, a pointer/handle) and mutated it. A component_template is an
* immutable description in a tree, and a tree node cannot hold a live pointer to
* another node. So targeting is modelled by *name*: "target" is a string naming
* the component to magnify, resolved elsewhere (by whatever pass walks the live
* tree and applies the zoom). This makes the magnifier a clean template, but it
* means the actual "apply zoom to that component" step is a behaviour/render
* concern, not tree structure -- and cross-references by name are a convention
* this module introduces, not something the foundation enforces. If the design
* later grows a first-class node-reference attribute, "target" moves onto it.
*
*   Render archetype: magnifier (declared here). Mostly a backend will read the
* zoom and transform the named target rather than draw the magnifier itself.
*
*   Attribute keys: "value" (double, zoom factor -- 1.0 = none), "min" / "max" /
* "step" (double, bounds and increment for a controller), "mode" (string:
* "fullscreen"/"region"/"follow"/"fixed"), "target" (string, the magnified
* component's name), "enabled" (bool), "active" (bool -- whether zoom is applied
* right now, a bypass switch). Arity: 0.
*
*   Declarative form: the clamping helpers (mc_set_zoom, mc_zoom_in, ...), the
* focus/region geometry, and the on_change / on_commit callbacks are gone --
* clamping and stepping act on a live control and ride the behaviour layer; the
* focus rectangle can return as extra keys when a backend needs it.
*
* USAGE:
*   using namespace uxoxo;
*   component_template m = component::magnifier(2.0,
*       { { { "target", ::djinterp::option_value(std::string("canvas")) },
*           { "mode",   ::djinterp::option_value(std::string("follow")) } } });
*
* path:      /inc/uxoxo/magnification_control_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_MAGNIFICATION_CONTROL_TEMPLATE_
#define UXOXO_MAGNIFICATION_CONTROL_TEMPLATE_ 1

// std
#include <string>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_magnifier
//   constant: the render archetype for a zoom meta-control. Declared here (the
// open-archetype pattern), at uxoxo:: root.
inline constexpr const char* archetype_magnifier = "magnifier";


NS_COMPONENT

    // magnifier_type
    //   the descriptor for the magnification control, owned by this module.
    D_NODISCARD
    inline const component_type* magnifier_type()
    {
        static const component_type descriptor =
            component_type{
                "magnifier",
                archetype_magnifier,
                option_set{ { { "value",   ::djinterp::option_value(1.0) },
                              { "min",     ::djinterp::option_value(1.0) },
                              { "max",     ::djinterp::option_value(10.0) },
                              { "step",    ::djinterp::option_value(0.25) },
                              { "mode",    ::djinterp::option_value(std::string("fullscreen")) },
                              { "target",  ::djinterp::option_value(std::string()) },
                              { "enabled", ::djinterp::option_value(true) },
                              { "active",  ::djinterp::option_value(false) } } },
                0,
                0
            };

        return &descriptor;
    }

    /*
    magnifier
      Builds a leaf `magnifier` template carrying a zoom factor.

    Parameter(s):
      _zoom:  the zoom factor (stored under "value"); 1.0 = no magnification.
      _attrs: optional extra attributes -- "target", "mode", "min"/"max"/"step",
              "active" -- overlaid on top (caller wins).
    Return:
      A closed (hole-free) component_template wrapping a single magnifier node.
    */
    D_NODISCARD
    inline component_template magnifier(
        double     _zoom  = 1.0,
        option_set _attrs = option_set{}
    )
    {
        option_set resolved = magnifier_type()->defaults;
        resolved.set("value", _zoom);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(magnifier_type(), resolved, {});
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_MAGNIFICATION_CONTROL_TEMPLATE_
