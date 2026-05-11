/******************************************************************************
* uxoxo [imgui]                                       imgui_button_helpers.hpp
*
* Shared button rendering utilities for the ImGui platform layer:
*   The toolbar, tab control, MySQL/MariaDB extras row, and database
* table chrome each independently push three button colors (Button,
* ButtonHovered, ButtonActive) plus a fourth disabled palette, draw
* an ImGui::Button, then pop the same count.  This header consolidates
* that pattern into a single function with two flavors:
*     - accent_button(label, normal, hover, active, disabled)
*         Explicit-color form.  Used by vendor renderers that have
*         their own accent palette outside the shared palette tags
*         (the MySQL ANALYZE / OPTIMIZE / CHECK trio, the MariaDB
*         temporal point-in-time row, etc.).
*
*     - accent_button<NormalTag, HoverTag, ActiveTag>(label, disabled)
*         Tag-driven form.  Pulls the three colors from the shared
*         palette so theme switches propagate without changing call
*         sites.  Common case for toolbar / chrome buttons.
* 
*   Both forms route through the canonical disabled palette tags
* (btn_disabled_tag, btn_text_disabled_tag) when `_disabled` is true,
* so disabled buttons look uniform across the platform regardless of
* their accent.
*   `badge_overlay(count)` is the second consolidation: the toolbar
* renderer's `imgui_draw_badge_overlay` is moved here so any renderer
* that wants to flag a button with a numeric counter (toolbar,
* tab close button, future notification surface) can use it.
*
* Contents:
*   1.  accent_button (explicit colors)
*   2.  accent_button (tag-driven)
*   3.  badge_overlay
*   4.  shape style helpers (rect, rounded, circle, pill)
*
*
* path:      /inc/uxoxo/platform/imgui/button/imgui_button_helpers.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.05.08
******************************************************************************/

#ifndef  UXOXO_COMPONENT_IMGUI_BUTTON_HELPERS_
#define  UXOXO_COMPONENT_IMGUI_BUTTON_HELPERS_ 1

// std
#include <cstdio>
#include <string>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./imgui_palette.hpp"
#include "./imgui_scope.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  1.  ACCENT BUTTON (explicit colors)
// ===========================================================================

/*
accent_button
  Renders an ImGui::Button with explicit (normal, hover, active) color
overrides for ImGuiCol_Button, ImGuiCol_ButtonHovered, and
ImGuiCol_ButtonActive respectively.  When `_disabled` is true, the
three slots are all set to the shared `btn_disabled_tag` palette
entry and the click result is suppressed so disabled buttons never
register interaction even if the cursor passes over them.

  All style state is restored before this function returns - safe to
call from any draw context.

Parameter(s):
  _label:    null-terminated button label.  Pass an "##id"-prefixed
             label for buttons whose visual is fully icon-driven.
  _normal:   ImGuiCol_Button color in the enabled state.
  _hover:    ImGuiCol_ButtonHovered color in the enabled state.
  _active:   ImGuiCol_ButtonActive color in the enabled state.
  _disabled: when true, overrides the three colors with the shared
             disabled palette and discards click events.
Return:
  true iff the button was clicked this frame and `_disabled` was false.
*/
[[nodiscard]] inline bool
accent_button(
    const char*   _label,
    const ImVec4& _normal,
    const ImVec4& _hover,
    const ImVec4& _active,
    bool          _disabled = false
) noexcept
{
    bool clicked;

    if (_disabled)
    {
        scoped_color colors {
            { ImGuiCol_Button,
                  palette::get<palette::btn_disabled_tag>() },
            { ImGuiCol_ButtonHovered,
                  palette::get<palette::btn_disabled_tag>() },
            { ImGuiCol_ButtonActive,
                  palette::get<palette::btn_disabled_tag>() }
        };

        scoped_disabled disabled(true);

        ImGui::Button(_label);

        return false;
    }

    scoped_color colors {
        { ImGuiCol_Button,        _normal },
        { ImGuiCol_ButtonHovered, _hover  },
        { ImGuiCol_ButtonActive,  _active }
    };

    clicked = ImGui::Button(_label);

    return clicked;
}


// ===========================================================================
//  2.  ACCENT BUTTON (tag-driven)
// ===========================================================================

