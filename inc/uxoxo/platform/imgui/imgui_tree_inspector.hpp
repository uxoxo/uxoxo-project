/*******************************************************************************
* uxoxo [imgui]                                          imgui_tree_inspector.hpp
*
*   F12-style live inspector for any uxoxo_tree.  Presents the tree
* in a Dear ImGui window with two panes - a navigable tree on the
* left and a details panel for the currently-selected node on the
* right - and handles its own open / closed state so the host
* application only needs to call render() each frame and toggle()
* in response to the F12 keystroke.
*
*   ARCHITECTURE:
*
*     The inspector is a thin assembly over the canonical three-
*   layer tree stack:
*
*       1.  uxoxo_tree                    foundational data structure
*       2.  uxoxo_tree_component          vendor-agnostic UI state
*       3.  imgui_uxoxo_tree_template     ImGui rendering
*
*     The inspector owns its own component (layer 2) over a
*   non-owning pointer to the application's live tree, plus a
*   view-state struct that configures the renderer (layer 3).
*   Mutations to the application's tree show up in the inspector
*   on the very next frame - there is no shadow copy and no
*   separate model.
*
*   DEFAULTS THAT WORK OUT-OF-THE-BOX:
*
*     label_fn unset    -> numeric "#N" labels (debug-friendly)
*     details_fn unset  -> id, validity, parent, first_child,
*                          next_sibling, child_count
*     window title      -> "uxoxo tree inspector"
*     split ratio       -> 55% tree / 45% details
*
*   The host application typically overrides label_fn (to use the
* payload tag) and details_fn (to enumerate payload properties).
*
*   USAGE:
*
*     struct app
*     {
*         uxoxo::ui_tree                            tree;
*         uxoxo::imgui::tree_inspector<ui_tree>     inspector;
*     };
*
*     void app_init(app& a)
*     {
*         a.inspector.attach(a.tree);
*         a.inspector.set_label_fn(
*             [&a](node_id id) -> const char*
*             {
*                 return a.tree.payload_of(id).tag.c_str();
*             });
*     }
*
*     // In dispatcher:
*     if (verb == "toggle_inspector") { a.inspector.toggle(); }
*
*     // In keystroke handler:
*     if (ImGui::IsKeyPressed(ImGuiKey_F12, false))
*         app_dispatch(a, "toggle_inspector");
*
*     // In frame draw:
*     a.inspector.render();
*
* DEPENDENCIES:
*   imgui.h                            Dear ImGui (user-provided)
*   uxoxo_tree.hpp                     foundation node_id / mutation
*   uxoxo_tree_component.hpp           layer-2 component template
*   imgui_uxoxo_tree_template.hpp      layer-3 imgui renderer
*
* TABLE OF CONTENTS
* =================
* I.    tree_inspector
*
*
* path:      /inc/uxoxo/platform/imgui/imgui_tree_inspector.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.06
*******************************************************************************/

#ifndef UXOXO_IMGUI_TREE_INSPECTOR_
#define UXOXO_IMGUI_TREE_INSPECTOR_ 1

// std
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
// imgui
#include <imgui.h>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../core/tree/uxoxo_tree.hpp"
#include "../../core/tree/uxoxo_tree_concepts.hpp"
#include "../../templates/component/tree/uxoxo_tree_component.hpp"
#include "tree/imgui_uxoxo_tree_template.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  I.   tree_inspector
// ===========================================================================

// tree_inspector
//   class: F12-style live inspector window for any uxoxo_tree_type.
// Owns its own uxoxo_tree_component (layer 2) for navigation state
// plus an imgui_uxoxo_tree_view_state (layer 3 config) for the
// per-frame renderer call.  Adds an ImGui window with a tree pane
// on the left and a details pane on the right, plus open / closed
// state and a window title.
template<typename _Tree>
class tree_inspector
{
public:

    using tree_type      = _Tree;
    using component_type =
        component::uxoxo_tree_component<_Tree>;
    using view_state_type =
        imgui_uxoxo_tree_view_state;
    using size_type      = std::size_t;

    // label_fn
    //   alias: per-node label producer forwarded to the underlying
    // component.  Returns a null-terminated string whose lifetime
    // extends to the end of the current frame.
    using label_fn   =
        typename component_type::label_fn;

