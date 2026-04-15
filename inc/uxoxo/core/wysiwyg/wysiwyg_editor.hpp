/*******************************************************************************
* uxoxo [component]                                          wysiwyg_editor.hpp
*
*   Framework-agnostic WYSIWYG editor that operates on any type satisfying
* the ui_tree structural interface (see ui_tree_traits.hpp / concepts.hpp).
* The editor manages selection state, interaction modes, and translates
* user gestures (click, drag, resize, restyle, delete) into tree mutations.
* It does NOT draw anything — it produces a list of "handle descriptors"
* that a renderer-specific overlay can draw however it likes.
*
*   The editor is parameterised on _TreeType, which must satisfy
* ui_tree_traits::is_complete_ui_tree (C++17) or the equivalent C++20
* concept ui_tree_concepts::complete_ui_tree.  This decouples the editor
* from the concrete ui_tree class while preserving full type safety.
*
*   The editor is further decoupled from input and rendering through
* two abstract interfaces:
*
*   - hit_provider:  given a screen coordinate, returns which node (if any)
*                    is at that point.  Each renderer backend implements
*                    this using its own spatial data (ImGui item rects,
*                    retained-mode command buffer hit testing, etc.)
*
*   - edit_overlay<_TreeType>:  receives handle descriptors and interaction
*                    feedback from the editor.  A renderer-specific subclass
*                    draws selection outlines, drag handles, resize grips,
*                    and property panels.  Templated on _TreeType so that
*                    show_property_editor receives a concrete tree reference.
*
*   All modifications flow through _TreeType::apply(), so constraints are
* enforced uniformly and observers are notified via the mutation signal.
*
*   Node identity uses the arena's node_id type (uint32_t) with
* null_node as the sentinel, matching the arena-backed ui_tree.
*
*   Structure:
*     1.  enums (interaction mode, handle kind, drag axis)
*     2.  handle_descriptor (what to draw for an interactive affordance)
*     3.  node_capabilities (what the editor can do to a specific node)
*     4.  node_bounds (screen-space AABB)
*     5.  hit_provider (abstract: screen coord -> node id)
*     6.  edit_overlay<_TreeType> (abstract: draw selection chrome)
*     7.  selection_state (multi-select with primary)
*     8.  drag_state (in-progress drag/resize tracking)
*     9.  wysiwyg_editor<_TreeType> (main class template)
*     10. traits
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/core/wysiwyg/wysiwyg_editor.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.14
*******************************************************************************/

#ifndef UXOXO_COMPONENT_WYSIWYG_EDITOR_
#define UXOXO_COMPONENT_WYSIWYG_EDITOR_ 1

// std
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
#include <optional>
// djinterp
#include <djinterp/core/container/arena/arena.hpp>  // node_id / null_node
// uxoxo
#include "../../uxoxo.hpp"
#include "../tree/ui_tree.hpp"
#include "../tree/ui_tree_traits.hpp"

#if D_ENV_CPP_FEATURE_LANG_CONCEPTS
    #include "../tree/ui_tree_concepts.hpp"
#endif


NS_UXOXO
NS_WYSIWYG


// -- type imports -----------------------------------------------------------
using ui_tree::complete_ui_tree;
using ui_tree::is_complete_ui_tree_v;
using ui_tree::mutation;
using ui_tree::DMutationResult;
using ui_tree::DMutationKind;
using ui_tree::DConstraintKind;
using ui_tree::property_value;
using ui_tree::ui_payload;

// import arena node identity
using djinterp::node_id;
using djinterp::null_node;


// =============================================================================
//  1.  ENUMS
// =============================================================================

// DInteractionMode
//   enum: the current editing mode the WYSIWYG editor is in.
enum class DInteractionMode : std::uint8_t
{
    select,     // clicking selects nodes
    move,       // dragging repositions nodes
    resize,     // dragging resizes nodes
    restyle,    // clicking opens a property editor for a node
    insert      // clicking places a new node from a palette
};

