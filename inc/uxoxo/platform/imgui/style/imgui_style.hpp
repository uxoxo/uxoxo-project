/******************************************************************************
* uxoxo [imgui]                                                imgui_style.hpp
*
* Shared ImGui style helpers and style-protocol bridge:
*   Small, component-agnostic utilities that the ImGui rendering backend
* uses to (1) translate uxoxo's semantic style vocabulary (DEmphasis,
* text_alignment) into ImGui's concrete vocabulary (ImVec4 colors,
* x-axis cursor offsets) and (2) resolve per-property style values off
* any style-readable component.
*   The style bridge is opt-in: `resolve_style_or<_Tag>(_c, _fallback)`
* checks ui::traits::is_style_readable_v<_Component> at compile time,
* and if the component is style-readable AND its style_type carries
* the requested tag, returns `_c.get_style().get<_Tag>()`.  Otherwise
* it returns the fallback.  Components that don't opt into the style
* protocol pay zero cost - the style branch is gated by if-constexpr
* and compiles out entirely.
*   The tag types here (fg_tag, check_mark_tag, ...) are the ImGui
* backend's palette vocabulary.  Other backends (TUI, GUI, VUI) are
* free to define their own tag families; a single style<...> can
* carry tags for multiple backends simultaneously, and each renderer
* picks off the ones it recognizes.
*
*   Migration note (2026.05.08): the convenience helper
* `resolve_style_or_palette<_StyleTag, _PaletteTag>(_c)` is added.
* It collapses the ubiquitous "style override falling back to the
* palette" pattern into a single call:
*
*     // before
*     ImVec4 c = resolve_style_or<fg_tag>(_c,
*                    palette::get<palette::btn_text_tag>());
*     // after
*     ImVec4 c = resolve_style_or_palette<fg_tag,
*                                         palette::btn_text_tag>(_c);
*   Going forward, every renderer that reads a color should prefer
* `resolve_style_or_palette` so theme switches (via palette::set<>)
* propagate uniformly while per-component style overrides continue
* to take precedence.
* Contents:
*   1.  Palette tags (fg_tag, check_mark_tag)
*   2.  emphasis_color           - DEmphasis -> ImVec4
*   3.  alignment_offset         - text_alignment + widths -> x-offset
*   4.  resolve_style_or         - style property with arbitrary fallback
*   5.  resolve_style_or_palette - style property with palette fallback
*
*
* path:      /inc/uxoxo/platform/imgui/style/imgui_style.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.04.18
******************************************************************************/

#ifndef  UXOXO_COMPONENT_IMGUI_STYLE_
#define  UXOXO_COMPONENT_IMGUI_STYLE_ 1

// std
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/text/text_align.hpp>
// imgui
#include <imgui.h>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/style/style_traits.hpp"
#include "../../../templates/component/component_types.hpp"
#include "../core/imgui_palette.hpp"


NS_UXOXO
NS_IMGUI


using djinterp::text_alignment;
using uxoxo::component::DEmphasis;
using uxoxo::component::is_style_readable_v;

// ===========================================================================
//  1.  PALETTE TAGS
// ===========================================================================
//   Empty tag types that identify properties the ImGui backend knows
// how to consume.  A consumer wiring a style<...> for an ImGui-rendered
// component attaches properties keyed by these tags; the renderer
// picks them up via resolve_style_or.
//
//   These tags are the *style protocol* tags, distinct from the
// `palette::*_tag` types in imgui_palette.hpp (which are *palette*
// tags).  A renderer typically pairs them: query the component for
// its style property keyed by `fg_tag`, fall back to the palette's
// `btn_text_tag` value when no per-component override exists.  See
// resolve_style_or_palette below for the canonical pairing.
//
//   Other backends will define their own style tags in their own
// headers.  A style object can carry tags for multiple backends
// simultaneously - each renderer only queries the ones it recognizes.

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


// ===========================================================================
//  2.  EMPHASIS COLOR
// ===========================================================================

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

// ===========================================================================
//  3.  ALIGNMENT OFFSET
// ===========================================================================

