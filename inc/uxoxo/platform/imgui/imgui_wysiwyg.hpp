/*******************************************************************************
* uxoxo [ui]                                                  imgui_wysiwyg.hpp
*
*   ImGui-specific implementations of the WYSIWYG editor's two abstract
* interfaces: hit_provider and edit_overlay.  Together with the framework-
* agnostic wysiwyg_editor, these form a complete in-editor experience for
* ImGui-based applications.
*
*   imgui_hit_provider
*     Maintains a per-frame map of node_id → screen rect.  During each
*     frame the materializer's draw handlers call register_rect() to
*     record where each node was drawn.  The editor then queries this
*     map for hit testing and bounding-box lookups.  The map is cleared
*     at the start of each frame via begin_frame().
*
*   imgui_edit_overlay<_TreeType>
*     Draws selection outlines, resize grips, move handles, delete and
*     restyle buttons, parent indicators, and drop-target highlights
*     using ImGui's foreground ImDrawList.  Also provides a basic
*     property editor panel via ImGui windows.  Templated on the
*     semantic tree type to match edit_overlay<_TreeType>.
*
*   imgui_wysiwyg_context<_TreeType>
*     Convenience aggregator that owns the hit_provider, overlay, and
*     editor together, and provides a single update() call for the
*     frame loop.  Templated on the semantic tree type.
*
*   Structure:
*     1.  imgui_hit_provider
*     2.  imgui_overlay_style (colour/size constants)
*     3.  imgui_edit_overlay<_TreeType>
*     4.  imgui_wysiwyg_context<_TreeType>
*
*   REQUIRES: C++17 or later, Dear ImGui.
*
*
* path:      /inc/uxoxo/platform/imgui/imgui_wysiwyg.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.14
*******************************************************************************/

#ifndef UXOXO_IMGUI_WYSIWYG_
#define UXOXO_IMGUI_WYSIWYG_ 1

// std
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
// imgui
#include "imgui.h"
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../core/wysiwyg/wysiwyg_editor.hpp"
#include "../../core/tree/ui_tree.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI


// -- type imports -----------------------------------------------------------
using djinterp::node_id;
using djinterp::null_node;

using uxoxo::ui_tree::complete_ui_tree;

using uxoxo::wysiwyg::edit_overlay;
using uxoxo::wysiwyg::hit_provider;
using uxoxo::wysiwyg::wysiwyg_editor;
using uxoxo::wysiwyg::handle_descriptor;
using uxoxo::wysiwyg::node_bounds;
using uxoxo::wysiwyg::node_capabilities;
using uxoxo::wysiwyg::DInteractionMode;
using uxoxo::wysiwyg::DHandleKind;


// =============================================================================
//  1.  IMGUI HIT PROVIDER
// =============================================================================

// imgui_hit_provider
//   class: concrete hit_provider backed by a per-frame rect map.
class imgui_hit_provider final : public hit_provider
{
public:
    imgui_hit_provider() = default;
    ~imgui_hit_provider() override = default;

    // -- per-frame lifecycle -----------------------------------------

    // begin_frame
    //   clears all registered rects.  Call at the start of each frame,
    // before any draw handlers execute.
    void
    begin_frame();

    // register_rect
    //   records the screen-space bounding box for a node.  Called by
    // materializer draw handlers after drawing a node's ImGui content.
    void
    register_rect(
        node_id _id,
        float   _x,
        float   _y,
        float   _w,
        float   _h
    );

    // register_rect (ImVec2 overload)
    //   convenience overload taking ImGui min/max corners.
    void
    register_rect(
        node_id _id,
        ImVec2  _min,
        ImVec2  _max
    );

    // -- hit_provider interface --------------------------------------

    node_id
    hit_test(
        float _x,
        float _y
    ) const override;

    std::optional<node_bounds>
    node_screen_bounds(
        node_id _id
    ) const override;

    // -- queries -----------------------------------------------------

    // rect_count
    //   returns how many node rects are registered this frame.
    std::size_t
    rect_count() const noexcept;

private:
    // stored_rect
    //   struct: a registered screen rect with draw order for z-sorting.
    struct stored_rect
    {
        float         x;
        float         y;
        float         w;
        float         h;
        std::uint32_t draw_order;
    };

    std::unordered_map<node_id,
                       stored_rect>      m_rects;
    std::uint32_t                        m_draw_counter = 0;
};


// -- imgui_hit_provider inline definitions -----------------------------------

// begin_frame
//   clears all registered rects and resets the draw counter.
D_INLINE void
imgui_hit_provider::begin_frame()
{
    m_rects.clear();
    m_draw_counter = 0;

    return;
}