// DHandleKind
//   enum: the type of interactive affordance to draw on a selected node.
enum class DHandleKind : std::uint8_t
{
    selection_outline,
    move_grip,
    resize_n,
    resize_s,
    resize_e,
    resize_w,
    resize_ne,
    resize_nw,
    resize_se,
    resize_sw,
    delete_button,
    restyle_button,
    parent_indicator,
    drop_target
};

// DDragAxis
//   enum: axis constraint for drag operations.
enum class DDragAxis : std::uint8_t
{
    free,
    horizontal,
    vertical
};


// =============================================================================
//  2.  HANDLE DESCRIPTOR
// =============================================================================

// handle_descriptor
//   struct: describes one interactive affordance on a selected node.
struct handle_descriptor
{
    DHandleKind kind;

    // the node this handle belongs to
    node_id     target_id;

    // bounding box in screen coordinates
    float x;
    float y;
    float w;
    float h;

    // whether this handle is currently interactive
    bool enabled;
};


// =============================================================================
//  3.  NODE CAPABILITIES
// =============================================================================

// node_capabilities
//   struct: what the WYSIWYG editor is allowed to do to a node.
struct node_capabilities
{
    bool can_select;
    bool can_move;
    bool can_resize;
    bool can_restyle;
    bool can_remove;
    bool can_receive_children;

    std::vector<std::string> styleable_properties;
};


// =============================================================================
//  4.  NODE BOUNDS
// =============================================================================

// node_bounds
//   struct: screen-space bounding box for a node.
struct node_bounds
{
    float x;
    float y;
    float w;
    float h;
};


// =============================================================================
//  5.  HIT PROVIDER (abstract)
// =============================================================================

// hit_provider
//   class: abstract interface for screen-to-node mapping.
class hit_provider
{
public:
    virtual ~hit_provider() = default;

    // hit_test
    //   returns the id of the topmost node at screen position (_x, _y),
    // or null_node if no node is at that point.
    virtual node_id
    hit_test(
        float _x,
        float _y
    ) const = 0;

    // node_screen_bounds
    //   returns the screen-space bounding box of a node, or nullopt
    // if the node is not currently visible.
    virtual std::optional<node_bounds>
    node_screen_bounds(
        node_id _id
    ) const = 0;
};


// =============================================================================
//  6.  EDIT OVERLAY (abstract, templated on tree type)
// =============================================================================

// edit_overlay
//   class: abstract interface for drawing WYSIWYG affordances.
// Templated on _TreeType so that show_property_editor receives a
// concrete tree reference rather than being coupled to ui_tree.
#if D_ENV_CPP_FEATURE_LANG_CONCEPTS
template<complete_ui_tree _TreeType>
#else
template<typename _TreeType>
#endif
class edit_overlay
{
    static_assert(
        is_complete_ui_tree_v<_TreeType>,
        "_TreeType must satisfy the complete ui_tree interface.");

public:
    virtual ~edit_overlay() = default;

    // begin_overlay
    //   called once per frame before any handle drawing.
    virtual void begin_overlay() = 0;

    // draw_handle
    //   draws a single handle descriptor.
    virtual void draw_handle(const handle_descriptor& _handle) = 0;

    // end_overlay
    //   called once per frame after all handles are drawn.
    virtual void end_overlay() = 0;

    // show_property_editor
    //   opens a property editing panel for a node.
    // Returns true if the user changed a property.
    virtual bool show_property_editor(
        node_id                  _id,
        const node_capabilities& _caps,
        const _TreeType&         _tree) = 0;
};


// =============================================================================
//  7.  SELECTION STATE
// =============================================================================

// selection_state
//   struct: tracks which nodes are currently selected.
struct selection_state
{
    // the primary (most recent) selection, or null_node
    node_id primary;

    // all selected node ids (includes primary)
    std::vector<node_id> selected;

    selection_state()
        : primary(null_node),
          selected()
    {
    }

    // is_selected
    //   returns whether a node is in the selection set.
    bool
    is_selected(
        node_id _id
    ) const
    {
        auto it = std::find(selected.begin(),
                            selected.end(),
                            _id);

        return (it != selected.end());
    }