/*
alignment_offset
  Computes the x-axis offset in pixels needed to position a text run
of `_text_width` inside a region of `_region_width` per the given
alignment.  Returns 0 when the text is wider than the region (the
left edge is the best we can do) and for text_alignment::left.

Parameter(s):
  _align:        the desired horizontal alignment.
  _region_width: width of the region the text lives in, in pixels.
  _text_width:   width of the text as measured by ImGui::CalcTextSize.
Return:
  The x-offset in pixels, always non-negative.
*/
[[nodiscard]] inline float
alignment_offset(
    text_alignment _align,
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
        case text_alignment::center:
            return (slack * 0.5f);

        case text_alignment::right:
            return slack;

        case text_alignment::left:
        default:
            return 0.0f;
    }
}

// ===========================================================================
//  4.  RESOLVE STYLE OR
// ===========================================================================

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

  This is the LOW-LEVEL primitive.  Most renderers should prefer
`resolve_style_or_palette` (below), which combines this with a
palette-tag fallback so theme switches propagate uniformly.

Parameter(s):
  _c:        the component to query.  Need not be style-readable.
  _fallback: the value to return when the component does not supply
             a value for _Tag.  Determines the return type.
Return:
  The resolved style value or _fallback, type-matched to _fallback.
*/
template<typename    _Tag,
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

    if constexpr (is_style_readable_v<clean_component_t>)
    {
        using style_t = typename clean_component_t::style_type;

        if constexpr (style_t::template has<_Tag>())
        {
            return _c.get_style().template get<_Tag>();
        }
    }

    return _fallback;
}


// ===========================================================================
//  5.  RESOLVE STYLE OR PALETTE
// ===========================================================================

/*
resolve_style_or_palette
  The canonical color resolver for ImGui renderers.  Returns the
value of style property `_StyleTag` pulled off `_c` if the component
opts into the style protocol AND carries that tag; otherwise returns
`palette::get<_PaletteTag>()`.

  Combines the two-layer color cascade that every renderer needs:
per-component style overrides take precedence over the global
palette, which itself is mutable at runtime via `palette::set<>`.
A theme change at the palette layer propagates to every renderer
that uses this helper without touching the renderers themselves;
a per-component override (set by attaching a style property) wins
locally.

  Both branches resolve at compile time.  For a component without
the style protocol, the call compiles to a single load from the
palette entry's static-inline storage - identical to writing
`palette::get<_PaletteTag>()` directly.  For a component with the
protocol but missing the requested tag, same story.  Only when
the component both opts in AND carries the tag does the function
emit an additional virtual-free indirection through `_c.get_style()`.

  EXAMPLE
  -------
  Old idiom (hand-rolled fallback chain):

      ImVec4 text = resolve_style_or<fg_tag>(
          _c,
          palette::get<palette::btn_text_tag>());

  New idiom:

      ImVec4 text = resolve_style_or_palette<
                        fg_tag,
                        palette::btn_text_tag>(_c);

  Same codegen, half the line count, no chance of fallback drift
between callers.

Parameter(s):
  _c: the component to query.  Need not be style-readable; renderers
      can apply this helper uniformly.
Return:
  The component's style value for `_StyleTag` if available, else
  the palette entry for `_PaletteTag`.  The return type is the
  palette entry's value type (ImVec4 for color tags, float for
  dimension tags).
*/
template<typename _StyleTag,
          typename _PaletteTag,
          typename _Component>
[[nodiscard]] inline auto
resolve_style_or_palette(
    const _Component& _c
) noexcept
{
    using clean_component_t =
        std::remove_cv_t<std::remove_reference_t<_Component>>;
    using palette_value_t  =
        std::remove_cv_t<std::remove_reference_t<
            decltype(palette::get<_PaletteTag>())>>;

    if constexpr (is_style_readable_v<clean_component_t>)
    {
        using style_t = typename clean_component_t::style_type;

        if constexpr (style_t::template has<_StyleTag>())
        {
            // explicit cast to the palette value type so the return
            // type is consistent across both branches; the style
            // protocol's tag value type must be implicitly convertible
            // to the palette tag's value type or this static_cast
            // fires a compile error
            return static_cast<palette_value_t>(
                _c.get_style().template get<_StyleTag>());
        }
    }

    return palette::get<_PaletteTag>();
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_STYLE_