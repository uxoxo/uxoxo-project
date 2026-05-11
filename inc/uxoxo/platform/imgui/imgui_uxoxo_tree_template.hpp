/*******************************************************************************
* uxoxo [imgui]                                    imgui_uxoxo_tree_template.hpp
*
*   Dear ImGui implementation of the
* uxoxo::component::uxoxo_tree_component template.  Translates the
* component's expansion / selection / focus state into ImGui draw
* commands every frame and writes user interactions back into the
* component.  No persistent state of its own beyond the
* per-instance view-state struct - the underlying component
* remains the single source of truth for navigation state.
*
*   Layout follows the dev_console pattern: a free-function
* renderer (imgui_draw_uxoxo_tree) takes the component and a
* view-state struct each frame; the view state owns renderer-only
* configuration (indent, arrow column, optional per-row
* callbacks).  Component-level behaviour (label producer, expansion
* defaults) lives on the component itself so that any backend
* renders the same model identically.
*
*   FLAT ROW RENDERING:
*     Rows are emitted via component.for_each_visible rather than
*   the recursive ImGui::TreeNodeEx idiom.  The flat form keeps
*   the component the sole authority for open / closed state (no
*   need to round-trip ImGui's per-id storage), gives uniform
*   row units that compose cleanly with ImGuiListClipper for
*   virtualised rendering of very large trees, and brackets the
*   user's style_push / style_pop callbacks around exactly one row.
*
*   USER-SUPPLIED CALLBACKS (on the view state):
*
*     style_push_fn  - emits PushStyleColor / PushStyleVar / etc.
*                      for the current row (called inside PushID).
*     style_pop_fn   - emits the matching pops in reverse order.
*     row_extra_fn   - draws additional widgets after the label on
*                      the same line (badges, counts, controls).
*
*   The push / pop pair is kept separate (rather than fused into a
* "scope" object) so the user can vary push count per row without
* the renderer tracking it - the symmetric contract is the user's
* responsibility but expressive in return.
*
*   USAGE:
*
*     uxoxo::component::uxoxo_tree_component<ui_tree>  cmp;
*     uxoxo::imgui::imgui_uxoxo_tree_view_state        vs;
*
*     cmp.attach(my_tree);
*     cmp.set_label_fn([&](node_id id) -> const char*
*                      { return my_tree.payload_of(id).tag.c_str(); });
*
*     vs.style_push_fn = [&](node_id id)
*                        { ... ImGui::PushStyleColor(...) ... };
*     vs.style_pop_fn  = [&](node_id)
*                        { ImGui::PopStyleColor(); };
*
*     // each frame inside an ImGui window:
*     uxoxo::imgui::imgui_draw_uxoxo_tree(cmp, vs);
*
* DEPENDENCIES:
*   imgui.h                     Dear ImGui (user-provided)
*   uxoxo_tree.hpp              foundation node_id
*   uxoxo_tree_component.hpp    layer-2 component template
*
* TABLE OF CONTENTS
* =================
* I.    imgui_uxoxo_tree_view_state
* II.   imgui_draw_uxoxo_tree
*
*
* path:      /inc/uxoxo/platform/imgui/tree/imgui_uxoxo_tree_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.06
*******************************************************************************/

#ifndef UXOXO_IMGUI_UXOXO_TREE_TEMPLATE_
#define UXOXO_IMGUI_UXOXO_TREE_TEMPLATE_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
// imgui
#include <imgui.h>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../core/tree/uxoxo_tree.hpp"
#include "../../../templates/component/tree/uxoxo_tree_component.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  I.   imgui_uxoxo_tree_view_state
// ===========================================================================

// imgui_uxoxo_tree_view_state
//   struct: persistent per-instance configuration consumed by
// imgui_draw_uxoxo_tree.  Renderer-specific fields only -
// everything model-side (label producer, expansion defaults,
// selection) belongs on the component.
//
//   Field defaults are chosen so that constructing this struct
// and immediately rendering produces a working tree with no setup:
// ImGui-default indent and frame-height arrow column, arrows on,
// double-click-toggles on, no per-row style or extras.
struct imgui_uxoxo_tree_view_state
{
    // -----------------------------------------------------------------
    //  layout
    // -----------------------------------------------------------------

    // indent_per_level
    //   field: horizontal indent in pixels added per depth level.
    // A negative value means "use ImGui::GetStyle().IndentSpacing".
    float                          indent_per_level = -1.0f;

    // arrow_width
    //   field: width in pixels reserved for the expand / collapse
    // arrow column.  A negative value means
    // "use ImGui::GetFrameHeight()".
    float                          arrow_width      = -1.0f;

    // show_arrows
    //   field: when true, rows with children render an arrow
    // button that toggles expansion on click.  When false, the
    // arrow column is omitted entirely; expansion can still be
    // toggled by double-click (when double_click_toggles is true)
    // or programmatically.
    bool                           show_arrows           = true;

    // double_click_toggles
    //   field: when true, double-clicking a row's label toggles
    // its expansion (in addition to single-click selecting the
    // node).
    bool                           double_click_toggles  = true;


    // -----------------------------------------------------------------
    //  per-row callbacks
    // -----------------------------------------------------------------

