/*******************************************************************************
* uxoxo [imgui]                                                 imgui_label.hpp
*
* Dear ImGui renderer for label_control:
*   Provides a single `imgui::render(const label_control&)` overload
* that draws the label into the current ImGui window.  Non-mutating:
* takes a const reference.  Callers wanting to update the displayed
* text should do so through `set_value(_lb, new_text)` from
* component_common.hpp or `lb_append(_lb, text)` from label.hpp -
* both fire the label's on_change callback.
*
*   The renderer honors every field on label_control: .visible is a
* hard skip, .enabled drives a scoped_disabled guard from imgui_scope,
* .alignment advances the cursor via alignment_offset, and the
* foreground color is resolved through resolve_style_or<fg_tag> so a
* style-readable label that carries an fg_tag property overrides the
* emphasis palette.  Components that don't opt into the style
* protocol fall back to emphasis_color(_lb.emph) exactly as before.
*
*   Migration note (2026.05.08): the manual BeginDisabled / EndDisabled
* and PushStyleColor / PopStyleColor pairs are now expressed as RAII
* guards from imgui_scope.hpp.  Codegen is unchanged - the guards
* inline to the same conditional begin/end and balanced push/pop
* sequences - but the function is shorter and exception-safe.
*
*
* path:      /inc/uxoxo/platform/imgui/label/imgui_label.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_IMGUI_COMPONENT_LABEL_
#define  UXOXO_IMGUI_COMPONENT_LABEL_ 1

// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/label/label.hpp"
#include "../core/imgui_scope.hpp"
#include "../style/imgui_style.hpp"


NS_UXOXO
NS_IMGUI


using uxoxo::component::label_control;

// ===========================================================================
//  1  LABEL CONTROL
// ===========================================================================

/*
render  (label_control)
  Renders a label_control into the current ImGui window.  Honors:
  - .visible   - skipped entirely when false.
  - .enabled   - wrapped in a scoped_disabled guard.  Components with
                 a read_only member are also covered (label_control
                 has none, so the guard reduces to !_lb.enabled).
  - foreground - resolved via resolve_style_or<fg_tag> with
                 emphasis_color(_lb.emph) as the fallback.  A
                 style-readable label_control variant that carries
                 an fg_tag property overrides the emphasis palette.
  - .alignment - the text cursor is advanced by alignment_offset
                 so the text sits left / center / right inside the
                 current content region.

  Non-mutating.  Text updates should flow through
`set_value(_lb, ...)` or `lb_append(_lb, ...)` so the on_change
callback fires.

Parameter(s):
  _lb: the label_control to render.
Return:
  none.
*/
inline void
render(
    const label_control& _lb
)
{
    ImVec4 color;
    float  avail;
    float  text_w;
    float  offset;

    if (!_lb.visible)
    {
        return;
    }

    color  = resolve_style_or<fg_tag>(_lb, emphasis_color(_lb.emph));
    avail  = ImGui::GetContentRegionAvail().x;
    text_w = ImGui::CalcTextSize(_lb.value.c_str()).x;
    offset = alignment_offset(_lb.alignment, avail, text_w);

    if (offset > 0.0f)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    }

    // RAII guards mirror the original push order; destructors fire in
    // reverse at function exit so PopStyleColor precedes EndDisabled
    scoped_disabled disabled = make_disabled_scope(_lb);
    scoped_color    colors {
        { ImGuiCol_Text, color }
    };

    ImGui::TextUnformatted(_lb.value.c_str());

    return;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_LABEL_