/******************************************************************************
* uxoxo [imgui]                                       imgui_database_table.hpp
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
*   Migration note (2026.05.08):
*     - `database_table_view_pair` is now a using-alias of the generic
*       `view_state_pair<table_view, database_table_state>` template
*       from imgui_view_pair.hpp.  No code changes are needed at call
*       sites since the alias preserves the same struct shape.
*     - The toolbar and status-bar BeginChild + ChildBg push/pop pairs
*       are replaced by `toolbar_scope` and `status_bar_scope` RAII
*       guards from imgui_chrome.hpp.
*     - The 3-color button push/pop blocks for Refresh and Commit are
*       replaced by single `accent_button(...)` calls from
*       imgui_button_helpers.hpp.  The disabled-state palette is the
*       canonical `palette::btn_disabled_tag`, applied uniformly by
*       accent_button when its `_disabled` argument is true.
*     - The local `imgui_db_toolbar_style` namespace is reduced to
*       database-specific accents (refresh blue, commit green,
*       read-only amber).  Background, indicator, status-text, and
*       sizing values are now drawn from `palette::` tags.
*
*   Structure:
*     1.  database_table_view_pair (alias for view_state_pair)
*     2.  database accent palette
*     3.  toolbar rendering
*     4.  status bar rendering
*     5.  imgui_draw_database_table_view (main entry point)
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included before
* this header.
*
*
* path:      /inc/uxoxo/platform/imgui/database/imgui_database_table.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.10
******************************************************************************/

#ifndef UXOXO_COMPONENT_IMGUI_DATABASE_TABLE_
#define UXOXO_COMPONENT_IMGUI_DATABASE_TABLE_ 1

// std
#include <string>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/table/table_view.hpp"
#include "../../../templates/component/database/database_table_view.hpp"
#include "../../../templates/render_context.hpp"
#include "../core/imgui_button_helpers.hpp"
#include "../core/imgui_chrome.hpp"
#include "../core/imgui_palette.hpp"
#include "../core/imgui_scope.hpp"
#include "../core/imgui_view_pair.hpp"
#include "../table/imgui_table_draw.hpp"


NS_UXOXO
NS_IMGUI

using uxoxo::component::render_context;
using uxoxo::component::database_table_state;

// =============================================================================
//  1.  DATABASE TABLE VIEW PAIR
// =============================================================================
//   Bundles a table_view and a database_table_state for registry
// dispatch.  The component registry passes a single `void*` per
// entry, so the pair adapter packages the two references the
// renderer needs.

// database_table_view_pair
//   alias: view_state_pair instantiated for table_view +
// database_table_state.  Brace-initialization at call sites is
// preserved because the alias resolves to a plain aggregate.
using database_table_view_pair =
    view_state_pair<table_view, database_table_state>;


// =============================================================================
//  2.  DATABASE ACCENT PALETTE
// =============================================================================
//   Database-specific button accents (Refresh / Commit) and the
// read-only-marker text color.  Kept local because no other
// component currently consumes a "refresh blue" or "commit green";
// promote to the shared palette only if a second consumer appears.

namespace imgui_db_accents
{
    inline const ImVec4 btn_refresh       = ImVec4(0.22f, 0.50f, 0.72f, 1.0f);
    inline const ImVec4 btn_refresh_hover = ImVec4(0.28f, 0.58f, 0.82f, 1.0f);
    inline const ImVec4 btn_commit        = ImVec4(0.30f, 0.62f, 0.35f, 1.0f);
    inline const ImVec4 btn_commit_hover  = ImVec4(0.36f, 0.72f, 0.40f, 1.0f);
    inline const ImVec4 read_only_text    = ImVec4(0.60f, 0.50f, 0.40f, 1.0f);

}   // namespace imgui_db_accents


// =============================================================================
//  3.  TOOLBAR RENDERING
// =============================================================================

