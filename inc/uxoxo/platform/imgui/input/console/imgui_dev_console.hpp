/******************************************************************************
* uxoxo [imgui]                                          imgui_dev_console.hpp
*
*   Dear ImGui draw handler for the dev_console component.  Renders the
* console as a dockable/hideable ImGui window containing:
*
*   - output pane (dcf_output):  scrollable colored log, auto-scroll
*   - input bar:  always present; text_input with prompt prefix
*   - submit button (dcf_submit_btn):  beside the input bar
*   - autosuggest dropdown (dcf_autosuggest):  below the input bar
*   - autocomplete inline (dcf_autocomplete):  ghost text in input
*   - history navigation (dcf_history):  up/down arrow in input
*   - log level filter (dcf_log_levels):  combo selector in toolbar
*
*   Every feature except the text_input is independently hideable and
* disableable at runtime via the imgui_console_view_state struct.
* The console window itself can be toggled by a configurable key
* (default: backtick/tilde) or by calling imgui_console_toggle().
*
*   Two layout modes are supported (selected via imgui_console_view_state.layout):
*
*     window         the console is wrapped in its own ImGui::Begin /
*                    ImGui::End window, with optional docking to top /
*                    bottom / full overlay.  Default mode; suitable for
*                    Quake-style overlay consoles, debug panes, or
*                    popouts.  Output pane fills available vertical
*                    space minus the input bar.
*     inline_widget  the console emits its widgets at the current ImGui
*                    cursor position with no surrounding window.  The
*                    caller owns the surrounding container - typically a
*                    toolbar BeginChild or a full window split.  The
*                    input bar fills the available horizontal space; the
*                    output pane (when shown) is drawn at a configurable
*                    fixed height (output_pane_height /
*                    output_pane_min_height fields).  This is the mode
*                    used to embed a console as a toolbar widget.
*
*   Initial visibility for both the console as a whole and the output
* pane is data-driven through the view state - set vs.open = false to
* boot with the console hidden, vs.show_output = false to boot with
* the output pane collapsed.  Both can be toggled at runtime.
*
*   Migration note (2026.05.08): the local `imgui_console_style`
* namespace has been trimmed.  Color slots that match shared palette
* tags (window_bg, window_border, color_normal, color_info,
* color_warning, color_error, color_debug, color_success, color_muted,
* color_highlight) now resolve to `palette::` tags from
* imgui_palette.hpp.  Console-specific surfaces (output pane, input
* bar, prompt, submit button, autosuggest dropdown, autocomplete
* ghost) and sizing constants stay local because they have no
* consumer outside the dev console renderer.  No behavioural change.
*
*   Structure:
*     1.  style constants
*     2.  imgui_console_view_state (runtime visibility/dock state)
*     3.  internal helpers
*     4.  output pane rendering
*     5.  input bar rendering
*     6.  autosuggest dropdown rendering
*     7.  log level selector
*     8.  shared body renderer
*     9.  imgui_draw_dev_console (main entry point)
*     10. toggle key handler
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included
* before this header.
*
*
* path:      /inc/uxoxo/platform/imgui/input/console/imgui_dev_console.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.04.10
******************************************************************************/

#ifndef UXOXO_IMGUI_COMPONENT_DEV_CONSOLE_
#define UXOXO_IMGUI_COMPONENT_DEV_CONSOLE_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../../uxoxo.hpp"
#include "../../../../templates/render_context.hpp"
#include "../../../../templates/component/input/console/dev_console.hpp"
#include "../../../../templates/component/output/text_output.hpp"
#include "../../../../templates/component/button/button.hpp"
#include "../core/imgui_palette.hpp"


NS_UXOXO
NS_IMGUI


using uxoxo::component::log_level;
using uxoxo::component::output_color_tag;
using uxoxo::component::render_context;

// ===========================================================================
//  1.  STYLE CONSTANTS
// ===========================================================================