    // select
    //   makes a node the primary selection.  If _additive is true,
    // adds to the existing selection; otherwise clears first.
    void
    select(
        node_id _id,
        bool    _additive
    )
    {
        if (!_additive)
        {
            selected.clear();
        }

        if (!is_selected(_id))
        {
            selected.push_back(_id);
        }

        primary = _id;

        return;
    }

    // deselect
    //   removes a node from the selection set.  If it was primary,
    // the primary falls back to the last remaining selection.
    void
    deselect(
        node_id _id
    )
    {
        auto it = std::find(selected.begin(),
                            selected.end(),
                            _id);

        if (it != selected.end())
        {
            selected.erase(it);
        }

        if (primary == _id)
        {
            primary = selected.empty()
                ? null_node
                : selected.back();
        }

        return;
    }

    // clear
    //   removes all selections.
    void
    clear()
    {
        selected.clear();
        primary = null_node;

        return;
    }

    // count
    std::size_t
    count() const noexcept
    {
        return selected.size();
    }

    // empty
    bool
    empty() const noexcept
    {
        return selected.empty();
    }
};


// =============================================================================
//  8.  DRAG STATE
// =============================================================================

// drag_state
//   struct: in-progress drag/resize operation.
struct drag_state
{
    DHandleKind handle;
    node_id     target_id;
    DDragAxis   axis;

    float       start_x;
    float       start_y;
    float       current_x;
    float       current_y;

    node_bounds original_bounds;

    bool        active;
};


// =============================================================================
//  9.  WYSIWYG EDITOR
// =============================================================================

// wysiwyg_editor
//   class: framework-agnostic WYSIWYG editor parameterised on the
// semantic UI tree type.  _TreeType must satisfy
// ui_tree_traits::is_complete_ui_tree (or the equivalent C++20
// concept).
#if D_ENV_CPP_FEATURE_LANG_CONCEPTS
template<complete_ui_tree _TreeType>
#else
template<typename _TreeType>
#endif
class wysiwyg_editor
{
    static_assert(
        is_complete_ui_tree_v<_TreeType>,
        "_TreeType must satisfy the complete ui_tree interface. "
        "See ui_tree_traits.hpp for required methods.");

public:
    using tree_type    = _TreeType;
    using overlay_type = edit_overlay<_TreeType>;

    explicit wysiwyg_editor(
        tree_type*    _tree,
        hit_provider* _hits,
        overlay_type* _overlay
    ) noexcept
        : m_tree(_tree),
            m_hits(_hits),
            m_overlay(_overlay),
            m_mode(DInteractionMode::select),
            m_selection(),
            m_drag(std::nullopt)
    {}

    ~wysiwyg_editor() = default;

    // -----------------------------------------------------------------
    //  mode
    // -----------------------------------------------------------------

    void
    set_mode(
        DInteractionMode _mode
    )
    {
        m_mode = _mode;
        m_drag = std::nullopt;

        return;
    }

    DInteractionMode
    mode() const noexcept
    {
        return m_mode;
    }

    // -----------------------------------------------------------------
    //  selection
    // -----------------------------------------------------------------

    const selection_state&
    selection() const noexcept
    {
        return m_selection;
    }

    void
    select_at(
        float _x,
        float _y,
        bool  _additive
    )
    {
        node_id hit = m_hits->hit_test(_x, _y);

        if (hit == null_node)
        {
            if (!_additive)
            {
                m_selection.clear();
            }

            return;
        }

        node_capabilities caps = query_capabilities(hit);

        if (!caps.can_select)
        {
            return;
        }

        m_selection.select(hit, _additive);

        return;
    }

    void
    select_node(
        node_id _id,
        bool    _additive
    )
    {
        if (!m_tree->valid(_id))
        {
            return;
        }

        node_capabilities caps = query_capabilities(_id);

        if (!caps.can_select)
        {
            return;
        }

        m_selection.select(_id, _additive);

        return;
    }