    // details_fn
    //   alias: callback that emits ImGui draw commands for the
    // details pane when a node is selected.  The inspector wraps
    // the call in a child window; the callback only needs to emit
    // ImGui::Text / Bullet / etc. for the rows it wants to show.
    using details_fn =
        std::function<void(const _Tree&, node_id)>;


    static_assert(uxoxo_tree_type<_Tree>,
        "tree_inspector: _Tree must satisfy uxoxo_tree_type "
        "(see uxoxo_tree_concepts.hpp).");


    // -----------------------------------------------------------------
    //  constructors / destructor / assignment
    // -----------------------------------------------------------------

    // tree_inspector (default)
    //   constructor: builds an unattached inspector.  Call attach()
    // before render() to bind to a tree.
    tree_inspector()
        : m_component(),
          m_vs(),
          m_open(false),
          m_title("uxoxo tree inspector"),
          m_split_ratio(0.55f),
          m_details_fn()
    {
    }

    // tree_inspector (tree)
    //   constructor: builds and attaches in one step.
    explicit
    tree_inspector(
        _Tree& _tree
    )
        : tree_inspector()
    {
        attach(_tree);
    }

    // copy / move are defaulted - the underlying component is
    // value-like and the tree pointer remains valid through
    // copy / move.  Note that two inspector instances with the
    // same window title will conflict in ImGui's per-window state;
    // change the title via set_window_title() if instantiating
    // multiple inspectors over the same tree.
    tree_inspector(const tree_inspector&)                = default;
    tree_inspector& operator=(const tree_inspector&)     = default;
    tree_inspector(tree_inspector&&) noexcept            = default;
    tree_inspector& operator=(tree_inspector&&) noexcept = default;
    ~tree_inspector()                                    = default;


    // =================================================================
    //  attach
    // =================================================================

    // attach
    //   method: binds the inspector's component to _tree.  Tree
    // pointer is held non-owningly via the component; lifetime is
    // the user's responsibility.
    void
    attach(
        _Tree& _tree
    ) noexcept
    {
        m_component.attach(_tree);

        return;
    }

    bool
    is_attached() const noexcept
    {
        return m_component.is_attached();
    }


    // =================================================================
    //  open / close
    // =================================================================

    void open()    noexcept { m_open = true;     return; }
    void close()   noexcept { m_open = false;    return; }
    void toggle()  noexcept { m_open = !m_open;  return; }

    bool
    is_open() const noexcept
    {
        return m_open;
    }


    // =================================================================
    //  configuration
    // =================================================================

    // set_label_fn
    //   method: installs the per-node label producer.  Forwarded
    // to the underlying component (which is the source of truth
    // for labels - any backend renderer reads from there).
    void
    set_label_fn(
        label_fn _fn
    )
    {
        m_component.set_label_fn(std::move(_fn));

        return;
    }

    // set_details_fn
    //   method: installs the details-pane callback.  Called once
    // per render() with the host tree and the currently selected
    // node id; the callback emits ImGui draw commands for the
    // pane.
    void
    set_details_fn(
        details_fn _fn
    )
    {
        m_details_fn = std::move(_fn);

        return;
    }

    // set_window_title
    //   method: changes the ImGui window title.  Default is
    // "uxoxo tree inspector".  Useful when multiple inspectors
    // are open simultaneously - ImGui keys per-window state by
    // title, so distinct titles keep their layouts independent.
    void
    set_window_title(
        const char* _title
    )
    {
        if (_title != nullptr)
        {
            m_title = _title;
        }

        return;
    }

    // set_split_ratio
    //   method: sets the fraction of the window's content width
    // given to the tree pane.  Clamped to [0.1, 0.9].
    void
    set_split_ratio(
        float _r
    ) noexcept
    {
        if (_r < 0.1f) { _r = 0.1f; }
        if (_r > 0.9f) { _r = 0.9f; }

        m_split_ratio = _r;

        return;
    }

    float
    split_ratio() const noexcept
    {
        return m_split_ratio;
    }


    // =================================================================
    //  underlying-component access
    // =================================================================

