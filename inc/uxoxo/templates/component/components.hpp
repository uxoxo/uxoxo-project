/*******************************************************************************
* uxoxo [ui]                                                     components.hpp
*
*   Concrete component types for UI construction.  Every type is a plain struct
* — no base class, no vtable.  The renderer discovers capabilities at compile
* time via the traits defined in component_traits.hpp and dispatches through
* std::visit on the component_var variant.
*
*   Adding a new component type:
*     1. Define the struct here (with appropriate members/signals).
*     2. Add it to component_var.
*     3. Add a lambda in each std::visit call in the renderer.
*     The compiler enforces exhaustiveness — a missing case won't compile.
*
*   Midnight Commander target layout:
*     ┌──────────────────────────────────────────────────────┐
*     │  menu_bar                                            │
*     ├─────────────────────────┬────────────────────────────┤
*     │  file_panel (list_view) │  file_panel (list_view)    │
*     │  in a panel w/ title    │  in a panel w/ title       │
*     ├─────────────────────────┼────────────────────────────┤
*     │  status_bar (per-panel) │  status_bar (per-panel)    │
*     ├─────────────────────────┴────────────────────────────┤
*     │  command_line                                        │
*     ├──────────────────────────────────────────────────────┤
*     │  function_bar (F1–F10)                               │
*     └──────────────────────────────────────────────────────┘
*
*
* path:      /inc/uxoxo/ui/components.hpp 
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.26
*******************************************************************************/

