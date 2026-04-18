/*******************************************************************************
* uxoxo [ui/imgui]                                          imgui_frame_draw.hpp
*
* ImGui renderer for the `frame` component:
*   Draws a bordered container with an optional title on any of the four
* edges (top/bottom/left/right), aligned along that edge per title_align.
* For horizontal placements (top/bottom) the title is rendered with normal
* ImGui text.  For vertical placements (left/right) the title is rendered
* as stacked characters — one character per line — because ImGui has no
* rotated-text primitive.  Stacking produces readable, predictable output
* in any font and avoids pulling in a text-rotation dependency.
*
*   When the title is non-empty and show_border is true, the border line
* on the title's edge is notched (broken) to leave a clear gap around the
* text.  This matches the appearance of Tcl/Tk ttk::labelframe, Qt
* QGroupBox, HTML fieldset+legend, and the vast majority of native
* "group box" widgets.  When the title is empty, the border is drawn as
* an unbroken rectangle.  When show_border is false, the title alone is
* drawn — no surrounding line.
*
*   Children are rendered via the caller-supplied `node_render_fn` inside
* an inset content region created with BeginChild.  The frame reserves
* ImGui layout space equal to the full content region it was handed on
* entry.  Emphasis is mapped to a border colour — normal uses the theme's
* ImGuiCol_Border.
*
*   This module depends on the node_render_fn callback type defined in
* imgui_stacked_view_draw.hpp, which is the canonical home for the
* shared container-child-rendering callback.
*
*
* path:      /inc/uxoxo/ui/imgui/imgui_frame_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.17
*******************************************************************************/

#ifndef  UXOXO_UI_IMGUI_FRAME_DRAW_
#define  UXOXO_UI_IMGUI_FRAME_DRAW_ 1

// std
#include <cstddef>
#include <cstdint>
#include <string>

// imgui
#include <imgui.h>

// uxoxo
#include "../../uxoxo.hpp"
#include "../components.hpp"
#include "./imgui_stacked_view_draw.hpp"        // for node_render_fn


NS_UXOXO
NS_UI
NS_IMGUI


// ===============================================================================
//  I.   STYLE CONSTANTS
// ===============================================================================

// D_FRAME_TITLE_INSET_PAD
//   constant: horizontal (or cross-axis) gap in pixels between the title
// text and the border-notch endpoints on either side of it.
#define D_FRAME_TITLE_INSET_PAD         4.0f

// D_FRAME_TITLE_EDGE_OFFSET
//   constant: minimum distance in pixels from a frame corner to the start
// of the title, applied to `left` and `right` alignments so the title
// never collides with the corner of the border.
#define D_FRAME_TITLE_EDGE_OFFSET       8.0f

// D_FRAME_CONTENT_PAD
//   constant: inner padding in pixels between the border and the child
// content region, applied on every edge.
#define D_FRAME_CONTENT_PAD             6.0f

// D_FRAME_BORDER_THICKNESS
//   constant: thickness in pixels of the border line.
#define D_FRAME_BORDER_THICKNESS        1.0f


// ===============================================================================
//  II.  INTERNAL HELPERS
// ===============================================================================

