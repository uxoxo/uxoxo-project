/******************************************************************************
* uxoxo [imgui]                                               imgui_button.hpp
*
* Dear ImGui renderer for the button<_Feature, _Icon> template:
*   Standalone renderer for the framework-agnostic `button` component.
* Honors every feature in the button_feature bitfield (icon, tooltip,
* shape, toggle, badge, shortcut, per-button color) and reads each
* mixin behind `if constexpr` so a `button<bf_none>` instantiation
* compiles to a minimal ImGui::Button call with no probing of absent
* members.
*   Two entry points are provided:
*     render(_b, _ctx)
*         Free function overload, ADL-friendly and the canonical
*         entry point.  Returns true iff the button was clicked
*         this frame.  Render context is accepted but currently
*         unused (reserved for future cross-component state).
*     imgui_draw_button(_b, _ctx)
*         The legacy registry-dispatched entry point.  Identical
*         semantics; preserved so the toolbar's type-erased
*         dispatch table continues to compile without changes.
*         Marked [[nodiscard]] so callers cannot accidentally drop
*         the click result.
*
*   The renderer routes user clicks through the component's
* on_click(self&) callback (when present), and writes the
* per-frame interaction state back into .pressed and .hovered so
* callers can poll those fields without keeping the previous
* frame's bool around.  .focused is not updated here because Dear
* ImGui does not currently report keyboard focus on buttons; the
* field is left for renderers that have access to a focus tracker.
*   Migration note (2026.05.08): the body of this function was
* previously a public method of imgui_toolbar_draw.hpp.  It is
* now the canonical home, and imgui_toolbar_draw.hpp includes
* this header.  Behavior is byte-identical; the toolbar's
* iteration / spacing logic is unchanged.
*
* Contents:
*   1.  internal helpers (sizing, label composition)
*   2.  render(button&, render_context&)             - canonical
*   3.  imgui_draw_button(button&, render_context&)  - registry shim
*
*
* path:      /inc/uxoxo/platform/imgui/button/imgui_button.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_IMGUI_BUTTON_
#define  UXOXO_COMPONENT_IMGUI_BUTTON_ 1

// std
#include <string>
#include <type_traits>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/button/button.hpp"
#include "../../../templates/render_context.hpp"
#include "../core/imgui_button_helpers.hpp"
#include "../core/imgui_palette.hpp"
#include "../core/imgui_scope.hpp"


NS_UXOXO
NS_IMGUI


using uxoxo::component::button;
using uxoxo::component::button_shape;
using uxoxo::component::button_size;
using uxoxo::component::render_context;


// ===========================================================================
//  1.  INTERNAL HELPERS
// ===========================================================================

NS_INTERNAL

    // button_height
    //   function: returns the pixel height for a button_size.  The
    // three breakpoints are kept local to the button renderer
    // because they describe the button's own visual identity,
    // distinct from the shared toolbar height in the palette
    // (toolbar buttons inherit toolbar_height, standalone buttons
    // use one of these three).
    D_NODISCARD D_INLINE float
    button_height(
        button_size _sz
    ) noexcept
    {
        switch (_sz)
        {
            case button_size::small:  return 22.0f;
            case button_size::large:  return 34.0f;
            case button_size::medium:
            default:                  return 28.0f;
        }
    }

    // shape_rounding
    //   function: returns the FrameRounding value for a button_shape.
    // Pulls default and pill values from the palette so theme
    // changes propagate uniformly; rect, circle, and custom use
    // explicit values because their visual identity is independent
    // of the global rounding.
    D_NODISCARD D_INLINE float
    shape_rounding(
        button_shape _shape,
        float        _custom
    ) noexcept
    {
        switch (_shape)
        {
            case button_shape::rect:
                return 0.0f;

            case button_shape::rounded:
                return palette::get<palette::default_rounding_tag>();

            case button_shape::circle:
                return 999.0f;

            case button_shape::pill:
                return palette::get<palette::pill_rounding_tag>();

            case button_shape::custom:
                return _custom;
        }

        return palette::get<palette::default_rounding_tag>();
    }

    // compose_button_label
    //   function: builds the displayed label string from the
    // button's icon, label, and shortcut mixins.  Each branch is
    // gated by `if constexpr` so absent mixins compile out
    // entirely - a button<bf_none> reaches the bare `result =
    // _b.label` path with no overhead.
    //
    // Layout:    [icon ]label[  shortcut]
    //
    //     icon-only:  the icon is appended and the label omitted.
    //     no-icon:    the label is the entire string.
    //     shortcut:   appended with two leading spaces for spacing.
    template<unsigned _Feature,
              typename _Icon>
    D_INLINE std::string
    compose_button_label(
        const button<_Feature, _Icon>& _b
    )
    {
        std::string result;
        bool        icon_only;

        icon_only = false;

        // icon prefix
        if constexpr (_b.has_icon)
        {
            if constexpr (std::is_convertible_v<_Icon, std::string>)
            {
                std::string icon_str = _b.icon;

                if (!icon_str.empty())
                {
                    result += icon_str;

                    if (!_b.icon_only)
                    {
                        result += " ";
                    }
                }
            }

            icon_only = _b.icon_only;
        }

        // label text (omitted in icon-only mode)
        if (!icon_only)
        {
            result += _b.label;
        }

        // shortcut suffix
        if constexpr (_b.has_shortcut)
        {
            if (!_b.shortcut_label.empty())
            {
                result += "  ";
                result += _b.shortcut_label;
            }
        }

        return result;
    }

NS_END  // internal


// ===========================================================================
//  2.  RENDER  (button - canonical entry point)
// ===========================================================================

/*
render  (button)
  Canonical render entry for the button<_Feature, _Icon> template.
ADL-friendly: a caller writing `render(my_button, ctx)` at namespace
scope will find this overload via the button type living in
`uxoxo::component`.

  Honors every feature in the bitfield:
  - bf_icon      icon prefix (or icon-only when .icon_only)
  - bf_tooltip   hover tooltip from .tooltip
  - bf_shape     FrameRounding chosen by .shape
  - bf_toggle    palette toggle-on bg + border drawn behind the label
  - bf_badge    badge_overlay() over the upper-right corner
  - bf_shortcut  shortcut suffix in the label
  - bf_color     per-instance bg/fg overrides

  Per-frame interaction state (.pressed, .hovered) is updated.  The
on_click callback is invoked when the button is clicked AND enabled;
disabled buttons consume the click but do not fire.

Parameter(s):
  _b:   the button to render.  Mutated on click.
  _ctx: render context.  Currently unused; reserved for future
        cross-component state (focus tracker, animation clock).
Return:
  true iff the button was clicked this frame.
*/
template<unsigned _Feature,
          typename _Icon>
