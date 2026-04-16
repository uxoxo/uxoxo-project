/*******************************************************************************
* uxoxo [ui]                                                imgui_tree_draw.hpp
*
*   ImGui draw handler for tree_view<_Data, _Feat, _Icon>.  Discovers node
* and view capabilities at compile time via if constexpr and the tree_traits
* detection idiom, then draws exactly what each feature combination needs.
*
*   Because tree_view and tree_node are templates, this entire handler is
* header-only.  The main entry point is imgui_draw_tree_view().
*
*   Integration:
*
*     imgui_tree_style   style;
*     imgui_tree_callbacks<my_data, my_feat, my_icon>  cb;
*     cb.label_fn = [](const my_data& d) { return d.name; };
*
*     // in frame loop:
*     imgui_draw_tree_view(my_view, style, cb);
*
*   The draw function mutates the view's navigation, selection, collapse,
* check, rename, and context state in response to ImGui input.  It does
* NOT mutate the node _Data — that flows through the callbacks so the
* application retains control.
*
*   Structure:
*     1.  imgui_tree_style           colour, size, and spacing constants
*     2.  imgui_tree_callbacks       application-provided callables
*     3.  imgui_draw_tree_view       main entry point (template)
*     4.  internal draw helpers      row, lines, checkbox, icon, label
*
*   REQUIRES: C++17 or later, Dear ImGui.
*
*
* path:      /inc/uxoxo/platform/imgui/imgui_tree_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.12
*******************************************************************************/

#ifndef UXOXO_UI_IMGUI_TREE_DRAW_
#define UXOXO_UI_IMGUI_TREE_DRAW_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>
// imgui
#include "imgui.h"
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../templates/component/tree/tree_node.hpp"
#include "../../templates/component/tree/tree_view.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI

using uxoxo::component::tree_node;
using uxoxo::component::tree_view;

// =============================================================================
//  1.  IMGUI TREE STYLE
// =============================================================================

// imgui_tree_style
//   struct: visual constants for tree rendering.
struct imgui_tree_style
{
    // -- row geometry ------------------------------------------------
    float row_height          = 22.0f;
    float indent_width        = 20.0f;
    float item_spacing_x      = 4.0f;

    // -- tree lines --------------------------------------------------
    ImU32 line_colour         = IM_COL32(100, 100, 100, 120);
    float line_thickness      = 1.0f;
    bool  draw_lines          = true;

    // -- expand / collapse arrow -------------------------------------
    ImU32 arrow_colour        = IM_COL32(180, 180, 180, 255);
    ImU32 arrow_hover_colour  = IM_COL32(220, 220, 220, 255);
    float arrow_size          = 10.0f;

    // -- checkbox ----------------------------------------------------
    float checkbox_size       = 14.0f;
    ImU32 check_border        = IM_COL32(160, 160, 160, 200);
    ImU32 check_fill          = IM_COL32(66, 135, 245, 255);
    ImU32 check_mark          = IM_COL32(255, 255, 255, 255);
    ImU32 check_indet         = IM_COL32(66, 135, 245, 180);
    float check_rounding      = 2.0f;

    // -- icon --------------------------------------------------------
    float icon_size           = 16.0f;

    // -- label -------------------------------------------------------
    ImU32 label_colour        = IM_COL32(220, 220, 220, 255);
    ImU32 label_disabled      = IM_COL32(120, 120, 120, 255);

    // -- selection / cursor ------------------------------------------
    ImU32 selected_bg         = IM_COL32(66, 135, 245, 80);
    ImU32 cursor_bg           = IM_COL32(66, 135, 245, 40);
    ImU32 cursor_border       = IM_COL32(66, 135, 245, 160);
    float cursor_rounding     = 3.0f;
    float cursor_thickness    = 1.0f;

    // -- hover -------------------------------------------------------
    ImU32 hover_bg            = IM_COL32(255, 255, 255, 15);

    // -- rename input ------------------------------------------------
    ImU32 rename_bg           = IM_COL32(40, 40, 40, 255);
    ImU32 rename_border       = IM_COL32(66, 135, 245, 200);

    // -- context menu ------------------------------------------------
    ImU32 context_separator   = IM_COL32(80, 80, 80, 200);