NS_INTERNAL

    /*
    imgui_frame_emphasis_color
      Maps a frame's semantic emphasis to a concrete ImGui border colour.
    `emphasis::normal` resolves to the current theme's ImGuiCol_Border so
    the frame blends with ambient chrome; all other emphases resolve to
    fixed palette entries that read consistently across themes.

    Parameter(s):
      _emph: the semantic emphasis to map.
    Return:
      An ImU32 packed colour suitable for ImDrawList primitives.
    */
    inline ImU32
    imgui_frame_emphasis_color(
        emphasis _emph
    )
    {
        ImU32 c;

        c = ImGui::GetColorU32(ImGuiCol_Border);

        switch (_emph)
        {
            case emphasis::primary:
                c = IM_COL32( 90, 140, 220, 255);
                break;
            case emphasis::secondary:
                c = IM_COL32(150, 150, 170, 255);
                break;
            case emphasis::success:
                c = IM_COL32( 90, 190, 110, 255);
                break;
            case emphasis::warning:
                c = IM_COL32(220, 180,  70, 255);
                break;
            case emphasis::danger:
                c = IM_COL32(220,  90,  90, 255);
                break;
            case emphasis::info:
                c = IM_COL32( 90, 170, 220, 255);
                break;
            case emphasis::muted:
                c = ImGui::GetColorU32(ImGuiCol_BorderShadow);
                break;
            case emphasis::normal:
            default:
                break;
        }

        return c;
    }

    /*
    imgui_frame_vertical_title_size
      Computes the bounding box of a title rendered as stacked characters
    for a vertical edge.  Width is the widest character in the title;
    height is character-count * line-height.

    Parameter(s):
      _title: the title string.
    Return:
      An ImVec2 containing the (width, height) bounding size.  Returns
      (0, 0) for an empty title.
    */
    inline ImVec2
    imgui_frame_vertical_title_size(
        const std::string& _title
    )
    {
        ImVec2 line_size;
        float  max_w;
        float  total_h;
        float  line_h;
        char   buf[2];

        if (_title.empty())
        {
            return ImVec2(0.0f, 0.0f);
        }

        max_w   = 0.0f;
        total_h = 0.0f;
        line_h  = ImGui::GetTextLineHeight();

        // measure each character individually; max width is the widest
        // character, total height is line_h * char_count
        buf[1] = '\0';
        for (char ch : _title)
        {
            buf[0]    = ch;
            line_size = ImGui::CalcTextSize(buf);
            if (line_size.x > max_w)
            {
                max_w = line_size.x;
            }
            total_h += line_h;
        }

        return ImVec2(max_w, total_h);
    }

    /*
    imgui_frame_draw_vertical_title
      Renders a title stacked character-by-character starting at
    `_origin`, one character per line spaced by ImGui's line height.
    ImGui has no text-rotation primitive; stacking gives a readable
    vertical label without requiring custom glyph transformation.

    Parameter(s):
      _dl:     the draw list to render into.  Must be non-null.
      _origin: the top-left of the first character's cell.
      _title:  the string to render vertically.
      _color:  packed ImU32 text colour.
    Return:
      none.
    */
    inline void
    imgui_frame_draw_vertical_title(
        ImDrawList*        _dl,
        ImVec2             _origin,
        const std::string& _title,
        ImU32              _color
    )
    {
        float       line_h;
        std::size_t i;
        char        buf[2];

        // parameter validation
        if ( (!_dl)           ||
             (_title.empty()) )
        {
            return;
        }

        // initialize
        line_h = ImGui::GetTextLineHeight();
        buf[1] = '\0';

        for (i = 0; i < _title.size(); ++i)
        {
            buf[0] = _title[i];
            _dl->AddText(
                ImVec2(_origin.x,
                       _origin.y + line_h * static_cast<float>(i)),
                _color,
                buf);
        }

        return;
    }

    /*
    imgui_frame_draw_border_notched
      Draws the frame's rectangular border, optionally with a notch
    (gap) on one edge to accommodate a title.  When `_has_notch` is
    false the border is drawn as a single rectangle; when true, the
    titled edge is drawn as two line segments with a gap between
    `_notch_start` and `_notch_end`, and the other three edges are
    drawn as full line segments.

    Parameter(s):
      _dl:          the draw list to render into.  Must be non-null.
      _min:         top-left of the border rectangle.
      _max:         bottom-right of the border rectangle.
      _pos:         which edge the notch is on (ignored if no notch).
      _notch_start: absolute coordinate where the notch begins, along
                    the notched edge's parallel axis.
      _notch_end:   absolute coordinate where the notch ends.
      _has_notch:   true to break the border for a title; false to
                    draw an unbroken rectangle.
      _color:       packed ImU32 border colour.
      _thickness:   line thickness in pixels.
    Return:
      none.
    */
    inline void
    imgui_frame_draw_border_notched(
        ImDrawList*    _dl,
        ImVec2         _min,
        ImVec2         _max,
        title_position _pos,
        float          _notch_start,
        float          _notch_end,
        bool           _has_notch,
        ImU32          _color,
        float          _thickness
    )
    {
        // parameter validation
        if (!_dl)
        {
            return;
        }

        // unbroken rectangle — simplest case
        if (!_has_notch)
        {
            _dl->AddRect(_min, _max, _color, 0.0f,
                         ImDrawFlags_None, _thickness);
            return;
        }

        // draw all four edges, breaking the one containing the title
        switch (_pos)
        {
            case title_position::top:
                // top edge, broken by notch
                _dl->AddLine(
                    ImVec2(_min.x,       _min.y),
                    ImVec2(_notch_start, _min.y),
                    _color, _thickness);
                _dl->AddLine(
                    ImVec2(_notch_end,   _min.y),
                    ImVec2(_max.x,       _min.y),
                    _color, _thickness);
                // left, right, bottom (unbroken)
                _dl->AddLine(_min,                    ImVec2(_min.x, _max.y),
                             _color, _thickness);
                _dl->AddLine(ImVec2(_max.x, _min.y),  _max,
                             _color, _thickness);
                _dl->AddLine(ImVec2(_min.x, _max.y),  _max,
                             _color, _thickness);
                break;

            case title_position::bottom:
                // bottom edge, broken by notch
                _dl->AddLine(
                    ImVec2(_min.x,       _max.y),
                    ImVec2(_notch_start, _max.y),
                    _color, _thickness);
                _dl->AddLine(
                    ImVec2(_notch_end,   _max.y),
                    ImVec2(_max.x,       _max.y),
                    _color, _thickness);
                // top, left, right (unbroken)
                _dl->AddLine(_min,                    ImVec2(_max.x, _min.y),
                             _color, _thickness);
                _dl->AddLine(_min,                    ImVec2(_min.x, _max.y),
                             _color, _thickness);
                _dl->AddLine(ImVec2(_max.x, _min.y),  _max,
                             _color, _thickness);
                break;

            case title_position::left:
                // left edge, broken by notch
                _dl->AddLine(
                    ImVec2(_min.x, _min.y),
                    ImVec2(_min.x, _notch_start),
                    _color, _thickness);
                _dl->AddLine(
                    ImVec2(_min.x, _notch_end),
                    ImVec2(_min.x, _max.y),
                    _color, _thickness);
                // top, right, bottom (unbroken)
                _dl->AddLine(_min,                    ImVec2(_max.x, _min.y),
                             _color, _thickness);
                _dl->AddLine(ImVec2(_max.x, _min.y),  _max,
                             _color, _thickness);
                _dl->AddLine(ImVec2(_min.x, _max.y),  _max,
                             _color, _thickness);
                break;

            case title_position::right:
                // right edge, broken by notch
                _dl->AddLine(
                    ImVec2(_max.x, _min.y),
                    ImVec2(_max.x, _notch_start),
                    _color, _thickness);
                _dl->AddLine(
                    ImVec2(_max.x, _notch_end),
                    ImVec2(_max.x, _max.y),
                    _color, _thickness);
                // top, left, bottom (unbroken)
                _dl->AddLine(_min,                    ImVec2(_max.x, _min.y),
                             _color, _thickness);
                _dl->AddLine(_min,                    ImVec2(_min.x, _max.y),
                             _color, _thickness);
                _dl->AddLine(ImVec2(_min.x, _max.y),  _max,
                             _color, _thickness);
                break;
        }

        return;
    }

    /*
    imgui_frame_title_offset_on_edge
      Computes where the title's leading corner sits along its edge,
    measured as an offset from the edge's min coordinate.  For `left`
    alignment this is a small inset from the corner; for `center` it
    is centred; for `right` it is flush against the far corner minus
    the same inset.  The result is clamped to >= 0 so a title wider
    than the edge still fits (truncation is the renderer's problem,
    not ours).

    Parameter(s):
      _edge_length:  length of the edge along the title's axis.
      _title_length: length of the title along the same axis.
      _align:        alignment policy along the edge.
    Return:
      Offset in pixels from the edge's min coordinate.
    */
    inline float
    imgui_frame_title_offset_on_edge(
        float          _edge_length,
        float          _title_length,
        text_alignment _align
    )
    {
        float offset;

        switch (_align)
        {
            case text_alignment::center:
                offset = (_edge_length - _title_length) * 0.5f;
                break;
            case text_alignment::right:
                offset = _edge_length - _title_length
                       - D_FRAME_TITLE_EDGE_OFFSET;
                break;
            case text_alignment::left:
            default:
                offset = D_FRAME_TITLE_EDGE_OFFSET;
                break;
        }

        // clamp — a title wider than the edge shouldn't wrap backward
        if (offset < 0.0f)
        {
            offset = 0.0f;
        }

        return offset;
    }

