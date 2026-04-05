/*******************************************************************************
* uxoxo [template]                                              ui_template.hpp
*
*   Framework-agnostic UI template.  Holds a complete, abstract description
* of a user interface — structure, events, shortcuts, focus policy, and
* theming hints — with zero knowledge of any specific rendering backend.
*
*   The template does as much work as possible at the abstract level:
*     - Builds and owns the node tree
*     - Registers key bindings as abstract (modifier, key) pairs
*     - Manages focus order from the tree's focusable nodes
*     - Stores event handler bindings (node_id × signal_name → callback)
*     - Resolves strategy enums at compile time via template_traits
*
*   To produce a concrete UI, call realize<Backend>(backend):
*     1. static_assert that Backend models the required capability level
*     2. Query template_traits for menu/scrollbar/dialog/color strategies
*     3. Apply key bindings via the backend's shortcut API (if available)
*     4. Walk the node tree and delegate rendering to the backend
*     5. Connect signals to registered handlers
*     6. Enter the backend's event loop
*
*   This header includes:
*     - key_modifier / key_code enumerations
*     - key_chord:     abstract key combination
*     - shortcut:      key_chord bound to a named action
*     - event_binding: signal connection descriptor
*     - focus_policy:  Tab-order management
*     - ui_template:   the central template class
*

* path:      /inc/uxoxo/template/ui_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.28
*******************************************************************************/

#ifndef  UXOXO_TEMPLATES_
#define  UXOXO_TEMPLATES_ 1

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <uxoxo>
#include <ui/signal.hpp>
#include <ui/component_traits.hpp>
#include <ui/components.hpp>
#include <ui/ui_template_traits.hpp>


NS_UXOXO
NS_TEMPLATES


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  KEY REPRESENTATION
// ═══════════════════════════════════════════════════════════════════════════════
//   Abstract, modality-independent key and modifier representation.
// Backends translate these to their native key codes.
//   - TUI: ncurses KEY_* constants or FTXUI Event::*
//   - GUI: Qt::Key_*, GDK_KEY_*, VK_*
//   - Web: KeyboardEvent.code strings
//   - VUI: voice command tokens

// key_modifier
//   Bitfield of modifier keys.  Combine with bitwise OR.
enum key_modifier : unsigned
{
    mod_none    = 0,
    mod_ctrl    = 1u << 0,
    mod_alt     = 1u << 1,
    mod_shift   = 1u << 2,
    mod_meta    = 1u << 3      // Windows key, Cmd on macOS
};

constexpr key_modifier operator|(key_modifier a, key_modifier b) noexcept
{
    return static_cast<key_modifier>(
        static_cast<unsigned>(a) |
        static_cast<unsigned>(b)
    );
}

constexpr key_modifier operator&(key_modifier a, key_modifier b) noexcept
{
    return static_cast<key_modifier>(
        static_cast<unsigned>(a) &
        static_cast<unsigned>(b)
    );
}

// key_code
//   Abstract key identifiers for commonly-bound keys.  Printable ASCII
// characters are represented by their char value cast to int.  This enum
// covers navigation and function keys that vary across frameworks.
enum class key_code : int
{
    // navigation
    up          = 0x100,
    down        = 0x101,
    left        = 0x102,
    right       = 0x103,
    home        = 0x104,
    end         = 0x105,
    page_up     = 0x106,
    page_down   = 0x107,

    // editing
    backspace   = 0x108,
    del         = 0x109,    // 'delete' is a keyword
    insert      = 0x10A,
    tab         = 0x10B,

    // action
    enter       = 0x10C,
    escape      = 0x10D,
    space       = 0x10E,

    // function keys
    f1  = 0x110, f2  = 0x111, f3  = 0x112, f4  = 0x113,
    f5  = 0x114, f6  = 0x115, f7  = 0x116, f8  = 0x117,
    f9  = 0x118, f10 = 0x119, f11 = 0x11A, f12 = 0x11B
};


/*****************************************************************************/

// key_chord
//   A single key press with modifiers.  The fundamental unit of a shortcut
// binding.  Framework-agnostic — each backend translates to native codes.
struct key_chord
{
    key_modifier mods = mod_none;
    key_code     key  = key_code::escape;