    // -- drop target -------------------------------------------------
    ImU32 drop_highlight      = IM_COL32(66, 245, 135, 80);
    float drop_thickness      = 2.0f;
};


// =============================================================================
//  2.  IMGUI TREE CALLBACKS
// =============================================================================

// imgui_tree_callbacks
//   struct: application-provided callables for data extraction, icon
// drawing, and action handling.  Template parameters must match the
// tree_view being drawn.
template<typename _Data,
         unsigned _Feat = uxoxo::component::tf_none,
         typename _Icon = int>
struct imgui_tree_callbacks
{
    using node_type = uxoxo::component::tree_node<_Data, _Feat, _Icon>;

    // label_fn  (REQUIRED)
    //   Extracts display text from a node's data payload.
    std::function<std::string(const _Data&)> label_fn;

    // icon_draw_fn  (optional, only used when tf_icons is set)
    //   Draws an icon at the current ImGui cursor position.
    //   Receives the icon value and the target size in pixels.
    std::function<void(const _Icon&, float)> icon_draw_fn;

    // rename_commit_fn  (optional, only used when tf_renamable is set)
    //   Called when the user commits a rename.  Receives the node
    //   and the new name string.  Returns true if accepted.
    std::function<bool(node_type&, const std::string&)> rename_commit_fn;

    // context_action_fn  (optional, only used when tf_context is set)
    //   Called when the user selects a context menu action.  Receives
    //   the node and the action bitflag.
    std::function<void(node_type&, unsigned)> context_action_fn;

    // tooltip_fn  (optional)
    //   If set, called on hover to show a tooltip.
    std::function<void(const node_type&)> tooltip_fn;
};


// =============================================================================
//  3.  INTERNAL HELPERS
// =============================================================================

NS_INTERNAL

// -- ancestor line mask --------------------------------------------------
//   To draw correct │ continuation lines, we need to know which ancestor
// depths have a "continuing" sibling below them (i.e. the ancestor at
// that depth is NOT the last child).  We precompute this as a bitmask
// per row by walking the visible entries once.
//
//   Bit i set → draw a │ at indent level i.

// D_MAX_TREE_DEPTH
//   maximum supported nesting for line drawing.
static constexpr std::size_t D_MAX_TREE_DEPTH = 64;

// build_ancestor_mask
//   Builds a 64-bit mask of which ancestor levels need a continuation
// line for the entry at _index.
template<typename _EntryVec>
std::uint64_t
build_ancestor_mask(
    const _EntryVec& _entries,
    std::size_t      _index
)
{
    std::uint64_t mask  = 0;
    std::size_t   depth = _entries[_index].depth;

    // walk forward to find, for each ancestor depth, whether there is
    // a subsequent sibling at that depth
    std::size_t target_depth = depth;

    for (std::size_t i = _index + 1; i < _entries.size(); ++i)
    {
        std::size_t d = _entries[i].depth;

        if (d < target_depth)
        {
            target_depth = d;
        }

        if (d <= depth)
        {
            // found a node at or above our depth — it means our
            // ancestor at depth d has more children
            if (d < D_MAX_TREE_DEPTH)
            {
                mask |= (std::uint64_t(1) << d);
            }

            if (d == 0)
            {
                break;
            }
        }
    }

    return mask;
}

