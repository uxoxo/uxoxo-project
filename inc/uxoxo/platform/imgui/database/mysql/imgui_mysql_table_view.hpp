/*******************************************************************************
* uxoxo [imgui]                                      imgui_mysql_table_view.hpp
*
*   Dear ImGui renderer for mysql_table_view.  Layers Oracle-MySQL-
* specific controls on top of the shared imgui_database_table_view
* drawer:
*
*   - an optimizer hint entry with Set and Clear buttons that
*     dispatches through mysql_table_view_set_optimizer_hint and
*     mysql_table_view_clear_optimizer_hint
*   - maintenance controls: ANALYZE, OPTIMIZE, and CHECK buttons.
*     CHECK captures the result string and opens a popup showing the
*     server's response
*   - a JSON column cell styler that tints the text and background of
*     native JSON binary columns, plugged into the shared grid drawer
*     via the cell_style_fn hook
*   - a status bar that uses mysql_table_view_status_text so the
*     footer shows optimizer hint and JSON column indicators
*     alongside the base connection and sync flags
*
*   ImGui is immediate mode and mysql_table_state carries no input
* buffers, so this header introduces a separate imgui_mysql_table_ui
* struct for text-input state and for the CHECK result popup.
* Users hold one instance per drawn view.
*
*   Structure:
*     1.   per-view imgui state buffers
*     2.   optimizer hint control
*     3.   maintenance controls  (ANALYZE / OPTIMIZE / CHECK + popup)
*     4.   JSON column cell styler
*     5.   extended toolbar
*     6.   extended status bar
*     7.   main compose entry point
*
*   REQUIRES: C++17 or later, Dear ImGui 1.80 or later.
*
*
* path:      /inc/uxoxo/imgui/component/table/database/
*                imgui_mysql_table_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef UXOXO_IMGUI_MYSQL_TABLE_VIEW_
#define UXOXO_IMGUI_MYSQL_TABLE_VIEW_ 1

// std
#include <algorithm>
#include <cstddef>
#include <string>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/table/database/mysql_table_view.hpp"
#include "./imgui_database_table_view.hpp"


NS_UXOXO
NS_IMGUI


// =============================================================================
//  1.  PER-VIEW IMGUI STATE BUFFERS
// =============================================================================
//   Text-entry buffer for the optimizer hint and popup state for the
// CHECK TABLE result dialog.  One instance per drawn
// mysql_table_view; owned by the caller and persisted across frames.

// imgui_mysql_table_ui
//   struct: per-view ImGui input buffers for the MySQL drawer.
struct imgui_mysql_table_ui
{
    // optimizer hint entry
    char optimizer_hint_buffer[256] = {};

    // CHECK TABLE popup
    std::string check_result_message;
    bool        open_check_popup     = false;

    // JSON column tint (RGBA, 32-bit packed)
    //   defaults to a subtle blue wash that reads well on both light
    // and dark ImGui themes.
    ImU32 json_cell_bg_color   = IM_COL32( 70, 130, 200,  40);
    ImU32 json_cell_text_color = IM_COL32(180, 210, 255, 255);
};


// =============================================================================
//  2.  OPTIMIZER HINT CONTROL
// =============================================================================

// imgui_mysql_table_view_draw_optimizer_hint_control
//   function: draws the optimizer hint entry with Set and Clear
// buttons.  The text input is seeded from _state.optimizer_hint on
// first draw and syncs back through mysql_table_view_set_optimizer_
// hint when the user clicks Set.  Returns true if the hint was
// changed.
D_INLINE bool
imgui_mysql_table_view_draw_optimizer_hint_control(
    mysql_table_state&    _state,
    imgui_mysql_table_ui& _ui
)
{
    bool acted = false;

    if (!ImGui::CollapsingHeader("Optimizer hint"))
    {
        return acted;
    }

    ImGui::Indent();

    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputTextWithHint("##optimizer_hint",
                             "e.g. INDEX(t idx_name) NO_CACHE",
                             _ui.optimizer_hint_buffer,
                             sizeof(_ui.optimizer_hint_buffer));

    ImGui::SameLine();

    if (ImGui::Button("Set"))
    {
        mysql_table_view_set_optimizer_hint(_state,
                                            _ui.optimizer_hint_buffer);
        acted = true;
    }

    ImGui::SameLine();

    if (!mysql_table_view_has_optimizer_hint(_state))
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Clear"))
    {
        mysql_table_view_clear_optimizer_hint(_state);

        // also clear the UI buffer
        _ui.optimizer_hint_buffer[0] = '\0';
        acted = true;
    }

    if (!mysql_table_view_has_optimizer_hint(_state))
    {
        ImGui::EndDisabled();
    }

    // active indicator
    if (mysql_table_view_has_optimizer_hint(_state))
    {
        ImGui::SameLine();
        ImGui::TextDisabled("active: /*+ %s */",
                            _state.optimizer_hint.c_str());
    }

    ImGui::Unindent();

    return acted;
}