// register_rect
//   records the screen-space bounding box for a node.
D_INLINE void
imgui_hit_provider::register_rect(
    node_id _id,
    float   _x,
    float   _y,
    float   _w,
    float   _h
)
{
    stored_rect sr;
    sr.x          = _x;
    sr.y          = _y;
    sr.w          = _w;
    sr.h          = _h;
    sr.draw_order = m_draw_counter++;

    m_rects[_id] = sr;

    return;
}

// register_rect (ImVec2 overload)
//   convenience overload taking ImGui min/max corners.
D_INLINE void
imgui_hit_provider::register_rect(
    node_id _id,
    ImVec2  _min,
    ImVec2  _max
)
{
    register_rect(_id,
                  _min.x,
                  _min.y,
                  _max.x - _min.x,
                  _max.y - _min.y);

    return;
}

// hit_test
//   returns the topmost node at a screen position.  When multiple
// rects overlap, the one with the highest draw order (drawn last,
// therefore on top) wins.
D_INLINE node_id
imgui_hit_provider::hit_test(
    float _x,
    float _y
) const
{
    node_id       best_id    = null_node;
    std::uint32_t best_order = 0;
    bool          found      = false;

    for (const auto& [id, sr] : m_rects)
    {
        bool inside =
            ( (_x >= sr.x)        &&
              (_y >= sr.y)        &&
              (_x <  sr.x + sr.w) &&
              (_y <  sr.y + sr.h) );

        if (!inside)
        {
            continue;
        }

        if ( (!found) ||
             (sr.draw_order > best_order) )
        {
            best_id    = id;
            best_order = sr.draw_order;
            found      = true;
        }
    }

    return best_id;
}

// node_screen_bounds
//   returns the screen-space bounding box of a node, or nullopt
// if the node was not drawn this frame.
D_INLINE std::optional<node_bounds>
imgui_hit_provider::node_screen_bounds(
    node_id _id
) const
{
    auto it = m_rects.find(_id);

    if (it == m_rects.end())
    {
        return std::nullopt;
    }

    node_bounds nb;
    nb.x = it->second.x;
    nb.y = it->second.y;
    nb.w = it->second.w;
    nb.h = it->second.h;

    return nb;
}

// rect_count
//   returns how many node rects are registered this frame.
D_INLINE std::size_t
imgui_hit_provider::rect_count() const noexcept
{
    return m_rects.size();
}


// =============================================================================
//  2.  IMGUI OVERLAY STYLE
// =============================================================================

// imgui_overlay_style
//   struct: visual constants for overlay drawing.
struct imgui_overlay_style
{
    // selection outline
    ImU32 selection_colour         = IM_COL32(66, 135, 245, 220);
    float selection_thickness      = 2.0f;
    float selection_rounding       = 2.0f;

    // primary selection (thicker/brighter)
    ImU32 primary_colour           = IM_COL32(66, 135, 245, 255);
    float primary_thickness        = 3.0f;

    // resize handles
    ImU32 handle_fill              = IM_COL32(255, 255, 255, 240);
    ImU32 handle_border            = IM_COL32(66, 135, 245, 255);
    float handle_border_thickness  = 1.5f;

    // move grip
    ImU32 grip_colour              = IM_COL32(66, 135, 245, 200);

    // disabled handles
    ImU32 disabled_colour          = IM_COL32(160, 160, 160, 100);

    // delete button
    ImU32 delete_bg                = IM_COL32(220, 60, 60, 220);
    ImU32 delete_fg                = IM_COL32(255, 255, 255, 255);

    // restyle button
    ImU32 restyle_bg               = IM_COL32(60, 180, 120, 220);
    ImU32 restyle_fg               = IM_COL32(255, 255, 255, 255);

    // parent indicator
    ImU32 parent_colour            = IM_COL32(180, 180, 180, 160);

    // drop target
    ImU32 drop_target_colour       = IM_COL32(66, 245, 135, 140);
    float drop_target_thickness    = 2.5f;

    // drag ghost
    ImU32 drag_ghost_colour        = IM_COL32(66, 135, 245, 80);
};


// =============================================================================
//  3.  IMGUI EDIT OVERLAY
// =============================================================================

// imgui_edit_overlay
//   class: concrete edit_overlay that draws via ImDrawList.
// Templated on _TreeType to match the edit_overlay<_TreeType>
// base class from wysiwyg_editor.hpp.
#if D_ENV_CPP_FEATURE_LANG_CONCEPTS
template<complete_ui_tree _TreeType>
#else
template<typename _TreeType>
#endif
class imgui_edit_overlay final : public edit_overlay<_TreeType>
{
    static_assert(
        uxoxo::ui_tree::is_complete_ui_tree_v<_TreeType>,
        "_TreeType must satisfy the complete ui_tree interface.");

public:
    imgui_edit_overlay()
        : m_draw_list(nullptr),
          m_primary(null_node),
          m_property_editor_open(false)
    {
    }

    ~imgui_edit_overlay() override = default;