//   Migration note (2026.05.08): values matching shared palette tags
// (window_bg, window_border, color_normal, color_info, color_warning,
// color_error, color_debug, color_success, color_highlight,
// color_muted) are now drawn from `palette::` (imgui_palette.hpp).
// Console-specific surfaces (output pane, input bar, prompt, submit
// button, autosuggest dropdown, autocomplete ghost) and sizing
// constants stay local because they have no consumer outside the
// dev console renderer.
namespace imgui_console_style
{
    // window
    inline constexpr float min_height   = 120.0f;

    // output pane - console-local
    inline const ImVec4 output_bg       = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    inline const ImVec4 output_border   = ImVec4(0.18f, 0.18f, 0.22f, 0.60f);

    // input bar - console-local
    inline const ImVec4 input_bg        = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
    inline const ImVec4 input_border    = ImVec4(0.25f, 0.25f, 0.30f, 0.80f);
    inline const ImVec4 prompt_color    = ImVec4(0.45f, 0.65f, 0.90f, 1.0f);

    // submit button - console-local accent
    inline const ImVec4 submit_bg       = ImVec4(0.22f, 0.45f, 0.65f, 1.0f);
    inline const ImVec4 submit_hover    = ImVec4(0.28f, 0.52f, 0.75f, 1.0f);
    inline const ImVec4 submit_active   = ImVec4(0.18f, 0.38f, 0.55f, 1.0f);

    // autosuggest dropdown - console-local
    inline const ImVec4 suggest_bg      = ImVec4(0.14f, 0.14f, 0.17f, 0.96f);
    inline const ImVec4 suggest_sel     = ImVec4(0.22f, 0.40f, 0.65f, 0.70f);
    inline const ImVec4 suggest_text    = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
    inline const ImVec4 suggest_border  = ImVec4(0.25f, 0.25f, 0.30f, 0.80f);

    // autocomplete ghost - console-local
    inline const ImVec4 ghost_text      = ImVec4(0.40f, 0.45f, 0.55f, 0.70f);

    // sizing - no palette equivalents
    inline constexpr float level_combo_width = 90.0f;
    inline constexpr std::size_t input_buf_size = 4096;
    inline constexpr ImGuiKey toggle_key = ImGuiKey_GraveAccent;

}   // namespace imgui_console_style


// ===========================================================================
//  2.  IMGUI CONSOLE VIEW STATE
// ===========================================================================
//   Runtime state controlling which features are visible and how
// the console window is docked.  This is renderer-side state, NOT
// part of the framework-agnostic dev_console - it lives alongside
// the console as a companion struct, similar to
// database_table_state.

// imgui_console_dock
//   enum: docking position for the console window.  Only consulted
// when the layout mode is `window`; ignored in `inline_widget` mode
// (the surrounding container does the positioning).
enum class imgui_console_dock : std::uint8_t
{
    floating,       // free-floating ImGui window
    top,            // anchored to the top of the viewport
    bottom,         // anchored to the bottom of the viewport
    full            // full viewport overlay (half-life style)
};

// imgui_console_layout
//   enum: how the console widgets are emitted into the ImGui frame.
// Toggled by setting view_state.layout; both modes honor the same
// per-feature flags (show_output, show_history, etc).
enum class imgui_console_layout : std::uint8_t
{
    window,         // wrap the console in its own ImGui::Begin /
                    //   ImGui::End window.  Honors `dock` for window
                    //   positioning.  Output pane fills available
                    //   vertical space.  This is the default and
                    //   matches the original behavior.
    inline_widget   // emit the console widgets at the current ImGui
                    //   cursor position with no surrounding window.
                    //   The caller owns the surrounding container
                    //   (typically a toolbar BeginChild or a full
                    //   window split).  The input bar fills the
                    //   available horizontal space, so the console
                    //   transparently expands to fit a toolbar of
                    //   any width.  Output pane uses
                    //   output_pane_height / output_pane_min_height.
};

