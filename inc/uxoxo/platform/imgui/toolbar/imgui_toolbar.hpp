/******************************************************************************
* uxoxo [imgui]                                              imgui_toolbar.hpp
*
* Dear ImGui draw handler for the toolbar and button components.
*   Renders a toolbar as a fixed child window anchored to the specified
* edge of the parent window.  Each button entry is drawn with full
* support for the button feature set: icon, tooltip, shape (rect,
* rounded, circle, pill), toggle highlight, badge counter, shortcut
* label, and per-button color overrides.
*   Separators draw a thin vertical (or horizontal) line.  Spacers
* push remaining entries to the far edge.
*   The typed draw function imgui_draw_button<Button> handles a
* concrete button instantiation and can be called standalone.  The
* toolbar draw function uses the type-erased toolbar_button_iface
* for its entries.
*   Migration note (2026.05.08): the local `imgui_toolbar_style`
* namespace was a bag of colors and dimensions that turned out to
* duplicate values present in five other namespaces.  All shared
* color slots and the badge / separator constants are now drawn from
* `palette::` (imgui_palette.hpp); the namespace is reduced to
* toolbar-specific button-size dimensions and tooltip delay.  Manual
* PushStyleColor / PopStyleColor and PushStyleVar / PopStyleVar
* sequences are now expressed as scoped_color and scoped_style_var
* RAII guards from imgui_scope.hpp.  The inline badge overlay helper
* moves to `badge_overlay()` in imgui_button_helpers.hpp.  No
* behavioural change.
*   Structure:
*     1.  toolbar-local style constants (sizes only)
*     2.  internal helpers (shape rendering)
*     3.  imgui_draw_button (typed, standalone)
*     4.  imgui_draw_toolbar_button (type-erased, for toolbar entries)
*     5.  imgui_draw_toolbar (main entry point)
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included before
* this header.
*
*
* path:      /inc/uxoxo/platform/imgui/toolbar/imgui_toolbar.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.04.10
******************************************************************************/

#ifndef UXOXO_COMPONENT_IMGUI_TOOLBAR_
#define UXOXO_COMPONENT_IMGUI_TOOLBAR_ 1

// std
#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/button/button.hpp"
#include "../../../templates/component/toolbar/toolbar.hpp"
#include "../../../templates/render_context.hpp"
#include "../core/imgui_button_helpers.hpp"
#include "../core/imgui_palette.hpp"
#include "../core/imgui_scope.hpp"


NS_UXOXO
NS_IMGUI

using uxoxo::component::button;
using uxoxo::component::button_shape;
using uxoxo::component::button_size;
using uxoxo::component::entry_kind;
using uxoxo::component::render_context;
using uxoxo::component::toolbar;
using uxoxo::component::toolbar_dock;
using uxoxo::component::toolbar_button_iface;

// ===========================================================================
//  1.  STYLE CONSTANTS (toolbar-local sizes only)
// ===========================================================================
//   Colors and shared dimensions live in `palette::` — see
// imgui_palette.hpp.  This namespace retains only the toolbar-
// specific button sizing and tooltip delay, which have no consumer
// outside the toolbar/button render path.

namespace imgui_toolbar_style
{
    // sizes by button_size
    inline constexpr float size_small_h    = 22.0f;
    inline constexpr float size_medium_h   = 28.0f;
    inline constexpr float size_large_h    = 34.0f;

    // separator padding (toolbar-internal layout)
    inline constexpr float separator_padding = 6.0f;

    // tooltip
    inline constexpr float tooltip_delay   = 0.5f;

}   // namespace imgui_toolbar_style


// ===========================================================================
//  2.  INTERNAL HELPERS
// ===========================================================================

NS_INTERNAL

    // imgui_button_height
    //   function: returns the pixel height for a button_size.
    inline float
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
    //   function: pushes one ImGuiStyleVar_FrameRounding for the given
    // button_shape.  Returns the count of style vars pushed (always
    // 1) so the caller can pair it with ImGui::PopStyleVar(N) or
    // hand it to a scoped_style_var.  Rounding values come from the
    // shared palette (default_rounding_tag, pill_rounding_tag).
    inline int
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
                ImGui::PushStyleVar(
                    ImGuiStyleVar_FrameRounding,
                    palette::get<palette::default_rounding_tag>());

                return 1;

            case button_shape::circle:
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);

                return 1;

            case button_shape::pill:
                ImGui::PushStyleVar(
                    ImGuiStyleVar_FrameRounding,
                    palette::get<palette::pill_rounding_tag>());

                return 1;

            case button_shape::custom:
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, _radius);

                return 1;
        }

        return 0;
    }

