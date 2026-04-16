/*******************************************************************************
* uxoxo [component]                                     imgui_toolbar_draw.hpp
*
*   Dear ImGui draw handler for the toolbar and button components.
*
*   Renders a toolbar as a fixed child window anchored to the specified
* edge of the parent window.  Each button entry is drawn with full
* support for the button feature set: icon, tooltip, shape (rect,
* rounded, circle, pill), toggle highlight, badge counter, shortcut
* label, and per-button color overrides.
*
*   Separators draw a thin vertical (or horizontal) line.  Spacers
* push remaining entries to the far edge.
*
*   The typed draw function imgui_draw_button<Button> handles a
* concrete button instantiation and can be called standalone.  The
* toolbar draw function uses the type-erased toolbar_button_iface
* for its entries.
*
*   Structure:
*     1.  style constants
*     2.  internal helpers (shape rendering, badge overlay)
*     3.  imgui_draw_button (typed, standalone)
*     4.  imgui_draw_toolbar_button (type-erased, for toolbar entries)
*     5.  imgui_draw_toolbar (main entry point)
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included before
* this header.
*
*
* path:      /inc/uxoxo/component/renderer/imgui/imgui_toolbar_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_COMPONENT_IMGUI_TOOLBAR_DRAW_
#define UXOXO_COMPONENT_IMGUI_TOOLBAR_DRAW_ 1

#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>
#include "../../../uxoxo.hpp"
#include "../../button.hpp"
#include "../../toolbar.hpp"
#include "../render_context.hpp"

// Dear ImGui — caller must have included imgui.h before this header.
#ifndef IMGUI_VERSION
    #error "imgui.h must be included before imgui_toolbar_draw.hpp"
#endif


NS_UXOXO
NS_COMPONENT


// =============================================================================
//  1.  STYLE CONSTANTS
// =============================================================================

namespace imgui_toolbar_style
{
    // toolbar background
    D_INLINE const ImVec4 bar_bg          = ImVec4(0.13f, 0.14f, 0.17f, 1.0f);
    D_INLINE const ImVec4 bar_border      = ImVec4(0.22f, 0.23f, 0.26f, 1.0f);

    // button defaults
    D_INLINE const ImVec4 btn_bg          = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
    D_INLINE const ImVec4 btn_hover       = ImVec4(0.28f, 0.30f, 0.36f, 1.0f);
    D_INLINE const ImVec4 btn_active      = ImVec4(0.18f, 0.19f, 0.22f, 1.0f);
    D_INLINE const ImVec4 btn_disabled    = ImVec4(0.16f, 0.16f, 0.18f, 0.60f);
    D_INLINE const ImVec4 btn_text        = ImVec4(0.82f, 0.82f, 0.85f, 1.0f);
    D_INLINE const ImVec4 btn_text_disabled = ImVec4(0.45f, 0.45f, 0.48f, 0.70f);

    // toggle
    D_INLINE const ImVec4 toggle_on_bg    = ImVec4(0.22f, 0.45f, 0.68f, 0.80f);
    D_INLINE const ImVec4 toggle_on_border= ImVec4(0.35f, 0.60f, 0.85f, 0.60f);

    // badge
    D_INLINE const ImVec4 badge_bg        = ImVec4(0.85f, 0.25f, 0.20f, 1.0f);
    D_INLINE const ImVec4 badge_text      = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    D_INLINE constexpr float badge_radius = 8.0f;

    // separator
    D_INLINE const ImVec4 separator_color = ImVec4(0.30f, 0.30f, 0.34f, 0.60f);
    D_INLINE constexpr float separator_thickness = 1.0f;
    D_INLINE constexpr float separator_padding   = 6.0f;

    // tooltip
    D_INLINE constexpr float tooltip_delay = 0.5f;

    // sizes by button_size
    D_INLINE constexpr float size_small_h  = 22.0f;
    D_INLINE constexpr float size_medium_h = 28.0f;
    D_INLINE constexpr float size_large_h  = 34.0f;

    // shape
    D_INLINE constexpr float default_rounding = 4.0f;
    D_INLINE constexpr float pill_rounding    = 14.0f;

}   // namespace imgui_toolbar_style


// =============================================================================
//  2.  INTERNAL HELPERS
// =============================================================================

NS_INTERNAL

    // imgui_button_height
    //   function: returns the pixel height for a button_size.
    D_INLINE float
    imgui_button_height(
        button_size _sz
    )
    {
        switch (_sz)
        {
            case button_size::small:  return imgui_toolbar_style::size_small_h;
            case button_size::large:  return imgui_toolbar_style::size_large_h;
            case button_size::medium:
            default:                  return imgui_toolbar_style::size_medium_h;
        }
    }

    // imgui_push_button_shape_style
    //   function: pushes ImGui style vars for a given button_shape.
    // Returns the number of style vars pushed (caller must pop).
    D_INLINE int
    imgui_push_button_shape_style(
        button_shape _shape,
        float        _radius
    )
    {
        switch (_shape)
        {
            case button_shape::rect:
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

                return 1;

            case button_shape::rounded:
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, _radius);

                return 1;

            case button_shape::circle:
                ImGui::PushStyleVar(
                    ImGuiStyleVar_FrameRounding, 999.0f);

                return 1;

            case button_shape::pill:
                ImGui::PushStyleVar(
                    ImGuiStyleVar_FrameRounding,
                    imgui_toolbar_style::pill_rounding);

                return 1;

            case button_shape::custom:
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, _radius);

                return 1;
        }

        return 0;
    }

    // imgui_draw_badge_overlay
    //   function: draws a badge counter at the top-right corner of
    // the last item.
    D_INLINE void
    imgui_draw_badge_overlay(
        int _count
    )
    {
        if (_count <= 0)
        {
            return;
        }

        ImVec2 item_max = ImGui::GetItemRectMax();
        ImDrawList* dl  = ImGui::GetWindowDrawList();

        std::string text = (_count > 99)
            ? "99+"
            : std::to_string(_count);

        ImVec2 text_sz = ImGui::CalcTextSize(text.c_str());

        float r  = imgui_toolbar_style::badge_radius;
        float cx = item_max.x - 2.0f;
        float cy = ImGui::GetItemRectMin().y + 2.0f;

        // background circle
        dl->AddCircleFilled(ImVec2(cx, cy),
                            r,
                            ImGui::GetColorU32(
                                imgui_toolbar_style::badge_bg));

        // text centered in circle
        dl->AddText(ImVec2(cx - text_sz.x * 0.5f,
                           cy - text_sz.y * 0.5f),
                    ImGui::GetColorU32(
                        imgui_toolbar_style::badge_text),
                    text.c_str());

        return;
    }

NS_END  // internal


// =============================================================================
//  3.  IMGUI DRAW BUTTON (typed, standalone)
// =============================================================================
//   Renders a single button with all its compile-time features.
// Can be called outside a toolbar context.

// imgui_draw_button
//   function: renders a button<_F, _I> using Dear ImGui.
// Returns true if the button was clicked this frame.
template <unsigned _F,
          typename _I>
bool
imgui_draw_button(
    button<_F, _I>& _btn,
    render_context& _ctx
)
{
    (void)_ctx;

    if (!_btn.visible)
    {
        return false;
    }

    // reset per-frame state
    _btn.pressed = false;
    _btn.hovered = false;

    // button height
    float h = internal::imgui_button_height(_btn.size);

    // -----------------------------------------------------------------
    //  shape style
    // -----------------------------------------------------------------
    int shape_vars = 0;

    if constexpr (_btn.has_shape)
    {
        shape_vars = internal::imgui_push_button_shape_style(
            _btn.shape,
            _btn.corner_radius);
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                            imgui_toolbar_style::default_rounding);
        shape_vars = 1;
    }

    // -----------------------------------------------------------------
    //  color overrides
    // -----------------------------------------------------------------
    int color_pushes = 0;

    // per-button color (bf_color)
    bool has_custom_bg = false;
    bool has_custom_fg = false;

    if constexpr (_btn.has_color)
    {
        has_custom_bg = (_btn.bg_a > 0.0f);
        has_custom_fg = (_btn.fg_a > 0.0f);
    }

    // disabled state
    if (!_btn.enabled)
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              imgui_toolbar_style::btn_disabled);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              imgui_toolbar_style::btn_disabled);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              imgui_toolbar_style::btn_disabled);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              imgui_toolbar_style::btn_text_disabled);
        color_pushes = 4;
    }
    else
    {
        // toggle highlight
        bool show_toggle_bg = false;

        if constexpr (_btn.has_toggle)
        {
            show_toggle_bg = _btn.toggled;
        }

        if (show_toggle_bg)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  imgui_toolbar_style::toggle_on_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  imgui_toolbar_style::btn_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  imgui_toolbar_style::btn_active);
            color_pushes = 3;
        }
        else if (has_custom_bg)
        {
            ImVec4 bg = ImVec4(_btn.bg_r, _btn.bg_g,
                               _btn.bg_b, _btn.bg_a);
            ImGui::PushStyleColor(ImGuiCol_Button, bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  imgui_toolbar_style::btn_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  imgui_toolbar_style::btn_active);
            color_pushes = 3;
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  imgui_toolbar_style::btn_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  imgui_toolbar_style::btn_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  imgui_toolbar_style::btn_active);
            color_pushes = 3;
        }

        // text color
        if (has_custom_fg)
        {
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                ImVec4(_btn.fg_r, _btn.fg_g,
                       _btn.fg_b, _btn.fg_a));
            ++color_pushes;
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_toolbar_style::btn_text);
            ++color_pushes;
        }
    }

    // disable interaction
    if (!_btn.enabled)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    }

    // -----------------------------------------------------------------
    //  compose label string (icon + text + shortcut)
    // -----------------------------------------------------------------
    std::string display_label;

    // icon prefix
    if constexpr (_btn.has_icon)
    {
        if constexpr (std::is_convertible_v<_I, std::string>)
        {
            std::string icon_str = _btn.icon;

            if (!icon_str.empty())
            {
                display_label += icon_str;

                if (!_btn.icon_only)
                {
                    display_label += " ";
                }
            }
        }
    }

    // label text (unless icon-only)
    bool icon_only = false;

    if constexpr (_btn.has_icon)
    {
        icon_only = _btn.icon_only;
    }

    if (!icon_only)
    {
        display_label += _btn.label;
    }

    // shortcut suffix
    if constexpr (_btn.has_shortcut)
    {
        if (!_btn.shortcut_label.empty())
        {
            display_label += "  " + _btn.shortcut_label;
        }
    }

    // -----------------------------------------------------------------
    //  draw the button
    // -----------------------------------------------------------------
    // for circle buttons, make width == height
    ImVec2 btn_size = ImVec2(0.0f, h);

    if constexpr (_btn.has_shape)
    {
        if (_btn.shape == button_shape::circle)
        {
            btn_size = ImVec2(h, h);
        }
    }

    bool clicked = ImGui::Button(display_label.c_str(),
                                 btn_size);

    // -----------------------------------------------------------------
    //  interaction state
    // -----------------------------------------------------------------
    _btn.hovered = ImGui::IsItemHovered();

    if (clicked)
    {
        btn_click(_btn);
    }

    // -----------------------------------------------------------------
    //  badge overlay (bf_badge)
    // -----------------------------------------------------------------
    if constexpr (_btn.has_badge)
    {
        if (_btn.badge_visible)
        {
            internal::imgui_draw_badge_overlay(_btn.badge_count);
        }
    }

    // -----------------------------------------------------------------
    //  toggle border (bf_toggle)
    // -----------------------------------------------------------------
    if constexpr (_btn.has_toggle)
    {
        if (_btn.toggled)
        {
            ImVec2 rmin = ImGui::GetItemRectMin();
            ImVec2 rmax = ImGui::GetItemRectMax();

            ImGui::GetWindowDrawList()->AddRect(
                rmin,
                rmax,
                ImGui::GetColorU32(
                    imgui_toolbar_style::toggle_on_border),
                imgui_toolbar_style::default_rounding,
                0,
                1.5f);
        }
    }

    // -----------------------------------------------------------------
    //  tooltip (bf_tooltip)
    // -----------------------------------------------------------------
    if constexpr (_btn.has_tooltip)
    {
        if ( (_btn.hovered)          &&
             (!_btn.tooltip.empty()) )
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(_btn.tooltip.c_str());
            ImGui::EndTooltip();
        }
    }

    // -----------------------------------------------------------------
    //  cleanup
    // -----------------------------------------------------------------
    if (!_btn.enabled)
    {
        ImGui::PopItemFlag();
    }

    ImGui::PopStyleColor(color_pushes);
    ImGui::PopStyleVar(shape_vars);

    return clicked;
}


// =============================================================================
//  4.  IMGUI DRAW TOOLBAR BUTTON (type-erased)
// =============================================================================
//   Draws a button through the toolbar_button_iface.  Uses the
// default shape and style since the concrete button type is erased.
// For full feature rendering, use imgui_draw_button<> directly.

// imgui_draw_toolbar_button
//   function: draws a type-erased toolbar button entry.
// Returns true if clicked.
D_INLINE bool
imgui_draw_toolbar_button(
    toolbar_button_iface& _iface,
    float                 _height
)
{
    if (!_iface.visible())
    {
        return false;
    }

    // colors
    int color_pushes = 0;

    if (!_iface.enabled())
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              imgui_toolbar_style::btn_disabled);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              imgui_toolbar_style::btn_disabled);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              imgui_toolbar_style::btn_disabled);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              imgui_toolbar_style::btn_text_disabled);
        color_pushes = 4;

        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              imgui_toolbar_style::btn_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              imgui_toolbar_style::btn_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              imgui_toolbar_style::btn_active);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              imgui_toolbar_style::btn_text);
        color_pushes = 4;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                        imgui_toolbar_style::default_rounding);

    bool clicked = ImGui::Button(
        _iface.label().c_str(),
        ImVec2(0.0f, _height));

    if (clicked && _iface.enabled())
    {
        _iface.click();
    }

    // tooltip
    if ( (ImGui::IsItemHovered())                &&
         (!_iface.tooltip_text().empty()) )
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(_iface.tooltip_text().c_str());
        ImGui::EndTooltip();
    }

    if (!_iface.enabled())
    {
        ImGui::PopItemFlag();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(color_pushes);

    return clicked;
}


// =============================================================================
//  5.  IMGUI DRAW TOOLBAR (main entry point)
// =============================================================================

// imgui_draw_toolbar
//   function: renders a toolbar as a fixed child window.
// Returns true if any button was clicked.
D_INLINE bool
imgui_draw_toolbar(
    toolbar&        _tb,
    render_context& _ctx
)
{
    (void)_ctx;

    if (!_tb.visible)
    {
        return false;
    }

    if (_tb.entries.empty())
    {
        return false;
    }

    bool interacted = false;
    bool horizontal = ( (_tb.dock == toolbar_dock::top)    ||
                        (_tb.dock == toolbar_dock::bottom) );

    // child window size
    ImVec2 child_size;

    if (horizontal)
    {
        child_size = ImVec2(0.0f, _tb.height);
    }
    else
    {
        child_size = ImVec2(_tb.width, 0.0f);
    }

    // background
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          imgui_toolbar_style::bar_bg);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          imgui_toolbar_style::bar_border);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar   |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::BeginChild("##toolbar",
                      child_size,
                      true,
                      flags);

    // compute button height (leave padding above and below)
    float btn_height = _tb.height - _tb.padding * 2.0f;

    if (btn_height < 16.0f)
    {
        btn_height = 16.0f;
    }

    // center buttons vertically
    if (horizontal)
    {
        float y_offset = (_tb.height -
                          ImGui::GetTextLineHeightWithSpacing()) *
                         0.5f - 2.0f;

        if (y_offset > 0.0f)
        {
            ImGui::SetCursorPosY(
                ImGui::GetCursorPosY() + y_offset);
        }
    }

    // iterate entries
    bool first = true;

    for (auto& entry : _tb.entries)
    {
        switch (entry.kind)
        {
            case entry_kind::button:
            {
                if ( (!entry.btn) ||
                     (!entry.btn->visible()) )
                {
                    break;
                }

                if (!first && horizontal)
                {
                    ImGui::SameLine(0.0f, _tb.spacing);
                }

                if (imgui_draw_toolbar_button(*entry.btn,
                                              btn_height))
                {
                    interacted = true;
                }

                first = false;

                break;
            }

            case entry_kind::separator:
            {
                if (horizontal)
                {
                    ImGui::SameLine(
                        0.0f,
                        imgui_toolbar_style::separator_padding);

                    // vertical separator line
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float  y1 = p.y;
                    float  y2 = p.y + btn_height;

                    ImGui::GetWindowDrawList()->AddLine(
                        ImVec2(p.x, y1),
                        ImVec2(p.x, y2),
                        ImGui::GetColorU32(
                            imgui_toolbar_style::separator_color),
                        imgui_toolbar_style::separator_thickness);

                    ImGui::Dummy(ImVec2(
                        imgui_toolbar_style::separator_thickness,
                        0.0f));

                    ImGui::SameLine(
                        0.0f,
                        imgui_toolbar_style::separator_padding);
                }
                else
                {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                break;
            }

            case entry_kind::spacer:
            {
                if (horizontal)
                {
                    // compute remaining width and consume it
                    float remaining = ImGui::GetContentRegionAvail().x;

                    // subtract widths of remaining entries
                    // (approximation: use remaining space)
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Dummy(ImVec2(
                        std::max(remaining * 0.5f, 0.0f),
                        0.0f));
                    ImGui::SameLine(0.0f, 0.0f);
                }
                else
                {
                    float remaining = ImGui::GetContentRegionAvail().y;

                    ImGui::Dummy(ImVec2(
                        0.0f,
                        std::max(remaining * 0.5f, 0.0f)));
                }

                break;
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    return interacted;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_TOOLBAR_DRAW_
