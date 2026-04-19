/*******************************************************************************
* uxoxo [imgui]                                                 imgui_style.hpp
*
* Shared ImGui style helpers and style-protocol bridge:
*   Small, component-agnostic utilities that the ImGui rendering backend
* uses to (1) translate uxoxo's semantic style vocabulary (DEmphasis,
* DTextAlignment) into ImGui's concrete vocabulary (ImVec4 colors,
* x-axis cursor offsets) and (2) resolve per-property style values off
* any style-readable component.
*
*   The style bridge is opt-in: `resolve_style_or<_Tag>(_c, _fallback)`
* checks ui::traits::is_style_readable_v<_Component> at compile time,
* and if the component is style-readable AND its style_type carries
* the requested tag, returns `_c.get_style().get<_Tag>()`.  Otherwise
* it returns the fallback.  Components that don't opt into the style
* protocol pay zero cost — the style branch is gated by if-constexpr
* and compiles out entirely.
*
*   The tag types here (fg_tag, check_mark_tag, ...) are the ImGui
* backend's palette vocabulary.  Other backends (TUI, GUI, VUI) are
* free to define their own tag families; a single style<...> can
* carry tags for multiple backends simultaneously, and each renderer
* picks off the ones it recognizes.
*
* Contents:
*   1  Palette tags (fg_tag, check_mark_tag)
*   2  emphasis_color     — DEmphasis -> ImVec4
*   3  alignment_offset   — DTextAlignment + widths -> x-offset
*   4  resolve_style_or   — style property with fallback
*
*
* path:      /inc/uxoxo/platform/imgui/style/imgui_style.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_IMGUI_STYLE_
#define  UXOXO_IMGUI_STYLE_ 1

// std
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// imgui
#include <imgui.h>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/style/style_traits.hpp"
#include "../../../templates/component/component_types.hpp"


NS_UXOXO
NS_COMPONENT
NS_IMGUI

// ===============================================================================
//  1  PALETTE TAGS
// ===============================================================================
//   Empty tag types that identify properties the ImGui backend knows
// how to consume.  A consumer wiring a style<...> for an ImGui-rendered
// component attaches properties keyed by these tags; the renderer
// picks them up via resolve_style_or.
//
//   Other backends will define their own tags in their own headers.
// A style object can carry tags for multiple backends simultaneously
// — each renderer only queries the ones it recognizes.

// fg_tag
//   type: foreground color tag.  Value type: ImVec4.  Used for label
// text, checkbox label text, and any future text-bearing component.
struct fg_tag
{};

// check_mark_tag
//   type: check-glyph color tag.  Value type: ImVec4.  Used by the
// checkbox renderer for the mark drawn inside the frame.
struct check_mark_tag
{};




// ===============================================================================
//  2  EMPHASIS COLOR
// ===============================================================================

/*
emphasis_color
  Resolves a DEmphasis level to the ImVec4 color that the renderer
should use for the component's foreground.  DEmphasis::normal returns
the current ImGui text color so callers can push the result
unconditionally without special-casing the default.

Parameter(s):
  _e: the emphasis level to resolve.
Return:
  The ImVec4 color corresponding to `_e`, opaque (alpha = 1.0).
*/
[[nodiscard]] inline ImVec4
emphasis_color(
    DEmphasis _e
) noexcept
{
    switch (_e)
    {
        case DEmphasis::muted:
            return ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

        case DEmphasis::primary:
            return ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

        case DEmphasis::secondary:
            return ImVec4(0.70f, 0.70f, 0.85f, 1.00f);

        case DEmphasis::success:
            return ImVec4(0.36f, 0.80f, 0.36f, 1.00f);

        case DEmphasis::warning:
            return ImVec4(0.95f, 0.75f, 0.20f, 1.00f);

        case DEmphasis::danger:
            return ImVec4(0.90f, 0.30f, 0.30f, 1.00f);

        case DEmphasis::info:
            return ImVec4(0.40f, 0.70f, 0.95f, 1.00f);

        case DEmphasis::normal:
        default:
            return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }
}




// ===============================================================================
//  3  ALIGNMENT OFFSET
// ===============================================================================

/*
alignment_offset
  Computes the x-axis offset in pixels needed to position a text run
of `_text_width` inside a region of `_region_width` per the given
alignment.  Returns 0 when the text is wider than the region (the
left edge is the best we can do) and for DTextAlignment::left.

Parameter(s):
  _align:        the desired horizontal alignment.
  _region_width: width of the region the text lives in, in pixels.
  _text_width:   width of the text as measured by ImGui::CalcTextSize.
Return:
  The x-offset in pixels, always non-negative.
*/
[[nodiscard]] inline float
alignment_offset(
    DTextAlignment _align,
    float          _region_width,
    float          _text_width
) noexcept
{
    float slack;

    slack = (_region_width - _text_width);

    if (slack <= 0.0f)
    {
        return 0.0f;
    }

    switch (_align)
    {
        case DTextAlignment::center:
            return (slack * 0.5f);

        case DTextAlignment::right:
            return slack;

        case DTextAlignment::left:
        default:
            return 0.0f;
    }
}




// ===============================================================================
//  4  RESOLVE STYLE OR
// ===============================================================================

/*
resolve_style_or
  Returns the value of style property _Tag pulled off _c, or _fallback
if no such value is available.  Two-stage if-constexpr check:

  1. Is _Component style-readable (does it expose get_style)?  Gated
     via ui::traits::is_style_readable_v.  If not, the whole style
     branch compiles out and _fallback is returned.

  2. Does _Component's style_type carry _Tag?  Gated via the uxoxo
     style's constexpr static `has<_Tag>()`.  If not, _fallback is
     returned without triggering the style class's missing-tag
     static_assert.

  The double if-constexpr keeps the renderer cheap for components
that opt out of the style protocol: compiled code for non-stylable
components is byte-identical to a hard-coded _fallback.

Parameter(s):
  _c:        the component to query.  Need not be style-readable.
  _fallback: the value to return when the component does not supply
             a value for _Tag.  Determines the return type.
Return:
  The resolved style value or _fallback, type-matched to _fallback.
*/
template <typename    _Tag,
          typename    _Component,
          typename    _Value>
[[nodiscard]] inline _Value
resolve_style_or(
    const _Component& _c,
    _Value            _fallback
) noexcept
{
    using clean_component_t =
        std::remove_cv_t<std::remove_reference_t<_Component>>;

    if constexpr (ui::traits::is_style_readable_v<clean_component_t>)
    {
        using style_t = typename clean_component_t::style_type;

        if constexpr (style_t::template has<_Tag>())
        {
            return _c.get_style().template get<_Tag>();
        }
    }

    return _fallback;
}


NS_END  // imgui
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_STYLE_