NS_INTERNAL

    // imgui_draw_db_toolbar
    //   function: draws the database toolbar with refresh and commit
    // buttons.  Returns true if any button was pressed.  Receives a
    // database_table_state so MySQL and MariaDB callers can pass
    // their derived state types directly via slicing.
    D_INLINE bool
    imgui_draw_db_toolbar(
        table_view&           _table_view,
        database_table_state& _state
    )
    {
        bool interacted = false;

        (void)_table_view;

        toolbar_scope toolbar("##db_toolbar");

        if (!toolbar.is_visible())
        {
            return false;
        }

        // -- connection indicator --------------------------------------
        //   Bracketed text retained from the pre-migration visual
        // (the chrome status_indicator() variant draws a dot, which
        // is a different visual identity not adopted here).  Color is
        // resolved through palette tags so theme switches propagate.
        {
            ImVec4 ind_color =
                ( _state.connected
                    ? palette::get<palette::indicator_ok_tag>()
                    : palette::get<palette::indicator_disconn_tag>() );

            scoped_color text_color {
                { ImGuiCol_Text, ind_color }
            };

            ImGui::TextUnformatted(
                _state.connected ? "[connected]" : "[disconnected]");
        }

        ImGui::SameLine();

        // -- refresh button --------------------------------------------
        //   Single accent_button replaces the manual 3-color push /
        // pop block; disabled state automatically overrides to the
        // palette btn_disabled tag and discards click events.
        if (accent_button(
                "Refresh",
                imgui_db_accents::btn_refresh,
                imgui_db_accents::btn_refresh_hover,
                imgui_db_accents::btn_refresh,
                !_state.connected))
        {
            database_table_view_refresh(_state);
            interacted = true;
        }

        ImGui::SameLine();

        // -- commit button ---------------------------------------------
        {
            const bool commit_disabled =
                ( (!_state.connected)     ||
                  (!_state.mutable_table) ||
                  (!_state.dirty) );

            if (accent_button(
                    "Commit",
                    imgui_db_accents::btn_commit,
                    imgui_db_accents::btn_commit_hover,
                    imgui_db_accents::btn_commit,
                    commit_disabled))
            {
                database_table_view_commit(_state);
                interacted = true;
            }
        }

        ImGui::SameLine();

        // -- dirty indicator -------------------------------------------
        if (_state.dirty)
        {
            scoped_color text_color {
                { ImGuiCol_Text,
                      palette::get<palette::indicator_warn_tag>() }
            };

            ImGui::TextUnformatted("*modified*");
            ImGui::SameLine();
        }

        // -- stale indicator -------------------------------------------
        if (_state.stale)
        {
            scoped_color text_color {
                { ImGuiCol_Text,
                      palette::get<palette::indicator_stale_tag>() }
            };

            ImGui::TextUnformatted("(stale)");
            ImGui::SameLine();
        }

        // -- read-only indicator ---------------------------------------
        if (!_state.mutable_table)
        {
            scoped_color text_color {
                { ImGuiCol_Text, imgui_db_accents::read_only_text }
            };

            ImGui::TextUnformatted("[read-only]");
        }

        return interacted;
    }


    // imgui_draw_db_status_bar
    //   function: draws the status bar at the bottom of the database
    // table view.  Uses status_bar_scope from imgui_chrome.hpp for
    // the fixed-height child window with palette-driven background.
    D_INLINE void
    imgui_draw_db_status_bar(
        const table_view&           _table_view,
        const database_table_state& _state
    )
    {
        status_bar_scope sb("##db_status");

        if (!sb.is_visible())
        {
            return;
        }

        scoped_color text_color {
            { ImGuiCol_Text,
                  palette::get<palette::text_muted_tag>() }
        };

        const std::string status =
            database_table_view_status_text(_state, _table_view);

        ImGui::TextUnformatted(status.c_str());

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
    bool  interacted;
    float grid_height;

    interacted = false;

    // toolbar
    interacted |= internal::imgui_draw_db_toolbar(_pair.view,
                                                  _pair.state);

    // grid (uses remaining space minus status bar height)
    grid_height = ( ImGui::GetContentRegionAvail().y -
                    palette::get<palette::status_bar_height_tag>() );

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
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_DATABASE_TABLE_