// imgui_console_view_state
//   struct: per-instance renderer state for a dev_console.
struct imgui_console_view_state
{
    // -- layout -------------------------------------------------------
    //   `layout` selects between the windowed (default) and inline
    // (toolbar-embedded) draw paths.  `dock` is consulted only when
    // layout is `window`.
    imgui_console_layout layout     = imgui_console_layout::window;
    imgui_console_dock   dock       = imgui_console_dock::floating;
    float                dock_ratio = 0.40f;    // fraction of viewport

    // -- inline-mode output pane sizing -------------------------------
    //   In windowed mode the output pane fills the remaining vertical
    // space inside the window.  In inline_widget mode there is no
    // window, so the output pane needs an explicit height; these
    // fields supply it.  output_pane_height is the current/initial
    // height in pixels; output_pane_min_height is the floor below
    // which the pane will not shrink even if the surrounding container
    // is squeezed.
    float                output_pane_height     = 160.0f;
    float                output_pane_min_height =  40.0f;

    // -- window visibility (toggled by key or button) -----------------
    //   When false, imgui_draw_dev_console is a no-op.  Use this for
    // "console initially hidden" - set false at construction, then
    // toggle with the `~` key (via imgui_console_handle_toggle_key)
    // or programmatically via imgui_console_toggle.
    bool open = true;

    // -- per-feature visibility (runtime overrides) -------------------
    //   show_output controls whether the scrollback pane is rendered;
    // set false for "output initially hidden" (the input bar still
    // draws, the pane just collapses).
    bool show_output      = true;
    bool show_suggest     = true;
    bool show_complete    = true;
    bool show_history     = true;
    bool show_submit_btn  = true;
    bool show_log_levels  = true;

    // -- per-feature enable (runtime overrides) -----------------------
    bool enable_suggest   = true;
    bool enable_complete  = true;
    bool enable_history   = true;

    // -- input focus management ---------------------------------------
    bool focus_input      = false;    // set true to force focus next frame
    bool was_just_opened  = false;    // true on the frame after toggle-open
};


// ===========================================================================
//  3.  INTERNAL HELPERS
// ===========================================================================

NS_INTERNAL

    // imgui_console_color_for_tag
    //   function: maps an output_color_tag to an ImVec4 color.
    inline ImVec4
    imgui_console_color_for_tag(
        output_color_tag _tag
    )
    {
        switch (_tag)
        {
            case output_color_tag::info:      return palette::get<palette::console_info_tag>();
            case output_color_tag::warning:   return palette::get<palette::console_warning_tag>();
            case output_color_tag::error:     return palette::get<palette::console_error_tag>();
            case output_color_tag::debug:     return palette::get<palette::console_debug_tag>();
            case output_color_tag::success:   return palette::get<palette::console_success_tag>();
            case output_color_tag::muted:     return palette::get<palette::text_muted_tag>();
            case output_color_tag::highlight: return palette::get<palette::console_highlight_tag>();
            case output_color_tag::normal:
            default:                          return palette::get<palette::console_normal_tag>();
        }
    }

    // imgui_console_apply_dock_position
    //   function: sets ImGui window position and size based on
    // dock mode before ImGui::Begin.
    inline void
    imgui_console_apply_dock_position(
        const imgui_console_view_state& _vs,
        const render_context&           _ctx
    )
    {
        float vw = _ctx.viewport_width;
        float vh = _ctx.viewport_height;
        float h  = vh * _vs.dock_ratio;

        if (h < imgui_console_style::min_height)
        {
            h = imgui_console_style::min_height;
        }

        switch (_vs.dock)
        {
            case imgui_console_dock::top:
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f),
                                        ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(vw, h),
                                          ImGuiCond_Always);
                break;

            case imgui_console_dock::bottom:
                ImGui::SetNextWindowPos(ImVec2(0.0f, vh - h),
                                        ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(vw, h),
                                          ImGuiCond_Always);
                break;

            case imgui_console_dock::full:
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f),
                                        ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(vw, vh * 0.5f),
                                          ImGuiCond_Always);
                break;

            case imgui_console_dock::floating:
            default:
                ImGui::SetNextWindowSize(ImVec2(520.0f, 340.0f),
                                          ImGuiCond_FirstUseEver);
                break;
        }

        return;
    }