    void
    deselect_node(
        node_id _id
    )
    {
        m_selection.deselect(_id);

        return;
    }

    void
    clear_selection()
    {
        m_selection.clear();

        return;
    }

    void
    select_parent()
    {
        if (m_selection.primary == null_node)
        {
            return;
        }

        node_id parent = m_tree->parent_of(m_selection.primary);

        if (parent != null_node)
        {
            m_selection.select(parent, false);
        }

        return;
    }

    void
    select_first_child()
    {
        if (m_selection.primary == null_node)
        {
            return;
        }

        node_id child = m_tree->first_child_of(m_selection.primary);

        if (child != null_node)
        {
            m_selection.select(child, false);
        }

        return;
    }

    // -----------------------------------------------------------------
    //  capability queries
    // -----------------------------------------------------------------

    node_capabilities
    query_capabilities(
        node_id _id
    ) const
    {
        node_capabilities caps;

        caps.can_select           = false;
        caps.can_move             = false;
        caps.can_resize           = false;
        caps.can_restyle          = false;
        caps.can_remove           = false;
        caps.can_receive_children = false;

        if (!m_tree->valid(_id))
        {
            return caps;
        }

        const auto& pl = m_tree->payload(_id);
        DConstraintKind ck = pl.node_constraint.kind;

        switch (ck)
        {
            case DConstraintKind::fixed:
            {
                caps.can_select = true;
                break;
            }

            case DConstraintKind::required:
            {
                caps.can_select  = true;
                caps.can_restyle = true;
                break;
            }

            case DConstraintKind::styleable:
            {
                caps.can_select  = true;
                caps.can_resize  = true;
                caps.can_restyle = true;
                break;
            }

            case DConstraintKind::movable:
            {
                caps.can_select  = true;
                caps.can_move    = true;
                caps.can_resize  = true;
                caps.can_restyle = true;
                break;
            }

            case DConstraintKind::optional:
            case DConstraintKind::generated:
            {
                caps.can_select  = true;
                caps.can_move    = true;
                caps.can_resize  = true;
                caps.can_restyle = true;
                caps.can_remove  = true;
                break;
            }
        }

        caps.can_receive_children = pl.can_receive_children;

        // collect styleable property names
        for (const auto& [key, pc] : pl.prop_constraints)
        {
            if ( (pc.kind == DConstraintKind::styleable) ||
                 (pc.kind == DConstraintKind::optional)  ||
                 (pc.kind == DConstraintKind::generated) )
            {
                caps.styleable_properties.push_back(key);
            }
        }

        // properties without explicit constraints are styleable
        // if the node constraint permits restyling
        if (caps.can_restyle)
        {
            for (const auto& [key, val] : pl.properties)
            {
                auto pc_it = pl.prop_constraints.find(key);

                if (pc_it == pl.prop_constraints.end())
                {
                    caps.styleable_properties.push_back(key);
                }
            }
        }

        return caps;
    }

    // -----------------------------------------------------------------
    //  gestures -> mutations
    // -----------------------------------------------------------------

    void
    on_pointer_down(
        float _x,
        float _y,
        bool  _additive
    )
    {
        node_id hit = m_hits->hit_test(_x, _y);

        // click on empty space clears selection
        if (hit == null_node)
        {
            if (!_additive)
            {
                m_selection.clear();
            }

            m_drag = std::nullopt;

            return;
        }

        node_capabilities caps = query_capabilities(hit);

        if (!caps.can_select)
        {
            return;
        }

        // toggle on additive click of already-selected node
        if ( (_additive) &&
             (m_selection.is_selected(hit)) )
        {
            m_selection.deselect(hit);

            return;
        }

        m_selection.select(hit, _additive);

        // begin potential drag
        bool can_drag =
            ( (m_mode == DInteractionMode::move)   && (caps.can_move) )   ||
            ( (m_mode == DInteractionMode::resize)  && (caps.can_resize) ) ||
            ( (m_mode == DInteractionMode::select)  && (caps.can_move) );

        if (can_drag)
        {
            drag_state ds;

            ds.target_id = hit;
            ds.start_x   = _x;
            ds.start_y   = _y;
            ds.current_x = _x;
            ds.current_y = _y;
            ds.active    = false;
            ds.axis      = DDragAxis::free;

            if ( (m_mode == DInteractionMode::resize) &&
                 (caps.can_resize) )
            {
                ds.handle = DHandleKind::resize_se;
            }
            else
            {
                ds.handle = DHandleKind::move_grip;
            }

            auto bounds = m_hits->node_screen_bounds(hit);

            if (bounds.has_value())
            {
                ds.original_bounds = bounds.value();
            }
            else
            {
                ds.original_bounds = {0.0f, 0.0f, 0.0f, 0.0f};
            }

            m_drag = ds;
        }

        return;
    }

