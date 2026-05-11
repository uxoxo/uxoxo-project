/******************************************************************************
* uxoxo [imgui]                                        imgui_mariadb_table.hpp
*
*   Dear ImGui draw handler for the mariadb_table_view component.  Layers
* MariaDB-specific chrome onto the shared imgui_database_table_draw
* layout:
*   - an extras toolbar row below the base toolbar carrying a Galera
*     sync-on-commit checkbox and the temporal query controls (AS OF,
*     BETWEEN, ALL VERSIONS, Clear).  Temporal controls are disabled
*     when the underlying table is not system-versioned
*   - an active-temporal accent tag inside the extras row showing the
*     current clause (e.g. "active: AS OF '2024-01-01'")
*   - a status bar that uses mariadb_table_view_status_text so the
*     footer shows temporal and Galera indicators alongside the base
*     connection and sync flags
*   The pair struct mariadb_table_view_pair bundles references to the
* table_view, the mariadb_table_state, and a per-view
* imgui_mariadb_table_ui holding ImGui input buffers — so the type-
* erased registry can dispatch with a single void* the same way
* database_table_view_pair does.
*   Migration note (2026.05.08):
*     - `mariadb_table_view_pair` is now a using-alias of the generic
*       `view_state_pair<table_view, mariadb_table_state,
*        imgui_mariadb_table_ui>` template from imgui_view_pair.hpp.
*       Brace-initialization at call sites is preserved.
*     - The local `imgui_draw_mariadb_temporal_button` helper was a
*       3-color push/pop wrapper used three times.  It has been
*       removed in favour of `accent_button(label, normal, hover,
*       normal, disabled)` from imgui_button_helpers.hpp.  The
*       Clear-temporal button's identical hand-rolled push/pop block
*       is also collapsed into an accent_button call.
*     - The extras-row BeginChild + ChildBg push/pop pair becomes a
*       `toolbar_scope` from imgui_chrome.hpp.  The status-bar pair
*       becomes a `status_bar_scope`.
*     - References into the now-removed shared `imgui_db_toolbar_style`
*       (toolbar_bg, btn_disabled, status_text, status_height) have
*       been redirected to the corresponding `palette::` tags.
*       MariaDB-specific accents (temporal purple, galera teal) stay
*       in `imgui_mariadb_toolbar_style` because they have no
*       consumer outside this file.
*   Structure:
*     1.  per-view imgui state
*     2.  mariadb_table_view_pair (alias for view_state_pair)
*     3.  toolbar style (mariadb-specific accents only)
*     4.  extras row rendering  (Galera + temporal controls)
*     5.  status bar rendering
*     6.  imgui_draw_mariadb_table_view (main entry point)
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included
* before this header.
*
*
* path:      /inc/uxoxo/platform/imgui/database/mariadb/imgui_mariadb_table.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.20
******************************************************************************/

#ifndef UXOXO_COMPONENT_IMGUI_DATABASE_MARIADB_TABLE_
#define UXOXO_COMPONENT_IMGUI_DATABASE_MARIADB_TABLE_ 1

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
#include "../core/imgui_button_helpers.hpp"
#include "../core/imgui_chrome.hpp"
#include "../core/imgui_palette.hpp"
#include "../core/imgui_scope.hpp"
#include "../core/imgui_view_pair.hpp"
#include "../table/imgui_table_draw.hpp"
#include "./imgui_database_table_draw.hpp"


NS_UXOXO
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
//   alias: view_state_pair instantiated for table_view +
// mariadb_table_state + imgui_mariadb_table_ui.  Brace-initialization
// at call sites is preserved because the alias resolves to a plain
// aggregate.
using mariadb_table_view_pair =
    view_state_pair<table_view, mariadb_table_state, imgui_mariadb_table_ui>;


// =============================================================================
//  3.  TOOLBAR STYLE
// =============================================================================
//   MariaDB-specific accent palette.  Temporal queries use a purple
// hue (distinguishing them from the database refresh blue and the
// commit green); Galera uses a teal-cyan; clear-temporal uses an
// amber tint.  Shared chrome colors come from `palette::` and are
// NOT duplicated here.

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

}   // namespace imgui_mariadb_toolbar_style


// =============================================================================
//  4.  EXTRAS ROW RENDERING
// =============================================================================