NS_END  // internal


// ===========================================================================
//  4.  OUTPUT PANE RENDERING
// ===========================================================================

// imgui_draw_console_output
//   function: draws the scrollable output pane.
template<typename _Console>
void
imgui_draw_console_output(
    _Console&                       _dc,
    const imgui_console_view_state& _vs,
    float                           _height
)
{
    if constexpr (!_Console::has_output)
    {
        (void)_dc;
        (void)_vs;
        (void)_height;

        return;
    }
    else
    {
        if (!_vs.show_output)
        {
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              imgui_console_style::output_bg);

        ImGui::BeginChild("##console_output",
                          ImVec2(0.0f, _height),
                          true,
                          ImGuiWindowFlags_HorizontalScrollbar);

        auto& output = _dc.output;

        // render visible lines
        for (const auto& line : output.lines)
        {
            // color per line
            ImVec4 color = palette::get<palette::console_normal_tag>();

            if constexpr (std::remove_reference_t<decltype(output)>::has_color)
            {
                if (line.color_tag == output_color_tag::custom)
                {
                    color = ImVec4(line.custom_r,
                                   line.custom_g,
                                   line.custom_b,
                                   line.custom_a);
                }
                else
                {
                    color = internal::imgui_console_color_for_tag(
                        line.color_tag);
                }
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(line.text.c_str());
            ImGui::PopStyleColor();
        }

        // auto-scroll
        if (output.auto_scroll &&
            ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        return;
    }
}


// ===========================================================================
//  5.  INPUT BAR RENDERING
// ===========================================================================

// imgui_draw_console_input_bar
//   function: draws the input line with prompt, text field, and
// optional submit button.  Returns true if the user submitted
// (pressed enter or clicked submit).
template<typename _Console>
bool
imgui_draw_console_input_bar(
    _Console&                 _dc,
    imgui_console_view_state& _vs
)
{
    bool submitted = false;

    // prompt label
    ImGui::PushStyleColor(ImGuiCol_Text,
                          imgui_console_style::prompt_color);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(_dc.prompt.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // input field
    ImGui::PushStyleColor(ImGuiCol_FrameBg,
                          imgui_console_style::input_bg);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          imgui_console_style::input_border);

    // compute width: fill remaining space minus submit button
    float submit_btn_width = 0.0f;

    if constexpr (_Console::has_submit_btn)
    {
        if (_vs.show_submit_btn)
        {
            submit_btn_width = ImGui::CalcTextSize("Submit").x + 20.0f;
        }
    }

    ImGui::SetNextItemWidth(
        ImGui::GetContentRegionAvail().x - submit_btn_width - 8.0f);

    // prepare input buffer
    static char input_buf[imgui_console_style::input_buf_size];
    std::size_t copy_len = std::min(
        _dc.input.value.size(),
        imgui_console_style::input_buf_size - 1);

    std::memcpy(input_buf,
                _dc.input.value.c_str(),
                copy_len);
    input_buf[copy_len] = '\0';

    ImGuiInputTextFlags input_flags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackHistory;

    // auto-focus on open
    if (_vs.focus_input)
    {
        ImGui::SetKeyboardFocusHere();
        _vs.focus_input = false;
    }

    // history callback
    struct history_ctx
    {
        _Console* dc;
        imgui_console_view_state* vs;
    };

    history_ctx hctx { &_dc, &_vs };

    auto history_callback = [](ImGuiInputTextCallbackData* _data) -> int
    {
        auto* ctx = static_cast<history_ctx*>(_data->UserData);

        if constexpr (_Console::has_history)
        {
            if (!ctx->vs->enable_history || !ctx->vs->show_history)
            {
                return 0;
            }

            if (_data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
            {
                if (_data->EventKey == ImGuiKey_UpArrow)
                {
                    dc_history_prev(*ctx->dc);
                    auto* entry = hv_current(ctx->dc->cmd_history);

                    if (entry)
                    {
                        _data->DeleteChars(0, _data->BufTextLen);
                        _data->InsertChars(0, entry->c_str());
                    }
                }
                else if (_data->EventKey == ImGuiKey_DownArrow)
                {
                    if (dc_history_next(*ctx->dc))
                    {
                        auto* entry = hv_current(ctx->dc->cmd_history);

                        if (entry)
                        {
                            _data->DeleteChars(0, _data->BufTextLen);
                            _data->InsertChars(0, entry->c_str());
                        }
                    }
                    else
                    {
                        // past newest - restore saved input
                        _data->DeleteChars(0, _data->BufTextLen);
                        _data->InsertChars(
                            0,
                            ctx->dc->saved_input.c_str());
                    }
                }
            }
        }

        return 0;
    };

    if (ImGui::InputText("##console_input",
                         input_buf,
                         imgui_console_style::input_buf_size,
                         input_flags,
                         history_callback,
                         &hctx))
    {
        // enter pressed - submit
        std::string cmd = input_buf;

        if (!cmd.empty())
        {
            dc_submit(_dc, cmd);
            input_buf[0] = '\0';
            _dc.input.value.clear();
            submitted = true;

            // re-focus input after submit
            _vs.focus_input = true;
        }
    }

    // sync back to model
    _dc.input.value = input_buf;

    ImGui::PopStyleColor(2);

    // update suggestions on input change
    if constexpr (_Console::has_autosuggest)
    {
        if ( (_vs.enable_suggest) && (_vs.show_suggest) )
        {
            dc_update_suggestions(_dc, _dc.input.value);
        }
    }

    // submit button
    if constexpr (_Console::has_submit_btn)
    {
        if (_vs.show_submit_btn)
        {
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button,
                                  imgui_console_style::submit_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  imgui_console_style::submit_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  imgui_console_style::submit_active);

            if (ImGui::Button("Submit"))
            {
                std::string cmd = _dc.input.value;

                if (!cmd.empty())
                {
                    dc_submit(_dc, cmd);
                    _dc.input.value.clear();
                    submitted = true;
                    _vs.focus_input = true;
                }
            }

            ImGui::PopStyleColor(3);
        }
    }

    return submitted;
}


// ===========================================================================
//  6.  AUTOSUGGEST DROPDOWN RENDERING
// ===========================================================================

// imgui_draw_console_suggestions
//   function: draws the autosuggest dropdown below the input bar.
// Returns true if a suggestion was accepted.
template<typename _Console>
bool
imgui_draw_console_suggestions(
    _Console&                       _dc,
    const imgui_console_view_state& _vs
)
{
    if constexpr (!_Console::has_autosuggest)
    {
        (void)_dc;
        (void)_vs;

        return false;
    }
    else
    {
        if ( (!_vs.show_suggest)  ||
             (!_vs.enable_suggest) ||
             (!_dc.suggest.visible) ||
             (_dc.suggest.suggestions.empty()) )
        {
            return false;
        }

        bool accepted = false;

        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              imgui_console_style::suggest_bg);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              imgui_console_style::suggest_border);

        // compute dropdown height
        std::size_t show_count = std::min(
            _dc.suggest.suggestions.size(),
            _dc.suggest.max_visible);

        float line_h = ImGui::GetTextLineHeightWithSpacing();
        float dropdown_h = static_cast<float>(show_count) * line_h + 4.0f;

        ImGui::BeginChild("##console_suggest",
                          ImVec2(0.0f, dropdown_h),
                          true,
                          ImGuiWindowFlags_NoScrollbar);

        for (std::size_t i = 0; i < _dc.suggest.suggestions.size(); ++i)
        {
            if (i >= _dc.suggest.max_visible)
            {
                break;
            }

            bool is_selected = (i == _dc.suggest.selected);

            const auto& suggestion = _dc.suggest.suggestions[i];

            // convert suggestion to display string
            std::string display;

            if constexpr (std::is_convertible_v<
                              decltype(suggestion),
                              std::string>)
            {
                display = suggestion;
            }
            else
            {
                display = "?";
            }

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                imgui_console_style::suggest_text);

            if (ImGui::Selectable(display.c_str(),
                                  is_selected))
            {
                // accept suggestion
                _dc.input.value = display;
                _dc.input.cursor = display.size();
                as_dismiss(_dc.suggest);
                accepted = true;
            }

            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);

        // keyboard navigation within suggestions
        if (ImGui::IsWindowFocused(
                ImGuiFocusedFlags_RootAndChildWindows))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                as_next(_dc.suggest);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                as_prev(_dc.suggest);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Tab))
            {
                auto* sel = as_selected_value(_dc.suggest);

                if (sel)
                {
                    if constexpr (std::is_convertible_v<
                                      decltype(*sel),
                                      std::string>)
                    {
                        _dc.input.value = *sel;
                        _dc.input.cursor = _dc.input.value.size();
                    }

                    as_dismiss(_dc.suggest);
                    accepted = true;
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                as_dismiss(_dc.suggest);
            }
        }

        return accepted;
    }
}


