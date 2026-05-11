/******************************************************************************
* uxoxo [imgui]                                                imgui_table.hpp
*
*   Dear ImGui draw handler for the table_view component.  Translates the
* framework-agnostic table_view struct into ImGui::BeginTable / EndTable
* draw calls with full support for:
*   - header / footer / total row regions with distinct styling
*   - merged cell spans
*   - column resizing, sorting, visibility
*   - cursor highlight and selection rendering
*   - inline cell editing via ImGui::InputText
*   - row striping and grid lines
*   - keyboard navigation (arrow keys, page up/down, home/end)
*   - scroll synchronisation between table_view and ImGui clipper
* 
* This module exposes a single entry point, imgui_draw_table_view, which
* is registered with the imgui_renderer's component registry.  It may also
* be called directly.
*   Migration note (2026.05.08): the local `imgui_table_style` namespace
* has been trimmed.  Color slots that match shared palette tags
* (header_bg, footer_bg, total_bg, data_bg_even, data_bg_odd,
* cursor_border, selection_bg, edit_bg, edit_border, sort_active,
* grid_alpha, header_text, data_text) now resolve to `palette::` tags
* from imgui_palette.hpp.  The two table-specific text colors
* (footer_text, total_text) and the edit-buffer sizing constant stay
* local because they have no consumer outside this file and a distinct
* visual identity.  No behavioural change.
*   Structure:
*     1.  style constants
*     2.  internal helpers (region colors, cell rendering)
*     3.  imgui_draw_table_view (main entry point)
*     4.  keyboard input handler
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included before
* this header.
*
*
* path:      /inc/uxoxo/platform/imgui/table/imgui_table.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.04.10
******************************************************************************/

#ifndef UXOXO_COMPONENT_IMGUI_TABLE_
#define UXOXO_COMPONENT_IMGUI_TABLE_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/table/table_view.hpp"
#include "../../../templates/renderer.hpp"
#include "../core/imgui_palette.hpp"
#include "../core/imgui_scope.hpp"


NS_UXOXO
NS_IMGUI

using djinterp::sort_order;
using djinterp::text_alignment;
using uxoxo::component::cell_region;
using uxoxo::component::cell_role;
using uxoxo::component::render_context;
using uxoxo::component::table_view;

// ===========================================================================
//  1.  STYLE CONSTANTS
// ===========================================================================
//   Default colors for table regions.  These are ImVec4 (r,g,b,a) values
// matching ImGui's color format.  A concrete application can override these
// by setting them before calling imgui_draw_table_view if a theming system
// is added later.

//   Migration note (2026.05.08): values matching shared palette tags
// (header_bg, footer_bg, total_bg, data_bg_even, data_bg_odd,
// cursor_border, selection_bg, edit_bg, edit_border, sort_active,
// grid_alpha, header_text, data_text) are now drawn from `palette::`
// (imgui_palette.hpp).  Two text colors (footer_text, total_text) and
// one sizing constant (edit_buffer_capacity) remain local because
// they have no consumer outside the table renderer and a distinct
// visual identity (footer muted, total amber-gold).
namespace imgui_table_style
{
    // text colors - table-specific, kept local
    D_INLINE const ImVec4 total_text  = ImVec4(0.90f, 0.82f, 0.55f, 1.0f);
    D_INLINE const ImVec4 footer_text = ImVec4(0.65f, 0.65f, 0.68f, 1.0f);

    // edit buffer max size - sizing constant, no palette equivalent
    D_INLINE constexpr std::size_t edit_buffer_capacity = 4096;

}   // namespace imgui_table_style


// ===========================================================================
//  2.  INTERNAL HELPERS
// ===========================================================================

