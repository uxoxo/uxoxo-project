/*******************************************************************************
* uxoxo [platform]                                            imgui_platform.hpp
*
* A Dear ImGui backend: templates realized as immediate-mode draw functions.
*   realize.hpp anticipates this backend: an immediate-mode platform registers
* no overrides of its own and renders by archetype, and its blueprint is a
* draw function. Here the blueprint pairs that function with a plain-text
* projection of the subtree, because a folded child is otherwise opaque -- a
* button needs its label element's text, not just a way to draw it.
*   Realizing stays pure: it builds functions and touches no ImGui state.
* Calling a blueprint's draw inside an ImGui window draws the template, and
* any interaction is posted to the imgui_frame passed in, as a uxoxo event
* whose kind is the element's "action" attribute. The host applies those events
* after the frame. Interactive archetypes without an action draw but post
* nothing.
*   Interpreted archetypes: text_leaf, container (row, and column through its
* "orientation" attribute), interactive, panel, tab_bar, tab, text_field,
* toggle, selectable and separator. Any other archetype degrades to its
* children drawn top to bottom, or its "text" attribute when it has none.
* Application element types get bespoke drawing the framework's way, by
* registering a renderer with set_renderer<imgui_platform>.
*
* path:      /inc/uxoxo/platform/imgui_platform.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.09.22
*******************************************************************************/

/*
TABLE OF CONTENTS
=================
1.  FRAME AND BLUEPRINT
    -------------------
    1.  imgui_frame
    2.  imgui_blueprint
2.  THE PLATFORM
    ------------
    1.  imgui_tones
    2.  imgui_platform
*/

#ifndef UXOXO_PLATFORM_IMGUI_PLATFORM_HPP
#define UXOXO_PLATFORM_IMGUI_PLATFORM_HPP 1

// std
#include <functional>  // std::function
#include <string>      // std::string
#include <vector>      // std::vector
// uxoxo
#include "../element.hpp"  // element_node, event, hole, option_set
#include "../uxoxo.hpp"    // NS_UXOXO, NS_PLATFORM


NS_UXOXO
NS_PLATFORM


//==============================================================================
// 1.  FRAME AND BLUEPRINT
//==============================================================================


// 1.1    imgui_frame
//------------------------------------------------------------------------------
// 1.1.1
// imgui_frame
//   struct: what one drawn frame hands back to its host -- the events its
// interactive elements posted, in the order they happened.
struct imgui_frame
{
    std::vector<event> events;

    // post
    //   modifier: records an interaction. _kind is the element's action.
    void post(const std::string& _kind,
              option_set         _payload = option_set{});
};

// 1.2    imgui_blueprint
//------------------------------------------------------------------------------
// 1.2.1
// imgui_draw
//   type: draws a realized subtree into the current ImGui window.
using imgui_draw = std::function<void(imgui_frame&)>;

// 1.2.2
// imgui_blueprint
//   struct: the platform's blueprint -- how to draw a subtree, and its text.
// An empty draw draws nothing.
struct imgui_blueprint
{
    imgui_draw  draw;
    std::string text;
};


//==============================================================================
// 2.  THE PLATFORM
//==============================================================================


// 2.1    imgui_tones
//------------------------------------------------------------------------------
// 2.1.1
// imgui_tones
//   struct: the colours behind the "tone" attribute (RGBA, 0..1) and the size
// of a heading relative to body text. The host sets these to match its theme.
struct imgui_tones
{
    float muted[4]  = { 0.45f, 0.45f, 0.45f, 1.00f };
    float accent[4] = { 0.20f, 0.40f, 0.80f, 1.00f };
    float error[4]  = { 0.75f, 0.15f, 0.10f, 1.00f };
    float ok[4]     = { 0.15f, 0.50f, 0.20f, 1.00f };
    float heading   = 1.30f;
};

// 2.2    imgui_platform
//------------------------------------------------------------------------------
// 2.2.1
// imgui_platform
//   platform: the ImGui backend, driven by archetype tokens.
struct imgui_platform
{
    using blueprint = imgui_blueprint;

    // on_pure
    //   algebra: an unfilled slot draws as a visible placeholder.
    struct on_pure
    {
        blueprint operator()(const hole& _hole) const;
    };

    // on_archetype
    //   algebra: interprets the archetype token; degrades on an unknown one.
    struct on_archetype
    {
        blueprint operator()(const element_node<blueprint>& _node) const;
    };

    // tones
    //   accessor: the process-wide tone table the archetypes read.
    static imgui_tones& tones();
};


NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_PLATFORM_IMGUI_PLATFORM_HPP