    // style_push_fn
    //   field: emits ImGui style pushes (PushStyleColor / Var /
    // Font) appropriate for a row.  Must be paired with style_pop_fn
    // at matching cardinality and reverse order.
    std::function<void(node_id)>   style_push_fn;

    // style_pop_fn
    //   field: emits ImGui style pops corresponding to style_push_fn.
    std::function<void(node_id)>   style_pop_fn;

    // row_extra_fn
    //   field: draws additional widgets on the row after the label,
    // on the same line (the renderer calls SameLine() for you).
    // Called inside the row's style scope.
    std::function<void(node_id)>   row_extra_fn;
};


// ===========================================================================
//  II.  imgui_draw_uxoxo_tree
// ===========================================================================

namespace detail
{

    // resolve_indent
    //   helper: returns the configured indent or ImGui's default.
    inline float
    resolve_indent(
        const imgui_uxoxo_tree_view_state& _vs
    )
    {
        if (_vs.indent_per_level >= 0.0f)
        {
            return _vs.indent_per_level;
        }

        return ImGui::GetStyle().IndentSpacing;
    }

    // resolve_arrow_width
    //   helper: returns the configured arrow column width or
    // ImGui's frame height as a sensible fallback.
    inline float
    resolve_arrow_width(
        const imgui_uxoxo_tree_view_state& _vs
    )
    {
        if (_vs.arrow_width >= 0.0f)
        {
            return _vs.arrow_width;
        }

        return ImGui::GetFrameHeight();
    }

    // node_has_children
    //   helper: returns whether _id has any children in _tree.
    template<typename _Tree>
    bool
    node_has_children(
        const _Tree& _tree,
        node_id      _id
    )
    {
        return _tree.valid(_tree.first_child_of(_id));
    }

    // draw_one_row
    //   helper: renders a single visible row.  Returns true if
    // the user's interaction modified the component's state.
    template<typename _Component>
    bool
    draw_one_row(
        _Component&                         _component,
        imgui_uxoxo_tree_view_state&        _vs,
        node_id                             _id,
        std::size_t                         _depth
    )
    {
        bool changed = false;

        ImGui::PushID(static_cast<int>(_id));

        // -- style push (user) -------------------------------------
        if (_vs.style_push_fn)
        {
            _vs.style_push_fn(_id);
        }

        // -- depth indent -----------------------------------------
        const float indent_total =
            static_cast<float>(_depth) * resolve_indent(_vs);

        if (indent_total > 0.0f)
        {
            ImGui::Indent(indent_total);
        }

        // -- arrow / spacer ---------------------------------------
        const bool has_children =
            node_has_children(_component.tree(), _id);
        const float aw = resolve_arrow_width(_vs);

        if (_vs.show_arrows)
        {
            if (has_children)
            {
                const ImGuiDir dir =
                    _component.is_expanded(_id) ? ImGuiDir_Down
                                                : ImGuiDir_Right;

                if (ImGui::ArrowButton("##arrow", dir))
                {
                    _component.toggle(_id);
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

        // -- label / selectable -----------------------------------
        const char* label = _component.resolve_label(_id);

        const bool was_selected = _component.is_selected(_id);

        ImGuiSelectableFlags sel_flags = 0;

        if (_vs.double_click_toggles)
        {
            sel_flags |= ImGuiSelectableFlags_AllowDoubleClick;
        }

        if (ImGui::Selectable(label, was_selected, sel_flags))
        {
            if (!was_selected)
            {
                _component.select(_id);
                _component.focus(_id);
                changed = true;
            }
            else
            {
                _component.focus(_id);
            }

            if ( (_vs.double_click_toggles) &&
                 (has_children) &&
                 (ImGui::IsMouseDoubleClicked(0)) )
            {
                _component.toggle(_id);
                changed = true;
            }
        }

        // -- row extras (user) ------------------------------------
        if (_vs.row_extra_fn)
        {
            ImGui::SameLine();
            _vs.row_extra_fn(_id);
        }

        // -- depth unindent ---------------------------------------
        if (indent_total > 0.0f)
        {
            ImGui::Unindent(indent_total);
        }

        // -- style pop (user) -------------------------------------
        if (_vs.style_pop_fn)
        {
            _vs.style_pop_fn(_id);
        }

        ImGui::PopID();

        return changed;
    }

}  // namespace detail


// imgui_draw_uxoxo_tree
//   function: emits ImGui draw commands for the visible subtree
// of _component into the current ImGui window.  Updates
// expansion / selection / focus on _component in response to
// clicks.  Returns true if any component state was modified
// during this call (useful for callers that want to know whether
// to re-emit dependent UI or persist state to disk).
//
//   No-op when the component is unattached.  Caller is responsible
// for placing the call inside an ImGui::Begin / End pair (or a
// BeginChild / EndChild scope) - the function neither opens nor
// closes a window of its own.
template<typename _Component>
bool
imgui_draw_uxoxo_tree(
    _Component&                         _component,
    imgui_uxoxo_tree_view_state&        _vs
)
{
    if (!_component.is_attached())
    {
        return false;
    }

    bool changed = false;

    _component.for_each_visible(
        [&](node_id _id, std::size_t _depth)
        {
            if (detail::draw_one_row(_component, _vs, _id, _depth))
            {
                changed = true;
            }
        });

    return changed;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_UXOXO_TREE_TEMPLATE_