    void
    on_pointer_move(
        float _x,
        float _y
    )
    {
        if (!m_drag.has_value())
        {
            return;
        }

        m_drag->current_x = _x;
        m_drag->current_y = _y;

        if ( (!m_drag->active) &&
             (drag_exceeds_threshold()) )
        {
            m_drag->active = true;
        }

        return;
    }

    std::optional<DMutationResult>
    on_pointer_up(
        float _x,
        float _y
    )
    {
        std::optional<DMutationResult> result = std::nullopt;

        if (!m_drag.has_value())
        {
            return result;
        }

        m_drag->current_x = _x;
        m_drag->current_y = _y;

        if (m_drag->active)
        {
            if (m_drag->handle == DHandleKind::move_grip)
            {
                result = finalise_move_drag();
            }
            else
            {
                result = finalise_resize_drag();
            }
        }

        m_drag = std::nullopt;

        return result;
    }

    std::optional<DMutationResult>
    on_delete()
    {
        if (m_selection.primary == null_node)
        {
            return std::nullopt;
        }

        node_capabilities caps = query_capabilities(m_selection.primary);

        if (!caps.can_remove)
        {
            return std::nullopt;
        }

        node_id to_remove = m_selection.primary;

        m_selection.deselect(to_remove);

        mutation mut = mutation::make_remove(to_remove);
        DMutationResult r = m_tree->apply(mut);

        return r;
    }

    void
    on_restyle()
    {
        if (m_selection.primary == null_node)
        {
            return;
        }

        node_capabilities caps = query_capabilities(m_selection.primary);

        if (!caps.can_restyle)
        {
            return;
        }

        m_overlay->show_property_editor(m_selection.primary,
                                        caps,
                                        *m_tree);

        return;
    }

    // -----------------------------------------------------------------
    //  handle generation
    // -----------------------------------------------------------------

    std::vector<handle_descriptor>
    generate_handles() const
    {
        std::vector<handle_descriptor> handles;

        for (node_id id : m_selection.selected)
        {
            if (!m_tree->valid(id))
            {
                continue;
            }

            node_capabilities caps = query_capabilities(id);

            build_handles_for_node(id, caps, handles);
        }

        return handles;
    }

    // -----------------------------------------------------------------
    //  frame integration
    // -----------------------------------------------------------------

    void
    update()
    {
        std::vector<handle_descriptor> handles = generate_handles();

        m_overlay->begin_overlay();

        for (const auto& h : handles)
        {
            m_overlay->draw_handle(h);
        }

        m_overlay->end_overlay();

        return;
    }

    // -----------------------------------------------------------------
    //  drag queries
    // -----------------------------------------------------------------

    bool
    dragging() const noexcept
    {
        return ( (m_drag.has_value()) &&
                 (m_drag->active) );
    }

    const std::optional<drag_state>&
    current_drag() const noexcept
    {
        return m_drag;
    }

    // -----------------------------------------------------------------
    //  drop target
    // -----------------------------------------------------------------

