/*******************************************************************************
* uxoxo [imgui]                             imgui_uxoxo_tree_styled_template.hpp
*
*   Styled variant of imgui_uxoxo_tree_template.  Renders the same
* uxoxo_tree_component as the bare path, but additionally consults
* a user-supplied per-node "style value" so that style hooks
* receive computed colours / fonts / alignment alongside the node
* id.  Style values are typically computed by a separate styling
* pipeline - the included style_engine is one such producer, but
* anything that maps node_id to a Value works.
*
*   The bare component (uxoxo_tree_component) is unchanged.  The
* styled renderer is parallel, not nested - users pick the path
* by which view-state struct and which draw function they call.
* This keeps the bare path zero-overhead (no per-row value
* materialisation) and the styled path explicit about its costs.
*
*   STYLE-VALUE PROVIDER:
*
*     The style_value_fn callback on the view state is queried
*   once per visible row, and the returned Value is forwarded to
*   style_push_fn / style_pop_fn / row_extra_fn.  When the
*   provider is unset, the renderer substitutes a default-
*   constructed Value (the hooks still fire if set).
*
*     For users with a style_engine, the bind_engine helper wires
*   the provider in one call:
*
*       imgui_uxoxo_tree_styled_view_state<my_style> vs;
*       imgui_uxoxo_tree_bind_engine(vs, my_engine);
*
*   For users with a hand-rolled style source, set the callback
* directly:
*
*       vs.style_value_fn = [&](node_id id) -> my_style
*           { return compute_style(my_app, id); };
*
*   USAGE:
*
*     uxoxo::component::uxoxo_tree_component<ui_tree>     cmp(tree);
*     uxoxo::imgui::imgui_uxoxo_tree_styled_view_state<
*         my_style>                                       vs;
*
*     uxoxo::imgui::imgui_uxoxo_tree_bind_engine(vs, my_engine);
*
*     vs.style_push_fn = [](node_id, const my_style& s)
*     {
*         ImGui::PushStyleColor(ImGuiCol_Text, s.fg);
*     };
*     vs.style_pop_fn  = [](node_id, const my_style&)
*     {
*         ImGui::PopStyleColor();
*     };
*
*     // each frame:
*     uxoxo::imgui::imgui_draw_uxoxo_styled_tree(cmp, vs);
*
*   THREAD SAFETY:  same single-threaded contract as the bare
* renderer; ImGui itself is single-threaded.
*
* DEPENDENCIES:
*   imgui.h                            Dear ImGui (user-provided)
*   uxoxo_tree.hpp                     foundation node_id
*   uxoxo_tree_component.hpp           layer-2 component template
*
* TABLE OF CONTENTS
* =================
* I.    imgui_uxoxo_tree_styled_view_state
* II.   imgui_draw_uxoxo_styled_tree
* III.  imgui_uxoxo_tree_bind_engine  (engine convenience wrapper)
*
*
* path:      /inc/uxoxo/platform/imgui/tree/imgui_uxoxo_tree_styled_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.06
*******************************************************************************/

#ifndef UXOXO_IMGUI_UXOXO_TREE_STYLED_TEMPLATE_
#define UXOXO_IMGUI_UXOXO_TREE_STYLED_TEMPLATE_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
// imgui
#include <imgui.h>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../core/tree/uxoxo_tree.hpp"
#include "../../../templates/component/tree/uxoxo_tree_component.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  I.   imgui_uxoxo_tree_styled_view_state
// ===========================================================================

// imgui_uxoxo_tree_styled_view_state
//   struct: persistent per-instance configuration consumed by
// imgui_draw_uxoxo_styled_tree.  Parametrised on the user's
// style-value type (typically a small POD-ish struct of colours,
// font ids, alignment flags).  Layout fields mirror the bare
// view state; the difference is that the per-row callbacks
// receive the looked-up Value alongside the node id.
template<typename _Value>
struct imgui_uxoxo_tree_styled_view_state
{
    static_assert(std::is_default_constructible_v<_Value>,
        "imgui_uxoxo_tree_styled_view_state: _Value must be "
        "default-constructible (the renderer substitutes a "
        "default-constructed Value when style_value_fn is unset).");


    // -----------------------------------------------------------------
    //  layout (mirror of bare view state)
    // -----------------------------------------------------------------

    float                                 indent_per_level     = -1.0f;
    float                                 arrow_width          = -1.0f;
    bool                                  show_arrows          = true;
    bool                                  double_click_toggles = true;


    // -----------------------------------------------------------------
    //  style provider
    // -----------------------------------------------------------------

    // style_value_fn
    //   field: maps node_id to the row's computed style.  Called
    // once per visible row.  When unset, the renderer substitutes
    // a default-constructed _Value before invoking the per-row
    // hooks.  Wire to a style_engine via imgui_uxoxo_tree_bind_engine
    // or to any custom source directly.
    std::function<_Value(node_id)>        style_value_fn;


    // -----------------------------------------------------------------
    //  per-row callbacks
    // -----------------------------------------------------------------

    // style_push_fn
    //   field: emits ImGui style pushes (PushStyleColor / Var /
    // Font) for the current row using the looked-up style value.
    // Must be paired with style_pop_fn at matching cardinality
    // and reverse order.
    std::function<void(node_id, const _Value&)>   style_push_fn;

    // style_pop_fn
    //   field: emits ImGui style pops corresponding to style_push_fn.
    std::function<void(node_id, const _Value&)>   style_pop_fn;

