/*******************************************************************************
* uxoxo [imgui]                                              imgui_tree_view.hpp
*
*   Dear ImGui rendering of the vendor-agnostic
* uxoxo::component::tree_view.  Translates the view's expansion,
* selection, and focus state into ImGui draw commands every frame
* and writes user interactions back into the view.  No persistent
* state of its own beyond per-instance configuration callbacks -
* the underlying tree_view remains the single source of truth.
*
*   The widget renders the visible subtree as a flat sequence of
* rows rather than using ImGui::TreeNodeEx / TreePop.  The flat
* form has three advantages over the recursive idiom:
*
*     1. The component tree_view's expansion model becomes the
*        sole authority for open / closed state.  No back-channel
*        is needed to keep ImGui's per-node storage in sync.
*
*     2. Each row is a uniform unit, which lets the widget cleanly
*        compose with ImGuiListClipper for virtualised rendering
*        of very large trees (future render_clipped()).
*
*     3. Style application is tighter: the user's style_push_fn /
*        style_pop_fn brackets exactly one row, with no surprise
*        carry-over from ImGui's internal indent stack.
*
*   USER-SUPPLIED CALLBACKS:
*
*     label_fn       - returns a null-terminated label for a node.
*     style_push_fn  - emits PushStyleColor / PushStyleVar / etc.
*     style_pop_fn   - emits the matching pops in reverse order.
*     row_extra_fn   - draws additional widgets on the row (counts,
*                      buttons, status badges) after the label.
*
*   The push / pop callbacks are kept separate rather than fused
* into a "style scope" object so that the user can push different
* numbers and kinds of style stack entries per call without the
* widget having to track them.  Callers wanting RAII semantics can
* bind both to lambdas that capture a small scope-guard helper.
*
*   USAGE:
*
*     stylized_tree<arena_uxoxo_tree<ui_payload>,
*                   my_style, my_rule> st;
*     component::tree_view<decltype(st)> view(st);
*     imgui::tree_view<decltype(view)>   widget(view);
*
*     widget.set_label_fn([&](node_id _id) -> const char*
*     {
*         return st.tree().payload_of(_id).tag.c_str();
*     });
*
*     widget.set_style_push_fn([](node_id, const my_style& _s)
*     {
*         ImGui::PushStyleColor(ImGuiCol_Text, _s.fg);
*     });
*     widget.set_style_pop_fn([](node_id, const my_style&)
*     {
*         ImGui::PopStyleColor();
*     });
*
*     // Each frame:
*     widget.render();
*
*   THREAD SAFETY:
*     ImGui itself is single-threaded.  This widget inherits that
*   contract and is single-threaded by design.
*
* DEPENDENCIES:
*   imgui.h                    Dear ImGui (user-provided)
*   uxoxo_tree.hpp             foundation node_id
*   uxoxo_tree_view.hpp        component tree_view this wraps
*
* TABLE OF CONTENTS
* =================
* I.    tree_view  (imgui)
*
*
* path:      /inc/uxoxo/platform/imgui/imgui_tree_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.06
*******************************************************************************/

#ifndef UXOXO_IMGUI_TREE_VIEW_
#define UXOXO_IMGUI_TREE_VIEW_ 1

// std
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <utility>
// imgui
#include "imgui.h"
// uxoxo
#include "../../uxoxo.hpp"
#include "../../core/tree/uxoxo_tree.hpp"
#include "../../core/tree/uxoxo_tree_view.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  I.   tree_view (imgui)
// ===========================================================================

// tree_view
//   class: ImGui rendering wrapper around a vendor-agnostic
// component::tree_view.  Holds non-owning pointer to the view
// plus per-instance configuration; render() is called once per
// frame inside an ImGui window.
template<typename _View>
class tree_view
{
public:

    using view_type           = _View;
    using stylized_tree_type  = typename _View::stylized_tree_type;
    using tree_type           = typename _View::tree_type;
    using value_type          = typename _View::value_type;
    using rule_type           = typename _View::rule_type;
    using engine_type         = typename _View::engine_type;
    using size_type           = typename _View::size_type;


    // label_fn
    //   alias: per-node label producer.  Returns a null-terminated
    // C string whose lifetime extends at least to the end of the
    // current ImGui frame.
    using label_fn       =
        std::function<const char*(node_id)>;

    // style_push_fn
    //   alias: emits ImGui style pushes (PushStyleColor,
    // PushStyleVar, PushFont, ...) appropriate for a row.  Must be
    // paired with a matching style_pop_fn that pops the same
    // entries in reverse order.
    using style_push_fn  =
        std::function<void(node_id, const value_type&)>;

