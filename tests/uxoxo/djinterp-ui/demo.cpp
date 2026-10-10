/*******************************************************************************
* djinterp [demo]                                                     demo.cpp
*
*   Demonstrates the ui:: component module by constructing a Midnight Commander
* layout, populating file panels, and wiring signals — all without any
* rendering code.  A real application would hand the resulting node tree to a
* TuiRenderer<BackendT, ThemeT>.
*
*   Build:  g++ -std=c++17 -I. -o demo demo.cpp
*******************************************************************************/

#include <ui/ui.hpp>
#include <iostream>

using namespace djinterp::ui;
namespace ct = djinterp::ui::component_traits;


int main()
{
    // ── Build the MC layout ──────────────────────────────────────────────
    auto root = mc::build_layout();

    // ── Populate left file list ──────────────────────────────────────────
    if (auto* left = find_by_id(*root, "left_list")) {
        auto& lv = std::get<list_view>(left->component);
        lv.items = {
            { { "..",       "DIR",   "2025-05-18" }, false, true  },
            { { "docs/",   "DIR",   "2025-05-12" }, false, true  },
            { { "src/",    "DIR",   "2025-05-15" }, false, true  },
            { { "README",  "12K",   "2025-05-10" }, false, false },
            { { "Makefile", "2.1K", "2025-05-19" }, false, false },
        };
        lv.selected = 0;
    }

    // ── Populate right file list ─────────────────────────────────────────
    if (auto* right = find_by_id(*root, "right_list")) {
        auto& lv = std::get<list_view>(right->component);
        lv.items = {
            { { "..",          "DIR",  "2025-05-18" }, false, true  },
            { { "report.pdf", "45K",  "2025-05-01" }, false, false },
            { { "notes.txt",  "1.2K", "2025-04-30" }, false, false },
        };
    }

    // ── Wire signals ─────────────────────────────────────────────────────
    scoped_connections conns;

    // command_line submitted → print to stdout
    if (auto* cl = find_by_id(*root, "cmdline")) {
        auto& cmd = std::get<command_line>(cl->component);
        conns += cmd.submitted.connect([](const std::string& v) {
            std::cout << "  [command_line submitted] " << v << "\n";
        });
    }

    // list_view selection changed → update status bar
    if (auto* left = find_by_id(*root, "left_list")) {
        auto& lv = std::get<list_view>(left->component);
        if (auto* sb = find_by_id(*root, "left_status")) {
            auto& bar = std::get<status_bar>(sb->component);
            conns += lv.selection_changed.connect([&bar, &lv](int idx) {
                if (idx >= 0 && idx < (int)lv.items.size())
                    bar.content = "▸ " + lv.items[idx].cells[0];
            });
        }
    }

    // F5 (Copy) pressed → print
    if (auto* fb_node = find_by_id(*root, "fbar")) {
        auto& fb = std::get<function_bar>(fb_node->component);
        conns += fb.slots[4].activated.connect([]() {
            std::cout << "  [F5 Copy] triggered\n";
        });
    }

    // ── Dump tree structure ──────────────────────────────────────────────
    std::cout << "── MC Layout Tree ──────────────────────────\n";
    
    struct indent_walker {
        int depth = 0;
        void operator()(node& n) {
            std::string indent(depth * 2, ' ');
            std::string type_name = std::visit(ct::overloaded{
                [](const label&)        { return "label"; },
                [](const heading&)      { return "heading"; },
                [](const separator&)    { return "separator"; },
                [](const button&)       { return "button"; },
                [](const textbox&)      { return "textbox"; },
                [](const checkbox&)     { return "checkbox"; },
                [](const radio_group&)  { return "radio_group"; },
                [](const list_view&)    { return "list_view"; },
                [](const progress_bar&) { return "progress_bar"; },
                [](const scrollbar&)    { return "scrollbar"; },
                [](const container&)    { return "container"; },
                [](const panel&)       { return "panel"; },
                [](const split_view&)   { return "split_view"; },
                [](const tab_bar&)      { return "tab_bar"; },
                [](const dialog&)       { return "dialog"; },
                [](const menu_bar&)     { return "menu_bar"; },
                [](const function_bar&) { return "function_bar"; },
                [](const status_bar&)   { return "status_bar"; },
                [](const command_line&) { return "command_line"; },
            }, n.component);

            std::string suffix;
            if (!n.id.empty()) suffix = "  #" + n.id;
            if (n.focusable()) suffix += "  [focusable]";

            // show title/label for named components
            std::visit(ct::overloaded{
                [&](const panel& p)       { suffix += "  \"" + p.title + "\""; },
                [&](const menu_bar& m)    { 
                    suffix += "  {";
                    for (size_t i = 0; i < m.items.size(); ++i) {
                        if (i) suffix += ", ";
                        suffix += m.items[i];
                    }
                    suffix += "}";
                },
                [&](const command_line& c){ suffix += "  prompt=\"" + c.prompt + "\""; },
                [&](const list_view& lv)  { 
                    suffix += "  (" + std::to_string(lv.items.size()) + " rows, " 
                           + std::to_string(lv.columns.size()) + " cols)"; 
                },
                [&](const auto&) {}
            }, n.component);

            std::cout << indent << type_name << suffix << "\n";
        }
    };

    // manual depth-tracking walk
    struct tree_printer {
        static void print(node& n, int d) {
            indent_walker w{d};
            w(n);
            if (auto* kids = n.children_ptr())
                for (auto& child : *kids)
                    if (child) print(*child, d + 1);
        }
    };
    tree_printer::print(*root, 0);

    // ── Exercise signals ─────────────────────────────────────────────────
    std::cout << "\n── Signal Tests ────────────────────────────\n";

    // simulate list selection change
    if (auto* left = find_by_id(*root, "left_list")) {
        auto& lv = std::get<list_view>(left->component);
        lv.selection_changed.emit(2);       // select "src/"
    }

    // simulate command entry
    if (auto* cl = find_by_id(*root, "cmdline")) {
        auto& cmd = std::get<command_line>(cl->component);
        cmd.submitted.emit("ls -la");
    }

    // simulate F5 press
    if (auto* fb_node = find_by_id(*root, "fbar")) {
        auto& fb = std::get<function_bar>(fb_node->component);
        fb.slots[4].activated.emit();
    }

    // verify status_bar was updated by signal
    if (auto* sb = find_by_id(*root, "left_status")) {
        auto& bar = std::get<status_bar>(sb->component);
        std::cout << "  [left_status] " << bar.content << "\n";
    }

    // ── Focus order ──────────────────────────────────────────────────────
    std::cout << "\n── Focusable Components (Tab order) ────────\n";
    auto focusable = collect_focusable(*root);
    for (size_t i = 0; i < focusable.size(); ++i) {
        std::string desc = focusable[i]->id.empty() 
            ? "(anonymous)" 
            : focusable[i]->id;
        std::cout << "  " << i << ": " << desc << "\n";
    }

    std::cout << "\n── Done (" << focusable.size() << " focusable components) ──\n";
    return 0;
}
