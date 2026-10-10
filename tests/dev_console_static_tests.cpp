/*******************************************************************************
* djinterp [test]                                              static_tests.cpp
*
*   Build:  g++ -std=c++17 -I. -o test static_tests.cpp && ./test
*******************************************************************************/

#include <container/dev_console.hpp>
#include <ui/qt/qt_dev_console.hpp>

#include <cassert>
#include <cstdio>
#include <string>

using namespace djinterp::container;
using namespace djinterp::ui::qt;
namespace ct  = djinterp::container::console_traits;
namespace qct = djinterp::ui::qt::qt_console_traits;

// ═══════════════════════════════════════════════════════════════════════════════
//  COMPILE-TIME: enums
// ═══════════════════════════════════════════════════════════════════════════════

static_assert(static_cast<int>(console_mode::panel)   == 0);
static_assert(static_cast<int>(console_mode::overlay) == 1);
static_assert(static_cast<int>(console_mode::toolbar) == 2);
static_assert(static_cast<int>(console_mode::dock)    == 3);
static_assert(static_cast<int>(console_mode::popup)   == 4);

static_assert(static_cast<int>(dock_area::bottom) == 0);
static_assert(static_cast<int>(dock_area::top)    == 1);
static_assert(static_cast<int>(overlay_edge::top)    == 0);
static_assert(static_cast<int>(overlay_edge::bottom) == 1);

// ═══════════════════════════════════════════════════════════════════════════════
//  COMPILE-TIME: traits
// ═══════════════════════════════════════════════════════════════════════════════

static_assert(!qct::is_qt_console_bridge_v<std::string>);
static_assert(!qct::is_qt_console_bridge_v<int>);
static_assert(!qct::is_qt_console_bridge_v<dev_console<>>);  // model, not bridge

// callback type instantiates
using handler_t = command_handler<dcf_none>;
static_assert(sizeof(handler_t) > 0);

using full_handler = command_handler<dcf_all>;
static_assert(sizeof(full_handler) > 0);


/*****************************************************************************/
// RUNTIME
/*****************************************************************************/

