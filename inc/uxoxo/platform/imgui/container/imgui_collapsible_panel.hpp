/*******************************************************************************
* uxoxo [imgui]                                           collapsible_panel.hpp
*
* ImGui backend for the collapsible_panel template component:
*   A Dear ImGui rendering adapter for the vendor-agnostic
* `uxoxo::component::collapsible_panel<>` template.  Drives
* ImGui::CollapsingHeader from the retained-state panel struct and
* syncs user-driven open/close transitions back into the panel's
* `.expanded` flag (firing on_change when cpf_change_callback is
* enabled).
*
*   Two entry points are provided:
*
*     render_header(panel)          — renders only the header row;
*                                      returns true if the caller
*                                      should draw the body on this
*                                      frame (visible && expanded).
*                                      Useful for callers that need
*                                      full control over body layout.
*
*     render(panel, body_fn)        — renders header AND, if the
*                                      panel is currently open,
*                                      auto-indents and invokes
*                                      body_fn(panel.body).
*
*   Feature-flag handling:
*   Every optional mixin member (.label, .summary, .on_change) is
* accessed behind `if constexpr (_Features & cpf_*)`, so even a
* cpf_none-instantiated panel compiles and renders — it just
* shows an unlabeled header (disambiguated by the panel's memory
* address via ImGui::PushID).
*
*   Animation:
*   ImGui::CollapsingHeader does not animate its collapse / expand
* transition natively; this backend does not attempt to drive the
* cpf_animated mixin's animation_progress field.  Callers who want
* animated body reveal should render the body conditionally on
* animation_progress > 0 and scale / fade accordingly.
*
* Contents:
*   1  Emphasis → ImVec4 mapping
*   2  render_header  — header-only
*   3  render         — header + body callable
*
*
* path:      /inc/uxoxo/platform/imgui/container/collapsible_panel.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_IMGUI_COMPONENT_COLLAPSIBLE_PANEL_
#define  UXOXO_IMGUI_COMPONENT_COLLAPSIBLE_PANEL_ 1

// std
#include <string>
#include <utility>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/component_types.hpp"
#include "../../../templates/component/container/collapsible_panel.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI


// ===============================================================================
//  1  EMPHASIS → IMVEC4 MAPPING
// ===============================================================================

// emphasis_to_header_tint
//   function: translates a DEmphasis value from the vendor-agnostic
// component vocabulary into an ImVec4 suitable for pushing onto
// ImGuiCol_Header.  Colors are approximate and intentionally close
// to Dear ImGui's default blue palette so `normal` emphasis blends
// with the host application's theme.
[[nodiscard]] inline ImVec4
emphasis_to_header_tint(component::DEmphasis _e) noexcept
{
    switch (_e)
    {
        case component::DEmphasis::normal:    return ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
        case component::DEmphasis::muted:     return ImVec4(0.40f, 0.40f, 0.40f, 0.31f);
        case component::DEmphasis::primary:   return ImVec4(0.26f, 0.59f, 0.98f, 0.55f);
        case component::DEmphasis::secondary: return ImVec4(0.50f, 0.50f, 0.60f, 0.40f);
        case component::DEmphasis::success:   return ImVec4(0.30f, 0.70f, 0.30f, 0.45f);
        case component::DEmphasis::warning:   return ImVec4(0.85f, 0.70f, 0.20f, 0.50f);
        case component::DEmphasis::danger:    return ImVec4(0.85f, 0.25f, 0.25f, 0.50f);
        case component::DEmphasis::info:      return ImVec4(0.30f, 0.65f, 0.85f, 0.45f);
    }

    return ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
}




// ===============================================================================
//  2  RENDER HEADER
// ===============================================================================

// render_header
//   function: draws the panel's header row via ImGui::CollapsingHeader
// and reconciles any user-driven open/close transition with the
// panel's `.expanded` flag.  Returns true if the caller should draw
// the body this frame (panel.visible && panel.expanded); false
// otherwise.  Does not render the body — callers are free to wrap
// their body in Indent/Unindent, BeginChild, or any other ImGui
// construct they prefer.
template <typename _Body,
          unsigned _Features>
bool render_header(component::collapsible_panel<_Body, _Features>& _p)
{
    // invisible panels draw nothing and consume no frame budget
    if (!_p.visible)
    {
        return false;
    }

    // -- build the effective label ------------------------------------
    //   cpf_labeled supplies the primary text; cpf_summary appends a
    // summary clause while collapsed.  Both fall back to an empty
    // string when their feature bit is not set.

    std::string label;

    if constexpr ((_Features & component::cpf_labeled) != 0u)
    {
        label = _p.label;
    }

    if constexpr ((_Features & component::cpf_summary) != 0u)
    {
        // only show the summary while collapsed — an expanded panel
        // already reveals its body, rendering the summary redundant
        if (!_p.expanded && !_p.summary.empty())
        {
            if (!label.empty())
            {
                label += "  -  ";
            }

            label += _p.summary;
        }
    }

    // -- style scope --------------------------------------------------
    //   Push emphasis tint only for non-default emphasis so the style
    // stack stays shallow for the common case.

    const bool is_styled = (_p.emph != component::DEmphasis::normal);

    if (is_styled)
    {
        ImGui::PushStyleColor(ImGuiCol_Header,
                              emphasis_to_header_tint(_p.emph));
    }

    // disabled panels are rendered through ImGui's BeginDisabled/
    // EndDisabled pair so interactions are suppressed and the widget
    // draws with the themed disabled alpha
    const bool is_disabled = !_p.enabled;

    if (is_disabled)
    {
        ImGui::BeginDisabled();
    }

    // -- identity -----------------------------------------------------
    //   ImGui identifies widgets by label-derived hash.  The panel
    // may not carry a label (cpf_labeled unset) or two panels may
    // share a label, so disambiguate using the panel's address.

    ImGui::PushID(static_cast<const void*>(&_p));

    // -- state sync (model -> imgui) ----------------------------------
    //   ImGuiCond_Always forces ImGui's stored open state to match
    // our model every frame.  User clicks still register because
    // CollapsingHeader processes input AFTER reading the forced
    // state, and its return value reflects the post-click state.

    ImGui::SetNextItemOpen(_p.expanded, ImGuiCond_Always);

    const char* const imgui_label =
        label.empty() ? "##collapsible_panel" : label.c_str();
    const bool reported =
        ImGui::CollapsingHeader(imgui_label);

    ImGui::PopID();

    if (is_disabled)
    {
        ImGui::EndDisabled();
    }

    if (is_styled)
    {
        ImGui::PopStyleColor();
    }

    // -- state sync (imgui -> model) ----------------------------------
    //   A divergence between our forced value and the reported value
    // means the user toggled the header this frame.  Mirror the new
    // state and fire the change callback if the feature is enabled.

    if (reported != _p.expanded)
    {
        _p.expanded = reported;

        if constexpr ((_Features & component::cpf_change_callback) != 0u)
        {
            if (_p.on_change)
            {
                _p.on_change(_p.expanded);
            }
        }
    }

    return _p.expanded;
}




// ===============================================================================
//  3  RENDER
// ===============================================================================

// render
//   function: composes render_header with an auto-indented body
// pass.  Invokes _body_fn(panel.body) iff the panel is visible and
// expanded on this frame.  The body callable receives a non-const
// reference to the panel's _Body payload so it can mutate contained
// state.
//
//   The indent() / unindent() bracket matches ImGui's CollapsingHeader
// convention: the widget itself does not indent its children, so the
// adapter does it explicitly for visual consistency with TreeNode.
template <typename _Body,
          unsigned _Features,
          typename _BodyFn>
void render(component::collapsible_panel<_Body, _Features>& _p,
            _BodyFn&&                                       _body_fn)
{
    // bail before touching the indent stack if the header isn't open
    if (!render_header(_p))
    {
        return;
    }

    ImGui::Indent();
    std::forward<_BodyFn>(_body_fn)(_p.body);
    ImGui::Unindent();

    return;
}


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_COLLAPSIBLE_PANEL_