NS_END  // internal


// ===========================================================================
//  3.  IMGUI DRAW BUTTON (typed, standalone)
// ===========================================================================
//   Renders a single button with all its compile-time features.
// Can be called outside a toolbar context.

// imgui_draw_button
//   function: renders a button<_ButtonFeature, _ButtonIcon> using Dear ImGui.
// Returns true if the button was clicked this frame.
template<unsigned _ButtonFeature,
          typename _ButtonIcon>
bool
imgui_draw_button(
    button<_ButtonFeature, _ButtonIcon>& _button,
    render_context& _context
)
{
    bool        clicked;
    bool        has_custom_bg;
    bool        has_custom_fg;
    bool        show_toggle_bg;
    bool        icon_only;
    float       h;
    int         shape_vars;
    std::string display_label;
    ImVec2      btn_size;

    (void)_context;

    if (!_button.visible)
    {
        return false;
    }

    // reset per-frame state
    _button.pressed = false;
    _button.hovered = false;

    // button height
    h = internal::imgui_button_height(_button.size);

    // -----------------------------------------------------------------
    //  shape style — single PushStyleVar paired with PopStyleVar(N)
    //  via the manual int return path.  Kept as bare push/pop because
    //  the count is dynamic across the button_shape branch.
    // -----------------------------------------------------------------
    if constexpr (_button.has_shape)
    {
        shape_vars = internal::imgui_push_button_shape_style(
            _button.shape,
            _button.corner_radius);
    }
    else
    {
        ImGui::PushStyleVar(
            ImGuiStyleVar_FrameRounding,
            palette::get<palette::default_rounding_tag>());
        shape_vars = 1;
    }

    // -----------------------------------------------------------------
    //  color overrides — accumulated through scoped_color so the
    //  destructor pops the correct count regardless of which branch
    //  was taken.  Order of pushes is preserved exactly.
    // -----------------------------------------------------------------
    has_custom_bg = false;
    has_custom_fg = false;

    if constexpr (_button.has_color)
    {
        has_custom_bg = (_button.bg_a > 0.0f);
        has_custom_fg = (_button.fg_a > 0.0f);
    }

    scoped_color colors;

    if (!_button.enabled)
    {
        colors.push(ImGuiCol_Button,
                    palette::get<palette::btn_disabled_tag>());
        colors.push(ImGuiCol_ButtonHovered,
                    palette::get<palette::btn_disabled_tag>());
        colors.push(ImGuiCol_ButtonActive,
                    palette::get<palette::btn_disabled_tag>());
        colors.push(ImGuiCol_Text,
                    palette::get<palette::btn_text_disabled_tag>());
    }
    else
    {
        // toggle highlight
        show_toggle_bg = false;

        if constexpr (_button.has_toggle)
        {
            show_toggle_bg = _button.toggled;
        }

        if (show_toggle_bg)
        {
            colors.push(ImGuiCol_Button,
                        palette::get<palette::btn_toggle_on_bg_tag>());
            colors.push(ImGuiCol_ButtonHovered,
                        palette::get<palette::btn_hover_tag>());
            colors.push(ImGuiCol_ButtonActive,
                        palette::get<palette::btn_active_tag>());
        }
        else if (has_custom_bg)
        {
            colors.push(
                ImGuiCol_Button,
                ImVec4(_button.bg_r, _button.bg_g,
                       _button.bg_b, _button.bg_a));
            colors.push(ImGuiCol_ButtonHovered,
                        palette::get<palette::btn_hover_tag>());
            colors.push(ImGuiCol_ButtonActive,
                        palette::get<palette::btn_active_tag>());
        }
        else
        {
            colors.push(ImGuiCol_Button,
                        palette::get<palette::btn_bg_tag>());
            colors.push(ImGuiCol_ButtonHovered,
                        palette::get<palette::btn_hover_tag>());
            colors.push(ImGuiCol_ButtonActive,
                        palette::get<palette::btn_active_tag>());
        }

        // text color
        if (has_custom_fg)
        {
            colors.push(
                ImGuiCol_Text,
                ImVec4(_button.fg_r, _button.fg_g,
                       _button.fg_b, _button.fg_a));
        }
        else
        {
            colors.push(ImGuiCol_Text,
                        palette::get<palette::btn_text_tag>());
        }
    }

    // -----------------------------------------------------------------
    //  disable interaction — kept as a manual conditional push/pop
    //  rather than scoped_disabled because BeginDisabled() multiplies
    //  the alpha of the colors we've already pushed; using the raw
    //  PushItemFlag preserves the explicit disabled palette intact.
    // -----------------------------------------------------------------
    if (!_button.enabled)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    }

    // -----------------------------------------------------------------
    //  compose label string (icon + text + shortcut)
    // -----------------------------------------------------------------

    // icon prefix
    if constexpr (_button.has_icon)
    {
        if constexpr (std::is_convertible_v<_ButtonIcon, std::string>)
        {
            std::string icon_str = _button.icon;

            if (!icon_str.empty())
            {
                display_label += icon_str;

                if (!_button.icon_only)
                {
                    display_label += " ";
                }
            }
        }
    }

    // label text (unless icon-only)
    icon_only = false;

    if constexpr (_button.has_icon)
    {
        icon_only = _button.icon_only;
    }

    if (!icon_only)
    {
        display_label += _button.label;
    }

    // shortcut suffix
    if constexpr (_button.has_shortcut)
    {
        if (!_button.shortcut_label.empty())
        {
            display_label += "  " + _button.shortcut_label;
        }
    }

    // -----------------------------------------------------------------
    //  draw the button
    // -----------------------------------------------------------------

    // for circle buttons, make width == height
    btn_size = ImVec2(0.0f, h);

    if constexpr (_button.has_shape)
    {
        if (_button.shape == button_shape::circle)
        {
            btn_size = ImVec2(h, h);
        }
    }

    clicked = ImGui::Button(display_label.c_str(),
                            btn_size);

    // -----------------------------------------------------------------
    //  interaction state
    // -----------------------------------------------------------------
    _button.hovered = ImGui::IsItemHovered();

    if (clicked)
    {
        btn_click(_button);
    }

    // -----------------------------------------------------------------
    //  badge overlay (bf_badge)
    // -----------------------------------------------------------------
    if constexpr (_button.has_badge)
    {
        if (_button.badge_visible)
        {
            badge_overlay(_button.badge_count);
        }
    }

    // -----------------------------------------------------------------
    //  toggle border (bf_toggle)
    // -----------------------------------------------------------------
    if constexpr (_button.has_toggle)
    {
        if (_button.toggled)
        {
            ImVec2 rmin = ImGui::GetItemRectMin();
            ImVec2 rmax = ImGui::GetItemRectMax();

            ImGui::GetWindowDrawList()->AddRect(
                rmin,
                rmax,
                ImGui::GetColorU32(
                    palette::get<palette::btn_toggle_on_border_tag>()),
                palette::get<palette::default_rounding_tag>(),
                0,
                1.5f);
        }
    }

    // -----------------------------------------------------------------
    //  tooltip (bf_tooltip)
    // -----------------------------------------------------------------
    if constexpr (_button.has_tooltip)
    {
        if ( (_button.hovered)          &&
             (!_button.tooltip.empty()) )
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(_button.tooltip.c_str());
            ImGui::EndTooltip();
        }
    }

    // -----------------------------------------------------------------
    //  cleanup — only PopItemFlag and PopStyleVar remain manual; the
    //  scoped_color destructor handles the per-button color stack.
    // -----------------------------------------------------------------
    if (!_button.enabled)
    {
        ImGui::PopItemFlag();
    }

    ImGui::PopStyleVar(shape_vars);

    return clicked;
}