// draw_tree_lines
//   Draws the indent guides (│ ├-- └--) for a single row.
D_INLINE void
draw_tree_lines(
    ImDrawList*        _dl,
    float              _row_x,
    float              _row_y,
    float              _row_h,
    float              _indent_w,
    std::size_t        _depth,
    bool               _is_last,
    std::uint64_t      _ancestor_mask,
    ImU32              _colour,
    float              _thickness
)
{
    if (_depth == 0)
    {
        return;
    }

    float mid_y = _row_y + (_row_h * 0.5f);

    // vertical continuation lines for ancestors
    for (std::size_t d = 0; d < _depth - 1; ++d)
    {
        bool continues = (_ancestor_mask & (std::uint64_t(1) << d)) != 0;

        if (!continues)
        {
            continue;
        }

        float x = _row_x + (static_cast<float>(d) * _indent_w)
                          + (_indent_w * 0.5f);

        _dl->AddLine(ImVec2(x, _row_y),
                     ImVec2(x, _row_y + _row_h),
                     _colour,
                     _thickness);
    }

    // the connector at our parent's depth
    float parent_x = _row_x
                   + (static_cast<float>(_depth - 1) * _indent_w)
                   + (_indent_w * 0.5f);

    // vertical segment: top of row to midpoint (├) or full height (│ above └)
    float vert_top = _row_y;
    float vert_bot = _is_last ? mid_y : (_row_y + _row_h);

    _dl->AddLine(ImVec2(parent_x, vert_top),
                 ImVec2(parent_x, vert_bot),
                 _colour,
                 _thickness);

    // horizontal segment: from parent_x to node position
    float horiz_end = _row_x
                    + (static_cast<float>(_depth) * _indent_w);

    _dl->AddLine(ImVec2(parent_x, mid_y),
                 ImVec2(horiz_end, mid_y),
                 _colour,
                 _thickness);

    return;
}

// draw_expand_arrow
//   Draws a right-pointing (collapsed) or down-pointing (expanded)
// triangle.  Returns true if clicked.
D_INLINE bool
draw_expand_arrow(
    ImDrawList* _dl,
    float       _x,
    float       _y,
    float       _size,
    bool        _expanded,
    ImU32       _colour,
    ImU32       _hover_colour
)
{
    ImVec2 min_pt(_x, _y);
    ImVec2 max_pt(_x + _size, _y + _size);

    // invisible button for click detection
    ImGui::SetCursorScreenPos(min_pt);
    ImGui::InvisibleButton("##arrow", ImVec2(_size, _size));

    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool hovered = ImGui::IsItemHovered();

    ImU32 col = hovered ? _hover_colour : _colour;

    float cx = _x + (_size * 0.5f);
    float cy = _y + (_size * 0.5f);
    float half = _size * 0.3f;

    if (_expanded)
    {
        // down-pointing triangle  ▼
        _dl->AddTriangleFilled(
            ImVec2(cx - half, cy - half * 0.5f),
            ImVec2(cx + half, cy - half * 0.5f),
            ImVec2(cx,        cy + half),
            col);
    }
    else
    {
        // right-pointing triangle  ▶
        _dl->AddTriangleFilled(
            ImVec2(cx - half * 0.5f, cy - half),
            ImVec2(cx + half,        cy),
            ImVec2(cx - half * 0.5f, cy + half),
            col);
    }

    return clicked;
}

// draw_checkbox_tristate
//   Draws a tri-state checkbox.  Returns true if clicked.
D_INLINE bool
draw_checkbox_tristate(
    ImDrawList*                        _dl,
    float                              _x,
    float                              _y,
    float                              _size,
    uxoxo::component::check_state      _state,
    const imgui_tree_style&            _style
)
{
    ImVec2 min_pt(_x, _y);
    ImVec2 max_pt(_x + _size, _y + _size);

    // invisible button for click detection
    ImGui::SetCursorScreenPos(min_pt);
    ImGui::InvisibleButton("##check", ImVec2(_size, _size));

    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    // border
    _dl->AddRect(min_pt,
                 max_pt,
                 _style.check_border,
                 _style.check_rounding,
                 ImDrawFlags_RoundCornersAll,
                 1.5f);

    float pad = _size * 0.2f;

    switch (_state)
    {
        case uxoxo::component::check_state::checked:
        {
            // filled background
            _dl->AddRectFilled(
                ImVec2(min_pt.x + 1.0f, min_pt.y + 1.0f),
                ImVec2(max_pt.x - 1.0f, max_pt.y - 1.0f),
                _style.check_fill,
                _style.check_rounding);

            // checkmark  ✓
            float x1 = _x + pad;
            float y1 = _y + (_size * 0.5f);
            float x2 = _x + (_size * 0.4f);
            float y2 = _y + _size - pad;
            float x3 = _x + _size - pad;
            float y3 = _y + pad;

            _dl->AddPolyline(
                (ImVec2[]){
                    ImVec2(x1, y1),
                    ImVec2(x2, y2),
                    ImVec2(x3, y3)},
                3,
                _style.check_mark,
                ImDrawFlags_None,
                2.0f);

            break;
        }

        case uxoxo::component::check_state::indeterminate:
        {
            // filled background (lighter)
            _dl->AddRectFilled(
                ImVec2(min_pt.x + 1.0f, min_pt.y + 1.0f),
                ImVec2(max_pt.x - 1.0f, max_pt.y - 1.0f),
                _style.check_indet,
                _style.check_rounding);

            // horizontal dash  -
            float dash_y = _y + (_size * 0.5f);

            _dl->AddLine(
                ImVec2(_x + pad, dash_y),
                ImVec2(_x + _size - pad, dash_y),
                _style.check_mark,
                2.0f);

            break;
        }

        case uxoxo::component::check_state::unchecked:
        default:
        {
            // border only (already drawn above)
            break;
        }
    }

    return clicked;
}