#ifndef  UXOXO_UI_COMPONENTS_
#define  UXOXO_UI_COMPONENTS_ 1

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vectoz`r>

#include <uxoxo>
#include <ui/signal.hpp>
#include <ui/component_traits.hpp>


NS_UXOXO
NS_UI


// ═══════════════════════════════════════════════════════════════════════════════
//  FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════════

struct node;
using  node_ptr  = std::unique_ptr<node>;
using  node_list = std::vector<node_ptr>;


// ═══════════════════════════════════════════════════════════════════════════════
//  ENUMERATIONS
// ═══════════════════════════════════════════════════════════════════════════════

// orientation
//   Used by separators, split_views, scrollbars.
enum class orientation : std::uint8_t
{
    horizontal,
    vertical
};

// text_alignment
//   Horizontal text alignment within a cell/area.
enum class text_alignment : std::uint8_t
{
    left,
    center,
    right
};

// emphasis
//   Semantic emphasis level.  The renderer maps this to concrete style
// (e.g. colors in TUI, font weight in GUI, pitch in VUI).
enum class emphasis : std::uint8_t
{
    normal,
    muted,          // dimmed / de-emphasised
    primary,        // main action / brand color
    secondary,
    success,        // green / positive
    warning,        // yellow / caution
    danger,         // red / destructive
    info            // blue / informational
};

// sort_order
//   Column sort state for list_view.
enum class sort_order : std::uint8_t
{
    none,
    ascending,
    descending
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §1  STATIC CONTENT
// ═══════════════════════════════════════════════════════════════════════════════

// label
//   Static text display.  The simplest possible component.
struct label
{
    static constexpr bool focusable = false;

    std::string    content;
    text_alignment alignment = text_alignment::left;
    emphasis       emph      = emphasis::normal;
};

/*****************************************************************************/

// heading
//   Section heading with a level (1 = largest).
struct heading
{
    static constexpr bool focusable = false;

    std::string content;
    int         heading_level = 1;
    emphasis    emph          = emphasis::primary;
};

/*****************************************************************************/

// separator
//   Visual divider line.  No content, no children, not focusable.
struct separator
{
    static constexpr bool focusable = false;

    orientation orient = orientation::horizontal;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  INTERACTIVE COMPONENTS
// ═══════════════════════════════════════════════════════════════════════════════

// button
//   Pressable action trigger with a text label.
struct button
{
    static constexpr bool focusable = true;

    std::string label;
    bool        enabled = true;
    emphasis    emph    = emphasis::primary;

    signal<>    activated;
};

/*****************************************************************************/

// textbox
//   Single-line editable text input.
struct textbox
{
    static constexpr bool focusable = true;

    std::string value;
    int         cursor = 0;
    std::string placeholder;
    bool        enabled = true;
    bool        masked  = false;    // password dots

    signal<>                     activated;       // Enter pressed
    signal<const std::string&>   value_changed;
};

/*****************************************************************************/

// checkbox
//   Boolean toggle with a text label.
struct checkbox
{
    static constexpr bool focusable = true;

    std::string label;
    bool        checked = false;
    bool        enabled = true;

    signal<bool> toggled;
};

/*****************************************************************************/

// radio_group
//   One-of-N selection from a fixed set of options.
struct radio_group
{
    static constexpr bool focusable = true;

    std::vector<std::string> options;
    int                      selected = 0;
    bool                     enabled  = true;
    orientation              orient   = orientation::vertical;

    signal<int>              selection_changed;
};

/*****************************************************************************/

// progress_bar
//   Bounded completion indicator.  value ∈ [0.0, 1.0].
struct progress_bar
{
    static constexpr bool focusable = false;

    float    value = 0.0f;
    emphasis emph  = emphasis::info;
};

/*****************************************************************************/

// scrollbar
//   Scroll position indicator.  value ∈ [0.0, 1.0].
// Typically rendered alongside a scrollable component, not standalone.
struct scrollbar
{
    static constexpr bool focusable = false;

    float       value  = 0.0f;
    float       thumb  = 0.1f;  // visible fraction (thumb size)
    orientation orient = orientation::vertical;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  LIST / TABLE VIEW
// ═══════════════════════════════════════════════════════════════════════════════
//   The core of Midnight Commander's file panels.

// column_def
//   Describes a single column in a list_view.
struct column_def
{
    std::string    name;
    int            width = 0;       // 0 = flex
    float          flex  = 1.0f;    // proportional weight when width == 0
    text_alignment align = text_alignment::left;
    sort_order     sort  = sort_order::none;
};

// list_row
//   A single row in a list_view.  cells[i] corresponds to columns[i].
struct list_row
{
    std::vector<std::string> cells;
    bool                     selected  = false;  // multi-select mark
    bool                     directory = false;  // icon hint for MC
};

// list_view
//   Scrollable, selectable item list with optional columns.
// This is the file-panel engine.
struct list_view
{
    static constexpr bool focusable  = true;
    static constexpr bool scrollable = true;

    std::vector<column_def> columns;
    std::vector<list_row>   items;
    int                     selected      = 0;
    int                     scroll_offset = 0;
    bool                    multi_select  = false;
    bool                    show_header   = true;

    signal<>    activated;           // Enter on selected item
    signal<int> selection_changed;   // cursor moved
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  CONTAINERS
// ═══════════════════════════════════════════════════════════════════════════════

// container
//   Generic grouping.  No border, no title.  Children laid out by direction.
struct container
{
    static constexpr bool focusable = false;

    node_list   children;
    orientation orient = orientation::vertical;
    int         gap    = 0;
};

/*****************************************************************************/

// panel
//   Bordered container with a title.  The fundamental framing element for
// MC's twin file panels.
struct panel
{
    static constexpr bool focusable = false;

    std::string title;
    node_list   children;
    emphasis    emph   = emphasis::normal;
};

/*****************************************************************************/

// split_view
//   Divides available space between exactly two children.  ratio ∈ (0.0, 1.0)
// describes the fraction given to the first child.
struct split_view
{
    static constexpr bool focusable = false;

    node_list   children;            // expects exactly 2
    orientation orient = orientation::horizontal;
    float       ratio  = 0.5f;
};

/*****************************************************************************/

// tab_bar
//   Tabbed container.  items[i] is the tab label, children[i] is the body.
struct tab_bar
{
    static constexpr bool focusable = true;

    std::vector<std::string> items;
    int                      selected = 0;
    node_list                children;   // one child per tab

    signal<int> selection_changed;
};

/*****************************************************************************/

// dialog
//   Modal (or modeless) overlay with title, body, and confirm/cancel.
struct dialog
{
    static constexpr bool focusable = true;

    std::string title;
    node_list   children;
    bool        modal   = true;
    int         width   = 0;     // 0 = auto
    int         height  = 0;     // 0 = auto

    signal<>    submitted;
    signal<>    cancelled;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  MIDNIGHT-COMMANDER–SPECIFIC COMPOSITES
// ═══════════════════════════════════════════════════════════════════════════════

// function_slot
//   A single F-key entry: "F5" → "Copy".
struct function_slot
{
    int         key_number = 0;     // 1–12
    std::string label;
    bool        enabled = true;

    signal<>    activated;
};

// function_bar
//   The bottom F-key strip:  1Help  2Menu  3View  ...  10Quit
struct function_bar
{
    static constexpr bool focusable = false;

    std::array<function_slot, 10> slots = {};

    // convenience: set a slot by F-key number (1-based)
    void set(int fkey, std::string label_text)
    {
        if (fkey >= 1 && fkey <= 10)
        {
            slots[fkey - 1].key_number = fkey;
            slots[fkey - 1].label = std::move(label_text);
        }
    }
};

/*****************************************************************************/

// menu_bar
//   Horizontal menu strip across the top.  Each item may have a drop-down
// submenu (handled by a separate menu system — see menu.hpp).
struct menu_bar
{
    static constexpr bool focusable = true;

    std::vector<std::string> items;
    int                      selected = 0;
    bool                     active   = false;   // drop-down open?

    signal<>    activated;           // Enter / click on selected item
    signal<int> selection_changed;
};

/*****************************************************************************/

// status_bar
//   Single-line information strip.  Supports multi-segment display:
//     "2 dirs, 3 files  │  12.5 KB  │  /home/user"
struct status_segment
{
    std::string    content;
    text_alignment align = text_alignment::left;
    float          flex  = 1.0f;
    emphasis       emph  = emphasis::muted;
};

struct status_bar
{
    static constexpr bool focusable = false;

    std::string                 content;     // simple single-string mode
    std::vector<status_segment> segments;    // multi-segment mode
};

/*****************************************************************************/

// command_line
//   Prompt + editable input at the bottom of MC.
//     user@host:/home $ ls -la_
struct command_line
{
    static constexpr bool focusable = true;

    std::string prompt;      // "user@host:/home $ "
    std::string value;
    int         cursor = 0;

    // history support
    std::vector<std::string> history;
    int                      history_pos = -1;

    signal<>                     activated;       // Enter
    signal<const std::string&>   value_changed;
    signal<const std::string&>   submitted;       // Enter (with value)
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §6  LAYOUT HINTS
// ═══════════════════════════════════════════════════════════════════════════════

// size_hint
//   Abstract sizing intent.  The renderer interprets these in its native
// coordinate system (cells for TUI, pixels for GUI).
struct size_hint
{
    std::optional<int> preferred;
    std::optional<int> min;
    std::optional<int> max;
    float              flex = 0.0f;

    static size_hint fixed(int n)                      { return { n, n, n, 0.0f }; }
    static size_hint flex_w(float w = 1.0f)            { return { {}, {}, {}, w }; }
    static size_hint at_least(int n, float w = 1.0f)   { return { {}, n, {}, w }; }
    static size_hint between(int lo, int hi, float w)  { return { {}, lo, hi, w }; }
};

// edges
//   Insets (padding / margin).
struct edges
{
    int top = 0, right = 0, bottom = 0, left = 0;

    static edges all(int n)              { return { n, n, n, n }; }
    static edges symmetric(int v, int h) { return { v, h, v, h }; }
    static edges none()                  { return {}; }
};

// layout
//   Combined layout hint attached to each node.
struct layout
{
    size_hint width;
    size_hint height;
    edges     padding;
    edges     margin;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §7  COMPONENT VARIANT & NODE
// ═══════════════════════════════════════════════════════════════════════════════
//   The closed set of all component types.  Adding a new type means adding it
// here — the compiler will emit errors at every std::visit call site that
// doesn't handle the new alternative.

using component_var = std::variant<
    // static
    label,
    heading,
    separator,
    // interactive
    button,
    textbox,
    checkbox,
    radio_group,
    // data
    list_view,
    progress_bar,
    scrollbar,
    // containers
    container,
    panel,
    split_view,
    tab_bar,
    dialog,
    // MC composites
    menu_bar,
    function_bar,
    status_bar,
    command_line
>;


/*****************************************************************************/

// node
//   A single node in the UI tree.  Holds:
//     - The component data (variant)
//     - Layout sizing hints
//     - An optional string id for lookup
//
//   No base class, no vtable. Dispatch is via std::visit on the variant.
struct node
{
    component_var component;
    layout        lay;
    std::string   id;

    // ── tree access ──────────────────────────────────────────────────────

    // children_ptr
    //   Returns a pointer to the node_list if this is a container type,
    // nullptr otherwise.  Dispatched at compile time per alternative.
    node_list* children_ptr()
    {
        return std::visit(component_traits::overloaded{
            [](container&  c) -> node_list* { return &c.children; },
            [](panel&      c) -> node_list* { return &c.children; },
            [](split_view& c) -> node_list* { return &c.children; },
            [](tab_bar&    c) -> node_list* { return &c.children; },
            [](dialog&     c) -> node_list* { return &c.children; },
            [](auto&)         -> node_list* { return nullptr; }
        }, component);
    }

    const node_list* children_ptr() const
    {
        return std::visit(component_traits::overloaded{
            [](const container&  c) -> const node_list* { return &c.children; },
            [](const panel&      c) -> const node_list* { return &c.children; },
            [](const split_view& c) -> const node_list* { return &c.children; },
            [](const tab_bar&    c) -> const node_list* { return &c.children; },
            [](const dialog&     c) -> const node_list* { return &c.children; },
            [](const auto&)         -> const node_list* { return nullptr; }
        }, component);
    }

    void add_child(node_ptr child)
    {
        auto* kids = children_ptr();
        if (kids)
        {
            kids->push_back(std::move(child));
        }
    }

    // focusable
    //   Query the static constexpr focusable flag on the held component.
    [[nodiscard]] bool focusable() const
    {
        return std::visit([](const auto& c) -> bool {
            using T = std::decay_t<decltype(c)>;
            if constexpr (component_traits::is_focusable_v<T>)
                return true;
            else
            {
                return false;
            }
        }, component);
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §8  TREE TRAVERSAL
// ═══════════════════════════════════════════════════════════════════════════════

// walk
//   Depth-first traversal.  Calls fn(node&) for every node in the tree.
template <typename _Fn>
void walk(node& root, _Fn&& fn)
{
    fn(root);
    if (auto* kids = root.children_ptr())
    {
        for (auto& child : *kids)
    }
            if (child)
            {
                walk(*child, fn);
            }
}

// collect_focusable
//   Returns all focusable nodes in tree order (for Tab cycling).
inline std::vector<node*> collect_focusable(node& root)
{
    std::vector<node*> result;
    walk(root, [&](node& n) {
        if (n.focusable())
        {
            result.push_back(&n);
        }
    });
    return result;
}

// find_by_id
//   Locate a node by its string id.
inline node* find_by_id(node& root, const std::string& target)
{
    node* found = nullptr;
    walk(root, [&](node& n) {
        if (!found && n.id == target)
        {
            found = &n;
        }
    });
    return found;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §9  BUILDER HELPERS
// ═══════════════════════════════════════════════════════════════════════════════
//   Named constructors that build a node with sensible defaults.
//   Each returns node_ptr (unique_ptr<node>).

namespace build {

    template <typename _Comp>
    node_ptr make(
        _Comp comp, 
        layout lay = {},
        std::string nid = ""
    )
    {
        auto n = std::make_unique<node>();
        n->component = std::move(comp);
        n->lay       = std::move(lay);
        n->id        = std::move(nid);
        return n;
    }

    // ── static content ───────────────────────────────────────────────────

    inline node_ptr lbl(
        std::string text,
        emphasis e = emphasis::normal
    )
    {
        return make(label{ std::move(text), text_alignment::left, e });
    }

    inline node_ptr hdg(
        std::string text,
        int level = 1
    )
    {
        return make(heading{ std::move(text), level });
    }

    inline node_ptr sep(orientation o = orientation::horizontal)
    {
        return make(separator{ o });
    }

    // ── interactive ──────────────────────────────────────────────────────

    inline node_ptr btn(
        std::string text,
        emphasis e = emphasis::primary
    )
    {
        return make(button{ std::move(text), true, e, {} });
    }

    inline node_ptr input(
        std::string placeholder = "",
        std::string nid = ""
    )
    {
        return make(
            textbox{ "", 0, std::move(placeholder), true, false, {}, {} },
            {}, std::move(nid)
        );
    }

    inline node_ptr chk(std::string text, bool initial = false)
    {
        return make(checkbox{ std::move(text), initial, true, {} });
    }

    inline node_ptr radio(
        std::vector<std::string> opts,
        int initial = 0
    )
    {
        return make(radio_group{ std::move(opts), initial, true, 
                                 orientation::vertical, {} });
    }

    inline node_ptr pbar(float val = 0.0f)
    {
        return make(progress_bar{ val });
    }

    // ── containers ───────────────────────────────────────────────────────

    inline node_ptr vbox(int gap = 0)
    {
        return make(container{ {}, orientation::vertical, gap });
    }

    inline node_ptr hbox(int gap = 0)
    {
        return make(container{ {}, orientation::horizontal, gap });
    }

    inline node_ptr frm(
        std::string title_text,
        emphasis e = emphasis::normal
    )
    {
        return make(panel{ std::move(title_text), {}, e });
    }

    inline node_ptr hsplit(float r = 0.5f)
    {
        return make(split_view{ {}, orientation::horizontal, r });
    }

    inline node_ptr vsplit(float r = 0.5f)
    {
        return make(split_view{ {}, orientation::vertical, r });
    }

    inline node_ptr dlg(
        std::string title_text,
        bool is_modal = true
    )
    {
        return make(dialog{ std::move(title_text), {}, is_modal, 0, 0, {}, {} });
    }

    // ── MC composites ────────────────────────────────────────────────────

    inline node_ptr file_list(
        std::vector<column_def> cols = {},
        std::string nid = ""
    )
    {
        if (cols.empty())
        {
            cols = {
                { "Name",     0,   2.0f, text_alignment::left  },
                { "Size",     8,   0.0f, text_alignment::right },
                { "Modified", 12,  0.0f, text_alignment::left  }
            };
        }
        return make(
            list_view{ std::move(cols), {}, 0, 0, false, true, {}, {} },
            {}, std::move(nid)
        );
    }

    inline node_ptr mbar(std::vector<std::string> entries)
    {
        return make(menu_bar{ std::move(entries), 0, false, {}, {} });
    }

    inline node_ptr fbar()
    {
        function_bar fb;
        fb.set(1,  "Help");   fb.set(2,  "Menu");
        fb.set(3,  "View");   fb.set(4,  "Edit");
        fb.set(5,  "Copy");   fb.set(6,  "Move");
        fb.set(7,  "Mkdir");  fb.set(8,  "Del");
        fb.set(9,  "PullDn"); fb.set(10, "Quit");
        return make(std::move(fb));
    }

    inline node_ptr sbar(std::string text = "")
    {
        return make(status_bar{ std::move(text), {} });
    }

    inline node_ptr cmdline(std::string prompt_text = "$ ")
    {
        return make(command_line{
            std::move(prompt_text), "", 0, {}, -1, {}, {}, {}
        });
    }

}   // namespace build


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §10  MIDNIGHT COMMANDER FACTORY
// ═══════════════════════════════════════════════════════════════════════════════
//   Assembles the canonical MC layout as a node tree in one call.

namespace mc {

    // build_layout
    //   Returns the complete MC screen tree:
    //     vbox
    //     ├── menu_bar
    //     ├── hsplit (0.5)
    //     │   ├── vbox (left file panel)
    //     │   │   ├── panel "~"
    //     │   │   │   └── list_view
    //     │   │   └── status_bar
    //     │   └── vbox (right file panel)
    //     │       ├── panel "~"
    //     │       │   └── list_view
    //     │       └── status_bar
    //     ├── command_line
    //     └── function_bar
    inline node_ptr build_layout()
    {
        using namespace build;

        // left file panel
        auto left_list   = file_list({}, "left_list");
        auto left_panel  = frm("~", emphasis::normal);
        left_panel->add_child(std::move(left_list));

        auto left_status = sbar("0 files");
        left_status->id  = "left_status";

        auto left = vbox();
        left->lay.width = size_hint::flex_w(1.0f);
        left->add_child(std::move(left_panel));
        left->add_child(std::move(left_status));

        // right file panel
        auto right_list   = file_list({}, "right_list");
        auto right_panel  = frm("~", emphasis::normal);
        right_panel->add_child(std::move(right_list));

        auto right_status = sbar("0 files");
        right_status->id  = "right_status";

        auto right = vbox();
        right->lay.width = size_hint::flex_w(1.0f);
        right->add_child(std::move(right_panel));
        right->add_child(std::move(right_status));

        // twin panels
        auto twin = hsplit(0.5f);
        twin->lay.height = size_hint::flex_w(1.0f);
        twin->add_child(std::move(left));
        twin->add_child(std::move(right));

        // menu bar
        auto mb = mbar({ "Left", "File", "Command", "Options", "Right" });
        mb->lay.height = size_hint::fixed(1);
        mb->id = "menu_bar";

        // command line
        auto cl = cmdline("user@host:~ $ ");
        cl->lay.height = size_hint::fixed(1);
        cl->id = "cmdline";

        // function bar
        auto fb = fbar();
        fb->lay.height = size_hint::fixed(1);
        fb->id = "fbar";

        // root
        auto root = vbox();
        root->add_child(std::move(mb));
        root->add_child(std::move(twin));
        root->add_child(std::move(cl));
        root->add_child(std::move(fb));

        return root;
    }

}   // namespace mc


NS_END  // ui
NS_END  // uxoxo

#endif  // UXOXO_UI_COMPONENTS_