// ===========================================================================
//  7.  LOG LEVEL SELECTOR
// ===========================================================================

// imgui_draw_console_log_level_combo
//   function: draws a log level filter combo box.
template<typename _Console>
void
imgui_draw_console_log_level_combo(
    _Console&                       _dc,
    const imgui_console_view_state& _vs
)
{
    if constexpr (!_Console::has_log_levels)
    {
        (void)_dc;
        (void)_vs;

        return;
    }
    else
    {
        if (!_vs.show_log_levels)
        {
            return;
        }

        static const char* level_names[] =
        {
            "All",
            "Debug",
            "Info",
            "Warning",
            "Error",
            "None"
        };

        static const log_level level_values[] =
        {
            log_level::all,
            log_level::debug,
            log_level::info,
            log_level::warning,
            log_level::error,
            log_level::none
        };

        // find current index
        int current = 0;

        for (int i = 0; i < 6; ++i)
        {
            if (level_values[i] == _dc.active_level)
            {
                current = i;

                break;
            }
        }

        ImGui::SetNextItemWidth(
            imgui_console_style::level_combo_width);

        if (ImGui::Combo("##log_level",
                         &current,
                         level_names,
                         6))
        {
            dc_set_log_level(_dc, level_values[current]);
        }

        return;
    }
}