// draw_context_menu_items
//   Draws standard context menu entries based on the action bitfield.
// Returns the selected action, or 0 if nothing was selected.
D_INLINE unsigned
draw_context_menu_items(
    unsigned _actions
)
{
    unsigned selected = 0;

    if ( (_actions & uxoxo::component::ctx_open) &&
         (ImGui::MenuItem("Open")) )
    {
        selected = uxoxo::component::ctx_open;
    }

    if ( (_actions & uxoxo::component::ctx_rename) &&
         (ImGui::MenuItem("Rename", "F2")) )
    {
        selected = uxoxo::component::ctx_rename;
    }

    if (_actions & (uxoxo::component::ctx_copy |
                    uxoxo::component::ctx_cut  |
                    uxoxo::component::ctx_paste))
    {
        ImGui::Separator();

        if ( (_actions & uxoxo::component::ctx_copy) &&
             (ImGui::MenuItem("Copy", "Ctrl+C")) )
        {
            selected = uxoxo::component::ctx_copy;
        }

        if ( (_actions & uxoxo::component::ctx_cut) &&
             (ImGui::MenuItem("Cut", "Ctrl+X")) )
        {
            selected = uxoxo::component::ctx_cut;
        }

        if ( (_actions & uxoxo::component::ctx_paste) &&
             (ImGui::MenuItem("Paste", "Ctrl+V")) )
        {
            selected = uxoxo::component::ctx_paste;
        }
    }

    if (_actions & uxoxo::component::ctx_new_child)
    {
        ImGui::Separator();

        if (ImGui::MenuItem("New Child"))
        {
            selected = uxoxo::component::ctx_new_child;
        }
    }

    if (_actions & uxoxo::component::ctx_properties)
    {
        ImGui::Separator();

        if (ImGui::MenuItem("Properties"))
        {
            selected = uxoxo::component::ctx_properties;
        }
    }

    if (_actions & uxoxo::component::ctx_delete)
    {
        ImGui::Separator();

        if (ImGui::MenuItem("Delete", "Del"))
        {
            selected = uxoxo::component::ctx_delete;
        }
    }

    return selected;
}

NS_END  // internal


// =============================================================================
//  3.  IMGUI DRAW TREE VIEW
// =============================================================================

/*
imgui_draw_tree_view
    Draws a tree_view using ImGui and handles all input.

    The function is fully adaptive: it queries features at compile time
via if constexpr and only draws/handles what the tree_view supports.
The caller must provide a label_fn callback that extracts display
text from the _Data type.

Parameter(s):
    _view:  the tree_view to draw (mutated by input).
    _style: visual constants.
    _cb:    application callbacks.
    _id:    ImGui ID scope string (default "##tree_view").
Return:
    none.
*/
template<typename _Data,
         unsigned _Feat,
         typename _Icon>
