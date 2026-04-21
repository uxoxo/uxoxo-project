/*******************************************************************************
* uxoxo [component]                              magnification_control_imgui.hpp
*
* ImGui backend for magnification_control:
*   A free function `render(magnification_control&)` that draws the
* magnifier's control surface (label, zoom slider, zoom-in / zoom-out
* / reset buttons, mode dropdown, region inputs when the mode wants
* them) and dispatches user input through the mc_* mutators.  Returns
* a `magnification_control_event` describing what changed during the
* frame.
*
*   The renderer does not perform the actual zoom — it only mutates
* the control's state.  The caller is responsible for observing the
* control (typically through its on_change callback) and applying the
* zoom factor to the target during the target's own render pass.
*
*   Scalar handling:
*     The control's `_Scalar` is an arithmetic template parameter
*   (defaults to double).  ImGui's slider widgets work in float, so
*   the renderer reads the current value as float, presents the
*   slider, and writes back via mc_set_zoom — which clamps to
*   [min_zoom, max_zoom] and only fires on_change when the value
*   actually changes.  Precision loss across the float round-trip
*   only matters for sliders; programmatic mutations bypass it.
*
*   ImGui dependencies:
*     - <imgui.h> for the core API.
*
*
* path:      /inc/uxoxo/component/imgui/magnification_control_imgui.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_MAGNIFICATION_CONTROL_IMGUI_
#define  UXOXO_COMPONENT_MAGNIFICATION_CONTROL_IMGUI_ 1

// std
#include <type_traits>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../magnification_control.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  EVENT STRUCT
// ===============================================================================

// magnification_control_event
//   struct: per-frame event report from rendering a magnification_control.
// All flags default to false.
struct magnification_control_event
{
    bool zoom_changed   = false;  // value mutated this frame
    bool zoomed_in      = false;  // zoom-in button fired
    bool zoomed_out     = false;  // zoom-out button fired
    bool reset          = false;  // reset button fired
    bool mode_changed   = false;  // mode was changed
    bool focus_changed  = false;  // focus point was changed
    bool region_changed = false;  // region dimensions were changed
};




// ===============================================================================
//  2  RENDER
// ===============================================================================

/*
render
  Draws the magnifier's control surface for one ImGui frame.  Layout:

      [Label:]
      [-]  [zoom slider (min_zoom .. max_zoom)]  [+]  [Reset]
      [mode dropdown]
      [focus_x] [focus_y]   (when mode in {region, follow, fixed})
      [width]   [height]    (when mode in {region, follow})

  All zoom mutations route through `mc_set_zoom`, which clamps to
the configured bounds and short-circuits when the clamped value
matches the current value.  This means rapidly dragging the slider
past `max_zoom` produces a single on_change event when the bound is
reached, not one per frame.

Parameter(s):
  _m: the magnification control to render.
Return:
  A magnification_control_event describing what changed during the
  frame.
*/
template <typename _TargetRef,
          typename _Scalar,
          bool     _Labeled,
          bool     _Clearable>
magnification_control_event
render(
    magnification_control<_TargetRef,
                          _Scalar,
                          _Labeled,
                          _Clearable>& _m
)
{
    static_assert(std::is_arithmetic_v<_Scalar>,
                  "render(magnification_control): _Scalar must be "
                  "arithmetic (float, double, int, etc.).");

    magnification_control_event evt;
    _Scalar                     pre_value;

    if (!_m.visible)
    {
        return evt;
    }

    pre_value = _m.value;

    ImGui::PushID(static_cast<const void*>(&_m));

    if constexpr (_Labeled)
    {
        if (!_m.label.empty())
        {
            ImGui::TextUnformatted(_m.label.c_str());
        }
    }

    if (!_m.enabled)
    {
        ImGui::BeginDisabled();
    }

    // -- zoom-out button ---------------------------------------------
    if (ImGui::Button("-"))
    {
        mc_zoom_out(_m);
        evt.zoomed_out = true;
    }

    ImGui::SameLine();

    // -- zoom slider -------------------------------------------------
    {
        float zoom_f = static_cast<float>(_m.value);
        float min_f  = static_cast<float>(_m.min_zoom);
        float max_f  = static_cast<float>(_m.max_zoom);

        ImGui::SetNextItemWidth(180.0f);

        if (ImGui::SliderFloat("##zoom",
                               &zoom_f,
                               min_f,
                               max_f,
                               "%.2fx"))
        {
            mc_set_zoom(_m, static_cast<_Scalar>(zoom_f));
        }
    }

    ImGui::SameLine();

    // -- zoom-in button ----------------------------------------------
    if (ImGui::Button("+"))
    {
        mc_zoom_in(_m);
        evt.zoomed_in = true;
    }

    ImGui::SameLine();

    // -- reset button ------------------------------------------------
    if (ImGui::Button("Reset"))
    {
        mc_reset_zoom(_m);
        evt.reset = true;
    }

    // -- mode dropdown -----------------------------------------------
    {
        const char* mode_labels[] = {
            "Fullscreen", "Region", "Follow", "Fixed"
        };
        int mode_idx = static_cast<int>(_m.mode);

        ImGui::SetNextItemWidth(120.0f);

        if (ImGui::Combo("Mode", &mode_idx, mode_labels, 4))
        {
            mc_set_mode(_m, static_cast<DMagnificationMode>(mode_idx));
            evt.mode_changed = true;
        }
    }

    // -- focus point (region / follow / fixed only) ------------------
    if (_m.mode != DMagnificationMode::fullscreen)
    {
        int focus[2] = { _m.focus_x, _m.focus_y };

        ImGui::SetNextItemWidth(150.0f);

        if (ImGui::InputInt2("Focus (x, y)", focus))
        {
            mc_set_focus(_m, focus[0], focus[1]);
            evt.focus_changed = true;
        }
    }

    // -- region dimensions (region / follow only) --------------------
    if ( (_m.mode == DMagnificationMode::region) ||
         (_m.mode == DMagnificationMode::follow) )
    {
        int region[2] = { _m.region_width, _m.region_height };

        ImGui::SetNextItemWidth(150.0f);

        if (ImGui::InputInt2("Region (w, h)", region))
        {
            mc_set_region(_m, region[0], region[1]);
            evt.region_changed = true;
        }
    }

    if (!_m.enabled)
    {
        ImGui::EndDisabled();
    }

    ImGui::PopID();

    // -- compute zoom_changed (covers slider, +, -, reset) -----------
    if (_m.value != pre_value)
    {
        evt.zoom_changed = true;
    }

    return evt;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MAGNIFICATION_CONTROL_IMGUI_