    // style_pop_fn
    //   alias: emits ImGui style pops matching the pushes from
    // style_push_fn, in reverse order.
    using style_pop_fn   =
        std::function<void(node_id, const value_type&)>;

    // row_extra_fn
    //   alias: draws additional widgets on a row after the label.
    // Called inside the row's style scope and on the same line as
    // the label; the implementation already calls SameLine() before
    // invoking, so the callback only emits widgets.
    using row_extra_fn   =
        std::function<void(node_id, const value_type&)>;


    // -----------------------------------------------------------------
    //  constructors / destructor / assignment
    // -----------------------------------------------------------------

    // tree_view (view)
    //   constructor: binds the widget to _view.  Configuration
    // defaults to a numeric label (#N), no per-row style, no row
    // extras, ImGui-default indent, double-click-toggles enabled.
    explicit
    tree_view(
        _View& _view
    )
        : m_view(&_view),
          m_label_fn(),
          m_style_push_fn(),
          m_style_pop_fn(),
          m_row_extra_fn(),
          m_indent_per_level(-1.0f),
          m_arrow_width(-1.0f),
          m_show_arrows(true),
          m_double_click_toggles(true)
    {
    }

    tree_view(const tree_view&)                = default;
    tree_view& operator=(const tree_view&)     = default;
    tree_view(tree_view&&) noexcept            = default;
    tree_view& operator=(tree_view&&) noexcept = default;
    ~tree_view()                               = default;


    // =================================================================
    //  view access
    // =================================================================

    // view
    //   method: returns a reference to the underlying component
    // tree_view that this widget renders.
    _View&
    view() noexcept
    {
        return *m_view;
    }

    const _View&
    view() const noexcept
    {
        return *m_view;
    }


    // =================================================================
    //  configuration - callbacks
    // =================================================================

    // set_label_fn
    //   method: installs the per-node label producer.  Pass an
    // empty function to revert to the numeric default.
    void
    set_label_fn(
        label_fn _fn
    )
    {
        m_label_fn = std::move(_fn);

        return;
    }

    // set_style_push_fn
    //   method: installs the per-row style-push callback.
    void
    set_style_push_fn(
        style_push_fn _fn
    )
    {
        m_style_push_fn = std::move(_fn);

        return;
    }

    // set_style_pop_fn
    //   method: installs the per-row style-pop callback.  Must
    // match the cardinality and ordering of set_style_push_fn.
    void
    set_style_pop_fn(
        style_pop_fn _fn
    )
    {
        m_style_pop_fn = std::move(_fn);

        return;
    }

    // set_row_extra_fn
    //   method: installs the per-row extra-content callback.
    void
    set_row_extra_fn(
        row_extra_fn _fn
    )
    {
        m_row_extra_fn = std::move(_fn);

        return;
    }


    // =================================================================
    //  configuration - layout
    // =================================================================

    // set_indent_per_level
    //   method: sets the horizontal indent in pixels added per
    // depth level.  Pass a negative value to use ImGui's default
    // (Style::IndentSpacing).
    void
    set_indent_per_level(
        float _px
    ) noexcept
    {
        m_indent_per_level = _px;

        return;
    }

    // set_arrow_width
    //   method: sets the width in pixels reserved for the
    // expand / collapse arrow column.  A negative value uses the
    // ImGui default frame height.
    void
    set_arrow_width(
        float _px
    ) noexcept
    {
        m_arrow_width = _px;

        return;
    }

    // set_show_arrows
    //   method: enables or disables the per-row expand / collapse
    // arrow.  When disabled, callers can still toggle expansion
    // through double-click (when set_double_click_toggles is on)
    // or programmatically via the underlying view.
    void
    set_show_arrows(
        bool _on
    ) noexcept
    {
        m_show_arrows = _on;

        return;
    }

    // set_double_click_toggles
    //   method: enables or disables double-click-to-toggle on the
    // label region.
    void
    set_double_click_toggles(
        bool _on
    ) noexcept
    {
        m_double_click_toggles = _on;

        return;
    }


    // =================================================================
    //  rendering
    // =================================================================

    // render
    //   method: emits ImGui draw commands for the visible subtree.
    // Updates expansion / selection / focus on the underlying view
    // as the user interacts.  Returns true if any view state was
    // modified during this call (useful for callers that want to
    // know whether to re-emit dependent UI or save state).
    bool
    render()
    {
        bool changed = false;

        m_view->for_each_visible(
            [&](node_id _id, size_type _depth)
            {
                if (render_row(_id, _depth))
                {
                    changed = true;
                }
            });

        return changed;
    }


private:

    // -----------------------------------------------------------------
    //  internal helpers
    // -----------------------------------------------------------------