NS_INTERNAL

    // imgui_region_bg_color
    //   function: returns the background color for a cell's region.
    D_INLINE ImVec4
    imgui_region_bg_color(
        cell_region _region,
        std::size_t _row,
        bool        _stripe
    )
    {
        switch (_region)
        {
            case cell_region::header:
                return palette::get<palette::table_header_bg_tag>();

            case cell_region::footer:
                return palette::get<palette::table_footer_bg_tag>();

            case cell_region::total:
                return palette::get<palette::table_total_bg_tag>();

            case cell_region::data:
            default:
            {
                if (_stripe && ((_row & 1) == 0))
                {
                    return palette::get<palette::table_data_bg_even_tag>();
                }

                return palette::get<palette::table_data_bg_odd_tag>();
            }
        }
    }

    // imgui_region_text_color
    //   function: returns the text color for a cell's region.
    D_INLINE ImVec4
    imgui_region_text_color(
        cell_region _region
    )
    {
        switch (_region)
        {
            case cell_region::header:
                return palette::get<palette::text_header_tag>();

            case cell_region::footer:
                return imgui_table_style::footer_text;

            case cell_region::total:
                return imgui_table_style::total_text;

            case cell_region::data:
            default:
                return palette::get<palette::text_body_tag>();
        }
    }

    // imgui_alignment_offset
    //   function: computes the x offset for text alignment within a
    // cell of the given width.
    D_INLINE float
    imgui_alignment_offset(
        text_alignment _align,
        float          _cell_width,
        float          _text_width
    )
    {
        switch (_align)
        {
            case text_alignment::center:
            {
                float offset = (_cell_width - _text_width) * 0.5f;

                return (offset > 0.0f) ? offset : 0.0f;
            }

            case text_alignment::right:
            {
                float offset = _cell_width - _text_width;

                return (offset > 0.0f) ? offset : 0.0f;
            }

            case text_alignment::left:
            default:
                return 0.0f;
        }
    }

    // imgui_sort_indicator_text
    //   function: returns a short string for the sort indicator.
    D_INLINE const char*
    imgui_sort_indicator_text(
        sort_order _order
    )
    {
        switch (_order)
        {
            case sort_order::ascending:  return " ^";
            case sort_order::descending: return " v";
            case sort_order::none:
            default:                     return "";
        }
    }

NS_END  // internal


// ===========================================================================
//  3.  IMGUI DRAW TABLE VIEW
// ===========================================================================