// ===========================================================================
//  8.  SHARED BODY RENDERER
// ===========================================================================
//   The body widgets (log level combo, clear button, output pane,
// input bar, suggestion dropdown) are identical between windowed and
// inline layout modes - the only thing that changes is whether a
// surrounding ImGui::Begin / ImGui::End wraps them, and how the
// output pane is sized vertically.  Factoring them out into a single
// helper keeps the two layout paths in lockstep.

NS_INTERNAL

    // imgui_draw_dev_console_body
    //   function: emits the inner widgets of the console at the
    // current ImGui cursor.  Caller controls the surrounding
    // container (window, child, toolbar, etc.).
    //
    //   When _inline_layout is true, the output pane uses the
    // explicit pixel height supplied via vs.output_pane_height
    // (clamped to vs.output_pane_min_height); when false, the output
    // pane fills the remaining vertical space inside the parent
    // window.  Returns true if the user interacted (submitted, clicked
    // submit, or accepted a suggestion).
    template<typename _Console>
    bool
    imgui_draw_dev_console_body(
        _Console&                 _dc,
        imgui_console_view_state& _vs,
        bool                      _inline_layout
    )
    {
        bool interacted = false;

        // -----------------------------------------------------------------
        //  log level combo (top bar)
        // -----------------------------------------------------------------
        imgui_draw_console_log_level_combo(_dc, _vs);

        // optional: clear button
        if constexpr (_Console::has_output)
        {
            if (_vs.show_output)
            {
                if constexpr (_Console::has_log_levels)
                {
                    if (_vs.show_log_levels)
                    {
                        ImGui::SameLine();
                    }
                }

                if (ImGui::SmallButton("Clear"))
                {
                    dc_clear_output(_dc);
                    interacted = true;
                }
            }
        }

        // -----------------------------------------------------------------
        //  output pane
        // -----------------------------------------------------------------
        //   The pane only draws when the console has the output
        // feature compiled in AND the renderer is currently showing
        // it (vs.show_output).  Two height modes:
        //
        //     inline_layout = false  fill the remaining vertical
        //                            space in the surrounding window,
        //                            minus a reservation for the
        //                            input bar and (if present) the
        //                            log level combo.
        //
        //     inline_layout = true   use the configured pixel height
        //                            from vs.output_pane_height,
        //                            clamped to vs.output_pane_min_height.
        if constexpr (_Console::has_output)
        {
            if (_vs.show_output)
            {
                float output_height = 0.0f;

                if (_inline_layout)
                {
                    output_height = _vs.output_pane_height;

                    if (output_height < _vs.output_pane_min_height)
                    {
                        output_height = _vs.output_pane_min_height;
                    }
                }
                else
                {
                    // compute output pane height: all available
                    // space minus input bar (~26px) and log level
                    // combo if visible
                    float reserved =
                        ImGui::GetTextLineHeightWithSpacing() + 12.0f;

                    if constexpr (_Console::has_log_levels)
                    {
                        if (_vs.show_log_levels)
                        {
                            reserved +=
                                ImGui::GetTextLineHeightWithSpacing() + 4.0f;
                        }
                    }

                    output_height =
                        ImGui::GetContentRegionAvail().y - reserved;

                    if (output_height < _vs.output_pane_min_height)
                    {
                        output_height = _vs.output_pane_min_height;
                    }
                }

                imgui_draw_console_output(_dc, _vs, output_height);
            }
        }

        // -----------------------------------------------------------------
        //  input bar
        // -----------------------------------------------------------------
        //   Separator before the input bar gives a visual divider
        // between scrollback and the prompt; in inline mode it also
        // separates the console from whatever toolbar widgets sit
        // alongside it.
        ImGui::Separator();

        if (imgui_draw_console_input_bar(_dc, _vs))
        {
            interacted = true;
        }

        // -----------------------------------------------------------------
        //  autosuggest dropdown
        // -----------------------------------------------------------------
        if (imgui_draw_console_suggestions(_dc, _vs))
        {
            interacted = true;
        }

        return interacted;
    }