void
imgui_draw_tree_view(
    uxoxo::component::tree_view<_Data, _Feat, _Icon>&          _view,
    const imgui_tree_style&                                    _style,
    const imgui_tree_callbacks<_Data, _Feat, _Icon>&           _cb,
    const char*                                                _id = "##tree_view"
)
{
    using view_type  = uxoxo::component::tree_view<_Data, _Feat, _Icon>;
    using node_type  = typename view_type::node_type;
    using entry_type = typename view_type::entry_type;

    // ensure visible list is current
    const auto& entries = _view.entries();

    if (entries.empty())
    {
        ImGui::TextDisabled("(empty)");

        return;
    }

    ImGui::PushID(_id);

    ImDrawList* dl       = ImGui::GetWindowDrawList();
    ImVec2      win_pos  = ImGui::GetCursorScreenPos();
    float       avail_w  = ImGui::GetContentRegionAvail().x;
    float       row_h    = _style.row_height;

    // update page_size from available height
    float avail_h = ImGui::GetContentRegionAvail().y;

    if (avail_h > 0.0f)
    {
        _view.page_size = static_cast<std::size_t>(avail_h / row_h);

        if (_view.page_size < 1)
        {
            _view.page_size = 1;
        }
    }

    // -- keyboard input --------------------------------------------------
    //   Only process keyboard when the tree region is focused.

    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (focused)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        {
            if (ImGui::GetIO().KeyShift)
            {
                // shift+up: extend selection
                if constexpr (view_type::is_collapsible)
                {
                    // no special handling needed beyond cursor move
                }

                _view.cursor_up();
                _view.toggle_select_at_cursor();
            }
            else
            {
                _view.cursor_up();
                _view.select_at_cursor();
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        {
            if (ImGui::GetIO().KeyShift)
            {
                _view.cursor_down();
                _view.toggle_select_at_cursor();
            }
            else
            {
                _view.cursor_down();
                _view.select_at_cursor();
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        {
            _view.cursor_left();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            _view.cursor_right();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Home))
        {
            _view.cursor_home();
            _view.select_at_cursor();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_End))
        {
            _view.cursor_end();
            _view.select_at_cursor();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
        {
            _view.page_up();
            _view.select_at_cursor();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
        {
            _view.page_down();
            _view.select_at_cursor();
        }

        // space: toggle collapse or checkbox
        if (ImGui::IsKeyPressed(ImGuiKey_Space))
        {
            if constexpr (view_type::is_checkable)
            {
                _view.toggle_check_at_cursor();
            }
            else if constexpr (view_type::is_collapsible)
            {
                _view.toggle_at_cursor();
            }
        }

        // enter: toggle collapse or open
        if (ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            if constexpr (view_type::is_collapsible)
            {
                _view.toggle_at_cursor();
            }
        }

        // F2: begin rename
        if constexpr (view_type::is_renamable)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_F2))
            {
                auto* cn = _view.cursor_node();

                if ( (cn) &&
                     (_cb.label_fn) )
                {
                    _view.begin_edit_with(_cb.label_fn(cn->data));
                }
            }
        }

        // escape: cancel rename or clear selection
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            if constexpr (view_type::is_renamable)
            {
                if (_view.editing)
                {
                    _view.cancel_edit();
                }
                else
                {
                    _view.clear_selection();
                }
            }
            else
            {
                _view.clear_selection();
            }
        }
    }

    // -- row rendering ---------------------------------------------------
    //   Use ImGuiListClipper for virtual scrolling.

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(entries.size()), row_h);

    while (clipper.Step())
    {
        for (int row_i = clipper.DisplayStart;
             row_i < clipper.DisplayEnd;
             ++row_i)
        {
            std::size_t idx = static_cast<std::size_t>(row_i);
            const entry_type& entry = entries[idx];
            node_type* node = entry.node;

            ImGui::PushID(row_i);

            float row_y = win_pos.y + (static_cast<float>(row_i) * row_h);
            float row_x = win_pos.x;

            // -- background: cursor, selection, hover --------------------
            ImVec2 row_min(row_x, row_y);
            ImVec2 row_max(row_x + avail_w, row_y + row_h);

            bool is_cursor   = (idx == _view.cursor);
            bool is_selected = _view.is_selected(idx);

            if (is_selected)
            {
                dl->AddRectFilled(row_min,
                                  row_max,
                                  _style.selected_bg);
            }

            if (is_cursor)
            {
                dl->AddRectFilled(row_min,
                                  row_max,
                                  _style.cursor_bg,
                                  _style.cursor_rounding);

                dl->AddRect(row_min,
                            row_max,
                            _style.cursor_border,
                            _style.cursor_rounding,
                            ImDrawFlags_RoundCornersAll,
                            _style.cursor_thickness);
            }

            // -- invisible button for whole-row interaction ---------------
            ImGui::SetCursorScreenPos(row_min);
            ImGui::InvisibleButton("##row", ImVec2(avail_w, row_h));

            bool row_hovered = ImGui::IsItemHovered();
            bool row_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            bool row_dbl     = ImGui::IsMouseDoubleClicked(
                                   ImGuiMouseButton_Left) && row_hovered;
            bool row_rclick  = ImGui::IsItemClicked(
                                   ImGuiMouseButton_Right);

            if ( (row_hovered) &&
                 (!is_selected) &&
                 (!is_cursor) )
            {
                dl->AddRectFilled(row_min,
                                  row_max,
                                  _style.hover_bg);
            }

            // handle clicks
            if (row_clicked)
            {
                _view.cursor = idx;

                if (ImGui::GetIO().KeyCtrl)
                {
                    _view.toggle_select_at_cursor();
                }
                else if (ImGui::GetIO().KeyShift)
                {
                    if (!_view.selected.empty())
                    {
                        _view.select_range(_view.selected.front(), idx);
                    }
                    else
                    {
                        _view.select_at_cursor();
                    }
                }
                else
                {
                    _view.select_at_cursor();
                }
            }

            // double-click: toggle expand or begin rename
            if (row_dbl)
            {
                if constexpr (view_type::is_collapsible)
                {
                    if (!node->is_leaf())
                    {
                        uxoxo::component::toggle_expanded(*node);
                        _view.visible_dirty = true;
                    }
                    else if constexpr (view_type::is_renamable)
                    {
                        if ( (node->renamable) &&
                             (_cb.label_fn) )
                        {
                            _view.begin_edit_with(
                                _cb.label_fn(node->data));
                        }
                    }
                }
                else if constexpr (view_type::is_renamable)
                {
                    if ( (node->renamable) &&
                         (_cb.label_fn) )
                    {
                        _view.begin_edit_with(
                            _cb.label_fn(node->data));
                    }
                }
            }

            // right-click: context menu
            if constexpr (view_type::has_context)
            {
                if (row_rclick)
                {
                    _view.cursor = idx;
                    _view.select_at_cursor();

                    ImVec2 mouse = ImGui::GetMousePos();
                    _view.open_context(
                        static_cast<int>(mouse.x),
                        static_cast<int>(mouse.y));
                }
            }

            // -- positioned drawing (layered on top of row bg) -----------
            float cx = row_x;
            float cy_centre = row_y + (row_h * 0.5f);

            // indent
            float indent = static_cast<float>(entry.depth)
                         * _style.indent_width;
            cx += indent;

            // tree lines
            if (_style.draw_lines)
            {
                std::uint64_t mask = internal::build_ancestor_mask(
                    entries, idx);

                internal::draw_tree_lines(
                    dl,
                    row_x,
                    row_y,
                    row_h,
                    _style.indent_width,
                    entry.depth,
                    entry.is_last_child,
                    mask,
                    _style.line_colour,
                    _style.line_thickness);
            }

            // expand/collapse arrow
            if constexpr (view_type::is_collapsible)
            {
                if (entry.has_children)
                {
                    bool arrow_clicked = internal::draw_expand_arrow(
                        dl,
                        cx,
                        row_y + (row_h - _style.arrow_size) * 0.5f,
                        _style.arrow_size,
                        node->expanded,
                        _style.arrow_colour,
                        _style.arrow_hover_colour);

                    if (arrow_clicked)
                    {
                        uxoxo::component::toggle_expanded(*node);
                        _view.visible_dirty = true;
                    }
                }

                cx += _style.arrow_size + _style.item_spacing_x;
            }

            // checkbox
            if constexpr (view_type::is_checkable)
            {
                bool check_clicked = internal::draw_checkbox_tristate(
                    dl,
                    cx,
                    row_y + (row_h - _style.checkbox_size) * 0.5f,
                    _style.checkbox_size,
                    node->checked,
                    _style);

                if (check_clicked)
                {
                    _view.cursor = idx;
                    _view.toggle_check_at_cursor();
                }

                cx += _style.checkbox_size + _style.item_spacing_x;
            }

            // icon
            if constexpr (view_type::has_icons)
            {
                if (_cb.icon_draw_fn)
                {
                    const _Icon& ic =
                        uxoxo::component::effective_icon(*node);

                    float icon_y = row_y
                                 + (row_h - _style.icon_size) * 0.5f;

                    ImGui::SetCursorScreenPos(ImVec2(cx, icon_y));
                    _cb.icon_draw_fn(ic, _style.icon_size);
                }

                cx += _style.icon_size + _style.item_spacing_x;
            }

            // label (or rename input)
            bool is_renaming = false;

            if constexpr (view_type::is_renamable)
            {
                is_renaming = ( (_view.editing) &&
                                (_view.edit_index == idx) );
            }

            float label_y = row_y
                          + (row_h - ImGui::GetTextLineHeight()) * 0.5f;

            if (is_renaming)
            {
                if constexpr (view_type::is_renamable)
                {
                    float input_w = avail_w - (cx - row_x) - 4.0f;

                    if (input_w < 60.0f)
                    {
                        input_w = 60.0f;
                    }

                    ImGui::SetCursorScreenPos(ImVec2(cx, label_y));

                    ImGui::PushStyleColor(
                        ImGuiCol_FrameBg, _style.rename_bg);
                    ImGui::PushStyleColor(
                        ImGuiCol_Border, _style.rename_border);

                    ImGui::SetNextItemWidth(input_w);

                    char buf[256];
                    std::strncpy(buf,
                                 _view.edit_buffer.c_str(),
                                 sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';

                    // auto-focus on first frame
                    ImGui::SetKeyboardFocusHere();

                    bool committed = ImGui::InputText(
                        "##rename",
                        buf,
                        sizeof(buf),
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_AutoSelectAll);

                    _view.edit_buffer = buf;

                    if (committed)
                    {
                        if ( (_cb.rename_commit_fn) &&
                             (_cb.rename_commit_fn(*node,
                                                    _view.edit_buffer)) )
                        {
                            _view.commit_edit();
                        }
                        else
                        {
                            _view.commit_edit();
                        }
                    }

                    // cancel on loss of focus
                    if ( (!ImGui::IsItemActive()) &&
                         (!ImGui::IsItemFocused()) &&
                         (!committed) )
                    {
                        // give it one frame before cancelling
                        // (InputText needs a frame to become active)
                        static bool s_first_frame = true;

                        if (s_first_frame)
                        {
                            s_first_frame = false;
                        }
                        else
                        {
                            _view.cancel_edit();
                            s_first_frame = true;
                        }
                    }

                    ImGui::PopStyleColor(2);
                }
            }
            else
            {
                // normal label
                if (_cb.label_fn)
                {
                    std::string text = _cb.label_fn(node->data);

                    ImGui::SetCursorScreenPos(ImVec2(cx, label_y));
                    ImGui::TextColored(
                        ImGui::ColorConvertU32ToFloat4(_style.label_colour),
                        "%s",
                        text.c_str());
                }
            }

            // tooltip
            if ( (row_hovered) &&
                 (_cb.tooltip_fn) &&
                 (!is_renaming) )
            {
                ImGui::BeginTooltip();
                _cb.tooltip_fn(*node);
                ImGui::EndTooltip();
            }

            ImGui::PopID();
        }
    }

    clipper.End();

    // reserve space for the full virtual list
    ImGui::SetCursorScreenPos(ImVec2(
        win_pos.x,
        win_pos.y + (static_cast<float>(entries.size()) * row_h)));

    // -- context menu ----------------------------------------------------

    if constexpr (view_type::has_context)
    {
        if (_view.context_open)
        {
            ImGui::OpenPopup("##tree_context");
            _view.context_open = false;
        }

        if (ImGui::BeginPopup("##tree_context"))
        {
            auto* ctx_node = _view.context_node();

            if (ctx_node)
            {
                unsigned action = internal::draw_context_menu_items(
                    ctx_node->context_actions);

                if ( (action != 0) &&
                     (_cb.context_action_fn) )
                {
                    _cb.context_action_fn(*ctx_node, action);
                }
            }

            ImGui::EndPopup();
        }
    }

    ImGui::PopID();

    return;
}


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_UI_IMGUI_TREE_DRAW_