// imgui_draw_table_view
//   function: renders a table_view using Dear ImGui.  Call within an
// ImGui window context (between ImGui::Begin / ImGui::End).
// Returns true if any user interaction occurred (click, edit,
// navigation, sort, resize).
D_INLINE bool
imgui_draw_table_view(
    table_view&     _table_view,
    render_context& _ctx
)
{
    bool interacted = false;

    // empty table guard
    if ( (_table_view.num_rows == 0) ||
         (_table_view.num_columns == 0) )
    {
        ImGui::TextDisabled("(empty table)");

        return false;
    }

    // compute visible range
    const auto [vis_row_start, vis_row_end] = table_view_visible_rows(_table_view);
    const auto [vis_col_start, vis_col_end] = table_view_visible_cols(_table_view);

    const std::size_t vis_col_count = vis_col_end - vis_col_start;

    // require at least one visible column
    if (vis_col_count == 0)
    {
        return false;
    }

    // build ImGui table flags
    ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable      |
                                  ImGuiTableFlags_ScrollX        |
                                  ImGuiTableFlags_ScrollY        |
                                  ImGuiTableFlags_BordersInnerV  |
                                  ImGuiTableFlags_RowBg          |
                                  ImGuiTableFlags_SizingStretchProp;

    if (_table_view.show_grid)
    {
        table_flags |= ImGuiTableFlags_BordersInnerH;
    }

    // begin ImGui table
    if (!ImGui::BeginTable("##tv",
                           static_cast<int>(vis_col_count),
                           table_flags))
    {
        return false;
    }

    // -----------------------------------------------------------------
    //  column setup
    // -----------------------------------------------------------------
    for (std::size_t c = vis_col_start; c < vis_col_end; ++c)
    {
        ImGuiTableColumnFlags col_flags = 0;

        float init_width = 0.0f;

        if (c < _table_view.columns.size())
        {
            const auto& col_desc = _table_view.columns[c];

            if (!col_desc.resizable)
            {
                col_flags |= ImGuiTableColumnFlags_NoResize;
            }

            if (!col_desc.visible)
            {
                col_flags |= ImGuiTableColumnFlags_Disabled;
            }

            init_width = static_cast<float>(col_desc.width);
        }

        ImGui::TableSetupColumn(
            (c < _table_view.columns.size())
                ? _table_view.columns[c].name.c_str()
                : "##col",
            col_flags,
            init_width);
    }

    // -----------------------------------------------------------------
    //  header row rendering
    // -----------------------------------------------------------------
    if (_table_view.header_rows > 0)
    {
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

        for (std::size_t c = vis_col_start; c < vis_col_end; ++c)
        {
            ImGui::TableSetColumnIndex(
                static_cast<int>(c - vis_col_start));

            // header text
            std::string header_text;

            if (c < _table_view.columns.size())
            {
                header_text = _table_view.columns[c].name;
            }
            else
            {
                header_text = table_view_cell_text(_table_view,
                                           0,
                                           c);
            }

            // sort indicator
            sort_order col_sort = sort_order::none;

            if (c < _table_view.columns.size())
            {
                col_sort = _table_view.columns[c].sort;
            }

            // draw header with sort indicator
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  palette::get<palette::text_header_tag>());

            if ( (c < _table_view.columns.size()) &&
                 (_table_view.columns[c].sortable) )
            {
                // clickable header for sorting
                std::string label = header_text +
                    internal::imgui_sort_indicator_text(col_sort);

                if (ImGui::Selectable(label.c_str()))
                {
                    table_view_toggle_sort(_table_view,
                                   c);
                    interacted = true;
                }
            }
            else
            {
                ImGui::TextUnformatted(header_text.c_str());
            }

            ImGui::PopStyleColor();
        }
    }

    // -----------------------------------------------------------------
    //  data / total / footer rows
    // -----------------------------------------------------------------
    for (std::size_t r = vis_row_start; r < vis_row_end; ++r)
    {
        // skip header rows (already rendered above)
        if (r < _table_view.header_rows)
        {
            continue;
        }

        ImGui::TableNextRow();

        const cell_region row_region = table_view_cell_region(
            _table_view,
            r,
            _table_view.header_columns);

        // row background (stripe)
        const ImVec4 row_bg = internal::imgui_region_bg_color(
            row_region,
            r,
            _table_view.stripe_rows);

        ImGui::TableSetBgColor(
            ImGuiTableBgTarget_RowBg0,
            ImGui::GetColorU32(row_bg));

        // columns
        for (std::size_t c = vis_col_start; c < vis_col_end; ++c)
        {
            ImGui::TableSetColumnIndex(
                static_cast<int>(c - vis_col_start));

            const cell_role role = table_view_cell_role(_table_view,
                                                r,
                                                c);

            // skip covered cells (part of a span)
            if (role == cell_role::covered)
            {
                continue;
            }

            // skip blank cells
            if (role == cell_role::blank)
            {
                continue;
            }

            const cell_region region = table_view_cell_region(_table_view,
                                                      r,
                                                      c);

            // determine if this cell is being edited
            const bool is_edit_cell =
                ( _table_view.editing              &&
                  (r == _table_view.edit_cell.row)  &&
                  (c == _table_view.edit_cell.col) );

            // determine if this cell is the cursor
            const bool is_cursor =
                ( (r == _table_view.cursor.row) &&
                  (c == _table_view.cursor.col) );

            // determine if this cell is selected
            const bool is_selected = table_view_is_selected(_table_view,
                                                     r,
                                                     c);

            // ---------------------------------------------------------
            //  selection highlight
            // ---------------------------------------------------------
            if (is_selected)
            {
                ImGui::TableSetBgColor(
                    ImGuiTableBgTarget_CellBg,
                    ImGui::GetColorU32(
                        palette::get<palette::selection_bg_tag>()));
            }

            // ---------------------------------------------------------
            //  inline editing
            // ---------------------------------------------------------
            if (is_edit_cell)
            {
                ImGui::PushStyleColor(
                    ImGuiCol_FrameBg,
                    palette::get<palette::table_edit_bg_tag>());
                ImGui::PushStyleColor(
                    ImGuiCol_Border,
                    palette::get<palette::table_edit_border_tag>());

                // size the input to fill the cell
                ImGui::SetNextItemWidth(-FLT_MIN);

                // static buffer for ImGui::InputText
                static char edit_buf[imgui_table_style::edit_buffer_capacity];
                std::size_t copy_len = std::min(
                    _table_view.edit_buffer.size(),
                    imgui_table_style::edit_buffer_capacity - 1);

                std::memcpy(edit_buf,
                            _table_view.edit_buffer.c_str(),
                            copy_len);
                edit_buf[copy_len] = '\0';

                ImGuiInputTextFlags input_flags =
                    ImGuiInputTextFlags_EnterReturnsTrue |
                    ImGuiInputTextFlags_AutoSelectAll;

                // auto-focus on first frame
                ImGui::SetKeyboardFocusHere();

                if (ImGui::InputText("##edit",
                                     edit_buf,
                                     imgui_table_style::edit_buffer_capacity,
                                     input_flags))
                {
                    // enter pressed - commit
                    _table_view.edit_buffer = edit_buf;
                    table_view_commit_edit(_table_view);
                    interacted = true;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                {
                    // escape - cancel
                    table_view_cancel_edit(_table_view);
                    interacted = true;
                }
                else
                {
                    // sync buffer back
                    _table_view.edit_buffer = edit_buf;
                }

                ImGui::PopStyleColor(2);

                continue;
            }

            // ---------------------------------------------------------
            //  normal cell rendering
            // ---------------------------------------------------------
            const std::string text = table_view_cell_text(_table_view,
                                                   r,
                                                   c);

            // text color by region
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                internal::imgui_region_text_color(region));

            // alignment
            if (c < _table_view.columns.size())
            {
                float cell_w = ImGui::GetContentRegionAvail().x;
                float text_w = ImGui::CalcTextSize(text.c_str()).x;
                float offset = internal::imgui_alignment_offset(
                    _table_view.columns[c].align,
                    cell_w,
                    text_w);

                if (offset > 0.0f)
                {
                    ImGui::SetCursorPosX(
                        ImGui::GetCursorPosX() + offset);
                }
            }

            // clickable cell - selectable for cursor + selection
            ImGui::PushID(static_cast<int>(r * _table_view.num_columns + c));

            if (ImGui::Selectable(text.c_str(),
                                  is_cursor,
                                  ImGuiSelectableFlags_SpanAllColumns))
            {
                // click - move cursor
                _table_view.cursor = { r, c };
                table_view_select_at_cursor(_table_view);
                interacted = true;
            }

            // double-click to edit
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(0))
            {
                if (_table_view.set_cell)
                {
                    table_view_begin_edit(_table_view,
                                  r,
                                  c);
                    interacted = true;
                }
            }

            ImGui::PopID();
            ImGui::PopStyleColor();

            // cursor border
            if (is_cursor)
            {
                ImVec2 rmin = ImGui::GetItemRectMin();
                ImVec2 rmax = ImGui::GetItemRectMax();

                ImGui::GetWindowDrawList()->AddRect(
                    rmin,
                    rmax,
                    ImGui::GetColorU32(
                        palette::get<palette::cursor_border_tag>()),
                    0.0f,
                    0,
                    2.0f);
            }
        }
    }

    ImGui::EndTable();

    // -----------------------------------------------------------------
    //  scroll bar info display
    // -----------------------------------------------------------------
    if (_table_view.num_rows > _table_view.page_rows)
    {
        ImGui::Text("Rows %zu-%zu of %zu",
                     vis_row_start + 1,
                     vis_row_end,
                     _table_view.num_rows);
    }

    return interacted;
}