    // component
    //   method: reference access to the internal
    // uxoxo_tree_component.  Useful for programmatic expand /
    // select operations from the host application.
    component_type&
    component() noexcept
    {
        return m_component;
    }

    const component_type&
    component() const noexcept
    {
        return m_component;
    }

    // view_state
    //   method: reference access to the imgui view-state struct
    // so the host can configure renderer fields directly
    // (style_push_fn, row_extra_fn, indent_per_level, ...).
    view_state_type&
    view_state() noexcept
    {
        return m_vs;
    }

    const view_state_type&
    view_state() const noexcept
    {
        return m_vs;
    }


    // =================================================================
    //  per-frame render
    // =================================================================

    // render
    //   method: emits ImGui draw commands for the inspector window
    // when open and attached.  Returns true if the window was
    // rendered this call, false if it was skipped (closed or
    // unattached).  Closing the window via the X button updates
    // is_open() through ImGui's *p_open contract so the caller
    // does not need a separate handler.
    bool
    render()
    {
        if ( (!m_open) || (!m_component.is_attached()) )
        {
            return false;
        }

        ImGui::SetNextWindowSize(
            ImVec2(720.0f, 480.0f),
            ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(m_title.c_str(), &m_open))
        {
            ImGui::End();
            return true;
        }

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float  left  = avail.x * m_split_ratio;

        // -- left pane: tree --------------------------------------
        ImGui::BeginChild(
            "##uxoxo_inspector_tree_pane",
            ImVec2(left, 0.0f),
            0);

        imgui_draw_uxoxo_tree(m_component, m_vs);

        ImGui::EndChild();

        ImGui::SameLine();

        // -- right pane: details ----------------------------------
        ImGui::BeginChild(
            "##uxoxo_inspector_details_pane",
            ImVec2(0.0f, 0.0f),
            0);

        render_details();

        ImGui::EndChild();

        ImGui::End();

        return true;
    }


    // =================================================================
    //  mutation hook
    // =================================================================

    // notify
    //   method: forwards a tree mutation to the component's
    // notify() so stale entries in its diverged / selection /
    // focus state can be cleaned up.  Optional - lazy validation
    // handles read-side cases automatically; calling notify()
    // just keeps the diverged set bounded under heavy churn.
    void
    notify(
        const mutation& _mut
    )
    {
        m_component.notify(_mut);

        return;
    }


private:

    // -----------------------------------------------------------------
    //  internal helpers
    // -----------------------------------------------------------------

    // render_details
    //   helper: emits the right-pane content.  Uses the
    // user-supplied details_fn when set, otherwise falls back to
    // a built-in dump of the node's structural metadata.
    void
    render_details()
    {
        const node_id sel = m_component.selected();

        if (sel == null_node)
        {
            ImGui::TextDisabled("(no node selected)");
            return;
        }

        const _Tree& t = m_component.tree();

        if (m_details_fn)
        {
            m_details_fn(t, sel);
            return;
        }

        // -- default details -------------------------------------
        // Structural metadata only; payload-specific information
        // is the user's responsibility via set_details_fn.
        ImGui::Text("id:        %llu",
                    static_cast<unsigned long long>(sel));
        ImGui::Text("valid:     %s",
                    t.valid(sel) ? "true" : "false");
        ImGui::Text("parent:    %llu",
                    static_cast<unsigned long long>(
                        t.parent_of(sel)));
        ImGui::Text("first ch:  %llu",
                    static_cast<unsigned long long>(
                        t.first_child_of(sel)));
        ImGui::Text("next sib:  %llu",
                    static_cast<unsigned long long>(
                        t.next_sibling_of(sel)));

        size_type count = 0;

        for (node_id c = t.first_child_of(sel);
             t.valid(c);
             c = t.next_sibling_of(c))
        {
            ++count;
        }

        ImGui::Text("children:  %zu", count);

        return;
    }


    // -----------------------------------------------------------------
    //  members
    // -----------------------------------------------------------------

    component_type   m_component;
    view_state_type  m_vs;

    bool             m_open;
    std::string      m_title;
    float            m_split_ratio;

    details_fn       m_details_fn;
};


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_TREE_INSPECTOR_