    // from_char
    //   Convenience constructor for printable ASCII shortcuts.
    //   Usage: key_chord::from_char('s', mod_ctrl)  →  Ctrl+S
    static key_chord from_char(char c, key_modifier m = mod_none)
    {
        return { m, static_cast<key_code>(static_cast<int>(c)) };
    }

    bool operator==(const key_chord& o) const noexcept
    {
        return ( (mods == o.mods) &&
                 (key  == o.key) );
    }

    bool operator!=(const key_chord& o) const noexcept
    {
        return !(*this == o);
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  SHORTCUT BINDING
// ═══════════════════════════════════════════════════════════════════════════════

// shortcut
//   Associates a key_chord with a named action and a callable handler.
// The action_id is used for serialisation, logging, and backend-agnostic
// command dispatch.
struct shortcut
{
    std::string              action_id;    // e.g. "file.copy", "edit.undo"
    key_chord                chord;
    std::function<void()>    handler;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  EVENT BINDING
// ═══════════════════════════════════════════════════════════════════════════════
//   Describes a connection between a node's signal (identified by node id +
// signal name) and a handler function.  The template stores these abstractly;
// realize() wires them to actual signal::connect() calls on the live tree.

// event_binding
//   An abstract signal → handler link.
struct event_binding
{
    std::string               node_id;       // target node in the tree
    std::string               signal_name;   // "activated", "value_changed", etc.
    std::function<void()>     handler;       // type-erased callback
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  FOCUS POLICY
// ═══════════════════════════════════════════════════════════════════════════════

// focus_order
//   enum: how the template determines Tab-cycle order.
enum class focus_order : std::uint8_t
{
    tree_order,     // depth-first walk of node tree (default)
    explicit_list,  // manually specified id sequence
    none            // no managed focus (backend handles it)
};

// focus_policy
//   Focus management configuration.  The template pre-computes the
// focusable node list from the tree; the backend may override it.
struct focus_policy
{
    focus_order                 order    = focus_order::tree_order;
    bool                       wrap     = true;     // wrap around at ends
    std::vector<std::string>   explicit_ids;        // for explicit_list mode

    // resolve
    //   Given the root node, produce the ordered focus id list.
    // For tree_order mode, walks the tree and collects focusable node ids.
    // For explicit_list, returns explicit_ids as-is.
    std::vector<std::string> resolve(const node& root) const
    {
        if (order == focus_order::explicit_list)
        {
            return explicit_ids;
        }

        // tree_order: depth-first collection
        std::vector<std::string> result;
        collect_focus_ids(root, result);

        return result;
    }

private:
    static void collect_focus_ids(const node&              n,
                                  std::vector<std::string>& out)
    {
        // check if this node is focusable and has an id
        if ( n.focusable() &&
             !n.id.empty() )
        {
            out.push_back(n.id);
        }

        // recurse into children
        const node_list* kids = n.children_ptr();
        if (kids)
        {
            for (const auto& child : *kids)
            {
                if (child)
                {
                    collect_focus_ids(*child, out);
                }
            }
        }

        return;
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  TEMPLATE WINDOW HINTS
// ═══════════════════════════════════════════════════════════════════════════════
//   Abstract sizing and positioning intent for the root window/screen.
// Backends interpret these in their native coordinate system.

// window_hints
//   Root viewport configuration.  All fields are optional; the backend
// uses sensible defaults when a field is not set.
struct window_hints
{
    std::string title;

    int  preferred_width  = 0;      // 0 = backend default / fullscreen
    int  preferred_height = 0;
    int  min_width        = 0;
    int  min_height       = 0;
    bool resizable        = true;
    bool fullscreen       = false;  // TUI: use full terminal
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §6  UI TEMPLATE
// ═══════════════════════════════════════════════════════════════════════════════
//   The central class.  Holds everything needed to describe a UI abstractly.
// No rendering code.  No framework dependency.  Just data and policy.
//
//   Usage:
//     ui_template tmpl;
//     tmpl.set_root(mc::build_layout());
//     tmpl.set_title("uxoxo");
//     tmpl.bind_key("app.quit", { mod_ctrl, key_code::q }, [&]{ quit(); });
//     tmpl.bind_event("cmdline", "submitted", [&]{ run_command(); });
//     tmpl.realize(my_backend);

class ui_template
{
public:
    // ── construction ─────────────────────────────────────────────────────

    ui_template() = default;

    explicit ui_template(node_ptr root_node)
        : m_root(std::move(root_node))
    {}

    // ── root tree ────────────────────────────────────────────────────────

    // set_root
    //   Set the root node of the UI tree.  Replaces any existing tree.
    void set_root(node_ptr root_node)
    {
        m_root = std::move(root_node);

        return;
    }

    // root
    //   Access the root node.
    [[nodiscard]] node* root() noexcept { return m_root.get(); }
    [[nodiscard]] const node* root() const noexcept { return m_root.get(); }

    // ── window hints ─────────────────────────────────────────────────────

    void set_title(std::string t)
    {
        m_hints.title = std::move(t);

        return;
    }

    void set_window_hints(window_hints h)
    {
        m_hints = std::move(h);

        return;
    }

    [[nodiscard]] const window_hints& hints() const noexcept
    {
        return m_hints;
    }

    // ── shortcuts ────────────────────────────────────────────────────────

    // bind_key
    //   Register an abstract keyboard shortcut.
    void bind_key(std::string             action_id,
                  key_chord               chord,
                  std::function<void()>   handler)
    {
        m_shortcuts.push_back({
            std::move(action_id),
            chord,
            std::move(handler)
        });

        return;
    }

    [[nodiscard]] const std::vector<shortcut>& shortcuts() const noexcept
    {
        return m_shortcuts;
    }

    // ── event bindings ───────────────────────────────────────────────────

    // bind_event
    //   Register an abstract signal handler for a node.
    void bind_event(std::string             node_id,
                    std::string             signal_name,
                    std::function<void()>   handler)
    {
        m_bindings.push_back({
            std::move(node_id),
            std::move(signal_name),
            std::move(handler)
        });

        return;
    }

    [[nodiscard]] const std::vector<event_binding>& bindings() const noexcept
    {
        return m_bindings;
    }

    // ── focus ────────────────────────────────────────────────────────────

    void set_focus_policy(focus_policy fp)
    {
        m_focus = std::move(fp);

        return;
    }

    [[nodiscard]] const focus_policy& focus() const noexcept
    {
        return m_focus;
    }

    // resolve_focus_order
    //   Pre-compute the focus id list from the current tree.
    [[nodiscard]] std::vector<std::string> resolve_focus_order() const
    {
        if (!m_root)
        {
            return {};
        }

        return m_focus.resolve(*m_root);
    }

    /***********************************************************************/

    // ── realization ──────────────────────────────────────────────────────
    //   realize() is the bridge between abstract and concrete.  It takes
    // a backend by reference, queries its capabilities at compile time,
    // and performs all framework-agnostic setup before delegating to the
    // backend's rendering methods.

    // realize
    //   Bind this template to a concrete backend and produce a live UI.
    template <typename _Backend>
    void realize(_Backend& backend) const
    {
        using traits = template_traits::detail;

        // enforce minimum capability
        static_assert(
            template_traits::can_render_v<_Backend>,
            "Backend must provide render_node() or render_component()."
        );
        static_assert(
            template_traits::has_event_loop_v<_Backend>,
            "Backend must provide a run() method for the event loop."
        );

        if (!m_root)
        {
            return;
        }

        // ── phase 1: window configuration ────────────────────────────
        apply_window_hints(backend);

        // ── phase 2: key bindings ────────────────────────────────────
        apply_shortcuts(backend);

        // ── phase 3: focus order ─────────────────────────────────────
        apply_focus_order(backend);

        // ── phase 4: event wiring ────────────────────────────────────
        wire_events(backend);

        // ── phase 5: initial render ──────────────────────────────────
        render_tree(backend);

        // ── phase 6: enter event loop ────────────────────────────────
        backend.run();

        return;
    }


private:

    // ── apply_window_hints ───────────────────────────────────────────────
    //   Forwards abstract window hints to the backend, guarded by
    // capability detection.
    template <typename _Backend>
    void apply_window_hints(_Backend& backend) const
    {
        // set window/terminal title if backend supports it
        if constexpr (template_traits::has_set_title_v<_Backend>)
        {
            if (!m_hints.title.empty())
            {
                backend.set_title(m_hints.title);
            }
        }

        // set window size if backend supports it and we have preferences
        if constexpr (template_traits::has_set_size_v<_Backend>)
        {
            if ( (m_hints.preferred_width  > 0) &&
                 (m_hints.preferred_height > 0) )
            {
                backend.set_size(m_hints.preferred_width,
                                 m_hints.preferred_height);
            }
        }

        return;
    }

    // ── apply_shortcuts ──────────────────────────────────────────────────
    //   Translates abstract key chords to backend-native shortcut
    // registrations.  Falls back to noting them for manual dispatch
    // if the backend lacks register_shortcut().
    template <typename _Backend>
    void apply_shortcuts(_Backend& backend) const
    {
        if constexpr (template_traits::has_register_shortcut_v<_Backend>)
        {
            for (const auto& sc : m_shortcuts)
            {
                backend.register_shortcut(
                    static_cast<int>(sc.chord.mods),
                    static_cast<int>(sc.chord.key),
                    sc.handler
                );
            }
        }
        // else: shortcuts stored in m_shortcuts for manual polling
        // by backends that process raw input events

        return;
    }

    // ── apply_focus_order ────────────────────────────────────────────────
    //   Pre-computes focus traversal and informs the backend.
    template <typename _Backend>
    void apply_focus_order(_Backend& backend) const
    {
        if constexpr (template_traits::has_set_focus_v<_Backend>)
        {
            auto order = m_focus.resolve(*m_root);
            if (!order.empty())
            {
                // set initial focus to the first focusable node
                backend.set_focus(order.front());
            }
        }

        return;
    }

    // ── wire_events ──────────────────────────────────────────────────────
    //   Walks the binding list, locates each target node by id, and
    // connects the handler to the appropriate signal.  This is where
    // the type-erased event_binding meets the strongly-typed signal<>.
    //
    //   The connection is made via std::visit on the component variant,
    // matching signal_name to the concrete signal member.  Unknown
    // signal names are silently ignored (the binding may be for a
    // backend-specific extension).
    template <typename _Backend>
    void wire_events([[maybe_unused]] _Backend& backend) const
    {
        for (const auto& binding : m_bindings)
        {
            node* target = find_by_id(*m_root, binding.node_id);
            if (!target)
            {
                continue;
            }

            connect_signal(*target, binding.signal_name, binding.handler);
        }

        return;
    }

    // connect_signal
    //   Matches a signal name string to the actual signal<> member on the
    // component variant and connects the handler.
    static void connect_signal(node&                          n,
                               const std::string&             signal_name,
                               const std::function<void()>&   handler)
    {
        std::visit(component_traits::overloaded{

            // button — activated
            [&](button& c) {
                if (signal_name == "activated")
                {
                    c.activated.connect([handler]() { handler(); });
                }
            },

            // textbox — activated, value_changed
            [&](textbox& c) {
                if (signal_name == "activated")
                {
                    c.activated.connect([handler]() { handler(); });
                }
                else if (signal_name == "value_changed")
                {
                    c.value_changed.connect(
                        [handler](const std::string&) { handler(); });
                }
            },

            // checkbox — toggled
            [&](checkbox& c) {
                if (signal_name == "toggled")
                {
                    c.toggled.connect([handler](bool) { handler(); });
                }
            },

            // radio_group — selection_changed
            [&](radio_group& c) {
                if (signal_name == "selection_changed")
                {
                    c.selection_changed.connect(
                        [handler](int) { handler(); });
                }
            },

            // list_view — activated, selection_changed
            [&](list_view& c) {
                if (signal_name == "activated")
                {
                    c.activated.connect([handler]() { handler(); });
                }
                else if (signal_name == "selection_changed")
                {
                    c.selection_changed.connect(
                        [handler](int) { handler(); });
                }
            },

            // menu_bar — activated, selection_changed
            [&](menu_bar& c) {
                if (signal_name == "activated")
                {
                    c.activated.connect([handler]() { handler(); });
                }
                else if (signal_name == "selection_changed")
                {
                    c.selection_changed.connect(
                        [handler](int) { handler(); });
                }
            },

            // tab_bar — selection_changed
            [&](tab_bar& c) {
                if (signal_name == "selection_changed")
                {
                    c.selection_changed.connect(
                        [handler](int) { handler(); });
                }
            },

            // dialog — submitted, cancelled
            [&](dialog& c) {
                if (signal_name == "submitted")
                {
                    c.submitted.connect([handler]() { handler(); });
                }
                else if (signal_name == "cancelled")
                {
                    c.cancelled.connect([handler]() { handler(); });
                }
            },

            // command_line — activated, value_changed, submitted
            [&](command_line& c) {
                if (signal_name == "activated")
                {
                    c.activated.connect([handler]() { handler(); });
                }
                else if (signal_name == "value_changed")
                {
                    c.value_changed.connect(
                        [handler](const std::string&) { handler(); });
                }
                else if (signal_name == "submitted")
                {
                    c.submitted.connect(
                        [handler](const std::string&) { handler(); });
                }
            },

            // everything else — no signals to wire
            [](auto&) {}

        }, n.component);

        return;
    }

    // ── render_tree ──────────────────────────────────────────────────────
    //   Delegates the full node tree to the backend's renderer.  Uses
    // if constexpr to select the best available render method.
    template <typename _Backend>
    void render_tree(_Backend& backend) const
    {
        // use frame-based rendering if available
        if constexpr (template_traits::uses_frame_rendering_v<_Backend>)
        {
            backend.begin_frame();
        }

        // prefer render_node (whole-tree) over render_component (per-node)
        if constexpr (template_traits::has_render_node_v<_Backend>)
        {
            backend.render_node(*m_root);
        }
        else if constexpr (template_traits::has_render_component_v<_Backend>)
        {
            render_walk(backend, *m_root);
        }

        if constexpr (template_traits::uses_frame_rendering_v<_Backend>)
        {
            backend.end_frame();
        }

        return;
    }

    // render_walk
    //   Depth-first render via render_component when render_node is
    // unavailable.
    template <typename _Backend>
    static void render_walk(_Backend& backend, const node& n)
    {
        backend.render_component(n.component);

        const node_list* kids = n.children_ptr();
        if (kids)
        {
            for (const auto& child : *kids)
            {
                if (child)
                {
                    render_walk(backend, *child);
                }
            }
        }

        return;
    }


    /***********************************************************************/

    // ── data ─────────────────────────────────────────────────────────────

    node_ptr                    m_root;
    window_hints                m_hints;
    std::vector<shortcut>       m_shortcuts;
    std::vector<event_binding>  m_bindings;
    focus_policy                m_focus;
    scoped_connections          m_connections;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §7  COMPILE-TIME STRATEGY REPORT
// ═══════════════════════════════════════════════════════════════════════════════
//   A diagnostic trait struct that summarises all resolved strategies for
// a given backend.  Useful for static_assert diagnostics and debugging.
//
//   Usage:
//     using report = strategy_report<my_backend>;
//     static_assert(report::menu == menu_strategy::native, "...");

template <typename _Backend>
struct strategy_report
{
    using backend_type = _Backend;

    static constexpr template_traits::modality            modality   =
        template_traits::backend_modality_v<_Backend>;
    static constexpr template_traits::menu_strategy       menu       =
        template_traits::resolve_menu_strategy_v<_Backend>;
    static constexpr template_traits::scrollbar_strategy   scrollbar =
        template_traits::resolve_scrollbar_strategy_v<_Backend>;
    static constexpr template_traits::dialog_strategy      dialog    =
        template_traits::resolve_dialog_strategy_v<_Backend>;
    static constexpr template_traits::color_depth          color     =
        template_traits::resolve_color_depth_v<_Backend>;

    static constexpr bool is_minimal     =
        template_traits::models_minimal_backend_v<_Backend>;
    static constexpr bool is_interactive =
        template_traits::models_interactive_backend_v<_Backend>;
    static constexpr bool is_full        =
        template_traits::models_full_backend_v<_Backend>;
};


NS_END  // templates
NS_END  // uxoxo


#endif  // UXOXO_TEMPLATES_