int main()
{
    std::printf("── sizeof ──────────────────────────────────\n");
    std::printf("  console_config : %zu\n", sizeof(console_config));
    std::printf("  console_colors : %zu\n", sizeof(console_colors));
    std::printf("  rgb            : %zu\n", sizeof(rgb));
    std::printf("  ✓ sizeof checks\n");

    // ═════════════════════════════════════════════════════════════════════
    //  CONSOLE CONFIG
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── config defaults ────────────────────────\n");
    {
        console_config cfg;
        assert(cfg.mode == console_mode::panel);
        assert(cfg.show_button);
        assert(cfg.button_text == "Console");
        assert(cfg.toggle_key == "`");
        assert(cfg.focus_on_show);
        assert(!cfg.auto_hide);
        assert(cfg.auto_hide_ms == 0);
        assert(cfg.hide_on_escape);
        assert(!cfg.hide_on_submit);
        assert(cfg.height_ratio > 0.0f);
        assert(cfg.min_height == 120);
        assert(cfg.slide_from == overlay_edge::top);
        assert(cfg.dock_pos == dock_area::bottom);
        assert(!cfg.dock_float);
        assert(cfg.font_size == 10);
        assert(!cfg.show_timestamps);
        assert(!cfg.show_badges);

        std::printf("  ✓ all defaults verified\n");
    }

    std::printf("── config customisation ───────────────────\n");
    {
        console_config cfg;
        cfg.mode           = console_mode::overlay;
        cfg.toggle_key     = "F12";
        cfg.show_button    = false;
        cfg.auto_hide      = true;
        cfg.auto_hide_ms   = 3000;
        cfg.hide_on_submit = true;
        cfg.opacity        = 0.8f;
        cfg.slide_from     = overlay_edge::bottom;
        cfg.font_family    = "JetBrains Mono";
        cfg.font_size      = 12;
        cfg.show_timestamps = true;
        cfg.show_badges    = true;

        assert(cfg.mode == console_mode::overlay);
        assert(cfg.toggle_key == "F12");
        assert(!cfg.show_button);
        assert(cfg.auto_hide);
        assert(cfg.auto_hide_ms == 3000);
        assert(cfg.hide_on_submit);
        assert(cfg.opacity < 0.85f);
        assert(cfg.slide_from == overlay_edge::bottom);
        assert(cfg.font_family == "JetBrains Mono");
        assert(cfg.font_size == 12);

        std::printf("  ✓ all custom values set correctly\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  COLOR SCHEMES
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── color schemes ──────────────────────────\n");
    {
        // dark (default)
        auto dark = console_colors::dark();
        assert(dark.background.r == 30);
        assert(dark.error.r == 255);
        assert(dark.error.g == 80);

        // light
        auto light = console_colors::light();
        assert(light.background.r == 245);
        assert(light.error.r == 200);

        // solarized
        auto sol = console_colors::solarized();
        assert(sol.background.r == 0);
        assert(sol.background.g == 43);
        assert(sol.background.b == 54);

        // custom
        console_colors custom;
        custom.background = { 0, 0, 0 };
        custom.output     = { 0, 255, 0 };  // matrix green
        assert(custom.background.r == 0);
        assert(custom.output.g == 255);

        // rgb sizeof
        static_assert(sizeof(rgb) == 3);

        std::printf("  ✓ dark, light, solarized, custom schemes\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  CALLBACK TYPES
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── callback types ─────────────────────────\n");
    {
        bool called = false;

        // bare handler
        handler_t handler = [&called](dev_console<>& dc,
                                       const std::string& cmd) {
            called = true;
            assert(cmd == "test");
            dc_print(dc, "result");
        };

        dev_console<> dc;
        handler(dc, "test");
        assert(called);
        assert(dc.log.size() == 1);
        assert(dc.log[0].text == "result");

        // full handler
        bool full_called = false;
        full_handler fh = [&full_called](dev_console<dcf_all>& dc,
                                          const std::string& cmd) {
            full_called = true;
            dc_print_with_badge(dc, "executed: " + cmd, "engine");
        };

        auto fdc = make_full_console("$ ");
        fh(fdc, "reload");
        assert(full_called);
        assert(fdc.log[0].badge == "engine");

        std::printf("  ✓ handlers callable, model mutation works\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  MODE-SPECIFIC CONFIG PATTERNS
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── mode config patterns ───────────────────\n");
    {
        // game console pattern (Quake-style)
        console_config game;
        game.mode           = console_mode::overlay;
        game.toggle_key     = "`";
        game.show_button    = false;
        game.hide_on_escape = true;
        game.opacity        = 0.92f;
        game.height_ratio   = 0.4f;
        game.slide_from     = overlay_edge::top;
        game.colors         = console_colors::solarized();
        assert(game.mode == console_mode::overlay);

        // IDE console pattern (dock)
        console_config ide;
        ide.mode        = console_mode::dock;
        ide.dock_pos    = dock_area::bottom;
        ide.dock_title  = "Terminal";
        ide.show_button = false;
        ide.toggle_key  = "Ctrl+`";
        ide.show_timestamps = true;
        assert(ide.mode == console_mode::dock);

        // search bar pattern (toolbar)
        console_config search;
        search.mode           = console_mode::toolbar;
        search.placeholder    = "Search...";
        search.show_button    = false;
        search.toggle_key.clear();
        search.hide_on_escape = false;
        assert(search.mode == console_mode::toolbar);

        // right-click console (popup)
        console_config popup;
        popup.mode           = console_mode::popup;
        popup.hide_on_submit = true;
        popup.auto_hide      = true;
        popup.auto_hide_ms   = 5000;
        popup.show_button    = false;
        popup.opacity        = 0.95f;
        assert(popup.mode == console_mode::popup);

        // embedded panel
        console_config panel;
        panel.mode       = console_mode::panel;
        panel.show_button = true;
        panel.min_height  = 200;
        panel.max_height  = 400;
        assert(panel.mode == console_mode::panel);

        std::printf("  ✓ game, IDE, search, popup, panel configs\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  FULL WORKFLOW WITH MODEL
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── model workflow (bridge-ready) ───────────\n");
    {
        auto dc = make_game_console("game> ");

        // set up suggest source
        dc_set_suggest_source(dc,
            [](const std::string& p) -> std::vector<std::string> {
                std::vector<std::string> cmds = {
                    "bind", "clear", "connect", "disconnect",
                    "exec", "help", "map", "quit", "say", "status"
                };
                std::vector<std::string> r;
                for (auto& c : cmds)
                    if (c.size() >= p.size() && c.substr(0, p.size()) == p)
                        r.push_back(c);
                return r;
            });

        // set up command handler (simulates what qt_dev_console does)
        auto handler = [](dev_console<dcf_history | dcf_autosuggest | dcf_log_levels>& dc,
                          const std::string& cmd) 
        {
            if (cmd == "help") {
                dc_print_info(dc, "Available commands: bind, clear, connect, "
                                  "disconnect, exec, help, map, quit, say, status");
            } else if (cmd == "status") {
                dc_print(dc, "Server: localhost:27015");
                dc_print(dc, "Players: 12/32");
                dc_print(dc, "Map: de_dust2");
            } else if (cmd == "clear") {
                dc_clear_log(dc);
            } else if (cmd == "quit") {
                dc_print_info(dc, "Goodbye!");
            } else {
                dc_print_error(dc, "Unknown command: " + cmd);
            }
        };

        // simulate user session
        dc_insert(dc, "h");
        dc_update_suggestions(dc);
        assert(dc.suggest_visible);
        assert(dc.suggestions[0] == "help");
        dc_accept_suggestion(dc);
        auto cmd = dc_submit(dc);
        handler(dc, cmd);
        assert(dc.log.size() == 2);  // command echo + info response

        dc_insert(dc, "status");
        cmd = dc_submit(dc);
        handler(dc, cmd);
        assert(dc.log.size() == 6);  // +1 echo +3 lines

        dc_insert(dc, "bogus");
        cmd = dc_submit(dc);
        handler(dc, cmd);
        assert(dc.log.size() == 8);  // +1 echo +1 error

        // filter: only errors
        dc_set_log_level(dc, log_level::error);
        auto vis = dc.visible_entries();
        // should see: 3 command echoes + 1 error = 4
        std::size_t cmd_count = 0, err_count = 0;
        for (auto idx : vis) {
            if (dc.log[idx].kind == entry_kind::command) ++cmd_count;
            if (dc.log[idx].kind == entry_kind::error)   ++err_count;
        }
        assert(err_count == 1);
        assert(cmd_count == 3);  // commands always pass filter

        // history
        dc_history_prev(dc);
        assert(dc.input == "bogus");
        dc_history_prev(dc);
        assert(dc.input == "status");
        dc_history_prev(dc);
        assert(dc.input == "help");

        // toggle
        dc_toggle(dc);
        assert(!dc.visible);
        dc_toggle(dc);
        assert(dc.visible);

        std::printf("  ✓ full session: suggest → submit → handler → filter → "
                     "history → toggle\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  QT BRIDGE STATUS
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── Qt bridge status ───────────────────────\n");
    {
    #if D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS
        std::printf("  Qt detected: qt_dev_console class available\n");

        using bridge_t = qt_dev_console<dcf_all>;
        static_assert(qct::is_qt_console_bridge_v<bridge_t>);
        static_assert(qct::has_widget_v<bridge_t>);
        static_assert(qct::has_toggle_v<bridge_t>);
        static_assert(qct::has_config_v<bridge_t>);
    #else
        std::printf("  Qt not detected: config/colors/traits/callbacks verified\n");
        std::printf("  (qt_dev_console class excluded by preprocessor guard)\n");
    #endif

        std::printf("  5 modes: panel, overlay, toolbar, dock, popup\n");
        std::printf("  3 color schemes: dark, light, solarized\n");
        std::printf("  ✓ bridge infrastructure ready\n");
    }

    std::printf("\n── ALL TESTS PASSED ─────────────────────────\n");
    return 0;
}