    node_id
    find_drop_target(
        float _x,
        float _y
    ) const
    {
        if (!m_drag.has_value())
        {
            return null_node;
        }

        node_id hit = m_hits->hit_test(_x, _y);

        if (hit == null_node)
        {
            return null_node;
        }

        // exclude the dragged node itself
        if (hit == m_drag->target_id)
        {
            return null_node;
        }

        // exclude descendants of the dragged node
        if (m_tree->is_descendant_of(hit, m_drag->target_id))
        {
            return null_node;
        }

        // target must accept children
        if (!m_tree->payload(hit).can_receive_children)
        {
            return null_node;
        }

        return hit;
    }

    std::size_t
    drop_position(
        node_id _target_id,
        float   _x,
        float   _y
    ) const
    {
        (void)_x;

        std::size_t child_count = m_tree->child_count_of(_target_id);

        if (child_count == 0)
        {
            return 0;
        }

        node_id child = m_tree->first_child_of(_target_id);
        std::size_t idx = 0;

        while (child != null_node)
        {
            auto bounds = m_hits->node_screen_bounds(child);

            if (bounds.has_value())
            {
                float midpoint = bounds->y + (bounds->h * 0.5f);

                if (_y < midpoint)
                {
                    return idx;
                }
            }

            child = m_tree->next_sibling_of(child);
            ++idx;
        }

        return child_count;
    }

private:
    void
    build_handles_for_node(
        node_id                         _id,
        const node_capabilities&        _caps,
        std::vector<handle_descriptor>& _out
    ) const
    {
        auto bounds_opt = m_hits->node_screen_bounds(_id);

        if (!bounds_opt.has_value())
        {
            return;
        }

        node_bounds b = bounds_opt.value();

        static constexpr float D_HANDLE_SIZE = 8.0f;
        static constexpr float D_BUTTON_SIZE = 16.0f;

        // selection outline -- always present for selected nodes
        handle_descriptor outline;
        outline.kind      = DHandleKind::selection_outline;
        outline.target_id = _id;
        outline.x         = b.x;
        outline.y         = b.y;
        outline.w         = b.w;
        outline.h         = b.h;
        outline.enabled   = true;
        _out.push_back(outline);

        // move grip -- centred at top edge
        {
            handle_descriptor grip;
            grip.kind      = DHandleKind::move_grip;
            grip.target_id = _id;
            grip.x         = b.x + (b.w * 0.5f) - (D_HANDLE_SIZE * 0.5f);
            grip.y         = b.y - D_HANDLE_SIZE;
            grip.w         = D_HANDLE_SIZE;
            grip.h         = D_HANDLE_SIZE;
            grip.enabled   = _caps.can_move;
            _out.push_back(grip);
        }

        // resize handles -- eight compass directions
        auto push_resize = [&](DHandleKind _kind,
                               float       _hx,
                               float       _hy)
        {
            handle_descriptor rh;
            rh.kind      = _kind;
            rh.target_id = _id;
            rh.x         = _hx - (D_HANDLE_SIZE * 0.5f);
            rh.y         = _hy - (D_HANDLE_SIZE * 0.5f);
            rh.w         = D_HANDLE_SIZE;
            rh.h         = D_HANDLE_SIZE;
            rh.enabled   = _caps.can_resize;
            _out.push_back(rh);
        };

        float cx = b.x + (b.w * 0.5f);
        float cy = b.y + (b.h * 0.5f);
        float rx = b.x + b.w;
        float by = b.y + b.h;

        push_resize(DHandleKind::resize_n,  cx,   b.y);
        push_resize(DHandleKind::resize_s,  cx,   by);
        push_resize(DHandleKind::resize_e,  rx,   cy);
        push_resize(DHandleKind::resize_w,  b.x,  cy);
        push_resize(DHandleKind::resize_ne, rx,   b.y);
        push_resize(DHandleKind::resize_nw, b.x,  b.y);
        push_resize(DHandleKind::resize_se, rx,   by);
        push_resize(DHandleKind::resize_sw, b.x,  by);

        // delete button -- top-right corner outside the bounds
        {
            handle_descriptor del;
            del.kind      = DHandleKind::delete_button;
            del.target_id = _id;
            del.x         = rx;
            del.y         = b.y - D_BUTTON_SIZE;
            del.w         = D_BUTTON_SIZE;
            del.h         = D_BUTTON_SIZE;
            del.enabled   = _caps.can_remove;
            _out.push_back(del);
        }

        // restyle button -- top-right, offset left from delete
        {
            handle_descriptor rs;
            rs.kind      = DHandleKind::restyle_button;
            rs.target_id = _id;
            rs.x         = rx - D_BUTTON_SIZE;
            rs.y         = b.y - D_BUTTON_SIZE;
            rs.w         = D_BUTTON_SIZE;
            rs.h         = D_BUTTON_SIZE;
            rs.enabled   = _caps.can_restyle;
            _out.push_back(rs);
        }

        // parent indicator -- small mark at top-left
        {
            node_id parent = m_tree->parent_of(_id);

            if (parent != null_node)
            {
                handle_descriptor pi;
                pi.kind      = DHandleKind::parent_indicator;
                pi.target_id = _id;
                pi.x         = b.x - D_HANDLE_SIZE;
                pi.y         = b.y - D_HANDLE_SIZE;
                pi.w         = D_HANDLE_SIZE;
                pi.h         = D_HANDLE_SIZE;
                pi.enabled   = true;
                _out.push_back(pi);
            }
        }

        return;
    }