NS_END  // internal


// ===============================================================================
//  III. PUBLIC DRAW FUNCTION
// ===============================================================================

/*
imgui_draw_frame
  Renders a frame component using ImGui primitives.  Draws the border
(notched on the titled edge if both show_border and a non-empty title
are present), positions the title at the requested alignment along its
edge, and recurses into the frame's children inside an inset content
region.  For vertical title placements (left/right), the title is
rendered as stacked characters — ImGui has no rotated-text primitive
and stacking gives a readable, predictable result.

  The frame occupies the full current ImGui content region on entry.
To size a frame explicitly, wrap the call in a BeginChild with the
desired extents, or set the cursor position and clip rect before
calling.  The frame reserves layout space equal to the full region it
consumed so ImGui's subsequent cursor math is correct.

Parameter(s):
  _fr:        the frame component to draw.
  _render_fn: callback invoked for each child node of the frame.  If
              null, children are skipped but the frame itself (border
              plus title) is still drawn.  If a child pointer is null,
              that child is skipped.
Return:
  none.
*/
inline void
imgui_draw_frame(
    const frame&          _fr,
    const node_render_fn& _render_fn
)
{
    ImVec2      outer_origin;
    ImVec2      outer_avail;
    ImVec2      border_min;
    ImVec2      border_max;
    ImVec2      inner_origin;
    ImVec2      inner_size;
    ImVec2      title_size;
    ImDrawList* dl;
    ImU32       border_color;
    ImU32       text_color;
    float       line_h;
    float       edge_len;
    float       title_offset;
    float       notch_start;
    float       notch_end;
    float       inset_top;
    float       inset_bot;
    float       inset_left;
    float       inset_right;
    bool        has_title;
    bool        vertical_title;
    bool        has_notch;

    // initialize — scope-wide layout geometry
    outer_origin   = ImGui::GetCursorScreenPos();
    outer_avail    = ImGui::GetContentRegionAvail();
    dl             = ImGui::GetWindowDrawList();
    line_h         = ImGui::GetTextLineHeight();
    border_color   = internal::imgui_frame_emphasis_color(_fr.emph);
    text_color     = ImGui::GetColorU32(ImGuiCol_Text);
    has_title      = !_fr.title.empty();
    vertical_title = ( (_fr.title_pos == title_position::left)  ||
                       (_fr.title_pos == title_position::right) );

    // ─── compute title extent ─────────────────────────────────────────
    if (!has_title)
    {
        title_size = ImVec2(0.0f, 0.0f);
    }
    else if (vertical_title)
    {
        title_size = internal::imgui_frame_vertical_title_size(_fr.title);
    }
    else
    {
        title_size = ImGui::CalcTextSize(_fr.title.c_str());
    }

    // ─── border rect ──────────────────────────────────────────────────
    //   The border is inset on the titled edge by half the title's
    // cross-axis size so the title, when drawn straddling the border
    // line, stays entirely inside the outer reserved region.
    border_min = outer_origin;
    border_max = ImVec2(outer_origin.x + outer_avail.x,
                        outer_origin.y + outer_avail.y);

    if (has_title)
    {
        switch (_fr.title_pos)
        {
            case title_position::top:
                border_min.y += title_size.y * 0.5f;
                break;
            case title_position::bottom:
                border_max.y -= title_size.y * 0.5f;
                break;
            case title_position::left:
                border_min.x += title_size.x * 0.5f;
                break;
            case title_position::right:
                border_max.x -= title_size.x * 0.5f;
                break;
        }
    }

    // ─── content-region insets (from outer_origin) ────────────────────
    inset_top   = D_FRAME_CONTENT_PAD;
    inset_bot   = D_FRAME_CONTENT_PAD;
    inset_left  = D_FRAME_CONTENT_PAD;
    inset_right = D_FRAME_CONTENT_PAD;

    if (has_title)
    {
        switch (_fr.title_pos)
        {
            case title_position::top:
                inset_top   += title_size.y;
                break;
            case title_position::bottom:
                inset_bot   += title_size.y;
                break;
            case title_position::left:
                inset_left  += title_size.x;
                break;
            case title_position::right:
                inset_right += title_size.x;
                break;
        }
    }

    // ─── title offset along its edge + notch range ───────────────────
    edge_len     = 0.0f;
    title_offset = 0.0f;
    notch_start  = 0.0f;
    notch_end    = 0.0f;
    has_notch    = has_title && _fr.show_border;

    if (has_title)
    {
        switch (_fr.title_pos)
        {
            case title_position::top:
            case title_position::bottom:
                edge_len     = outer_avail.x;
                title_offset = internal::imgui_frame_title_offset_on_edge(
                                   edge_len, title_size.x, _fr.title_align);
                notch_start  = border_min.x + title_offset
                             - D_FRAME_TITLE_INSET_PAD;
                notch_end    = border_min.x + title_offset + title_size.x
                             + D_FRAME_TITLE_INSET_PAD;
                break;

            case title_position::left:
            case title_position::right:
                edge_len     = outer_avail.y;
                title_offset = internal::imgui_frame_title_offset_on_edge(
                                   edge_len, title_size.y, _fr.title_align);
                notch_start  = border_min.y + title_offset
                             - D_FRAME_TITLE_INSET_PAD;
                notch_end    = border_min.y + title_offset + title_size.y
                             + D_FRAME_TITLE_INSET_PAD;
                break;
        }
    }

    // ─── draw the border ─────────────────────────────────────────────
    if (_fr.show_border)
    {
        internal::imgui_frame_draw_border_notched(
            dl,
            border_min,
            border_max,
            _fr.title_pos,
            notch_start,
            notch_end,
            has_notch,
            border_color,
            D_FRAME_BORDER_THICKNESS);
    }

    // ─── draw the title ──────────────────────────────────────────────
    if (has_title)
    {
        ImVec2 title_origin;

        switch (_fr.title_pos)
        {
            case title_position::top:
                // straddle the top border line: title vertical centre
                // aligns with border_min.y
                title_origin = ImVec2(
                    border_min.x + title_offset,
                    border_min.y - title_size.y * 0.5f);
                dl->AddText(title_origin, text_color, _fr.title.c_str());
                break;

            case title_position::bottom:
                title_origin = ImVec2(
                    border_min.x + title_offset,
                    border_max.y - title_size.y * 0.5f);
                dl->AddText(title_origin, text_color, _fr.title.c_str());
                break;

            case title_position::left:
                // straddle the left border line: title horizontal centre
                // aligns with border_min.x
                title_origin = ImVec2(
                    border_min.x - title_size.x * 0.5f,
                    border_min.y + title_offset);
                internal::imgui_frame_draw_vertical_title(
                    dl, title_origin, _fr.title, text_color);
                break;

            case title_position::right:
                title_origin = ImVec2(
                    border_max.x - title_size.x * 0.5f,
                    border_min.y + title_offset);
                internal::imgui_frame_draw_vertical_title(
                    dl, title_origin, _fr.title, text_color);
                break;
        }
    }

    // ─── compute inner content region for children ───────────────────
    inner_origin = ImVec2(outer_origin.x + inset_left,
                          outer_origin.y + inset_top);
    inner_size   = ImVec2(outer_avail.x - inset_left - inset_right,
                          outer_avail.y - inset_top  - inset_bot);

    // clamp — BeginChild rejects zero / negative sizes
    if (inner_size.x < 1.0f)
    {
        inner_size.x = 1.0f;
    }
    if (inner_size.y < 1.0f)
    {
        inner_size.y = 1.0f;
    }

    // ─── render children inside the content region ───────────────────
    ImGui::SetCursorScreenPos(inner_origin);
    ImGui::PushID(static_cast<const void*>(&_fr));

    if (ImGui::BeginChild("##uxoxo_frame_inner",
                          inner_size,
                          false,
                          ImGuiWindowFlags_NoScrollbar))
    {
        if (_render_fn)
        {
            for (const auto& child : _fr.children)
            {
                if (child)
                {
                    _render_fn(*child);
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::PopID();

    // ─── reserve layout space for the entire outer region ────────────
    ImGui::SetCursorScreenPos(outer_origin);
    ImGui::Dummy(outer_avail);

    return;
}


NS_END  // imgui
NS_END  // ui
NS_END  // uxoxo


#endif  // UXOXO_UI_IMGUI_FRAME_DRAW_