// ===========================================================================
//  4.  IMGUI DRAW TOOLBAR BUTTON (type-erased)
// ===========================================================================
//   Draws a button through the toolbar_button_iface.  Uses the
// default shape and style since the concrete button type is erased.
// For full feature rendering, use imgui_draw_button<> directly.

// imgui_draw_toolbar_button
//   function: draws a type-erased toolbar button entry.
// Returns true if clicked.
inline bool
imgui_draw_toolbar_button(
    toolbar_button_iface& _iface,
    float                 _height
)
{
    bool clicked;

    if (!_iface.visible())
    {
        return false;
    }

    // colors — all four tags pushed regardless of state, choosing
    // disabled vs default palette branches
    scoped_color colors;

    if (!_iface.enabled())
    {
        colors.push(ImGuiCol_Button,
                    palette::get<palette::btn_disabled_tag>());
        colors.push(ImGuiCol_ButtonHovered,
                    palette::get<palette::btn_disabled_tag>());
        colors.push(ImGuiCol_ButtonActive,
                    palette::get<palette::btn_disabled_tag>());
        colors.push(ImGuiCol_Text,
                    palette::get<palette::btn_text_disabled_tag>());

        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    }
    else
    {
        colors.push(ImGuiCol_Button,
                    palette::get<palette::btn_bg_tag>());
        colors.push(ImGuiCol_ButtonHovered,
                    palette::get<palette::btn_hover_tag>());
        colors.push(ImGuiCol_ButtonActive,
                    palette::get<palette::btn_active_tag>());
        colors.push(ImGuiCol_Text,
                    palette::get<palette::btn_text_tag>());
    }

    scoped_style_var rounding(
        ImGuiStyleVar_FrameRounding,
        palette::get<palette::default_rounding_tag>());

    clicked = ImGui::Button(
        _iface.label().c_str(),
        ImVec2(0.0f, _height));

    if (clicked && _iface.enabled())
    {
        _iface.click();
    }

    // tooltip
    if ( (ImGui::IsItemHovered())              &&
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

    return clicked;
}


// ===========================================================================
//  5.  IMGUI DRAW TOOLBAR (main entry point)
// ===========================================================================

// imgui_draw_toolbar
//   function: renders a toolbar as a fixed child window.
// Returns true if any button was clicked.
inline bool
imgui_draw_toolbar(
    toolbar&        _tb,
    render_context& _context
)
{
    bool   interacted;
    bool   horizontal;
    bool   first;
    float  btn_height;
    ImVec2 child_size;

    (void)_context;

    if (!_tb.visible)
    {
        return false;
    }

    if (_tb.entries.empty())
    {
        return false;
    }

    interacted = false;
    horizontal = ( (_tb.dock == toolbar_dock::top) ||
                   (_tb.dock == toolbar_dock::bottom) );

    // child window size
    if (horizontal)
    {
        child_size = ImVec2(0.0f, _tb.height);
    }
    else
    {
        child_size = ImVec2(_tb.width, 0.0f);
    }

    // background — palette-sourced bg + border colors.  toolbar_scope
    // is not used here because the toolbar may be vertical (left/
    // right dock), which the chrome scope does not handle.
    scoped_color chrome_colors {
        { ImGuiCol_ChildBg, palette::get<palette::toolbar_bg_tag>()     },
        { ImGuiCol_Border,  palette::get<palette::toolbar_border_tag>() }
    };

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::BeginChild("##toolbar",
                      child_size,
                      true,
                      flags);

    // compute button height (leave padding above and below)
    btn_height = (_tb.height - (_tb.padding * 2.0f));

    if (btn_height < 16.0f)
    {
        btn_height = 16.0f;
    }

    // center buttons vertically
    if (horizontal)
    {
        float y_offset = ((_tb.height -
                           ImGui::GetTextLineHeightWithSpacing()) *
                          0.5f) - 2.0f;

        if (y_offset > 0.0f)
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_offset);
        }
    }

    // iterate entries
    first = true;

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
                            palette::get<palette::separator_tag>()),
                        palette::get<palette::separator_thickness_tag>());

                    ImGui::Dummy(ImVec2(
                        palette::get<palette::separator_thickness_tag>(),
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

    return interacted;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_TOOLBAR_