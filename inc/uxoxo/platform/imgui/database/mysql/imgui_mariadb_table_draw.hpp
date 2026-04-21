/*******************************************************************************
* uxoxo [imgui]                                    imgui_mariadb_table_draw.hpp
*
*   Dear ImGui draw handler for the mariadb_table_view component.  Layers
* MariaDB-specific chrome onto the shared imgui_database_table_draw
* layout:
*
*   - an extras toolbar row below the base toolbar carrying a Galera
*     sync-on-commit checkbox and the temporal query controls (AS OF,
*     BETWEEN, ALL VERSIONS, Clear).  Temporal controls are disabled
*     when the underlying table is not system-versioned
*   - an active-temporal accent tag inside the extras row showing the
*     current clause (e.g. "active: AS OF '2024-01-01'")
*   - a status bar that uses mariadb_table_view_status_text so the
*     footer shows temporal and Galera indicators alongside the base
*     connection and sync flags
*
*   The pair struct mariadb_table_view_pair bundles references to the
* table_view, the mariadb_table_state, and a per-view
* imgui_mariadb_table_ui holding ImGui input buffers — so the type-
* erased registry can dispatch with a single void* the same way
* database_table_view_pair does.
*
*   Structure:
*     1.  per-view imgui state
*     2.  mariadb_table_view_pair
*     3.  toolbar style
*     4.  extras row rendering  (Galera + temporal controls)
*     5.  status bar rendering
*     6.  imgui_draw_mariadb_table_view (main entry point)
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included
* before this header.
*
*
* path:      /inc/uxoxo/platform/imgui/table/database/
*                imgui_mariadb_table_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef UXOXO_IMGUI_COMPONENT_TABLE_MARIADB_DRAW_
#define UXOXO_IMGUI_COMPONENT_TABLE_MARIADB_DRAW_ 1

// std
#include <string>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/table/table_view.hpp"
#include "../../../templates/component/database/mariadb_table_view.hpp"
#include "../../../templates/render_context.hpp"
#include "../table/imgui_table_draw.hpp"
#include "./imgui_database_table_draw.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI

using uxoxo::component::render_context;
using uxoxo::component::mariadb_table_state;


// =============================================================================
//  1.  PER-VIEW IMGUI STATE
// =============================================================================
//   ImGui is immediate mode and mariadb_table_state carries no input
// buffers, so the MariaDB drawer introduces a companion struct for
// text-entry state.  Callers hold one instance per drawn view.

// imgui_mariadb_table_ui
//   struct: per-view ImGui input buffers for the MariaDB drawer.
struct imgui_mariadb_table_ui
{
    // AS OF timestamp entry
    char as_of_buffer[128]        = {};

    // BETWEEN range entries
    char between_from_buffer[128] = {};
    char between_to_buffer[128]   = {};
};


// =============================================================================
//  2.  MARIADB TABLE VIEW PAIR
// =============================================================================
//   Bundled references for registry dispatch — parallels
// database_table_view_pair but carries the MariaDB state and the
// MariaDB UI buffers.

// mariadb_table_view_pair
//   struct: bundles a table_view, mariadb_table_state, and
// imgui_mariadb_table_ui for registry dispatch.
struct mariadb_table_view_pair
{
    table_view&             view;
    mariadb_table_state&    state;
    imgui_mariadb_table_ui& ui;
};


// =============================================================================
//  3.  TOOLBAR STYLE
// =============================================================================
//   Reuses the base imgui_db_toolbar_style palette for continuity and
// adds MariaDB-specific accents for temporal indicators.

namespace imgui_mariadb_toolbar_style
{
    D_INLINE const ImVec4 temporal_accent        =
        ImVec4(0.50f, 0.32f, 0.70f, 1.0f);
    D_INLINE const ImVec4 temporal_active_text   =
        ImVec4(0.78f, 0.58f, 0.95f, 1.0f);
    D_INLINE const ImVec4 btn_temporal           =
        ImVec4(0.44f, 0.30f, 0.62f, 1.0f);
    D_INLINE const ImVec4 btn_temporal_hover     =
        ImVec4(0.52f, 0.36f, 0.74f, 1.0f);
    D_INLINE const ImVec4 btn_clear              =
        ImVec4(0.55f, 0.45f, 0.30f, 1.0f);
    D_INLINE const ImVec4 btn_clear_hover        =
        ImVec4(0.68f, 0.55f, 0.36f, 1.0f);
    D_INLINE const ImVec4 galera_accent          =
        ImVec4(0.30f, 0.58f, 0.62f, 1.0f);

    D_INLINE constexpr float extras_height = 32.0f;

}   // namespace imgui_mariadb_toolbar_style


// =============================================================================
//  4.  EXTRAS ROW RENDERING
// =============================================================================

NS_INTERNAL

    // imgui_draw_mariadb_temporal_button
    //   function: draws a temporal action button with the MariaDB
    // accent palette.  Returns true if clicked and not disabled.
    D_INLINE bool
    imgui_draw_mariadb_temporal_button(
        const char* _label,
        bool        _disabled
    )
    {
        if (_disabled)
        {
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                imgui_db_toolbar_style::btn_disabled);
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                imgui_db_toolbar_style::btn_disabled);
            ImGui::PushStyleColor(
                ImGuiCol_ButtonActive,
                imgui_db_toolbar_style::btn_disabled);
        }
        else
        {
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                imgui_mariadb_toolbar_style::btn_temporal);
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                imgui_mariadb_toolbar_style::btn_temporal_hover);
            ImGui::PushStyleColor(
                ImGuiCol_ButtonActive,
                imgui_mariadb_toolbar_style::btn_temporal);
        }

        const bool clicked =
            ( ImGui::Button(_label) &&
              !_disabled );

        ImGui::PopStyleColor(3);

        return clicked;
    }


    // imgui_draw_mariadb_extras
    //   function: draws the MariaDB extras row (Galera checkbox,
    // temporal controls, active-temporal indicator).  Temporal
    // controls are disabled when the table is not system-versioned.
    // Returns true if any control was interacted with.
    D_INLINE bool
    imgui_draw_mariadb_extras(
        mariadb_table_state&    _state,
        imgui_mariadb_table_ui& _ui
    )
    {
        bool interacted = false;

        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              imgui_db_toolbar_style::toolbar_bg);

        ImGui::BeginChild(
            "##mariadb_extras",
            ImVec2(0.0f,
                   imgui_mariadb_toolbar_style::extras_height),
            false,
            ImGuiWindowFlags_NoScrollbar);

        // Galera sync checkbox
        {
            ImGui::PushStyleColor(
                ImGuiCol_CheckMark,
                imgui_mariadb_toolbar_style::galera_accent);

            bool galera = _state.galera_sync_on_commit;

            if (ImGui::Checkbox("Galera sync",
                                &galera))
            {
                mariadb_table_view_set_galera_sync(_state,
                                                   galera);
                interacted = true;
            }

            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        const bool versioned = _state.system_versioned;

        // AS OF entry
        ImGui::TextUnformatted("AS OF");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##as_of",
                                 "'2024-01-01' / TRANSACTION n",
                                 _ui.as_of_buffer,
                                 sizeof(_ui.as_of_buffer));

        ImGui::SameLine();

        if (imgui_draw_mariadb_temporal_button(
                "Go##as_of",
                !versioned || (_ui.as_of_buffer[0] == '\0')))
        {
            mariadb_table_view_refresh_as_of(_state,
                                             _ui.as_of_buffer);
            interacted = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // BETWEEN entries
        ImGui::TextUnformatted("BETWEEN");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputTextWithHint("##between_from",
                                 "from",
                                 _ui.between_from_buffer,
                                 sizeof(_ui.between_from_buffer));

        ImGui::SameLine();

        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputTextWithHint("##between_to",
                                 "to",
                                 _ui.between_to_buffer,
                                 sizeof(_ui.between_to_buffer));

        ImGui::SameLine();

        const bool between_disabled =
            ( !versioned                                   ||
              (_ui.between_from_buffer[0] == '\0')         ||
              (_ui.between_to_buffer[0]   == '\0') );

        if (imgui_draw_mariadb_temporal_button(
                "Go##between",
                between_disabled))
        {
            mariadb_table_view_refresh_between(_state,
                                               _ui.between_from_buffer,
                                               _ui.between_to_buffer);
            interacted = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // ALL VERSIONS
        if (imgui_draw_mariadb_temporal_button("All Versions",
                                               !versioned))
        {
            mariadb_table_view_refresh_all_versions(_state);
            interacted = true;
        }

        ImGui::SameLine();

        // Clear (live snapshot) — only useful when temporal is active
        {
            const bool clear_disabled = !_state.temporal_active;

            if (clear_disabled)
            {
                ImGui::PushStyleColor(
                    ImGuiCol_Button,
                    imgui_db_toolbar_style::btn_disabled);
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    imgui_db_toolbar_style::btn_disabled);
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonActive,
                    imgui_db_toolbar_style::btn_disabled);
            }
            else
            {
                ImGui::PushStyleColor(
                    ImGuiCol_Button,
                    imgui_mariadb_toolbar_style::btn_clear);
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    imgui_mariadb_toolbar_style::btn_clear_hover);
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonActive,
                    imgui_mariadb_toolbar_style::btn_clear);
            }

            if ( ImGui::Button("Clear") &&
                 !clear_disabled )
            {
                mariadb_table_view_clear_temporal(_state);
                interacted = true;
            }

            ImGui::PopStyleColor(3);
        }

        // active-temporal indicator
        if (_state.temporal_active)
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                imgui_mariadb_toolbar_style::temporal_active_text);
            ImGui::Text("active: %s",
                        _state.temporal_text.c_str());
            ImGui::PopStyleColor();
        }
        else if (!versioned)
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                imgui_db_toolbar_style::status_text);
            ImGui::TextUnformatted("(not system-versioned)");
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        return interacted;
    }


// =============================================================================
//  5.  STATUS BAR RENDERING
// =============================================================================

    // imgui_draw_mariadb_status_bar
    //   function: draws the status bar using
    // mariadb_table_view_status_text so temporal and Galera
    // indicators appear alongside the base connection and sync
    // flags.
    D_INLINE void
    imgui_draw_mariadb_status_bar(
        const table_view&          _table_view,
        const mariadb_table_state& _state
    )
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              imgui_db_toolbar_style::toolbar_bg);

        ImGui::BeginChild("##mariadb_status",
                          ImVec2(0.0f,
                                 imgui_db_toolbar_style::status_height),
                          false,
                          ImGuiWindowFlags_NoScrollbar);

        ImGui::PushStyleColor(ImGuiCol_Text,
                              imgui_db_toolbar_style::status_text);

        const std::string status =
            mariadb_table_view_status_text(_state,
                                           _table_view);

        ImGui::TextUnformatted(status.c_str());

        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        return;
    }

NS_END  // internal


// =============================================================================
//  6.  IMGUI DRAW MARIADB TABLE VIEW
// =============================================================================

// imgui_draw_mariadb_table_view
//   function: renders a MariaDB-backed table with the base toolbar,
// a MariaDB extras row (Galera + temporal controls), the delegated
// grid, and an extended status bar.  Returns true if any user
// interaction occurred.
D_INLINE bool
imgui_draw_mariadb_table_view(
    mariadb_table_view_pair& _pair,
    render_context&          _ctx
)
{
    bool interacted = false;

    // base toolbar — mariadb_table_state IS-A database_table_state,
    // so the internal helper binds directly without adaptation
    interacted |=
        internal::imgui_draw_db_toolbar(_pair.view,
                                        _pair.state);

    // MariaDB extras row (Galera + temporal)
    interacted |=
        internal::imgui_draw_mariadb_extras(_pair.state,
                                            _pair.ui);

    // grid (remaining space minus status bar height)
    float grid_height =
        ImGui::GetContentRegionAvail().y -
        imgui_db_toolbar_style::status_height;

    if (grid_height < 0.0f)
    {
        grid_height = 0.0f;
    }

    ImGui::BeginChild("##mariadb_grid",
                      ImVec2(0.0f, grid_height),
                      false);

    interacted |= imgui_draw_table_view(_pair.view,
                                        _ctx);

    interacted |= imgui_table_view_handle_input(_pair.view);

    ImGui::EndChild();

    // extended status bar
    internal::imgui_draw_mariadb_status_bar(_pair.view,
                                            _pair.state);

    return interacted;
}


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_TABLE_MARIADB_DRAW_