    std::optional<DMutationResult>
    finalise_move_drag()
    {
        node_id target = find_drop_target(m_drag->current_x,
                                          m_drag->current_y);

        if (target == null_node)
        {
            return std::nullopt;
        }

        std::size_t pos = drop_position(target,
                                        m_drag->current_x,
                                        m_drag->current_y);

        mutation mut = mutation::make_move(m_drag->target_id,
                                           target,
                                           pos);

        DMutationResult r = m_tree->apply(mut);

        return r;
    }

    std::optional<DMutationResult>
    finalise_resize_drag()
    {
        float dx = m_drag->current_x - m_drag->start_x;
        float dy = m_drag->current_y - m_drag->start_y;

        float new_w = m_drag->original_bounds.w;
        float new_h = m_drag->original_bounds.h;

        // adjust based on which resize handle is active
        switch (m_drag->handle)
        {
            case DHandleKind::resize_e:
            case DHandleKind::resize_ne:
            case DHandleKind::resize_se:
            {
                new_w += dx;
                break;
            }

            case DHandleKind::resize_w:
            case DHandleKind::resize_nw:
            case DHandleKind::resize_sw:
            {
                new_w -= dx;
                break;
            }

            default:
                break;
        }

        switch (m_drag->handle)
        {
            case DHandleKind::resize_s:
            case DHandleKind::resize_se:
            case DHandleKind::resize_sw:
            {
                new_h += dy;
                break;
            }

            case DHandleKind::resize_n:
            case DHandleKind::resize_ne:
            case DHandleKind::resize_nw:
            {
                new_h -= dy;
                break;
            }

            default:
                break;
        }

        // clamp to minimum
        static constexpr float D_MIN_DIMENSION = 16.0f;

        if (new_w < D_MIN_DIMENSION) { new_w = D_MIN_DIMENSION; }
        if (new_h < D_MIN_DIMENSION) { new_h = D_MIN_DIMENSION; }

        mutation mut_w = mutation::make_set_property(
            m_drag->target_id,
            "width",
            static_cast<double>(new_w));

        mutation mut_h = mutation::make_set_property(
            m_drag->target_id,
            "height",
            static_cast<double>(new_h));

        std::vector<mutation> batch = {mut_w, mut_h};

        DMutationResult r = m_tree->apply_batch(batch);

        return r;
    }

    bool
    drag_exceeds_threshold() const
    {
        if (!m_drag.has_value())
        {
            return false;
        }

        float dx = m_drag->current_x - m_drag->start_x;
        float dy = m_drag->current_y - m_drag->start_y;
        float dist_sq = (dx * dx) + (dy * dy);

        return (dist_sq > (D_DRAG_THRESHOLD * D_DRAG_THRESHOLD));
    }

    tree_type*                    m_tree;
    hit_provider*                 m_hits;
    overlay_type*                 m_overlay;

