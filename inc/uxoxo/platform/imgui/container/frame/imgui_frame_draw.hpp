/*******************************************************************************
* uxoxo [imgui]                                            imgui_frame_draw.hpp
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
*   Since `frame` no longer owns a child list, this renderer exposes
* three complementary entry points for attaching content:
*
*     imgui_frame_begin / imgui_frame_end
*       Paired begin/end calls with a state struct passed between.
*       Use when content cannot be wrapped in a lambda — stateful
*       flow, early-return, interspersed draw calls.
*
*     imgui_draw_frame
*       Single-call form taking a user content callable.  A thin
*       wrapper around begin/end; convenient for the common case.
*
*     imgui_frame_scope
*       RAII wrapper that calls begin in its constructor and end in
*       its destructor.  Use when exception safety matters or when
*       the "never forget end" guarantee is worth a stack object.
*
*   Feature-flagged behaviour is dispatched with if constexpr on the
* compile-time feature constants exposed by frame<_F>:
*
*     frf_emphasis     → border colour mapped from semantic severity.
*     frf_collapsible  → title acts as a toggle; collapsed frames omit
*                        the content region and shrink their layout
*                        reservation.
*     frf_hover_state  → the frame's `hovered` member is updated each
*                        draw from IsMouseHoveringRect over the border.
*     frf_tooltip      → on hover, the frame's `tooltip` string is
*                        shown via ImGui::SetTooltip.
*
*   The renderer itself is header-only and depends only on ImGui and
* the frame component.  It does not depend on components.hpp or any
* node-variant infrastructure.
*
*
* path:      /inc/uxoxo/platform/imgui/container/frame/imgui_frame_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_IMGUI_COMPONENT_FRAME_DRAW_
#define  UXOXO_IMGUI_COMPONENT_FRAME_DRAW_ 1

// std
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../../uxoxo.hpp"
#include "../../../../templates/component/container/frame/frame.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI


// bring the frame component names into scope for the renderer.
// The renderer is coupled to the frame component and having to
// write `component::frame<_F>`, `component::frame_title_position`,
// etc. at every call site hurts readability without adding safety.
using component::frame;
using component::frame_title_position;
using component::frame_title_align;
using component::frame_emphasis;
using component::has_frf;
using component::frf_emphasis;
using component::frf_collapsible;
using component::frf_hover_state;
using component::frf_tooltip;


// ===============================================================================
//  I.   STYLE CONSTANTS
// ===============================================================================

// D_FRAME_TITLE_INSET_PAD
//   constant: horizontal (or cross-axis) gap in pixels between the title
// text and the border-notch endpoints on either side of it.
#define D_FRAME_TITLE_INSET_PAD         4.0f

// D_FRAME_TITLE_EDGE_OFFSET
//   constant: minimum distance in pixels from a frame corner to the start
// of the title, applied to `start` and `end` alignments so the title
// never collides with the corner of the border.
#define D_FRAME_TITLE_EDGE_OFFSET       8.0f

// D_FRAME_CONTENT_PAD
//   constant: inner padding in pixels between the border and the child
// content region, applied on every edge.
#define D_FRAME_CONTENT_PAD             6.0f

// D_FRAME_BORDER_THICKNESS
//   constant: thickness in pixels of the border line.
#define D_FRAME_BORDER_THICKNESS        1.0f

// D_FRAME_COLLAPSED_PAD
//   constant: extra pixels of padding placed around the title when the
// frame is drawn collapsed, so the reserved layout space leaves a
// comfortable margin around the title text.
#define D_FRAME_COLLAPSED_PAD           4.0f


// ===============================================================================
//  II.  INTERNAL HELPERS
// ===============================================================================

NS_INTERNAL

    /*
    imgui_frame_emphasis_color
      Maps a frame's semantic emphasis to a concrete ImGui border colour.
    `frame_emphasis::normal` resolves to the current theme's
    ImGuiCol_Border so the frame blends with ambient chrome; all other
    emphases resolve to fixed palette entries that read consistently
    across themes.

    Parameter(s):
      _emph: the semantic emphasis to map.
    Return:
      An ImU32 packed colour suitable for ImDrawList primitives.
    */
    inline ImU32
    imgui_frame_emphasis_color(
        frame_emphasis _emph
    )
    {
        ImU32 c;

        c = ImGui::GetColorU32(ImGuiCol_Border);

        switch (_emph)
        {
            case frame_emphasis::primary:
                c = IM_COL32( 90, 140, 220, 255);
                break;
            case frame_emphasis::secondary:
                c = IM_COL32(150, 150, 170, 255);
                break;
            case frame_emphasis::success:
                c = IM_COL32( 90, 190, 110, 255);
                break;
            case frame_emphasis::warning:
                c = IM_COL32(220, 180,  70, 255);
                break;
            case frame_emphasis::danger:
                c = IM_COL32(220,  90,  90, 255);
                break;
            case frame_emphasis::info:
                c = IM_COL32( 90, 170, 220, 255);
                break;
            case frame_emphasis::muted:
                c = ImGui::GetColorU32(ImGuiCol_BorderShadow);
                break;
            case frame_emphasis::normal:
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
        ImDrawList*          _dl,
        ImVec2               _min,
        ImVec2               _max,
        frame_title_position _pos,
        float                _notch_start,
        float                _notch_end,
        bool                 _has_notch,
        ImU32                _color,
        float                _thickness
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
            case frame_title_position::top:
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

            case frame_title_position::bottom:
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

            case frame_title_position::left:
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

            case frame_title_position::right:
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
    measured as an offset from the edge's min coordinate.  For `start`
    alignment this is a small inset from the corner; for `center` it
    is centred; for `end` it is flush against the far corner minus
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
        float             _edge_length,
        float             _title_length,
        frame_title_align _align
    )
    {
        float offset;

        switch (_align)
        {
            case frame_title_align::center:
                offset = (_edge_length - _title_length) * 0.5f;
                break;
            case frame_title_align::end:
                offset = _edge_length - _title_length
                       - D_FRAME_TITLE_EDGE_OFFSET;
                break;
            case frame_title_align::start:
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
//  III. DRAW STATE
// ===============================================================================
//   State produced by imgui_frame_begin and consumed by imgui_frame_end.
// The state carries just enough layout information for end to correctly
// reserve outer layout space and balance the BeginChild / EndChild pair
// that wraps the content region.

// imgui_frame_draw_state
//   struct: layout state passed from imgui_frame_begin to
// imgui_frame_end.  Opaque to callers except that it should be kept
// on the stack across the content-draw region.
struct imgui_frame_draw_state
{
    ImVec2 outer_origin;     // top-left of the entire frame rect
    ImVec2 outer_avail;      // size of the entire frame rect
    bool   child_begun;      // true when BeginChild was called (→ call EndChild)
    bool   collapsed;        // true when the frame was drawn collapsed
    bool   visible;          // false skips content; end becomes a no-op
};


// ===============================================================================
//  IV.  BEGIN / END
// ===============================================================================

/*
imgui_frame_begin
  Begins a frame render.  Computes the layout, draws the border and
title chrome, updates feature-gated state (hovered, collapsible
toggle), optionally shows a tooltip, and — unless the frame is
invisible or collapsed — opens a BeginChild scope for the inner
content region.  The caller then draws content inside the child
scope and calls imgui_frame_end with the returned state.

  When the frame is collapsible and currently collapsed, the
content region is skipped entirely; the reserved layout space
shrinks to just the title row (or column) plus border padding.
The title rect acts as a clickable toggle whenever user_collapsible
is true.

  When the frame is invisible (`visible == false`), this function
returns early with a sentinel state; imgui_frame_end then becomes
a no-op.  This keeps the begin/end contract symmetric.

Parameter(s):
  _fr: the frame to render.  Non-const because the renderer writes
       to the hovered flag (if present) and may mutate the collapsed
       state (if present) in response to a click.
Return:
  An imgui_frame_draw_state carrying layout information for the
  matching imgui_frame_end call.
*/
template <unsigned _F>
imgui_frame_draw_state
imgui_frame_begin(
    frame<_F>& _fr
)
{
    imgui_frame_draw_state st;
    ImVec2                 outer_origin;
    ImVec2                 outer_avail;
    ImVec2                 border_min;
    ImVec2                 border_max;
    ImVec2                 inner_origin;
    ImVec2                 inner_size;
    ImVec2                 title_size;
    ImVec2                 title_rect_min;
    ImVec2                 title_rect_max;
    ImDrawList*            dl;
    ImU32                  border_color;
    ImU32                  text_color;
    float                  edge_len;
    float                  title_offset;
    float                  notch_start;
    float                  notch_end;
    float                  inset_top;
    float                  inset_bot;
    float                  inset_left;
    float                  inset_right;
    bool                   has_title;
    bool                   vertical_title;
    bool                   has_notch;
    bool                   is_collapsed;
    bool                   hovered_now;

    // invisible frame — return sentinel, matching end is a no-op
    if (!_fr.visible)
    {
        st.outer_origin = ImVec2(0.0f, 0.0f);
        st.outer_avail  = ImVec2(0.0f, 0.0f);
        st.child_begun  = false;
        st.collapsed    = false;
        st.visible      = false;
        return st;
    }

    // initialize — scope-wide layout geometry
    outer_origin   = ImGui::GetCursorScreenPos();
    outer_avail    = ImGui::GetContentRegionAvail();
    dl             = ImGui::GetWindowDrawList();
    text_color     = ImGui::GetColorU32(ImGuiCol_Text);
    has_title      = !_fr.title.empty();
    vertical_title = _fr.is_vertical_title();

    // resolve emphasis → border colour (feature-gated)
    if constexpr (frame<_F>::has_emphasis)
    {
        border_color = internal::imgui_frame_emphasis_color(_fr.emph);
    }
    else
    {
        border_color = ImGui::GetColorU32(ImGuiCol_Border);
    }

    // resolve collapsed state (feature-gated)
    is_collapsed = false;
    if constexpr (frame<_F>::is_collapsible)
    {
        is_collapsed = _fr.collapsed;
    }

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
    //   When collapsed the border shrinks to hug just the title row /
    // column plus a small padding; the full outer_avail is still
    // reserved at layout-end time only if NOT collapsed.
    border_min = outer_origin;
    border_max = ImVec2(outer_origin.x + outer_avail.x,
                        outer_origin.y + outer_avail.y);

    if (is_collapsed)
    {
        // collapse the border perpendicular to the title edge
        switch (_fr.title_pos)
        {
            case frame_title_position::top:
                border_max.y = border_min.y
                             + title_size.y
                             + D_FRAME_COLLAPSED_PAD * 2.0f;
                border_min.y += title_size.y * 0.5f;
                break;
            case frame_title_position::bottom:
                border_min.y = border_max.y
                             - title_size.y
                             - D_FRAME_COLLAPSED_PAD * 2.0f;
                border_max.y -= title_size.y * 0.5f;
                break;
            case frame_title_position::left:
                border_max.x = border_min.x
                             + title_size.x
                             + D_FRAME_COLLAPSED_PAD * 2.0f;
                border_min.x += title_size.x * 0.5f;
                break;
            case frame_title_position::right:
                border_min.x = border_max.x
                             - title_size.x
                             - D_FRAME_COLLAPSED_PAD * 2.0f;
                border_max.x -= title_size.x * 0.5f;
                break;
        }
    }
    else if (has_title)
    {
        switch (_fr.title_pos)
        {
            case frame_title_position::top:
                border_min.y += title_size.y * 0.5f;
                break;
            case frame_title_position::bottom:
                border_max.y -= title_size.y * 0.5f;
                break;
            case frame_title_position::left:
                border_min.x += title_size.x * 0.5f;
                break;
            case frame_title_position::right:
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
            case frame_title_position::top:
                inset_top   += title_size.y;
                break;
            case frame_title_position::bottom:
                inset_bot   += title_size.y;
                break;
            case frame_title_position::left:
                inset_left  += title_size.x;
                break;
            case frame_title_position::right:
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
            case frame_title_position::top:
            case frame_title_position::bottom:
                edge_len     = border_max.x - border_min.x;
                title_offset = internal::imgui_frame_title_offset_on_edge(
                                   edge_len, title_size.x, _fr.title_align);
                notch_start  = border_min.x + title_offset
                             - D_FRAME_TITLE_INSET_PAD;
                notch_end    = border_min.x + title_offset + title_size.x
                             + D_FRAME_TITLE_INSET_PAD;
                break;

            case frame_title_position::left:
            case frame_title_position::right:
                edge_len     = border_max.y - border_min.y;
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

    // ─── compute & cache the title rect for hit-testing ──────────────
    //   title_rect_min / _max bound the interactive title hit zone
    // used by hover detection and the collapsible toggle.
    title_rect_min = ImVec2(0.0f, 0.0f);
    title_rect_max = ImVec2(0.0f, 0.0f);

    if (has_title)
    {
        switch (_fr.title_pos)
        {
            case frame_title_position::top:
                title_rect_min = ImVec2(
                    border_min.x + title_offset,
                    border_min.y - title_size.y * 0.5f);
                title_rect_max = ImVec2(
                    title_rect_min.x + title_size.x,
                    title_rect_min.y + title_size.y);
                break;
            case frame_title_position::bottom:
                title_rect_min = ImVec2(
                    border_min.x + title_offset,
                    border_max.y - title_size.y * 0.5f);
                title_rect_max = ImVec2(
                    title_rect_min.x + title_size.x,
                    title_rect_min.y + title_size.y);
                break;
            case frame_title_position::left:
                title_rect_min = ImVec2(
                    border_min.x - title_size.x * 0.5f,
                    border_min.y + title_offset);
                title_rect_max = ImVec2(
                    title_rect_min.x + title_size.x,
                    title_rect_min.y + title_size.y);
                break;
            case frame_title_position::right:
                title_rect_min = ImVec2(
                    border_max.x - title_size.x * 0.5f,
                    border_min.y + title_offset);
                title_rect_max = ImVec2(
                    title_rect_min.x + title_size.x,
                    title_rect_min.y + title_size.y);
                break;
        }
    }

    // ─── draw the title ──────────────────────────────────────────────
    if (has_title)
    {
        switch (_fr.title_pos)
        {
            case frame_title_position::top:
            case frame_title_position::bottom:
                dl->AddText(title_rect_min, text_color, _fr.title.c_str());
                break;

            case frame_title_position::left:
            case frame_title_position::right:
                internal::imgui_frame_draw_vertical_title(
                    dl, title_rect_min, _fr.title, text_color);
                break;
        }
    }

    // ─── push an ID scope for interactive bits ───────────────────────
    ImGui::PushID(static_cast<const void*>(&_fr));

    // ─── collapsible toggle over the title rect ──────────────────────
    if constexpr (frame<_F>::is_collapsible)
    {
        // only render the toggle when there's actually a title to hit
        if ( (has_title) &&
             (_fr.user_collapsible) )
        {
            ImVec2 cursor_restore;
            ImVec2 btn_size;

            cursor_restore = ImGui::GetCursorScreenPos();
            btn_size       = ImVec2(
                title_rect_max.x - title_rect_min.x,
                title_rect_max.y - title_rect_min.y);

            // clamp — InvisibleButton rejects zero / negative sizes
            if (btn_size.x < 1.0f)
            {
                btn_size.x = 1.0f;
            }
            if (btn_size.y < 1.0f)
            {
                btn_size.y = 1.0f;
            }

            ImGui::SetCursorScreenPos(title_rect_min);
            if (ImGui::InvisibleButton("##uxoxo_frame_toggle", btn_size))
            {
                _fr.collapsed = !_fr.collapsed;
                if (_fr.on_toggle)
                {
                    _fr.on_toggle(_fr.collapsed);
                }
                is_collapsed = _fr.collapsed;
            }
            ImGui::SetCursorScreenPos(cursor_restore);
        }
    }

    // ─── hover state (frf_hover_state) ───────────────────────────────
    hovered_now = false;
    if constexpr (frame<_F>::has_hover_state)
    {
        hovered_now = ImGui::IsMouseHoveringRect(border_min, border_max);
        _fr.hovered = hovered_now;
    }
    else if constexpr (frame<_F>::has_tooltip)
    {
        // tooltip without a stored hover flag — still need the test
        hovered_now = ImGui::IsMouseHoveringRect(border_min, border_max);
    }

    // ─── tooltip on hover (frf_tooltip) ──────────────────────────────
    if constexpr (frame<_F>::has_tooltip)
    {
        if ( (hovered_now)           &&
             (!_fr.tooltip.empty()) )
        {
            ImGui::SetTooltip("%s", _fr.tooltip.c_str());
        }
    }

    // ─── open content region (skipped when collapsed) ────────────────
    st.outer_origin = outer_origin;
    st.outer_avail  = outer_avail;
    st.collapsed    = is_collapsed;
    st.visible      = true;
    st.child_begun  = false;

    if (!is_collapsed)
    {
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

        ImGui::SetCursorScreenPos(inner_origin);

        if (ImGui::BeginChild("##uxoxo_frame_inner",
                              inner_size,
                              false,
                              ImGuiWindowFlags_NoScrollbar))
        {
            st.child_begun = true;
        }
        else
        {
            // BeginChild returned false — EndChild still required
            st.child_begun = true;
        }
    }

    return st;
}

/*
imgui_frame_end
  Finalises a frame render.  Closes the BeginChild scope that
imgui_frame_begin opened (if any), pops the ID scope, and reserves
ImGui layout space equal to the full region the frame consumed so
subsequent cursor math is correct.

  For a collapsed frame the reserved layout space shrinks to the
visible chrome extent (title row / column plus padding) rather than
the full region that would have been occupied if expanded.

Parameter(s):
  _state: the state returned by the matching imgui_frame_begin.
Return:
  none.
*/
inline void
imgui_frame_end(
    const imgui_frame_draw_state& _state
)
{
    ImVec2 reserve_size;

    // invisible frame — begin returned a sentinel, nothing to close
    if (!_state.visible)
    {
        return;
    }

    // close the content scope if one was opened
    if (_state.child_begun)
    {
        ImGui::EndChild();
    }

    ImGui::PopID();

    // reserve layout space
    //   Expanded:  the full outer region the frame consumed.
    //   Collapsed: just the visible chrome extent along the title's
    //              perpendicular axis, full extent along the parallel
    //              axis (so sibling frames still line up).
    ImGui::SetCursorScreenPos(_state.outer_origin);

    if (_state.collapsed)
    {
        //   A collapsed frame occupies a strip: full width (or height)
        // along the title's edge, and just enough on the perpendicular
        // axis to contain the collapsed chrome.  We don't know the
        // title edge here without re-deriving it, so we fall back to
        // a single line of ImGui content height — this is a safe
        // underestimate that lets collapsed frames stack neatly.
        reserve_size = ImVec2(
            _state.outer_avail.x,
            ImGui::GetTextLineHeightWithSpacing()
                + D_FRAME_COLLAPSED_PAD * 2.0f);
    }
    else
    {
        reserve_size = _state.outer_avail;
    }

    ImGui::Dummy(reserve_size);

    return;
}


// ===============================================================================
//  V.   CALLBACK CONVENIENCE
// ===============================================================================

/*
imgui_draw_frame
  Single-call convenience wrapper around imgui_frame_begin and
imgui_frame_end.  Invokes `_content` between them for the frame's
interior.  When the frame is invisible or collapsed, the content
callable is not invoked.

  `_content` is any nullary callable — lambda, function pointer,
functor.  Exceptions propagate out of `_content`; in that case the
begin/end pair is still balanced by imgui_frame_end being called
after the throw (which this wrapper does not guarantee — prefer
imgui_frame_scope for exception-safe rendering).

Parameter(s):
  _fr:      the frame to render.
  _content: a nullary callable that draws the frame's interior.  May
            be empty or a no-op.
Return:
  none.
*/
template <unsigned _F,
          typename _ContentFn>
void
imgui_draw_frame(
    frame<_F>&   _fr,
    _ContentFn&& _content
)
{
    imgui_frame_draw_state st;

    st = imgui_frame_begin(_fr);

    if ( (st.visible)        &&
         (st.child_begun)    &&
         (!st.collapsed) )
    {
        _content();
    }

    imgui_frame_end(st);

    return;
}

/*
imgui_draw_frame
  Overload that draws only the frame chrome and reserves layout
space.  Useful when the frame is purely decorative or when content
is drawn through a separate path.

Parameter(s):
  _fr: the frame to render.
Return:
  none.
*/
template <unsigned _F>
void
imgui_draw_frame(
    frame<_F>& _fr
)
{
    imgui_frame_draw_state st;

    st = imgui_frame_begin(_fr);
    imgui_frame_end(st);

    return;
}


// ===============================================================================
//  VI.  RAII SCOPE
// ===============================================================================

// imgui_frame_scope
//   class: RAII wrapper that calls imgui_frame_begin in its
// constructor and imgui_frame_end in its destructor.  Provides
// exception-safe frame rendering — if the content draw throws, the
// destructor still balances the begin/end pair.  Non-copyable,
// non-movable; intended to live on the stack for the duration of
// the content scope.
template <unsigned _F>
class imgui_frame_scope
{
public:
    explicit imgui_frame_scope(
            frame<_F>& _fr
        )
            : m_state(imgui_frame_begin(_fr))
        {}

    ~imgui_frame_scope()
    {
        imgui_frame_end(m_state);
    }

    imgui_frame_scope(const imgui_frame_scope&)            = delete;
    imgui_frame_scope& operator=(const imgui_frame_scope&) = delete;
    imgui_frame_scope(imgui_frame_scope&&)                 = delete;
    imgui_frame_scope& operator=(imgui_frame_scope&&)      = delete;

    // -- queries ----------------------------------------------------------
    //   Allow content-drawing code to ask whether the content region
    // actually opened (i.e. the frame is visible, expanded, and the
    // BeginChild succeeded).  Skip drawing when false.

    [[nodiscard]] bool
    content_open() const noexcept
    {
        return ( (m_state.visible)     &&
                 (m_state.child_begun) &&
                 (!m_state.collapsed) );
    }

    [[nodiscard]] bool
    collapsed() const noexcept
    {
        return m_state.collapsed;
    }

    [[nodiscard]] bool
    visible() const noexcept
    {
        return m_state.visible;
    }

private:
    imgui_frame_draw_state m_state;
};


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_FRAME_DRAW_
