/*******************************************************************************
* uxoxo [imgui]                                              imgui_checkbox.hpp
*
* Dear ImGui renderer for DCheckState-valued components:
*   Provides a single `imgui::render(_Type&)` overload SFINAE-gated on
* has_check_state_value_v<_Type> from toggleable_common.hpp.  This
* covers every checkbox<...> template instance (all of which carry a
* DCheckState value) and any future tri-state component that adopts
* the same shape — no one-line addition per new instantiation.
*
*   Click mutations flow through the generic `toggle` from
* toggleable_common.hpp rather than touching .value directly.  That
* way on_change fires through set_value, undo state is recorded
* automatically if the component's instantiation enabled the undo
* mixin, and any future decoration those verbs grow (rate-limiting,
* validation, telemetry) applies to ImGui-driven edits for free.
*
*   Colors are resolved through resolve_style_or: fg_tag drives the
* label text color (ImGuiCol_Text), check_mark_tag drives the glyph
* color (ImGuiCol_CheckMark).  Components that don't opt into the
* style protocol inherit whatever colors the ambient ImGui style
* provides — the style branch compiles out entirely for those.
*
*   Tri-state display uses ImGui::PushItemFlag(ImGuiItemFlags_MixedValue,
* ...), which requires imgui_internal.h.  If a project needs to stay
* strictly on the public ImGui API, that single call can be swapped
* for an InvisibleButton + draw-list rendering in a follow-up; the
* public render() signature will not change.
*
*   Binary-mode checkboxes (instantiated with _TriState == false) still
* use DCheckState as their storage type; the mixed state simply never
* appears in their state machine.  The renderer treats all three
* states uniformly at the display layer.
*
* Contents:
*   1  internal::checkbox_label_ptr — label / anonymous-id resolution
*   2  render(_Type&) — DCheckState-valued checkbox
*
*
* path:      /inc/uxoxo/platform/imgui/checkbox/imgui_checkbox.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_IMGUI_COMPONENT_CHECKBOX_DRAW_
#define  UXOXO_IMGUI_COMPONENT_CHECKBOX_DRAW_ 1

// std
#include <type_traits>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/checkbox/checkbox.hpp"
#include "../../../templates/component/component_traits.hpp"
#include "../../../templates/component/component_types.hpp"
#include "../../../templates/component/switch/toggleable_common.hpp"
#include "../style/imgui_style.hpp"


NS_UXOXO
NS_COMPONENT
NS_IMGUI


// ===============================================================================
//  1  DETAIL HELPERS
// ===============================================================================

NS_INTERNAL

    // checkbox_label_ptr
    //   function: resolves the display string for a checkbox, returning
    // an anonymous "##checkbox" label when the component was
    // instantiated without the label_data mixin.  This keeps ImGui's
    // click target wide enough to hit without forcing the component
    // to carry a label string it does not need.
    template <typename _Type>
    [[nodiscard]] inline const char*
    checkbox_label_ptr(
        const _Type& _c
    ) noexcept
    {
        if constexpr (has_label_v<_Type>)
        {
            return _c.label.c_str();
        }
        else
        {
            (void)_c;

            return "##checkbox";
        }
    }

}   // NS_INTERNAL




// ===============================================================================
//  2  CHECKBOX RENDERER
// ===============================================================================

/*
render  (DCheckState-valued component)
  Renders any component whose .value is a DCheckState as an ImGui
checkbox.  Uses ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, ...)
to draw the canonical mixed / indeterminate glyph when the current
state is DCheckState::mixed.  Reads:
  - .visible         — skipped when false.
  - .enabled         — wrapped in BeginDisabled when false.
  - .read_only       — wrapped in BeginDisabled when true.
  - .label           — displayed as the checkbox label when the
                       label_data mixin is active; otherwise an
                       anonymous id is used.
  - fg_tag           — resolved for the label text color
                       (ImGuiCol_Text).  Fallback: the ambient
                       ImGui text color.
  - check_mark_tag   — resolved for the glyph color
                       (ImGuiCol_CheckMark).  Fallback: the ambient
                       ImGui check-mark color.

  On click, dispatches through `toggle` from toggleable_common.hpp
which gives checked -> unchecked and (unchecked | mixed) -> checked,
and then invokes on_commit if present.  on_change is already fired
by set_value inside toggle, so no explicit on_change dispatch here.

Parameter(s):
  _c: the component to render.  May be mutated on click.
Return:
  none.
*/
template <typename _Type,
          std::enable_if_t<
              has_check_state_value_v<_Type>,
              int> = 0>
void
render(
    _Type& _c
)
{
    const char* label;
    bool        disabled;
    bool        mixed;
    bool        checked_visual;
    bool        clicked;
    ImVec4      text_color;
    ImVec4      mark_color;

    if (!_c.visible)
    {
        return;
    }

    label          = internal::checkbox_label_ptr(_c);
    disabled       = ( (!_c.enabled) ||
                       (_c.read_only) );
    mixed          = (_c.value == DCheckState::mixed);
    checked_visual = (_c.value == DCheckState::checked);

    text_color = resolve_style_or<fg_tag>(
        _c,
        ImGui::GetStyleColorVec4(ImGuiCol_Text));
    mark_color = resolve_style_or<check_mark_tag>(
        _c,
        ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));

    if (disabled)
    {
        ImGui::BeginDisabled();
    }

    ImGui::PushStyleColor(ImGuiCol_Text,      text_color);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, mark_color);
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);

    clicked = ImGui::Checkbox(label, &checked_visual);

    ImGui::PopItemFlag();
    ImGui::PopStyleColor(2);

    if (disabled)
    {
        ImGui::EndDisabled();
    }

    // route the click through the generic toggleable verbs so that
    // on_change, undo state, and any future decoration in set_value
    // all run exactly as they do for programmatic mutations
    if (clicked)
    {
        toggle(_c);

        if (_c.on_commit)
        {
            _c.on_commit(_c.value);
        }
    }

    return;
}


NS_END  // imgui
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_CHECKBOX_DRAW_