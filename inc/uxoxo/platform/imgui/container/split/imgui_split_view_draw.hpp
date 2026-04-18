/*******************************************************************************
* uxoxo [ui/imgui]                                     imgui_split_view_draw.hpp
*
* ImGui renderer for the `split_view` template:
*   Lays out N panes along the main axis with interactive splitters
* between them.  The renderer is a function template because split_view
* itself is a template; it compiles to whichever feature set the caller
* instantiates.
*
*   Because split_view lives at the component-template layer (not the
* ui/node layer), pane content is routed by `user_id` rather than by
* node pointer.  The caller supplies a `split_pane_render_fn` that
* receives a user_id and the pane's content rect, and is responsible
* for drawing the content however it sees fit.
*
*   All interactive behaviour — drag-to-resize, hover cursor, double-
* click dispatch — is driven through the template's own free functions
* (spl_begin_drag, spl_update_drag, spl_handle_double_click, etc.) so
* the drag state, constraints, snap points, and callbacks all go
* through the canonical code paths.
*
*   The renderer requires the sf_drag_state feature on the split_view
* to support interactive resizing; without it, splitters are drawn as
* inert lines and the user can resize only via programmatic
* spl_move_splitter calls.
*
*
* path:      /inc/uxoxo/ui/imgui/imgui_split_view_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.17
*******************************************************************************/

#ifndef  UXOXO_UI_IMGUI_SPLIT_VIEW_DRAW_
#define  UXOXO_UI_IMGUI_SPLIT_VIEW_DRAW_ 1

// std
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <functional>

// imgui
#include <imgui.h>

// uxoxo
#include "../../uxoxo.hpp"
#include "../../templates/component/split/split_view.hpp"


NS_UXOXO
NS_UI
NS_IMGUI


// ===============================================================================
//  I.   RENDER CALLBACK TYPE
// ===============================================================================

// split_pane_render_fn
//   type: callback invoked once per pane.  The integrating layer
// receives the pane's user_id and content rect (ImVec2 size) and is
// free to render whatever content corresponds to that user_id.  The
// ImGui cursor is positioned at the pane's content origin on entry;
// the callback is inside a BeginChild frame, so its cursor math is
// relative to the pane's own content region.
using split_pane_render_fn =
    std::function<void(std::size_t user_id, ImVec2 size)>;


// ===============================================================================
//  II.  STYLE CONSTANTS
// ===============================================================================

// D_SPLIT_SEPARATOR_THICKNESS
//   constant: pixel thickness of the visible separator line drawn at
// the centre of each splitter's interactive hit region.
#define D_SPLIT_SEPARATOR_THICKNESS     1.0f

// D_SPLIT_MIN_PANE_EXTENT
//   constant: minimum main-axis size of a pane for which BeginChild
// will be issued.  Below this the pane is skipped (no child frame
// created) — ImGui's BeginChild does not accept sub-pixel sizes.
#define D_SPLIT_MIN_PANE_EXTENT         1.0f


// ===============================================================================
//  III. INTERNAL HELPERS
// ===============================================================================

NS_INTERNAL

    /*
    imgui_split_separator_color
      Selects the separator line colour based on the splitter's
    interaction state.  Active (being dragged) and hovered states use
    the theme's active/hover separator colours; idle uses the plain
    separator colour.

    Parameter(s):
      _is_hovered: true if the splitter is under the cursor.
      _is_active:  true if the splitter is being dragged.
    Return:
      A packed ImU32 colour.
    */
    inline ImU32
    imgui_split_separator_color(
        bool _is_hovered,
        bool _is_active
    )
    {
        if (_is_active)
        {
            return ImGui::GetColorU32(ImGuiCol_SeparatorActive);
        }
        if (_is_hovered)
        {
            return ImGui::GetColorU32(ImGuiCol_SeparatorHovered);
        }

        return ImGui::GetColorU32(ImGuiCol_Separator);
    }

    /*
    imgui_split_draw_separator_line
      Draws a thin visible line at the centre of a splitter's hit
    rect.  The hit rect itself is invisible (an InvisibleButton); this
    line gives the user something to see.

    Parameter(s):
      _dl:        the draw list to render into.
      _min:       top-left of the splitter hit rect.
      _max:       bottom-right of the splitter hit rect.
      _is_horiz:  true for horizontally-oriented view (vertical line).
      _color:     packed ImU32 line colour.
    Return:
      none.
    */
    inline void
    imgui_split_draw_separator_line(
        ImDrawList* _dl,
        ImVec2      _min,
        ImVec2      _max,
        bool        _is_horiz,
        ImU32       _color
    )
    {
        if (!_dl)
        {
            return;
        }

        if (_is_horiz)
        {
            float cx = (_min.x + _max.x) * 0.5f;
            _dl->AddLine(
                ImVec2(cx, _min.y),
                ImVec2(cx, _max.y),
                _color,
                D_SPLIT_SEPARATOR_THICKNESS);
        }
        else
        {
            float cy = (_min.y + _max.y) * 0.5f;
            _dl->AddLine(
                ImVec2(_min.x, cy),
                ImVec2(_max.x, cy),
                _color,
                D_SPLIT_SEPARATOR_THICKNESS);
        }

        return;
    }