// ===========================================================================
//  4.  KEYBOARD INPUT HANDLER
// ===========================================================================

// imgui_table_view_handle_input
//   function: processes keyboard input for table navigation and
// editing.  Call after imgui_draw_table_view while the table's
// ImGui window is focused.  Returns true if input was consumed.
D_INLINE bool
imgui_table_view_handle_input(
    table_view& _table_view
)
{
    // skip input handling while editing (InputText owns the keyboard)
    if (_table_view.editing)
    {
        return false;
    }

    // require window focus
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        return false;
    }

    bool consumed = false;

    // arrow keys
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
    {
        table_view_move_up(_table_view);
        consumed = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
    {
        table_view_move_down(_table_view);
        consumed = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    {
        table_view_move_left(_table_view);
        consumed = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        table_view_move_right(_table_view);
        consumed = true;
    }

    // page up / down
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
    {
        table_view_page_up(_table_view);
        consumed = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
    {
        table_view_page_down(_table_view);
        consumed = true;
    }

    // home / end
    if (ImGui::IsKeyPressed(ImGuiKey_Home))
    {
        if (ImGui::GetIO().KeyCtrl)
        {
            table_view_move_top(_table_view);
        }
        else
        {
            table_view_move_home(_table_view);
        }

        consumed = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_End))
    {
        if (ImGui::GetIO().KeyCtrl)
        {
            table_view_move_bottom(_table_view);
        }
        else
        {
            table_view_move_end(_table_view);
        }

        consumed = true;
    }

    // enter to begin editing
    if (ImGui::IsKeyPressed(ImGuiKey_Enter))
    {
        if (_table_view.set_cell)
        {
            table_view_begin_edit_at_cursor(_table_view);
            consumed = true;
        }
    }

    // escape to clear selection
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        table_view_clear_selection(_table_view);
        consumed = true;
    }

    // ctrl+a to select all
    if ( (ImGui::GetIO().KeyCtrl) &&
         (ImGui::IsKeyPressed(ImGuiKey_A)) )
    {
        table_view_select_all(_table_view);
        consumed = true;
    }

    // update selection after cursor movement
    if (consumed)
    {
        table_view_select_at_cursor(_table_view);
    }

    return consumed;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_TABLE_