// =============================================================================
//  3.  MAINTENANCE CONTROLS  (ANALYZE / OPTIMIZE / CHECK + popup)
// =============================================================================

// imgui_mysql_table_view_draw_check_popup
//   function: draws the modal popup that shows the most recent
// CHECK TABLE result.  Called every frame; the popup only appears
// after imgui_mysql_table_view_draw_maintenance_controls requests
// it via _ui.open_check_popup.
D_INLINE void
imgui_mysql_table_view_draw_check_popup(
    imgui_mysql_table_ui& _ui
)
{
    // latch the open request
    if (_ui.open_check_popup)
    {
        ImGui::OpenPopup("CHECK TABLE result");
        _ui.open_check_popup = false;
    }

    if (!ImGui::BeginPopupModal("CHECK TABLE result",
                                nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const char* text =
        ( _ui.check_result_message.empty()
            ? "(no result returned)"
            : _ui.check_result_message.c_str() );

    ImGui::TextWrapped("%s",
                       text);

    ImGui::Separator();

    if (ImGui::Button("Close",
                      ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();

    return;
}

// imgui_mysql_table_view_draw_maintenance_controls
//   function: draws the ANALYZE / OPTIMIZE / CHECK button row.
// Buttons are disabled when the table is not connected.  CHECK
// captures the result into _ui.check_result_message and requests the
// result popup.  Returns true if any maintenance op fired.
D_INLINE bool
imgui_mysql_table_view_draw_maintenance_controls(
    mysql_table_state&    _state,
    imgui_mysql_table_ui& _ui
)
{
    bool acted = false;

    const bool connected =
        mysql_table_view_is_connected(_state);

    if (!connected)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("ANALYZE"))
    {
        mysql_table_view_analyze(_state);
        acted = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("OPTIMIZE"))
    {
        mysql_table_view_optimize(_state);
        acted = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("CHECK"))
    {
        _ui.check_result_message = mysql_table_view_check(_state);
        _ui.open_check_popup     = true;
        acted                    = true;
    }

    if (!connected)
    {
        ImGui::EndDisabled();
    }

    // render the popup every frame (no-op until open_check_popup
    // latches it)
    imgui_mysql_table_view_draw_check_popup(_ui);

    return acted;
}


// =============================================================================
//  4.  JSON COLUMN CELL STYLER
// =============================================================================

// imgui_mysql_table_view_json_cell_styler
//   function: builds a cell_style_fn lambda that tints JSON binary
// columns.  Captures _state and _ui by reference — both must
// outlive the returned callable.  For JSON columns, pushes an
// ImGuiCol_Text colour and sets the cell background via
// TableSetBgColor (which requires no pop), so the returned push
// count is exactly 1 for JSON columns and 0 otherwise.
D_INLINE cell_style_fn
imgui_mysql_table_view_json_cell_styler(
    const mysql_table_state&    _state,
    const imgui_mysql_table_ui& _ui
)
{
    return [&_state, &_ui]
           (std::size_t _row,
            std::size_t _column) -> int
    {
        (void)_row;

        if (!mysql_table_view_is_json_column(_state,
                                             _column))
        {
            return 0;
        }

        // cell background (no pop needed)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                               _ui.json_cell_bg_color);

        // text colour (pops to 1)
        ImGui::PushStyleColor(ImGuiCol_Text,
                              _ui.json_cell_text_color);

        return 1;
    };
}


// =============================================================================
//  5.  EXTENDED TOOLBAR
// =============================================================================

// imgui_mysql_table_view_draw_extended_toolbar
//   function: draws the base Refresh / Commit toolbar followed by
// MySQL-specific toolbar items (maintenance buttons inline,
// optimizer hint as a collapsing section).  Returns true if any
// action was triggered.
D_INLINE bool
imgui_mysql_table_view_draw_extended_toolbar(
    table_view&           _table_view,
    mysql_table_state&    _state,
    imgui_mysql_table_ui& _ui
)
{
    // base toolbar (Refresh / Commit)
    bool acted =
        imgui_database_table_view_draw_toolbar(_table_view,
                                               _state);

    // maintenance buttons inline with base
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    if (imgui_mysql_table_view_draw_maintenance_controls(_state,
                                                         _ui))
    {
        acted = true;
    }

    // optimizer hint as a collapsing section beneath
    if (imgui_mysql_table_view_draw_optimizer_hint_control(_state,
                                                           _ui))
    {
        acted = true;
    }

    return acted;
}


// =============================================================================
//  6.  EXTENDED STATUS BAR
// =============================================================================

// imgui_mysql_table_view_draw_status_bar
//   function: draws a status bar using mysql_table_view_status_text
// so the footer shows optimizer hint and JSON column indicators
// alongside the base connection and sync flags.
D_INLINE void
imgui_mysql_table_view_draw_status_bar(
    const table_view&        _table_view,
    const mysql_table_state& _state
)
{
    const std::string text =
        mysql_table_view_status_text(_state,
                                     _table_view);

    ImGui::TextUnformatted(text.c_str());

    return;
}


// =============================================================================
//  7.  MAIN COMPOSE ENTRY POINT
// =============================================================================

// imgui_mysql_table_view_draw
//   function: composes the extended toolbar, the shared grid (with
// a JSON column styler installed into the passed config), and the
// MySQL status bar inside a PushID block keyed on _id.  If the
// caller's config already has a cell_styler, it is preserved and
// not replaced.  Returns true if any toolbar action fired.
D_INLINE bool
imgui_mysql_table_view_draw(
    const char*                        _id,
    table_view&                        _table_view,
    mysql_table_state&                 _state,
    imgui_mysql_table_ui&              _ui,
    imgui_database_table_view_config   _config
        = imgui_database_table_view_config{}
)
{
    bool acted = false;

    ImGui::PushID(_id);

    // install the JSON cell styler only if the caller did not
    // supply one of their own — callers wanting to combine stylers
    // can chain them externally.
    if (!_config.cell_styler)
    {
        _config.cell_styler =
            imgui_mysql_table_view_json_cell_styler(_state,
                                                    _ui);
    }

    // extended toolbar
    if (_config.show_toolbar)
    {
        if (imgui_mysql_table_view_draw_extended_toolbar(_table_view,
                                                         _state,
                                                         _ui))
        {
            acted = true;
        }

        ImGui::Separator();
    }

    // shared grid drawer — mysql_table_state IS-A database_table_state,
    // so it passes through without adaptation
    imgui_database_table_view_draw_grid(_id,
                                        _table_view,
                                        _state,
                                        _config);

    // MySQL-extended status bar
    if (_config.show_status_bar)
    {
        ImGui::Separator();
        imgui_mysql_table_view_draw_status_bar(_table_view,
                                               _state);
    }

    ImGui::PopID();

    return acted;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_MYSQL_TABLE_VIEW_