NS_END  // internal


// ===============================================================================
//  IV.  PUBLIC DRAW FUNCTION
// ===============================================================================

/*
imgui_draw_split_view
  Renders a split_view by laying out its panes along the main axis
with interactive splitters between them.  Pane content is drawn by
the caller-supplied render callback.  Resize-to-fit, drag interaction,
hover cursors, and double-click dispatch are all wired into the
corresponding spl_* free functions so the template's invariants and
callbacks fire through their canonical paths.

  Behaviour depending on the view's feature set:
    - Without sf_drag_state the splitters are inert visual lines.  The
      caller can still resize programmatically via spl_move_splitter.
    - Without sf_resize_policy container-fit resize uses the default
      proportional strategy.
    - With sf_double_click, a double-click on a splitter dispatches
      to spl_handle_double_click.
    - With sf_snap_points, drag updates honour configured snap points
      inside spl_update_drag.

Parameter(s):
  _sv:      the split_view to render.  Non-const because drag and
            hover state are updated in place.
  _pane_cb: callback invoked per pane with the pane's user_id and
            content size.  If null, panes are drawn as empty frames.
Return:
  none.
*/
template <unsigned _PF, unsigned _CF>
void
imgui_draw_split_view(
    split_view<_PF, _CF>&       _sv,
    const split_pane_render_fn& _pane_cb
)
{
    ImVec2      outer_origin;
    ImVec2      outer_avail;
    ImDrawList* dl;
    float       total_on_axis;
    float       cross_extent;
    float       offset;
    bool        is_horiz;
    std::size_t n;
    std::size_t i;
    char        id_buf[32];

    // initialize
    n = _sv.panes.size();
    if (n == 0)
    {
        return;
    }

    outer_origin = ImGui::GetCursorScreenPos();
    outer_avail  = ImGui::GetContentRegionAvail();
    dl           = ImGui::GetWindowDrawList();
    is_horiz     = (_sv.orientation == split_orientation::horizontal);

    total_on_axis = is_horiz ? outer_avail.x : outer_avail.y;
    cross_extent  = is_horiz ? outer_avail.y : outer_avail.x;

    // fit panes to the available extent.  We call spl_resize every
    // frame; it's cheap and keeps the layout valid when the parent
    // region changes size between frames.
    {
        float current = spl_total_size(_sv);
        if (std::abs(current - total_on_axis) > 0.5f)
        {
            spl_resize(_sv, total_on_axis);
        }
    }

    // clear hover state — it's re-set below if a splitter is hovered
    if constexpr (has_sf(_CF, sf_drag_state))
    {
        if (_sv.mode != split_drag_mode::dragging)
        {
            _sv.hovered_splitter = static_cast<std::size_t>(-1);
            _sv.mode             = split_drag_mode::idle;
        }
    }

    // ─── iterate panes + splitters ───────────────────────────────────
    offset = 0.0f;
    for (i = 0; i < n; ++i)
    {
        const auto& pane      = _sv.panes[i];
        float       pane_size = pane.size;

        // skip panes below the minimum extent (collapsed / zero-sized).
        // Their space is simply not rendered; subsequent panes advance
        // the offset normally.
        if (pane_size >= D_SPLIT_MIN_PANE_EXTENT)
        {
            ImVec2 pane_origin =
                is_horiz
                    ? ImVec2(outer_origin.x + offset, outer_origin.y)
                    : ImVec2(outer_origin.x,         outer_origin.y + offset);
            ImVec2 pane_content_size =
                is_horiz
                    ? ImVec2(pane_size,    cross_extent)
                    : ImVec2(cross_extent, pane_size);

            ImGui::SetCursorScreenPos(pane_origin);
            ImGui::PushID(static_cast<int>(i));

            std::snprintf(id_buf, sizeof(id_buf),
                          "##uxoxo_split_pane_%zu", i);
            if (ImGui::BeginChild(id_buf,
                                  pane_content_size,
                                  false,
                                  ImGuiWindowFlags_NoScrollbar))
            {
                if (_pane_cb)
                {
                    _pane_cb(pane.user_id, pane_content_size);
                }
            }
            ImGui::EndChild();

            ImGui::PopID();
        }

        offset += pane_size;

        // splitter after this pane (not after the last one)
        if (i + 1 < n)
        {
            bool splitter_visible     = true;
            float splitter_thickness  = _sv.splitter_thickness;

            // per-splitter overrides
            if constexpr (has_sf(_CF, sf_splitter_state))
            {
                if (i < _sv.splitters.size())
                {
                    splitter_visible = _sv.splitters[i].visible;
                    if (_sv.splitters[i].thickness_override > 0.0f)
                    {
                        splitter_thickness =
                            _sv.splitters[i].thickness_override;
                    }
                }
            }

            ImVec2 split_origin =
                is_horiz
                    ? ImVec2(outer_origin.x + offset, outer_origin.y)
                    : ImVec2(outer_origin.x,         outer_origin.y + offset);
            ImVec2 split_size =
                is_horiz
                    ? ImVec2(splitter_thickness, cross_extent)
                    : ImVec2(cross_extent,       splitter_thickness);

            ImGui::SetCursorScreenPos(split_origin);
            ImGui::PushID(static_cast<int>(i));
            std::snprintf(id_buf, sizeof(id_buf),
                          "##uxoxo_split_handle_%zu", i);

            // invisible hit button carries all the interaction
            ImGui::InvisibleButton(id_buf, split_size);

            bool hovered = ImGui::IsItemHovered();
            bool active  = ImGui::IsItemActive();

            // hover → cursor feedback + drag_state update
            if (hovered)
            {
                ImGui::SetMouseCursor(
                    is_horiz
                        ? ImGuiMouseCursor_ResizeEW
                        : ImGuiMouseCursor_ResizeNS);

                if constexpr (has_sf(_CF, sf_drag_state))
                {
                    _sv.hovered_splitter = i;
                    if (_sv.mode == split_drag_mode::idle)
                    {
                        _sv.mode = split_drag_mode::hovering;
                    }
                }
            }

            // drag begin — InvisibleButton becomes active on press
            if constexpr (has_sf(_CF, sf_drag_state))
            {
                if ( (active) &&
                     (_sv.mode != split_drag_mode::dragging) )
                {
                    float pointer_axis =
                        is_horiz
                            ? ImGui::GetIO().MousePos.x
                            : ImGui::GetIO().MousePos.y;
                    spl_begin_drag(_sv, i, pointer_axis);
                }

                // drag update while active
                if ( (active) &&
                     (_sv.mode == split_drag_mode::dragging) &&
                     (_sv.active_splitter == i) )
                {
                    float delta =
                        is_horiz
                            ? ImGui::GetIO().MouseDelta.x
                            : ImGui::GetIO().MouseDelta.y;
                    if (delta != 0.0f)
                    {
                        spl_update_drag(_sv, delta);
                    }
                }

                // drag end
                if ( (!active) &&
                     (_sv.mode == split_drag_mode::dragging) &&
                     (_sv.active_splitter == i) )
                {
                    spl_end_drag(_sv);
                }
            }
            else
            {
                // no drag state feature: fall back to direct resize
                // on active + mouse delta.  Still respects constraints
                // via spl_move_splitter.
                if ( (active) &&
                     (_sv.resizable) )
                {
                    float delta =
                        is_horiz
                            ? ImGui::GetIO().MouseDelta.x
                            : ImGui::GetIO().MouseDelta.y;
                    if (delta != 0.0f)
                    {
                        spl_move_splitter(_sv, i, delta);
                    }
                }
            }

            // double-click dispatch
            if constexpr (has_sf(_CF, sf_double_click))
            {
                if ( (hovered) &&
                     (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) )
                {
                    spl_handle_double_click(_sv, i);
                }
            }

            // visible separator line, unless the per-splitter state
            // explicitly hides it
            if (splitter_visible)
            {
                ImU32 col =
                    internal::imgui_split_separator_color(hovered, active);
                internal::imgui_split_draw_separator_line(
                    dl,
                    split_origin,
                    ImVec2(split_origin.x + split_size.x,
                           split_origin.y + split_size.y),
                    is_horiz,
                    col);
            }

            ImGui::PopID();

            offset += splitter_thickness;
        }
    }

    // ─── reserve layout space equal to what we consumed ──────────────
    ImGui::SetCursorScreenPos(outer_origin);
    ImGui::Dummy(outer_avail);

    return;
}


NS_END  // imgui
NS_END  // ui
NS_END  // uxoxo


#endif  // UXOXO_UI_IMGUI_SPLIT_VIEW_DRAW_
