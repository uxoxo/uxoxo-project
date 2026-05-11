/*******************************************************************************
* uxoxo [imgui]                                           imgui_mysql_table.hpp
*
*   ImGui draw handler for the mysql_table_view component.  Layers Oracle-
* MySQL-specific chrome onto the shared imgui_database_table_draw
* layout:
*   - an extras toolbar row below the base toolbar carrying the
*     optimizer hint entry (with Set / Clear) and the maintenance
*     button set: ANALYZE, OPTIMIZE, CHECK
*   - a modal popup for the CHECK TABLE result, rendered every frame
*     and latched open when CHECK is clicked
*   - a status bar that uses mysql_table_view_status_text so the
*     footer shows the active optimizer hint and JSON column count
*     alongside the base connection and sync flags
*   Per-cell JSON column tinting is not injected from this handler —
* the grid is delegated to imgui_draw_table_view and the tint would
* need to hook its cell rendering path.  The JSON column presence is
* instead surfaced in the status bar via mysql_table_view_status_text
* ("[json:N]").  Callers wanting per-cell tinting can either enable a
* cell-styler hook in imgui_draw_table_view or wrap this drawer and
* overlay.
*   The pair struct mysql_table_view_pair bundles references to the
* table_view, the mysql_table_state, and a per-view
* imgui_mysql_table_ui holding ImGui input buffers and the CHECK
* popup state — so the type-erased registry can dispatch with a
* single void* the same way database_table_view_pair does.
*   Migration note (2026.05.08):
*     - `mysql_table_view_pair` is now a using-alias of the generic
*       `view_state_pair<table_view, mysql_table_state,
*        imgui_mysql_table_ui>` template from imgui_view_pair.hpp.
*       Brace-initialization at call sites is preserved.
*     - The local `imgui_draw_mysql_accent_button` helper was a
*       3-color push/pop wrapper used five times in this file.  It
*       has been removed in favour of `accent_button(label, normal,
*       hover, normal, disabled)` from imgui_button_helpers.hpp.
*       Each call site reads with one less indirection.
*     - The extras-row BeginChild + ChildBg push/pop pair becomes a
*       `toolbar_scope` from imgui_chrome.hpp.  The status-bar pair
*       becomes a `status_bar_scope`.
*     - References into the now-removed shared `imgui_db_toolbar_style`
*       (toolbar_bg, btn_disabled, status_text, status_height) have
*       been redirected to the corresponding `palette::` tags.
*       MySQL-specific accents (analyze cyan, optimize orange, check
*       green, hint blue, clear amber) stay in
*       `imgui_mysql_toolbar_style` because they have no consumer
*       outside this file.
*   Structure:
*     1.  per-view imgui state
*     2.  mysql_table_view_pair (alias for view_state_pair)
*     3.  toolbar style (mysql-specific accents only)
*     4.  extras row rendering  (optimizer hint + maintenance)
*     5.  check-result popup
*     6.  status bar rendering
*     7.  imgui_draw_mysql_table_view (main entry point)
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included
* before this header.
*
*
* path:      /inc/uxoxo/platform/imgui/database/mysql/imgui_mysql_table.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef UXOXO_COMPONENT_IMGUI_DATABASE_TABLE_MYSQL_
#define UXOXO_COMPONENT_IMGUI_DATABASE_TABLE_MYSQL_ 1

// std
#include <string>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/table/table_view.hpp"
#include "../../../templates/component/database/mysql_table_view.hpp"
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
using uxoxo::component::mysql_table_state;


// =============================================================================
//  1.  PER-VIEW IMGUI STATE
// =============================================================================
//   ImGui is immediate mode and mysql_table_state carries no input
// buffers, so the MySQL drawer introduces a companion struct for
// text-entry state and CHECK-popup latching.  Callers hold one
// instance per drawn view.

// imgui_mysql_table_ui
//   struct: per-view ImGui input buffers for the MySQL drawer.
struct imgui_mysql_table_ui
{
    // optimizer hint entry
    char optimizer_hint_buffer[256] = {};

    // CHECK TABLE popup state
    std::string check_result_message;
    bool        open_check_popup     = false;
};


// =============================================================================
//  2.  MYSQL TABLE VIEW PAIR
// =============================================================================
//   Bundled references for registry dispatch — parallels
// database_table_view_pair but carries the MySQL state and the
// MySQL UI buffers.

// mysql_table_view_pair
//   alias: view_state_pair instantiated for table_view +
// mysql_table_state + imgui_mysql_table_ui.  Brace-initialization
// at call sites is preserved because the alias resolves to a plain
// aggregate.
using mysql_table_view_pair =
    view_state_pair<table_view, mysql_table_state, imgui_mysql_table_ui>;


// =============================================================================
//  3.  TOOLBAR STYLE
// =============================================================================
//   MySQL-specific accent palette.  Analyze is read-mostly (cyan),
// optimize mutates physical layout (orange), and check is read-only
// (green).  Hint and clear use blue and amber respectively.  Shared
// chrome colors (toolbar_bg, btn_disabled, etc.) come from
// `palette::` and are NOT duplicated here.

namespace imgui_mysql_toolbar_style
{
    D_INLINE const ImVec4 hint_accent        =
        ImVec4(0.30f, 0.55f, 0.75f, 1.0f);

    D_INLINE const ImVec4 btn_analyze        =
        ImVec4(0.24f, 0.55f, 0.60f, 1.0f);
    D_INLINE const ImVec4 btn_analyze_hover  =
        ImVec4(0.30f, 0.66f, 0.72f, 1.0f);

    D_INLINE const ImVec4 btn_optimize       =
        ImVec4(0.72f, 0.48f, 0.24f, 1.0f);
    D_INLINE const ImVec4 btn_optimize_hover =
        ImVec4(0.85f, 0.58f, 0.30f, 1.0f);

    D_INLINE const ImVec4 btn_check          =
        ImVec4(0.32f, 0.60f, 0.38f, 1.0f);
    D_INLINE const ImVec4 btn_check_hover    =
        ImVec4(0.40f, 0.72f, 0.46f, 1.0f);

    D_INLINE const ImVec4 btn_hint           =
        ImVec4(0.28f, 0.45f, 0.68f, 1.0f);
    D_INLINE const ImVec4 btn_hint_hover     =
        ImVec4(0.36f, 0.56f, 0.80f, 1.0f);

    D_INLINE const ImVec4 btn_clear          =
        ImVec4(0.55f, 0.45f, 0.30f, 1.0f);
    D_INLINE const ImVec4 btn_clear_hover    =
        ImVec4(0.68f, 0.55f, 0.36f, 1.0f);

}   // namespace imgui_mysql_toolbar_style


// =============================================================================
//  4.  EXTRAS ROW RENDERING
// =============================================================================

NS_INTERNAL

    // imgui_draw_mysql_extras
    //   function: draws the MySQL extras row (optimizer hint entry
    // with Set / Clear, and the ANALYZE / OPTIMIZE / CHECK
    // maintenance buttons).  CHECK captures the result into the UI
    // buffer and latches the result popup.  Maintenance buttons are
    // disabled when the table is not connected.  Returns true if
    // any control was interacted with.
    D_INLINE bool
    imgui_draw_mysql_extras(
        mysql_table_state&    _state,
        imgui_mysql_table_ui& _ui
    )
    {
        bool interacted;
        bool connected;

        interacted = false;

        toolbar_scope extras("##mysql_extras");

        if (!extras.is_visible())
        {
            return false;
        }

        // -- optimizer hint label + input + Set + Clear ----------------
        {
            scoped_color hint_label_color {
                { ImGuiCol_Text,
                      imgui_mysql_toolbar_style::hint_accent }
            };

            ImGui::TextUnformatted("Hint:");
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##optimizer_hint",
                                 "e.g. INDEX(t idx_name) NO_CACHE",
                                 _ui.optimizer_hint_buffer,
                                 sizeof(_ui.optimizer_hint_buffer));

        ImGui::SameLine();

        if (accent_button(
                "Set",
                imgui_mysql_toolbar_style::btn_hint,
                imgui_mysql_toolbar_style::btn_hint_hover,
                imgui_mysql_toolbar_style::btn_hint,
                false))
        {
            mysql_table_view_set_optimizer_hint(
                _state,
                _ui.optimizer_hint_buffer);
            interacted = true;
        }

        ImGui::SameLine();

        {
            const bool clear_disabled =
                !mysql_table_view_has_optimizer_hint(_state);

            if (accent_button(
                    "Clear",
                    imgui_mysql_toolbar_style::btn_clear,
                    imgui_mysql_toolbar_style::btn_clear_hover,
                    imgui_mysql_toolbar_style::btn_clear,
                    clear_disabled))
            {
                mysql_table_view_clear_optimizer_hint(_state);

                _ui.optimizer_hint_buffer[0] = '\0';
                interacted                   = true;
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // -- maintenance buttons — all require connection --------------
        connected = mysql_table_view_is_connected(_state);

        if (accent_button(
                "ANALYZE",
                imgui_mysql_toolbar_style::btn_analyze,
                imgui_mysql_toolbar_style::btn_analyze_hover,
                imgui_mysql_toolbar_style::btn_analyze,
                !connected))
        {
            mysql_table_view_analyze(_state);
            interacted = true;
        }

        ImGui::SameLine();

        if (accent_button(
                "OPTIMIZE",
                imgui_mysql_toolbar_style::btn_optimize,
                imgui_mysql_toolbar_style::btn_optimize_hover,
                imgui_mysql_toolbar_style::btn_optimize,
                !connected))
        {
            mysql_table_view_optimize(_state);
            interacted = true;
        }

        ImGui::SameLine();

        if (accent_button(
                "CHECK",
                imgui_mysql_toolbar_style::btn_check,
                imgui_mysql_toolbar_style::btn_check_hover,
                imgui_mysql_toolbar_style::btn_check,
                !connected))
        {
            _ui.check_result_message = mysql_table_view_check(_state);
            _ui.open_check_popup     = true;
            interacted               = true;
        }

        // -- active optimizer hint indicator ---------------------------
        if (mysql_table_view_has_optimizer_hint(_state))
        {
            ImGui::SameLine();

            scoped_color active_hint_color {
                { ImGuiCol_Text,
                      imgui_mysql_toolbar_style::hint_accent }
            };

            ImGui::Text("/*+ %s */",
                        _state.optimizer_hint.c_str());
        }

        return interacted;
    }