bool
render(
    button<_Feature, _Icon>& _b,
    render_context&          _ctx
)
{
    bool   clicked;
    bool   show_toggle;
    bool   has_custom_bg;
    bool   has_custom_fg;
    float  h;
    float  rounding;
    ImVec2 size;

    (void)_ctx;

    // -- visibility / interaction state reset --------------------------
    if (!_b.visible)
    {
        return false;
    }

    _b.pressed = false;
    _b.hovered = false;

    // -- sizing --------------------------------------------------------
    h    = internal::button_height(_b.size);
    size = ImVec2(0.0f, h);

    if constexpr (_b.has_shape)
    {
        if (_b.shape == button_shape::circle)
        {
            // circle buttons are square; width = height for a clean
            // circular hit region
            size = ImVec2(h, h);
        }

        rounding = internal::shape_rounding(_b.shape,
                                            _b.corner_radius);
    }
    else
    {
        rounding = palette::get<palette::default_rounding_tag>();
    }

    scoped_style_var rounding_guard(ImGuiStyleVar_FrameRounding,
                                    rounding);

    // -- color state ---------------------------------------------------
    //   The scoped_color accumulator collects every push needed
    // (button bg trio, text color, disabled overrides).  Destructor
    // pops the correct count regardless of which branch fired.

    has_custom_bg = false;
    has_custom_fg = false;

    if constexpr (_b.has_color)
    {
        has_custom_bg = (_b.bg_a > 0.0f);
        has_custom_fg = (_b.fg_a > 0.0f);
    }

    scoped_color colors;

    if (!_b.enabled)
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
        show_toggle = false;

        if constexpr (_b.has_toggle)
        {
            show_toggle = _b.toggled;
        }

        // bg trio
        if (show_toggle)
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
            colors.push(ImGuiCol_Button,
                        ImVec4(_b.bg_r, _b.bg_g,
                               _b.bg_b, _b.bg_a));
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
            colors.push(ImGuiCol_Text,
                        ImVec4(_b.fg_r, _b.fg_g,
                               _b.fg_b, _b.fg_a));
        }
        else
        {
            colors.push(ImGuiCol_Text,
                        palette::get<palette::btn_text_tag>());
        }
    }

    // -- disabled interaction gate -------------------------------------
    //   Kept as a raw PushItemFlag rather than scoped_disabled so
    // the explicitly-pushed disabled palette is preserved verbatim
    // (BeginDisabled would multiply the alpha of the colors above).
    if (!_b.enabled)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    }

    // -- draw the button -----------------------------------------------
    const std::string display = internal::compose_button_label(_b);

    clicked = ImGui::Button(display.c_str(), size);

    // -- update interaction state --------------------------------------
    _b.hovered = ImGui::IsItemHovered();
    _b.pressed = clicked;

    if ( (clicked) &&
         (_b.enabled) )
    {
        // route through the component's click verb if a callback
        // is present; falls back to a no-op which still returns
        // true so callers polling the return value see the click
        if (_b.on_click)
        {
            _b.on_click(_b);
        }
    }

    // -- toggle border (drawn AFTER the button so it sits on top) ------
    if constexpr (_b.has_toggle)
    {
        if (_b.toggled)
        {
            ImVec2 rmin = ImGui::GetItemRectMin();
            ImVec2 rmax = ImGui::GetItemRectMax();

            ImGui::GetWindowDrawList()->AddRect(
                rmin,
                rmax,
                ImGui::GetColorU32(
                    palette::get<palette::btn_toggle_on_border_tag>()),
                rounding,
                0,
                1.5f);
        }
    }

    // -- badge overlay -------------------------------------------------
    if constexpr (_b.has_badge)
    {
        if (_b.badge_visible)
        {
            badge_overlay(_b.badge_count);
        }
    }

    // -- tooltip -------------------------------------------------------
    if constexpr (_b.has_tooltip)
    {
        if ( (_b.hovered)          &&
             (!_b.tooltip.empty()) )
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(_b.tooltip.c_str());
            ImGui::EndTooltip();
        }
    }

    // -- cleanup -------------------------------------------------------
    //   scoped_color and scoped_style_var destructors handle their
    // pops; only PushItemFlag is raw and needs an explicit pop.
    if (!_b.enabled)
    {
        ImGui::PopItemFlag();
    }

    return ( (clicked) &&
             (_b.enabled) );
}


// ===========================================================================
//  3.  IMGUI DRAW BUTTON  (registry shim)
// ===========================================================================

/*
imgui_draw_button
  Registry-dispatched entry point preserved for the type-erased
toolbar dispatch table and any other site that registered against
the old function name.  Identical semantics to `render(_b, _ctx)`.

Parameter(s):
  _b:   the button to render.
  _ctx: render context.
Return:
  true iff the button was clicked and enabled this frame.
*/
template<unsigned _Feature,
          typename _Icon>
[[nodiscard]] D_INLINE bool
imgui_draw_button(
    button<_Feature, _Icon>& _b,
    render_context&          _ctx
)
{
    return render(_b, _ctx);
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_BUTTON_