    // -- style access ------------------------------------------------

    imgui_overlay_style&
    style() noexcept
    {
        return m_style;
    }

    const imgui_overlay_style&
    style() const noexcept
    {
        return m_style;
    }

    // -- primary selection tracking ----------------------------------
    //   The overlay needs to know which node is primary so it can
    // draw a thicker outline.  The context sets this each frame.

    void
    set_primary(
        node_id _id
    )
    {
        m_primary = _id;

        return;
    }

    // -- edit_overlay interface --------------------------------------

    void
    begin_overlay() override
    {
        m_draw_list = ImGui::GetForegroundDrawList();

        return;
    }

    void
    draw_handle(
        const handle_descriptor& _handle
    ) override
    {
        if (!m_draw_list)
        {
            return;
        }

        switch (_handle.kind)
        {
            case DHandleKind::selection_outline:
            {
                draw_selection_outline(_handle);
                break;
            }

            case DHandleKind::move_grip:
            {
                draw_move_grip(_handle);
                break;
            }

            case DHandleKind::resize_n:
            case DHandleKind::resize_s:
            case DHandleKind::resize_e:
            case DHandleKind::resize_w:
            case DHandleKind::resize_ne:
            case DHandleKind::resize_nw:
            case DHandleKind::resize_se:
            case DHandleKind::resize_sw:
            {
                draw_resize_handle(_handle);
                break;
            }

            case DHandleKind::delete_button:
            {
                draw_delete_button(_handle);
                break;
            }

            case DHandleKind::restyle_button:
            {
                draw_restyle_button(_handle);
                break;
            }

            case DHandleKind::parent_indicator:
            {
                draw_parent_indicator(_handle);
                break;
            }

            case DHandleKind::drop_target:
            {
                draw_drop_target(_handle);
                break;
            }
        }

        return;
    }

    void
    end_overlay() override
    {
        m_draw_list = nullptr;

        return;
    }

    bool
    show_property_editor(
        node_id                  _id,
        const node_capabilities& _caps,
        const _TreeType&         _tree
    ) override
    {
        (void)_id;
        (void)_caps;
        (void)_tree;

        // TODO: implement ImGui property editor window
        return false;
    }

private:
    void
    draw_selection_outline(
        const handle_descriptor& _handle
    )
    {
        bool is_primary = (_handle.target_id == m_primary);

        ImU32 colour = is_primary
            ? m_style.primary_colour
            : m_style.selection_colour;

        float thickness = is_primary
            ? m_style.primary_thickness
            : m_style.selection_thickness;

        m_draw_list->AddRect(
            ImVec2(_handle.x, _handle.y),
            ImVec2(_handle.x + _handle.w,
                   _handle.y + _handle.h),
            colour,
            m_style.selection_rounding,
            ImDrawFlags_RoundCornersAll,
            thickness);

        return;
    }

    void
    draw_resize_handle(
        const handle_descriptor& _handle
    )
    {
        ImU32 fill   = _handle.enabled
            ? m_style.handle_fill
            : m_style.disabled_colour;

        ImU32 border = _handle.enabled
            ? m_style.handle_border
            : m_style.disabled_colour;

        ImVec2 min_p(_handle.x, _handle.y);
        ImVec2 max_p(_handle.x + _handle.w,
                     _handle.y + _handle.h);

        m_draw_list->AddRectFilled(min_p, max_p, fill);
        m_draw_list->AddRect(min_p,
                             max_p,
                             border,
                             0.0f,
                             ImDrawFlags_RoundCornersAll,
                             m_style.handle_border_thickness);

        return;
    }

    void
    draw_move_grip(
        const handle_descriptor& _handle
    )
    {
        ImU32 colour = _handle.enabled
            ? m_style.grip_colour
            : m_style.disabled_colour;

        float cx = _handle.x + (_handle.w * 0.5f);
        float cy = _handle.y + (_handle.h * 0.5f);
        float r  = _handle.w * 0.4f;

        m_draw_list->AddCircleFilled(
            ImVec2(cx, cy), r, colour);

        return;
    }

    void
    draw_delete_button(
        const handle_descriptor& _handle
    )
    {
        ImU32 bg = _handle.enabled
            ? m_style.delete_bg
            : m_style.disabled_colour;

        ImVec2 min_p(_handle.x, _handle.y);
        ImVec2 max_p(_handle.x + _handle.w,
                     _handle.y + _handle.h);

        m_draw_list->AddRectFilled(min_p, max_p, bg, 2.0f);

        // draw X
        if (_handle.enabled)
        {
            float pad = 3.0f;

            m_draw_list->AddLine(
                ImVec2(_handle.x + pad, _handle.y + pad),
                ImVec2(max_p.x - pad, max_p.y - pad),
                m_style.delete_fg,
                1.5f);

            m_draw_list->AddLine(
                ImVec2(max_p.x - pad, _handle.y + pad),
                ImVec2(_handle.x + pad, max_p.y - pad),
                m_style.delete_fg,
                1.5f);
        }

        return;
    }

