/*******************************************************************************
* uxoxo [tests]                                         imgui_platform_smoke.cpp
*
* The ImGui platform, driven headless through ImGui's null backend.
*   Every archetype the platform interprets is realized and drawn for several
* frames, with no ImGui assertion and no clash of IDs; then the attributes
* every element shares are checked by simulated input: a "target" rides
* along with what an element posts, a "key" keeps a widget's identity while
* siblings before it come and go, a tab reports coming to the front, a tree
* node reports opening, and a splitter reports how far it was dragged.
*   A probe element -- a renderer registered with set_renderer, the way an
* application adds its own -- records the rectangle and ID of the item drawn
* just before it, which is how the simulated mouse finds its targets.
*
* path:      /tests/platform/imgui_platform_smoke.cpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/
// std
#include <cstdio>   // std::printf
#include <map>      // std::map
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// imgui
#include <imgui.h>            // ImGui
#include <imgui_internal.h>   // GImGui, the ID clash record
#include <imgui_impl_null.h>  // the headless backend
// uxoxo
#include <uxoxo/element_button.hpp>        // component::button
#include <uxoxo/element_color_field.hpp>   // component::color_field
#include <uxoxo/element_column.hpp>        // component::column
#include <uxoxo/element_combo.hpp>         // component::combo
#include <uxoxo/element_label.hpp>         // component::label
#include <uxoxo/element_menu.hpp>          // component::menu_bar, ...
#include <uxoxo/element_number_field.hpp>  // component::int_field, ...
#include <uxoxo/element_panel.hpp>         // component::panel
#include <uxoxo/element_row.hpp>           // component::row
#include <uxoxo/element_section.hpp>       // component::section
#include <uxoxo/element_selectable.hpp>    // component::selectable
#include <uxoxo/element_separator.hpp>     // component::separator
#include <uxoxo/element_splitter.hpp>      // component::splitter
#include <uxoxo/element_table.hpp>         // component::table, table_row
#include <uxoxo/element_tabs.hpp>          // component::tabs, tab
#include <uxoxo/element_text_field.hpp>    // component::text_field
#include <uxoxo/element_toggle.hpp>        // component::toggle
#include <uxoxo/element_tree_node.hpp>     // component::tree_node
#include <uxoxo/platform/imgui_platform.hpp>  // imgui_platform
#include <uxoxo/realize.hpp>               // realize, set_renderer


namespace
{

using namespace ::uxoxo;
using ::uxoxo::platform::imgui_blueprint;
using ::uxoxo::platform::imgui_frame;
using ::uxoxo::platform::imgui_platform;

// probe_record
//   struct: what a probe saw of the item before it.
struct probe_record
{
    ImVec2  min;
    ImVec2  max;
    ImGuiID id = 0;
};

// probes
//   the records, by the probe's "slot" attribute.
std::map<std::string, probe_record>&
probes()
{
    static std::map<std::string, probe_record> table;

    return table;
}

// probe_type
//   the probe's descriptor: no archetype a backend knows, so only its
// registered renderer draws it.
const element_type*
probe_type()
{
    static const element_type descriptor =
        element_type{ "probe",
                      "probe",
                      option_set{},
                      0,
                      0,
                      option_set{},
                      handler_fn{} };

    return &descriptor;
}

// probe
//   an element that records the item drawn just before it.
element_template
probe(
    const std::string& _slot
)
{
    option_set attrs;
    attrs.set("slot", _slot);

    return make_element(probe_type(), attrs, {});
}

// install_probe
//   registers the probe's renderer, the way an application would.
void
install_probe()
{
    set_renderer<imgui_platform>(
        probe_type(),
        [](const element_node<imgui_blueprint>& _node)
        {
            const std::string slot = _node.attrs.as_string("slot", "");

            return imgui_blueprint{
                [slot](imgui_frame&)
                {
                    probe_record& record = probes()[slot];
                    record.min = ImGui::GetItemRectMin();
                    record.max = ImGui::GetItemRectMax();
                    record.id  = ImGui::GetItemID();

                    return;
                },
                std::string(),
                std::string() };
        });

    return;
}

// attrs
//   a one-entry option record, for brevity below.
template<typename Value>
option_set
attr(
    const std::string& _key,
    Value&&            _value
)
{
    option_set record;
    record.set(_key, std::forward<Value>(_value));

    return record;
}

// id_clash
//   whether two items shared an ID in the frame just drawn. ImGui records
// every clash only when compiled with IMGUI_DEBUG_HIGHLIGHT_ALL_ID_CONFLICTS,
// which this test's build defines.
bool
id_clash()
{
    ImGuiContext& context = *GImGui;

    for (const ImGuiStoragePair& pair :
         context.DebugDrawIdConflictsHighlightSet.Data)
    {
        if (pair.val_i != context.FrameCount)
        {
            continue;
        }

        // ImGui's own multiline input submits its ID twice -- as an item,
        // and again when its child window ends -- and the full check flags
        // it every frame. An ID that is a child window's is that pattern.
        bool child = false;

        for (const ImGuiWindow* window : context.Windows)
        {
            child = ( (child) ||
                      (window->ChildId == pair.key) );
        }

        if (!child)
        {
            return true;
        }
    }

    return false;
}

// frame
//   one ImGui frame drawing a blueprint in a full-viewport window; returns
// the events it posted.
std::vector<event>
frame(
    const imgui_blueprint& _blueprint
)
{
    ImGui_ImplNull_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("smoke",
                 nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_MenuBar      |
                 ImGuiWindowFlags_NoSavedSettings);

    imgui_frame posted;

    if (_blueprint.draw)
    {
        _blueprint.draw(posted);
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplNullRender_RenderDrawData(ImGui::GetDrawData());

    return posted.events;
}

// centre
//   the middle of a probed rectangle.
ImVec2
centre(
    const std::string& _slot
)
{
    const probe_record& record = probes()[_slot];

    return ImVec2((record.min.x + record.max.x) * 0.5f,
                  (record.min.y + record.max.y) * 0.5f);
}

// find_event
//   the first event of a kind, or nullptr.
const event*
find_event(
    const std::vector<event>& _events,
    const std::string&        _kind
)
{
    for (const event& each : _events)
    {
        if (each.kind == _kind)
        {
            return &each;
        }
    }

    return nullptr;
}

// everything
//   a template using every archetype the platform interprets.
element_template
everything()
{
    using namespace ::uxoxo::component;

    std::vector<element_template> rows;
    rows.push_back(table_row({ label("a"), label("1") }));
    rows.push_back(table_row({ label("b"), int_field(2, "cell") }));

    return column({
        menu_bar({ menu("File",
                        { menu_item("Open", "open",
                                    attr("shortcut", "Ctrl+O")),
                          separator(),
                          menu_item("Grid", "grid",
                                    attr("checkable", true)) }) }),
        row({ button(label("OK"), true, attr("key", "ok")),
              toggle("on", true, "flip"),
              label("tinted", attr("a", 1.0)) }),
        section("numbers",
                { int_field(5, "n", attr("drag", true)),
                  real_field(0.5, "r"),
                  slider(0.25, 0.0, 1.0, "s"),
                  combo({ "one", "two" }, 1, "pick"),
                  color_field(0.2, 0.4, 0.6, 1.0, "colour"),
                  text_field("text", "edit") }),
        table("t", { "name", "value" }, std::move(rows),
              attr("borders", "all")),
        splitter(false, "split"),
        panel("p",
              { tree_node("root", 1,
                          { tree_node("leaf", 2, {},
                                      attr("leaf", true)),
                            selectable("pick me", false, "sel", 3) },
                          attr("open", true)) },
              attr("height", 120.0)),
        tabs("tabs", { tab("one", { label("page one") }),
                       tab("two", { label("page two") }) }) });
}

}  // anonymous namespace


int
main()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplNull_Init();
    install_probe();

    int passed = 0;
    int failed = 0;

    // check
    //   one named section's verdict.
    auto check = [&passed, &failed](const char* _name,
                                    bool        _ok)
    {
        std::printf("  %-34s %s\n", _name, _ok ? "PASS" : "FAIL");

        if (_ok)
        {
            ++passed;
        }
        else
        {
            ++failed;
        }

        return;
    };

    // 1, 2. every archetype draws, frame after frame, with no ID clash
    {
        const imgui_blueprint all = realize<imgui_platform>(everything());
        bool                  clash = false;

        for (int i = 0; i < 4; ++i)
        {
            (void)frame(all);
            clash = ( (clash) ||
                      (id_clash()) );
        }

        check("every archetype draws", true);
        check("no two widgets share an ID", !clash);
    }

    // 3. a tab coming to the front says so
    {
        using namespace ::uxoxo::component;

        option_set front;
        front.set("action", "front");
        front.set("value", 4L);

        const imgui_blueprint pages = realize<imgui_platform>(
            tabs("pages", { tab("first", { label("x") }, false, front) }));
        const std::vector<event> events = frame(pages);
        const event*             seen   = find_event(events, "front");

        check("tab reports coming to the front",
              ( (seen != nullptr) &&
                (seen->payload.as_long("value", 0) == 4L) ));
    }

    // 4. a button's target rides along with its press
    {
        using namespace ::uxoxo::component;

        option_set extra;
        extra.set("action", "press");
        extra.set("target", 7L);
        extra.set("value", "ok");

        const imgui_blueprint ui = realize<imgui_platform>(
            column({ button(label("Press"), true, extra), probe("button") }));

        (void)frame(ui);

        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(centre("button").x, centre("button").y);
        (void)frame(ui);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        (void)frame(ui);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);

        const std::vector<event> events = frame(ui);
        const event*             press  = find_event(events, "press");

        check("target rides along with a press",
              ( (press != nullptr)                                  &&
                (press->payload.as_long("target", 0) == 7L)         &&
                (press->payload.as_string("value", "") == "ok") ));
    }

    // 5. a key keeps a widget's identity as siblings come and go
    {
        using namespace ::uxoxo::component;

        const imgui_blueprint before = realize<imgui_platform>(
            column({ text_field("x", "edit", attr("key", "name")),
                     probe("field") }));
        (void)frame(before);
        const ImGuiID first = probes()["field"].id;

        const imgui_blueprint after = realize<imgui_platform>(
            column({ label("new"),
                     label("rows"),
                     text_field("x", "edit", attr("key", "name")),
                     probe("field") }));
        (void)frame(after);
        const ImGuiID second = probes()["field"].id;

        check("key keeps a widget's identity",
              ( (first != 0) &&
                (first == second) ));
    }

    // 6. a tree node reports opening, with its value and its new state
    {
        using namespace ::uxoxo::component;

        option_set node;
        node.set("toggle_action", "toggle");

        const imgui_blueprint tree = realize<imgui_platform>(
            column({ tree_node("closed", 9, { label("inside") }, node),
                     probe("node") }));
        (void)frame(tree);

        // the arrow sits at the left of the node's row
        const probe_record& record = probes()["node"];
        ImGuiIO&            io     = ImGui::GetIO();

        io.AddMousePosEvent(record.min.x + ImGui::GetFontSize() * 0.5f,
                            (record.min.y + record.max.y) * 0.5f);
        (void)frame(tree);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);

        std::vector<event> events = frame(tree);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);

        const std::vector<event> later = frame(tree);
        events.insert(events.end(), later.begin(), later.end());

        const event* toggled = find_event(events, "toggle");

        check("tree node reports opening",
              ( (toggled != nullptr)                         &&
                (toggled->payload.as_long("value", 0) == 9L) &&
                (toggled->payload.as_bool("open", false)) ));
    }

    // 7. a splitter reports how far it was dragged
    {
        using namespace ::uxoxo::component;

        option_set bar;
        bar.set("length", 200.0);

        const imgui_blueprint ui = realize<imgui_platform>(
            column({ splitter(true, "split", bar), probe("bar") }));
        (void)frame(ui);

        ImGuiIO&     io    = ImGui::GetIO();
        const ImVec2 start = centre("bar");

        io.AddMousePosEvent(start.x, start.y);
        (void)frame(ui);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        (void)frame(ui);
        io.AddMousePosEvent(start.x + 10.0f, start.y);

        const std::vector<event> events = frame(ui);
        const event*             moved  = find_event(events, "split");

        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        (void)frame(ui);

        check("splitter reports its drag",
              ( (moved != nullptr) &&
                (moved->payload.as_double("value", 0.0) == 10.0) ));
    }

    // 8. a backend that knows none of the new archetypes still degrades
    {
        const std::string text =
            realize<platform::ascii_platform>(everything());

        check("ascii degrades on new archetypes",
              (text.find("page one") != std::string::npos));
    }

    // 9. a hole draws as its slot, visibly unclosed
    {
        const imgui_blueprint open = realize<imgui_platform>(
            hole_at("slot"));

        check("a hole draws as its slot", (open.text == "<slot>"));
    }

    ImGui_ImplNull_Shutdown();
    ImGui::DestroyContext();

    std::printf("passed: %d   failed: %d\n", passed, failed);

    return (failed == 0) ? 0 : 1;
}