// =============================================================================
//  5.  CHECK-RESULT POPUP
// =============================================================================

    // imgui_draw_mysql_check_popup
    //   function: draws the modal popup that shows the most recent
    // CHECK TABLE result.  Called every frame; the popup only
    // appears after imgui_draw_mysql_extras latches it via
    // _ui.open_check_popup.
    D_INLINE void
    imgui_draw_mysql_check_popup(
        imgui_mysql_table_ui& _ui
    )
    {
        const char* text;

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

        text = ( _ui.check_result_message.empty()
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


// =============================================================================
//  6.  STATUS BAR RENDERING
// =============================================================================

    // imgui_draw_mysql_status_bar
    //   function: draws the status bar using
    // mysql_table_view_status_text so optimizer hint and JSON
    // column indicators appear alongside the base connection and
    // sync flags.
    D_INLINE void
    imgui_draw_mysql_status_bar(
        const table_view&        _table_view,
        const mysql_table_state& _state
    )
    {
        status_bar_scope sb("##mysql_status");

        if (!sb.is_visible())
        {
            return;
        }

        scoped_color text_color {
            { ImGuiCol_Text,
                  palette::get<palette::text_muted_tag>() }
        };

        const std::string status =
            mysql_table_view_status_text(_state, _table_view);

        ImGui::TextUnformatted(status.c_str());

        return;
    }

NS_END  // internal


// =============================================================================
//  7.  IMGUI DRAW MYSQL TABLE VIEW
// =============================================================================

// imgui_draw_mysql_table_view
//   function: renders an Oracle-MySQL-backed table with the base
// toolbar, a MySQL extras row (optimizer hint + maintenance), the
// delegated grid, an extended status bar, and the CHECK-result
// popup overlay.  Returns true if any user interaction occurred.
D_INLINE bool
imgui_draw_mysql_table_view(
    mysql_table_view_pair& _pair,
    render_context&        _ctx
)
{
    bool  interacted;
    float grid_height;

    interacted = false;

    // base toolbar — mysql_table_state IS-A database_table_state,
    // so the internal helper binds directly without adaptation
    interacted |=
        internal::imgui_draw_db_toolbar(_pair.view,
                                        _pair.state);

    // MySQL extras row (optimizer hint + ANALYZE / OPTIMIZE / CHECK)
    interacted |=
        internal::imgui_draw_mysql_extras(_pair.state,
                                          _pair.ui);

    // grid (remaining space minus status bar height)
    grid_height = ( ImGui::GetContentRegionAvail().y -
                    palette::get<palette::status_bar_height_tag>() );

    if (grid_height < 0.0f)
    {
        grid_height = 0.0f;
    }

    ImGui::BeginChild("##mysql_grid",
                      ImVec2(0.0f, grid_height),
                      false);

    interacted |= imgui_draw_table_view(_pair.view,
                                        _ctx);

    interacted |= imgui_table_view_handle_input(_pair.view);

    ImGui::EndChild();

    // extended status bar
    internal::imgui_draw_mysql_status_bar(_pair.view,
                                          _pair.state);

    // CHECK-result popup (rendered every frame, no-op until latched)
    internal::imgui_draw_mysql_check_popup(_pair.ui);

    return interacted;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_DATABASE_TABLE_MYSQL_