/*
accent_button (tag-driven overload)
  Pulls (normal, hover, active) colors from the shared palette via
the supplied tag types and forwards to the explicit-color overload.
Use for buttons whose accent belongs to the shared palette (toolbar
chrome, primary action buttons); vendor-specific accents that do not
warrant a palette tag should use the explicit-color form instead.

Parameter(s):
  _label:    null-terminated button label.
  _disabled: when true, the three colors are replaced with the shared
             disabled palette and clicks are suppressed.
Return:
  true iff the button was clicked this frame and `_disabled` was false.
*/
template<typename _NormalTag,
          typename _HoverTag,
          typename _ActiveTag = _HoverTag>
[[nodiscard]] inline bool
accent_button(
    const char* _label,
    bool        _disabled = false
) noexcept
{
    return accent_button(
        _label,
        palette::get<_NormalTag>(),
        palette::get<_HoverTag>(),
        palette::get<_ActiveTag>(),
        _disabled);
}


// ===========================================================================
//  3.  BADGE OVERLAY
// ===========================================================================

/*
badge_overlay
  Draws a circular numeric badge over the upper-right corner of the
last-rendered ImGui item.  Pulls badge_bg_tag and badge_text_tag from
the shared palette; the radius comes from badge_radius_tag.  Numbers
above 99 are clamped to "99+" to keep the badge readable inside the
fixed radius.

  No-op when `_count` is 0 - the badge surface is reserved for
non-zero counters so a badge-eligible button can be rendered with
the same call regardless of state.

Parameter(s):
  _count: the badge counter.  0 produces no draw; 1..99 print as-is;
          values >= 100 print "99+".
Return:
  none.
*/
inline void
badge_overlay(
    int _count
) noexcept
{
    char        buffer[8];
    ImDrawList* dl;
    ImVec2      item_max;
    ImVec2      text_sz;
    float       radius;
    float       cx;
    float       cy;

    if (_count <= 0)
    {
        return;
    }

    if (_count > 99)
    {
        std::snprintf(buffer, sizeof(buffer), "99+");
    }
    else
    {
        std::snprintf(buffer, sizeof(buffer), "%d", _count);
    }

    dl       = ImGui::GetWindowDrawList();
    item_max = ImGui::GetItemRectMax();
    text_sz  = ImGui::CalcTextSize(buffer);
    radius   = palette::get<palette::badge_radius_tag>();
    cx       = (item_max.x - 2.0f);
    cy       = (ImGui::GetItemRectMin().y + 2.0f);

    // background circle
    dl->AddCircleFilled(
        ImVec2(cx, cy),
        radius,
        ImGui::GetColorU32(palette::get<palette::badge_bg_tag>()));

    // centered text
    dl->AddText(
        ImVec2(cx - (text_sz.x * 0.5f),
               cy - (text_sz.y * 0.5f)),
        ImGui::GetColorU32(palette::get<palette::badge_text_tag>()),
        buffer);

    return;
}


// ===========================================================================
//  4.  SHAPE STYLE HELPERS
// ===========================================================================

// button_shape_kind
//   enum: shape style for accent buttons.  Mirrors button_shape from
// the toolbar component so renderers can route a uniform value through
// either the typed button path or the accent_button helpers.
enum class button_shape_kind
{
    rect    = 0,
    rounded = 1,
    circle  = 2,
    pill    = 3
};

/*
push_shape_style
  Pushes a single ImGui::PushStyleVar(FrameRounding, ...) corresponding
to `_shape`.  Returns 1 (the count to pass to PopStyleVar) so callers
can either invoke PopStyleVar(N) directly or feed it into a
scoped_style_var that owns the pop.

  Rect uses 0 rounding; rounded uses default_rounding_tag from the
palette; circle uses _corner_radius (since circular buttons control
their corners explicitly); pill uses pill_rounding_tag.

Parameter(s):
  _shape:         the desired shape style.
  _corner_radius: corner radius for circle / custom-rounded buttons.
                  Ignored for rect, rounded, and pill.
Return:
  The number of style-vars pushed (always 1).
*/
inline int
push_shape_style(
    button_shape_kind _shape,
    float             _corner_radius = 0.0f
) noexcept
{
    float rounding;

    switch (_shape)
    {
        case button_shape_kind::rect:
            rounding = 0.0f;
            break;

        case button_shape_kind::rounded:
            rounding = palette::get<palette::default_rounding_tag>();
            break;

        case button_shape_kind::circle:
            rounding = _corner_radius;
            break;

        case button_shape_kind::pill:
            rounding = palette::get<palette::pill_rounding_tag>();
            break;

        default:
            rounding = palette::get<palette::default_rounding_tag>();
            break;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);

    return 1;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_BUTTON_HELPERS_