NS_INTERNAL

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
        bool interacted;
        bool versioned;

        interacted = false;

        toolbar_scope extras("##mariadb_extras");

        if (!extras.is_visible())
        {
            return false;
        }

        // -- Galera sync checkbox --------------------------------------
        {
            scoped_color check_color {
                { ImGuiCol_CheckMark,
                      imgui_mariadb_toolbar_style::galera_accent }
            };

            bool galera = _state.galera_sync_on_commit;

            if (ImGui::Checkbox("Galera sync",
                                &galera))
            {
                mariadb_table_view_set_galera_sync(_state,
                                                   galera);
                interacted = true;
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        versioned = _state.system_versioned;

        // -- AS OF entry -----------------------------------------------
        ImGui::TextUnformatted("AS OF");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##as_of",
                                 "'2024-01-01' / TRANSACTION n",
                                 _ui.as_of_buffer,
                                 sizeof(_ui.as_of_buffer));

        ImGui::SameLine();

        if (accent_button(
                "Go##as_of",
                imgui_mariadb_toolbar_style::btn_temporal,
                imgui_mariadb_toolbar_style::btn_temporal_hover,
                imgui_mariadb_toolbar_style::btn_temporal,
                ( !versioned ||
                  (_ui.as_of_buffer[0] == '\0') )))
        {
            mariadb_table_view_refresh_as_of(_state,
                                             _ui.as_of_buffer);
            interacted = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // -- BETWEEN entries -------------------------------------------
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

        {
            const bool between_disabled =
                ( !versioned                                ||
                  (_ui.between_from_buffer[0] == '\0')      ||
                  (_ui.between_to_buffer[0]   == '\0') );

            if (accent_button(
                    "Go##between",
                    imgui_mariadb_toolbar_style::btn_temporal,
                    imgui_mariadb_toolbar_style::btn_temporal_hover,
                    imgui_mariadb_toolbar_style::btn_temporal,
                    between_disabled))
            {
                mariadb_table_view_refresh_between(
                    _state,
                    _ui.between_from_buffer,
                    _ui.between_to_buffer);
                interacted = true;
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // -- ALL VERSIONS ----------------------------------------------
        if (accent_button(
                "All Versions",
                imgui_mariadb_toolbar_style::btn_temporal,
                imgui_mariadb_toolbar_style::btn_temporal_hover,
                imgui_mariadb_toolbar_style::btn_temporal,
                !versioned))
        {
            mariadb_table_view_refresh_all_versions(_state);
            interacted = true;
        }

        ImGui::SameLine();

        // -- Clear (live snapshot) — only useful when temporal active --
        {
            const bool clear_disabled = !_state.temporal_active;

            if (accent_button(
                    "Clear",
                    imgui_mariadb_toolbar_style::btn_clear,
                    imgui_mariadb_toolbar_style::btn_clear_hover,
                    imgui_mariadb_toolbar_style::btn_clear,
                    clear_disabled))
            {
                mariadb_table_view_clear_temporal(_state);
                interacted = true;
            }
        }

        // -- active-temporal indicator ---------------------------------
        if (_state.temporal_active)
        {
            ImGui::SameLine();

            scoped_color active_color {
                { ImGuiCol_Text,
                      imgui_mariadb_toolbar_style::temporal_active_text }
            };

            ImGui::Text("active: %s",
                        _state.temporal_text.c_str());
        }
        else if (!versioned)
        {
            ImGui::SameLine();

            scoped_color hint_color {
                { ImGuiCol_Text,
                      palette::get<palette::text_muted_tag>() }
            };

            ImGui::TextUnformatted("(not system-versioned)");
        }

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
        status_bar_scope sb("##mariadb_status");

        if (!sb.is_visible())
        {
            return;
        }

        scoped_color text_color {
            { ImGuiCol_Text,
                  palette::get<palette::text_muted_tag>() }
        };

        const std::string status =
            mariadb_table_view_status_text(_state, _table_view);

        ImGui::TextUnformatted(status.c_str());

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
    bool  interacted;
    float grid_height;

    interacted = false;

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
    grid_height = ( ImGui::GetContentRegionAvail().y -
                    palette::get<palette::status_bar_height_tag>() );

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
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_DATABASE_MARIADB_TABLE_