NS_END  // internal


// ===========================================================================
//  9.  MAIN ENTRY POINT
// ===========================================================================

// imgui_draw_dev_console
//   function: renders a dev_console using the layout mode selected by
// vs.layout (window or inline_widget).  Returns true if any user
// interaction occurred.
//
//   layout = window         the console is drawn in its own
//                           ImGui::Begin / ImGui::End window,
//                           positioned via vs.dock.  Backwards-
//                           compatible with all prior calling code.
//
//   layout = inline_widget  the console widgets are emitted at the
//                           caller's current ImGui cursor with no
//                           window wrapper.  Use this when embedding
//                           the console inside a toolbar, split view,
//                           or any custom container - the caller has
//                           already opened the parent window /
//                           BeginChild / etc.  The input bar fills
//                           the available horizontal space.
template<typename _Console>
bool
imgui_draw_dev_console(
    _Console&                 _dc,
    imgui_console_view_state& _vs,
    render_context&           _ctx
)
{
    // skip if not visible (both model and view state)
    if ( (!_dc.visible) || (!_vs.open) )
    {
        return false;
    }

    // -----------------------------------------------------------------
    //  inline layout: no window wrapper, draw at current cursor
    // -----------------------------------------------------------------
    //   The caller owns the surrounding container (toolbar, split
    // view, etc.) so we skip the dock positioning, the styled Begin/
    // End pair, and any window-level state syncing.  The body
    // renderer fills the available width naturally because every
    // sized widget inside it queries GetContentRegionAvail().x.
    if (_vs.layout == imgui_console_layout::inline_widget)
    {
        // focus input on first frame after opening
        if (_vs.was_just_opened)
        {
            _vs.focus_input      = true;
            _vs.was_just_opened  = false;
        }

        return internal::imgui_draw_dev_console_body(
            _dc,
            _vs,
            /*_inline_layout=*/true);
    }

    // -----------------------------------------------------------------
    //  windowed layout (default)
    // -----------------------------------------------------------------
    bool interacted = false;

    // apply dock positioning
    internal::imgui_console_apply_dock_position(_vs, _ctx);

    // window flags
    ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoCollapse;

    // docked consoles are not movable or resizable
    if (_vs.dock != imgui_console_dock::floating)
    {
        win_flags |= ImGuiWindowFlags_NoMove;
        win_flags |= ImGuiWindowFlags_NoResize;
        win_flags |= ImGuiWindowFlags_NoTitleBar;
    }

    // window style
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          palette::get<palette::window_bg_tag>());
    ImGui::PushStyleColor(ImGuiCol_Border,
                          palette::get<palette::window_border_tag>());

    bool window_open = _vs.open;

    if (!ImGui::Begin("Console",
                      &window_open,
                      win_flags))
    {
        ImGui::End();
        ImGui::PopStyleColor(2);
        _vs.open = window_open;

        return false;
    }

    _vs.open = window_open;

    // focus input on first frame after opening
    if (_vs.was_just_opened)
    {
        _vs.focus_input      = true;
        _vs.was_just_opened  = false;
    }

    // body widgets
    if (internal::imgui_draw_dev_console_body(
            _dc,
            _vs,
            /*_inline_layout=*/false))
    {
        interacted = true;
    }

    ImGui::End();
    ImGui::PopStyleColor(2);

    return interacted;
}


// ===========================================================================
//  10. TOGGLE KEY HANDLER
// ===========================================================================

// imgui_console_toggle
//   function: toggles the console open/closed.
inline void
imgui_console_toggle(
    imgui_console_view_state& _vs
)
{
    _vs.open = !_vs.open;

    if (_vs.open)
    {
        _vs.was_just_opened = true;
    }

    return;
}

// imgui_console_handle_toggle_key
//   function: checks if the configured toggle key was pressed
// and toggles the console.  Call once per frame, outside the
// console window.  Returns true if the key was consumed.
inline bool
imgui_console_handle_toggle_key(
    imgui_console_view_state& _vs,
    ImGuiKey                  _key = imgui_console_style::toggle_key
)
{
    if (ImGui::IsKeyPressed(_key))
    {
        imgui_console_toggle(_vs);

        return true;
    }

    return false;
}

// imgui_console_set_dock
//   function: changes the docking mode.
inline void
imgui_console_set_dock(
    imgui_console_view_state& _vs,
    imgui_console_dock        _dock,
    float                     _ratio = 0.40f
)
{
    _vs.dock       = _dock;
    _vs.dock_ratio = _ratio;

    return;
}

NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_DEV_CONSOLE_