    void
    draw_restyle_button(
        const handle_descriptor& _handle
    )
    {
        ImU32 bg = _handle.enabled
            ? m_style.restyle_bg
            : m_style.disabled_colour;

        ImVec2 min_p(_handle.x, _handle.y);
        ImVec2 max_p(_handle.x + _handle.w,
                     _handle.y + _handle.h);

        m_draw_list->AddRectFilled(min_p, max_p, bg, 2.0f);

        return;
    }

    void
    draw_parent_indicator(
        const handle_descriptor& _handle
    )
    {
        float cx = _handle.x + (_handle.w * 0.5f);
        float cy = _handle.y + (_handle.h * 0.5f);
        float r  = _handle.w * 0.3f;

        m_draw_list->AddCircleFilled(
            ImVec2(cx, cy), r, m_style.parent_colour);

        return;
    }

    void
    draw_drop_target(
        const handle_descriptor& _handle
    )
    {
        m_draw_list->AddRect(
            ImVec2(_handle.x, _handle.y),
            ImVec2(_handle.x + _handle.w,
                   _handle.y + _handle.h),
            m_style.drop_target_colour,
            2.0f,
            ImDrawFlags_RoundCornersAll,
            m_style.drop_target_thickness);

        return;
    }

    imgui_overlay_style m_style;
    ImDrawList*         m_draw_list;
    node_id             m_primary;
    bool                m_property_editor_open;
};


// =============================================================================
//  4.  IMGUI WYSIWYG CONTEXT
// =============================================================================

// imgui_wysiwyg_context
//   class: convenience aggregator owning the hit provider, overlay,
// and editor.  Provides a single-call frame integration point.
// Templated on the semantic tree type.
#if D_ENV_CPP_FEATURE_LANG_CONCEPTS
template<complete_ui_tree _TreeType>
#else
template<typename _TreeType>
#endif
class imgui_wysiwyg_context
{
    static_assert(
        uxoxo::ui_tree::is_complete_ui_tree_v<_TreeType>,
        "_TreeType must satisfy the complete ui_tree interface.");

public:
    using tree_type    = _TreeType;
    using overlay_type = imgui_edit_overlay<_TreeType>;
    using editor_type  = wysiwyg_editor<_TreeType>;

    explicit imgui_wysiwyg_context(
            tree_type* _tree
        )
            : m_hits(),
              m_overlay(),
              m_editor(_tree, &m_hits, &m_overlay)
        {
        }

    ~imgui_wysiwyg_context() = default;

    imgui_wysiwyg_context(const imgui_wysiwyg_context&) = delete;
    imgui_wysiwyg_context& operator=(const imgui_wysiwyg_context&) = delete;

    // -- accessors ---------------------------------------------------

    imgui_hit_provider&
    hits() noexcept
    {
        return m_hits;
    }

    overlay_type&
    overlay() noexcept
    {
        return m_overlay;
    }

    editor_type&
    editor() noexcept
    {
        return m_editor;
    }

    // -- frame lifecycle ---------------------------------------------

    // begin_frame
    //   clears the hit provider's rect map.  Call before any
    // materializer draw handlers run.
    void
    begin_frame()
    {
        m_hits.begin_frame();

        return;
    }

    // end_frame
    //   sets the primary selection on the overlay, then runs the
    // editor update (handle generation + overlay drawing).
    // Call after all materializer draw handlers have completed.
    void
    end_frame()
    {
        m_overlay.set_primary(m_editor.selection().primary);
        m_editor.update();

        return;
    }

    // -- input forwarding --------------------------------------------
    //   Thin wrappers that translate ImGui input state into editor
    // gesture calls.  Call process_input() once per frame between
    // begin_frame() and end_frame().

    void
    process_input()
    {
        ImGuiIO& io = ImGui::GetIO();

        // pointer down
        if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) &&
             (!io.WantCaptureMouse) )
        {
            ImVec2 pos = io.MousePos;
            bool additive = io.KeyShift;

            m_editor.on_pointer_down(pos.x, pos.y, additive);
        }

        // pointer move
        if ( (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) &&
             (!io.WantCaptureMouse) )
        {
            ImVec2 pos = io.MousePos;

            m_editor.on_pointer_move(pos.x, pos.y);
        }

        // pointer up
        if ( (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) &&
             (!io.WantCaptureMouse) )
        {
            ImVec2 pos = io.MousePos;

            m_editor.on_pointer_up(pos.x, pos.y);
        }

        return;
    }

private:
    imgui_hit_provider m_hits;
    overlay_type       m_overlay;
    editor_type        m_editor;
};


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_WYSIWYG_