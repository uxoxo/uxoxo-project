/*******************************************************************************
* uxoxo [imgui]                                    imgui_mariadb_table_view.hpp
*
*   Dear ImGui renderer for mariadb_table_view.  Layers MariaDB-specific
* controls on top of the shared imgui_database_table_view drawer:
*
*   - temporal query controls: an AS OF timestamp entry with a button,
*     a BETWEEN range (from / to) entry with a button, an ALL VERSIONS
*     button, and a Clear button that restores the live snapshot.  All
*     four are disabled when the underlying table is not system-
*     versioned
*   - a Galera sync-on-commit checkbox that dispatches through
*     mariadb_table_view_set_galera_sync
*   - a status bar that uses mariadb_table_view_status_text so the
*     footer shows temporal / Galera indicators alongside the base
*     connection and sync flags
*
*   ImGui is immediate mode and mariadb_table_state carries no input
* buffers, so this header introduces a separate imgui_mariadb_table_ui
* struct for text-input state.  Users hold one instance per drawn
* view.
*
*   Structure:
*     1.   per-view imgui state buffers
*     2.   temporal controls
*     3.   Galera sync control
*     4.   extended toolbar
*     5.   extended status bar
*     6.   main compose entry point
*
*   REQUIRES: C++17 or later, Dear ImGui 1.80 or later.
*
*
* path:      /inc/uxoxo/imgui/component/table/database/
*                imgui_mariadb_table_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef UXOXO_IMGUI_MARIADB_TABLE_VIEW_
#define UXOXO_IMGUI_MARIADB_TABLE_VIEW_ 1

// std
#include <cstddef>
#include <string>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/table/database/mariadb_table_view.hpp"
#include "./imgui_database_table_view.hpp"


NS_UXOXO
NS_IMGUI


// =============================================================================
//  1.  PER-VIEW IMGUI STATE BUFFERS
// =============================================================================
//   Text-entry buffers for temporal controls.  One instance per drawn
// mariadb_table_view; owned by the caller and persisted across
// frames.

// imgui_mariadb_table_ui
//   struct: per-view ImGui input buffers for the MariaDB drawer.
struct imgui_mariadb_table_ui
{
    // AS OF timestamp entry
    char as_of_buffer[128]        = {};

    // BETWEEN range entries
    char between_from_buffer[128] = {};
    char between_to_buffer[128]   = {};

    // collapsed/expanded state of the temporal section
    bool temporal_expanded        = false;
};


// =============================================================================
//  2.  TEMPORAL CONTROLS
// =============================================================================

// imgui_mariadb_table_view_draw_temporal_controls
//   function: draws the AS OF / BETWEEN / ALL VERSIONS / Clear
// controls as a collapsible section.  All controls are disabled if
// the table is not system-versioned.  Returns true if a temporal
// refresh was triggered.
D_INLINE bool
imgui_mariadb_table_view_draw_temporal_controls(
    mariadb_table_state&    _state,
    imgui_mariadb_table_ui& _ui
)
{
    bool acted = false;

    const bool versioned = _state.system_versioned;

    // header with availability indicator
    const char* header_label =
        ( versioned
            ? "Temporal query"
            : "Temporal query (unavailable: not system-versioned)" );

    if (!ImGui::CollapsingHeader(header_label))
    {
        return acted;
    }

    if (!versioned)
    {
        ImGui::BeginDisabled();
    }

    ImGui::Indent();

    // AS OF row
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputTextWithHint("##as_of",
                             "'2024-01-01 00:00:00' or TRANSACTION n",
                             _ui.as_of_buffer,
                             sizeof(_ui.as_of_buffer));

    ImGui::SameLine();

    if (ImGui::Button("AS OF"))
    {
        if (_ui.as_of_buffer[0] != '\0')
        {
            mariadb_table_view_refresh_as_of(_state,
                                             _ui.as_of_buffer);
            acted = true;
        }
    }

    // BETWEEN row
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##between_from",
                             "from",
                             _ui.between_from_buffer,
                             sizeof(_ui.between_from_buffer));

    ImGui::SameLine();

    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##between_to",
                             "to",
                             _ui.between_to_buffer,
                             sizeof(_ui.between_to_buffer));

    ImGui::SameLine();

    if (ImGui::Button("BETWEEN"))
    {
        if ( (_ui.between_from_buffer[0] != '\0') &&
             (_ui.between_to_buffer[0]   != '\0') )
        {
            mariadb_table_view_refresh_between(_state,
                                               _ui.between_from_buffer,
                                               _ui.between_to_buffer);
            acted = true;
        }
    }

    // ALL VERSIONS / Clear row
    if (ImGui::Button("ALL VERSIONS"))
    {
        mariadb_table_view_refresh_all_versions(_state);
        acted = true;
    }

    ImGui::SameLine();

    // Clear is useful only when a temporal query is active
    if (!_state.temporal_active)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Clear (live snapshot)"))
    {
        mariadb_table_view_clear_temporal(_state);
        acted = true;
    }

    if (!_state.temporal_active)
    {
        ImGui::EndDisabled();
    }

    // current temporal indicator
    if (_state.temporal_active)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]",
                            _state.temporal_text.c_str());
    }

    ImGui::Unindent();

    if (!versioned)
    {
        ImGui::EndDisabled();
    }

    return acted;
}


// =============================================================================
//  3.  GALERA SYNC CONTROL
// =============================================================================

// imgui_mariadb_table_view_draw_galera_control
//   function: draws the Galera sync-on-commit checkbox.  Returns
// true if the value was toggled.
D_INLINE bool
imgui_mariadb_table_view_draw_galera_control(
    mariadb_table_state& _state
)
{
    bool enabled = _state.galera_sync_on_commit;

    if (ImGui::Checkbox("Galera sync on commit",
                        &enabled))
    {
        mariadb_table_view_set_galera_sync(_state,
                                           enabled);

        return true;
    }

    return false;
}


// =============================================================================
//  4.  EXTENDED TOOLBAR
// =============================================================================

// imgui_mariadb_table_view_draw_extended_toolbar
//   function: draws the base Refresh / Commit toolbar followed by
// MariaDB-specific toolbar items (Galera checkbox inline, temporal
// controls as a collapsing section).  Returns true if any action
// was triggered.
D_INLINE bool
imgui_mariadb_table_view_draw_extended_toolbar(
    table_view&             _table_view,
    mariadb_table_state&    _state,
    imgui_mariadb_table_ui& _ui
)
{
    // base toolbar (Refresh / Commit)
    bool acted =
        imgui_database_table_view_draw_toolbar(_table_view,
                                               _state);

    // Galera checkbox inline with base buttons
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    if (imgui_mariadb_table_view_draw_galera_control(_state))
    {
        acted = true;
    }

    // temporal controls as a collapsing section beneath
    if (imgui_mariadb_table_view_draw_temporal_controls(_state,
                                                        _ui))
    {
        acted = true;
    }

    return acted;
}


// =============================================================================
//  5.  EXTENDED STATUS BAR
// =============================================================================

// imgui_mariadb_table_view_draw_status_bar
//   function: draws a status bar using mariadb_table_view_status_text
// so the footer shows temporal and Galera indicators alongside the
// base connection and sync flags.
D_INLINE void
imgui_mariadb_table_view_draw_status_bar(
    const table_view&          _table_view,
    const mariadb_table_state& _state
)
{
    const std::string text =
        mariadb_table_view_status_text(_state,
                                       _table_view);

    ImGui::TextUnformatted(text.c_str());

    return;
}


// =============================================================================
//  6.  MAIN COMPOSE ENTRY POINT
// =============================================================================

// imgui_mariadb_table_view_draw
//   function: composes the extended toolbar, the shared grid, and
// the MariaDB status bar inside a PushID block keyed on _id.
// Returns true if any toolbar action fired.
D_INLINE bool
imgui_mariadb_table_view_draw(
    const char*                              _id,
    table_view&                              _table_view,
    mariadb_table_state&                     _state,
    imgui_mariadb_table_ui&                  _ui,
    const imgui_database_table_view_config&  _config
        = imgui_database_table_view_config{}
)
{
    bool acted = false;

    ImGui::PushID(_id);

    // extended toolbar (base + Galera + temporal)
    if (_config.show_toolbar)
    {
        if (imgui_mariadb_table_view_draw_extended_toolbar(_table_view,
                                                           _state,
                                                           _ui))
        {
            acted = true;
        }

        ImGui::Separator();
    }

    // shared grid drawer — mariadb_table_state IS-A database_table_state,
    // so it passes through without adaptation
    imgui_database_table_view_draw_grid(_id,
                                        _table_view,
                                        _state,
                                        _config);

    // MariaDB-extended status bar
    if (_config.show_status_bar)
    {
        ImGui::Separator();
        imgui_mariadb_table_view_draw_status_bar(_table_view,
                                                 _state);
    }

    ImGui::PopID();

    return acted;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_MARIADB_TABLE_VIEW_