    DInteractionMode              m_mode;
    selection_state               m_selection;
    std::optional<drag_state>     m_drag;

    static constexpr float        D_DRAG_THRESHOLD = 4.0f;
};


// =============================================================================
//  10. TRAITS
// =============================================================================

namespace wysiwyg_traits
{
NS_INTERNAL

    // has_hit_test
    //   trait: detects _Type::hit_test(float, float) const.
    template<typename _Type,
             typename = void>
    struct has_hit_test : std::false_type
    {
    };

    template<typename _Type>
    struct has_hit_test<
        _Type,
        std::void_t<decltype(std::declval<const _Type>().hit_test(
            0.0f, 0.0f))>>
        : std::true_type
    {
    };

    // has_node_screen_bounds
    //   trait: detects _Type::node_screen_bounds(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_node_screen_bounds : std::false_type
    {
    };

    template<typename _Type>
    struct has_node_screen_bounds<
        _Type,
        std::void_t<decltype(std::declval<const _Type>().node_screen_bounds(
            std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // has_begin_overlay
    //   trait: detects _Type::begin_overlay().
    template<typename _Type,
             typename = void>
    struct has_begin_overlay : std::false_type
    {
    };

    template<typename _Type>
    struct has_begin_overlay<
        _Type,
        std::void_t<decltype(std::declval<_Type>().begin_overlay())>>
        : std::true_type
    {
    };

    // has_draw_handle
    //   trait: detects _Type::draw_handle(const handle_descriptor&).
    template<typename _Type,
             typename = void>
    struct has_draw_handle : std::false_type
    {
    };

    template<typename _Type>
    struct has_draw_handle<
        _Type,
        std::void_t<decltype(std::declval<_Type>().draw_handle(
            std::declval<const handle_descriptor&>()))>>
        : std::true_type
    {
    };

    // has_end_overlay
    //   trait: detects _Type::end_overlay().
    template<typename _Type,
             typename = void>
    struct has_end_overlay : std::false_type
    {
    };

    template<typename _Type>
    struct has_end_overlay<
        _Type,
        std::void_t<decltype(std::declval<_Type>().end_overlay())>>
        : std::true_type
    {
    };

    // has_show_property_editor
    //   trait: detects _Type::show_property_editor(node_id,
    //          const node_capabilities&, const _TreeType&).
    template<typename _Type,
             typename _TreeType,
             typename = void>
    struct has_show_property_editor : std::false_type
    {
    };

    template<typename _Type,
             typename _TreeType>
    struct has_show_property_editor<
        _Type,
        _TreeType,
        std::void_t<decltype(std::declval<_Type>().show_property_editor(
            std::declval<node_id>(),
            std::declval<const node_capabilities&>(),
            std::declval<const _TreeType&>()))>>
        : std::true_type
    {
    };

NS_END  // internal

// is_hit_provider
//   trait: true if a type satisfies the hit_provider interface --
// hit_test(float, float) and node_screen_bounds(node_id).
template<typename _Type>
struct is_hit_provider : std::conjunction<
    internal::has_hit_test<_Type>,
    internal::has_node_screen_bounds<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_hit_provider_v =
    is_hit_provider<_Type>::value;

// is_edit_overlay
//   trait: true if a type satisfies the edit_overlay interface for
// a given tree type -- begin_overlay(), draw_handle(), end_overlay(),
// and show_property_editor(node_id, const node_capabilities&,
// const _TreeType&).
template<typename _Type,
         typename _TreeType>
struct is_edit_overlay : std::conjunction<
    internal::has_begin_overlay<_Type>,
    internal::has_draw_handle<_Type>,
    internal::has_end_overlay<_Type>,
    internal::has_show_property_editor<_Type, _TreeType>>
{
};

template<typename _Type,
         typename _TreeType>
D_INLINE constexpr bool is_edit_overlay_v =
    is_edit_overlay<_Type, _TreeType>::value;

}   // namespace wysiwyg_traits


NS_END  // wysiwyg
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_WYSIWYG_EDITOR_