    // resolved_indent_per_level
    //   helper: returns the configured indent or ImGui's default.
    float
    resolved_indent_per_level() const
    {
        if (m_indent_per_level >= 0.0f)
        {
            return m_indent_per_level;
        }

        return ImGui::GetStyle().IndentSpacing;
    }

    // resolved_arrow_width
    //   helper: returns the configured arrow column width or
    // ImGui's frame height as a sensible fallback.
    float
    resolved_arrow_width() const
    {
        if (m_arrow_width >= 0.0f)
        {
            return m_arrow_width;
        }

        return ImGui::GetFrameHeight();
    }

    // node_has_children
    //   helper: returns whether _id has any children in the host
    // tree.  Drives the arrow vs spacer choice on the row.
    bool
    node_has_children(
        node_id _id
    ) const
    {
        const tree_type& t = m_view->tree();

        return t.valid(t.first_child_of(_id));
    }

    // default_label
    //   helper: numeric fallback label used when no label_fn is
    // configured.  Writes into a thread-local buffer that remains
    // valid until the next call on the same thread.
    static const char*
    default_label(
        node_id _id
    )
    {
        thread_local char buf[32];

        std::snprintf(
            buf,
            sizeof(buf),
            "#%llu",
            static_cast<unsigned long long>(_id));

        return buf;
    }

    // resolved_label
    //   helper: returns the user-supplied label for _id or the
    // numeric default.
    const char*
    resolved_label(
        node_id _id
    ) const
    {
        if (m_label_fn)
        {
            const char* p = m_label_fn(_id);

            if (p != nullptr)
            {
                return p;
            }
        }

        return default_label(_id);
    }

    // render_row
    //   helper: renders a single row (one visible node at a depth
    // offset).  Returns true if the row's interaction modified
    // the view's state.
    bool
    render_row(
        node_id   _id,
        size_type _depth
    )
    {
        bool changed = false;

        const value_type& cs = m_view->computed_style_of(_id);

        ImGui::PushID(static_cast<int>(_id));

        // -- style push (user) --
        if (m_style_push_fn)
        {
            m_style_push_fn(_id, cs);
        }

        // -- depth indent --
        const float indent_total =
            static_cast<float>(_depth) * resolved_indent_per_level();

        if (indent_total > 0.0f)
        {
            ImGui::Indent(indent_total);
        }

        // -- arrow / spacer --
        const bool has_children = node_has_children(_id);
        const float aw          = resolved_arrow_width();

        if (m_show_arrows)
        {
            if (has_children)
            {
                const ImGuiDir dir =
                    m_view->is_expanded(_id) ? ImGuiDir_Down
                                             : ImGuiDir_Right;

                if (ImGui::ArrowButton("##arrow", dir))
                {
                    m_view->toggle(_id);
                    changed = true;
                }
            }
            else
            {
                // Reserve the arrow column so labels align.
                ImGui::Dummy(ImVec2(aw, 0.0f));
            }

            ImGui::SameLine();
        }

        // -- label / selectable --
        const char* label = resolved_label(_id);

        const bool was_selected = m_view->is_selected(_id);

        ImGuiSelectableFlags sel_flags = 0;

        if (m_double_click_toggles)
        {
            sel_flags |= ImGuiSelectableFlags_AllowDoubleClick;
        }

        if (ImGui::Selectable(label, was_selected, sel_flags))
        {
            // Single click activates selection regardless of
            // whether a double-click follows.
            if (!was_selected)
            {
                m_view->select(_id);
                m_view->focus(_id);
                changed = true;
            }
            else
            {
                m_view->focus(_id);
            }

            if ( (m_double_click_toggles) &&
                 (has_children) &&
                 (ImGui::IsMouseDoubleClicked(0)) )
            {
                m_view->toggle(_id);
                changed = true;
            }
        }

        // -- row extras (user) --
        if (m_row_extra_fn)
        {
            ImGui::SameLine();
            m_row_extra_fn(_id, cs);
        }

        // -- depth unindent --
        if (indent_total > 0.0f)
        {
            ImGui::Unindent(indent_total);
        }

        // -- style pop (user) --
        if (m_style_pop_fn)
        {
            m_style_pop_fn(_id, cs);
        }

        ImGui::PopID();

        return changed;
    }


    // -----------------------------------------------------------------
    //  members
    // -----------------------------------------------------------------

    _View*          m_view;

    label_fn        m_label_fn;
    style_push_fn   m_style_push_fn;
    style_pop_fn    m_style_pop_fn;
    row_extra_fn    m_row_extra_fn;

    // negative sentinel = use ImGui defaults
    float           m_indent_per_level;
    float           m_arrow_width;

    bool            m_show_arrows;
    bool            m_double_click_toggles;
};


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_TREE_VIEW_