    // row_extra_fn
    //   field: draws additional widgets on the row after the label.
    // Receives the looked-up style value too, so badges and counts
    // can colour-match the row.
    std::function<void(node_id, const _Value&)>   row_extra_fn;
};


// ===========================================================================
//  II.  imgui_draw_uxoxo_styled_tree
// ===========================================================================

namespace detail
{

    // resolve_styled_indent
    //   helper: returns the configured indent or ImGui's default.
    template<typename _Value>
    inline float
    resolve_styled_indent(
        const imgui_uxoxo_tree_styled_view_state<_Value>& _vs
    )
    {
        if (_vs.indent_per_level >= 0.0f)
        {
            return _vs.indent_per_level;
        }

        return ImGui::GetStyle().IndentSpacing;
    }

    // resolve_styled_arrow_width
    template<typename _Value>
    inline float
    resolve_styled_arrow_width(
        const imgui_uxoxo_tree_styled_view_state<_Value>& _vs
    )
    {
        if (_vs.arrow_width >= 0.0f)
        {
            return _vs.arrow_width;
        }

        return ImGui::GetFrameHeight();
    }

    // styled_node_has_children
    template<typename _Tree>
    inline bool
    styled_node_has_children(
        const _Tree& _tree,
        node_id      _id
    )
    {
        return _tree.valid(_tree.first_child_of(_id));
    }

    // draw_one_styled_row
    //   helper: renders a single styled row.  Looks up the style
    // value once and forwards it to all per-row callbacks.
    // Returns true if the user's interaction modified component
    // state.
    template<typename _Component, typename _Value>
    bool
    draw_one_styled_row(
        _Component&                                          _component,
        imgui_uxoxo_tree_styled_view_state<_Value>&          _vs,
        node_id                                              _id,
        std::size_t                                          _depth
    )
    {
        bool changed = false;

        // -- look up the style value (once per row) ---------------
        _Value value{};

        if (_vs.style_value_fn)
        {
            value = _vs.style_value_fn(_id);
        }

        ImGui::PushID(static_cast<int>(_id));

        // -- style push (user) -----------------------------------
        if (_vs.style_push_fn)
        {
            _vs.style_push_fn(_id, value);
        }

        // -- depth indent ----------------------------------------
        const float indent_total =
            static_cast<float>(_depth) *
            resolve_styled_indent(_vs);

        if (indent_total > 0.0f)
        {
            ImGui::Indent(indent_total);
        }

        // -- arrow / spacer --------------------------------------
        const bool has_children =
            styled_node_has_children(_component.tree(), _id);
        const float aw = resolve_styled_arrow_width(_vs);

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
                ImGui::Dummy(ImVec2(aw, 0.0f));
            }

            ImGui::SameLine();
        }

        // -- label / selectable ----------------------------------
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

        // -- row extras (user) -----------------------------------
        if (_vs.row_extra_fn)
        {
            ImGui::SameLine();
            _vs.row_extra_fn(_id, value);
        }

        // -- depth unindent --------------------------------------
        if (indent_total > 0.0f)
        {
            ImGui::Unindent(indent_total);
        }

        // -- style pop (user) ------------------------------------
        if (_vs.style_pop_fn)
        {
            _vs.style_pop_fn(_id, value);
        }

        ImGui::PopID();

        return changed;
    }

}  // namespace detail


// imgui_draw_uxoxo_styled_tree
//   function: emits ImGui draw commands for the visible subtree
// of _component, looking up a per-node style value through
// _vs.style_value_fn and forwarding it to the per-row hooks.
// Updates expansion / selection / focus on _component in
// response to clicks.  Returns true if any component state was
// modified during this call.
//
//   No-op when the component is unattached.  Caller is responsible
// for placing the call inside an ImGui::Begin / End pair (or a
// BeginChild / EndChild scope) - the function neither opens nor
// closes a window of its own.
template<typename _Component, typename _Value>
bool
imgui_draw_uxoxo_styled_tree(
    _Component&                                  _component,
    imgui_uxoxo_tree_styled_view_state<_Value>&  _vs
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
            if (detail::draw_one_styled_row(
                    _component, _vs, _id, _depth))
            {
                changed = true;
            }
        });

    return changed;
}


// ===========================================================================
//  III. imgui_uxoxo_tree_bind_engine
// ===========================================================================

// imgui_uxoxo_tree_bind_engine
//   function: convenience wrapper that wires a style-engine-shaped
// source (anything with .computed_style_of(node_id) returning a
// const reference to a value type) into a styled view state's
// style_value_fn.  The engine is captured by reference; lifetime
// is the caller's responsibility.
//
//   Works with the included style_engine but is structurally
// duck-typed - any type providing the same accessor satisfies it.
//
//   Example:
//
//     stylized_tree<ui_tree, my_style, my_rule> styled(my_tree);
//     // ... add rules ...
//
//     imgui_uxoxo_tree_styled_view_state<my_style> vs;
//     imgui_uxoxo_tree_bind_engine(vs, styled.engine());
//
//     // styling now flows from styled's engine into vs's per-row
//     // hooks.
template<typename _Engine, typename _Value>
void
imgui_uxoxo_tree_bind_engine(
    imgui_uxoxo_tree_styled_view_state<_Value>&  _vs,
    _Engine&                                     _engine
)
{
    _vs.style_value_fn =
        [&_engine](node_id _id) -> _Value
        {
            return _engine.computed_style_of(_id);
        };

    return;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_UXOXO_TREE_STYLED_TEMPLATE_
