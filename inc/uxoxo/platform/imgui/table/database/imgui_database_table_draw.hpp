/*******************************************************************************
* uxoxo [imgui]                                   imgui_database_table_draw.hpp
*
*   Dear ImGui draw handler for the database_table_view component.  Draws
* the database-specific chrome — toolbar (refresh, commit), status bar,
* connection/dirty/stale indicators — and delegates the grid portion to
* imgui_draw_table_view.
*
*   This module works with the table_view + database_table_state companion
* pair.  The draw handler receives a database_table_view_pair (a lightweight
* struct bundling references to both) so that the type-erased registry can
* dispatch with a single void*.
*
*   Structure:
*     1.  database_table_view_pair (bundled references for registry dispatch)
*     2.  toolbar rendering
*     3.  status bar rendering
*     4.  imgui_draw_database_table_view (main entry point)
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included before
* this header.
*
*
* path:      /inc/uxoxo/platform/imgui/table/database/
                 imgui_database_table_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_IMGUI_COMPONENT_TABLE_DATABASE_DRAW_
#define UXOXO_IMGUI_COMPONENT_TABLE_DATABASE_DRAW_ 1

// std
#include <string>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../../uxoxo.hpp"
#include "../../../../templates/component/table/table_view.hpp"
#include "../../../../templates/component/table/database/database_table_view.hpp"
#include "../../../../templates/render_context.hpp"
#include "../imgui_table_draw.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI

using uxoxo::component::render_context;
using uxoxo::component::database_table_state;

// =============================================================================
//  1.  DATABASE TABLE VIEW PAIR
// =============================================================================
//   The component registry dispatches through a single void*.  Since a
// database view is the combination of a table_view and a
// database_table_state, we bundle references to both into a lightweight
// struct that can be stored and passed through the registry.

// database_table_view_pair
//   struct: bundles a table_view and database_table_state for
// registry dispatch.
struct database_table_view_pair
{
    table_view&           view;
    database_table_state& state;
};


// =============================================================================
//  2.  TOOLBAR STYLE
// =============================================================================

namespace imgui_db_toolbar_style
{
    D_INLINE const ImVec4 toolbar_bg        = ImVec4(0.14f, 0.15f, 0.18f, 1.0f);
    D_INLINE const ImVec4 btn_refresh       = ImVec4(0.22f, 0.50f, 0.72f, 1.0f);
    D_INLINE const ImVec4 btn_refresh_hover = ImVec4(0.28f, 0.58f, 0.82f, 1.0f);
    D_INLINE const ImVec4 btn_commit        = ImVec4(0.30f, 0.62f, 0.35f, 1.0f);
    D_INLINE const ImVec4 btn_commit_hover  = ImVec4(0.36f, 0.72f, 0.40f, 1.0f);
    D_INLINE const ImVec4 btn_disabled      = ImVec4(0.30f, 0.30f, 0.32f, 0.50f);
    D_INLINE const ImVec4 indicator_dirty   = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
    D_INLINE const ImVec4 indicator_stale   = ImVec4(0.70f, 0.45f, 0.20f, 1.0f);
    D_INLINE const ImVec4 indicator_ok      = ImVec4(0.35f, 0.70f, 0.40f, 1.0f);
    D_INLINE const ImVec4 indicator_disconn = ImVec4(0.75f, 0.25f, 0.25f, 1.0f);
    D_INLINE const ImVec4 status_text       = ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
    D_INLINE const ImVec4 read_only_text    = ImVec4(0.60f, 0.50f, 0.40f, 1.0f);

    D_INLINE constexpr float toolbar_height = 32.0f;
    D_INLINE constexpr float status_height  = 22.0f;

}   // namespace imgui_db_toolbar_style


// =============================================================================
//  3.  TOOLBAR RENDERING
// =============================================================================

NS_INTERNAL

    // imgui_draw_db_toolbar
    //   function: draws the database toolbar with refresh and commit
    // buttons.  Returns true if any button was pressed.
    D_INLINE bool
    imgui_draw_db_toolbar(
        table_view&           _table_view,
        database_table_state& _state
    )
    {
        bool interacted = false;

        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              imgui_db_toolbar_style::toolbar_bg);

        ImGui::BeginChild("##db_toolbar",
                          ImVec2(0.0f,
                                 imgui_db_toolbar_style::toolbar_height),
                          false,
                          ImGuiWindowFlags_NoScrollbar);

        // connection indicator
        if (_state.connected)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_db_toolbar_style::indicator_ok);
            ImGui::TextUnformatted("[connected]");
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_db_toolbar_style::indicator_disconn);
            ImGui::TextUnformatted("[disconnected]");
        }

        ImGui::PopStyleColor();

        ImGui::SameLine();

        // refresh button
        {
            bool disabled = !_state.connected;

            if (disabled)
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
                    imgui_db_toolbar_style::btn_refresh);
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    imgui_db_toolbar_style::btn_refresh_hover);
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonActive,
                    imgui_db_toolbar_style::btn_refresh);
            }

            if (ImGui::Button("Refresh") && !disabled)
            {
                database_table_view_refresh(_state);
                interacted = true;
            }

            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();

        // commit button
        {
            bool disabled =
                ( (!_state.connected)    ||
                  (!_state.mutable_table) ||
                  (!_state.dirty) );

            if (disabled)
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
                    imgui_db_toolbar_style::btn_commit);
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    imgui_db_toolbar_style::btn_commit_hover);
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonActive,
                    imgui_db_toolbar_style::btn_commit);
            }

            if (ImGui::Button("Commit") && !disabled)
            {
                database_table_view_commit(_state);
                interacted = true;
            }

            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();

        // dirty indicator
        if (_state.dirty)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_db_toolbar_style::indicator_dirty);
            ImGui::TextUnformatted("*modified*");
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        // stale indicator
        if (_state.stale)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_db_toolbar_style::indicator_stale);
            ImGui::TextUnformatted("(stale)");
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        // read-only indicator
        if (!_state.mutable_table)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_db_toolbar_style::read_only_text);
            ImGui::TextUnformatted("[read-only]");
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        return interacted;
    }


    // imgui_draw_db_status_bar
    //   function: draws the status bar at the bottom of the database
    // table view.
    D_INLINE void
    imgui_draw_db_status_bar(
        const table_view&           _table_view,
        const database_table_state& _state
    )
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              imgui_db_toolbar_style::toolbar_bg);

        ImGui::BeginChild("##db_status",
                          ImVec2(0.0f,
                                 imgui_db_toolbar_style::status_height),
                          false,
                          ImGuiWindowFlags_NoScrollbar);

        ImGui::PushStyleColor(ImGuiCol_Text,
                              imgui_db_toolbar_style::status_text);

        std::string status = database_table_view_status_text(_state,
                                                              _table_view);
        ImGui::TextUnformatted(status.c_str());

        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        return;
    }

NS_END  // internal


// =============================================================================
//  4.  IMGUI DRAW DATABASE TABLE VIEW
// =============================================================================

// imgui_draw_database_table_view
//   function: renders a database-backed table with toolbar, grid,
// and status bar.  The grid is delegated to imgui_draw_table_view.
// Returns true if any user interaction occurred.
D_INLINE bool
imgui_draw_database_table_view(
    database_table_view_pair& _pair,
    render_context&           _ctx
)
{
    bool interacted = false;

    // toolbar
    interacted |= internal::imgui_draw_db_toolbar(_pair.view,
                                                   _pair.state);

    // grid (uses remaining space minus status bar height)
    float grid_height =
        ImGui::GetContentRegionAvail().y -
        imgui_db_toolbar_style::status_height;

    if (grid_height < 0.0f)
    {
        grid_height = 0.0f;
    }

    ImGui::BeginChild("##db_grid",
                      ImVec2(0.0f, grid_height),
                      false);

    interacted |= imgui_draw_table_view(_pair.view,
                                         _ctx);

    // keyboard input for the grid
    interacted |= imgui_table_view_handle_input(_pair.view);

    ImGui::EndChild();

    // status bar
    internal::imgui_draw_db_status_bar(_pair.view,
                                       _pair.state);

    return interacted;
}


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_TABLE_DATABASE_DRAW_