/*******************************************************************************
* uxoxo [component]                                           window_panel.hpp
*
*   A window_panel represents a logical subdivision within a platform window.
* Panels are the hosting surface for views — each panel holds a reference to
* a single view (table_view, future list_view, etc.) and asks the renderer
* to draw it.
*
*   Panels carry layout metadata (position, size, docking) that the platform
* window uses to arrange them.  The panel itself is framework-agnostic; the
* concrete renderer decides how to realise docking, tabs, and splits.
*
*   Structure:
*     1.  enums (dock_position, panel_state)
*     2.  window_panel struct
*     3.  panel operations (free functions)
*     4.  traits
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/component/window/window_panel.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_COMPONENT_WINDOW_PANEL_
#define UXOXO_COMPONENT_WINDOW_PANEL_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../renderer.hpp"
#include "../../render_context.hpp"


NS_UXOXO
NS_COMPONENT


// =============================================================================
//  1.  ENUMS
// =============================================================================

// dock_position
//   enum: preferred docking location within the parent window.
enum class dock_position : std::uint8_t
{
    none,
    left,
    right,
    top,
    bottom,
    center,
    floating
};

// panel_state
//   enum: visibility and lifecycle state of a panel.
enum class panel_state : std::uint8_t
{
    hidden,
    visible,
    collapsed,
    detached
};


// =============================================================================
//  2.  WINDOW PANEL
// =============================================================================

// window_panel
//   struct: a logical subdivision within a platform window that hosts
// a single view component.
struct window_panel
{
    // identity
    std::string name;
    std::string title;

    // layout
    float          x      = 0.0f;
    float          y      = 0.0f;
    float          width  = 0.0f;
    float          height = 0.0f;
    dock_position  dock   = dock_position::none;
    panel_state    state  = panel_state::visible;

    // flags
    bool closable   = true;
    bool resizable  = true;
    bool movable    = true;
    bool scrollable = true;

    // hosted view (type-erased)
    //   the panel stores a void* to the view and the type_index
    // identifying its concrete type.  The renderer uses the
    // type_index to look up the registered draw handler.
    void*           view_ptr  = nullptr;
    std::type_index view_type = std::type_index(typeid(void));

    // draw callback
    //   populated by panel_bind_view; calls through the renderer's
    // component registry.
    using draw_fn = std::function<bool(render_context&)>;
    draw_fn on_draw;
};


// =============================================================================
//  3.  PANEL OPERATIONS
// =============================================================================

// panel_bind_view
//   function: binds a concrete view to a panel and wires the draw
// callback through the renderer's component registry.  The view
// must outlive the panel.
template<typename _ViewType>
void
panel_bind_view(
    window_panel& _panel,
    _ViewType&    _view,
    renderer&     _renderer
)
{
    _panel.view_ptr  = static_cast<void*>(&_view);
    _panel.view_type = std::type_index(typeid(_ViewType));

    // capture the renderer and view for the draw callback
    _panel.on_draw = [&_renderer, &_view](
        render_context& _ctx
    ) -> bool
    {
        return _renderer.draw_component(_view,
                                        _ctx);
    };

    return;
}

// panel_unbind_view
//   function: detaches the view from a panel, clearing the draw
// callback.
D_INLINE void
panel_unbind_view(
    window_panel& _panel
)
{
    _panel.view_ptr  = nullptr;
    _panel.view_type = std::type_index(typeid(void));
    _panel.on_draw   = nullptr;

    return;
}

// panel_draw
//   function: draws the panel's bound view if visible and bound.
// Returns false if the panel is hidden, has no bound view, or
// drawing failed.
D_INLINE bool
panel_draw(
    window_panel&   _panel,
    render_context& _ctx
)
{
    // skip hidden or collapsed panels
    if ( (_panel.state == panel_state::hidden) ||
         (_panel.state == panel_state::collapsed) )
    {
        return false;
    }

    // require a bound draw callback
    if (!_panel.on_draw)
    {
        return false;
    }

    return _panel.on_draw(_ctx);
}

// panel_show
//   function: makes a panel visible.
D_INLINE void
panel_show(
    window_panel& _panel
)
{
    _panel.state = panel_state::visible;

    return;
}

// panel_hide
//   function: hides a panel.
D_INLINE void
panel_hide(
    window_panel& _panel
)
{
    _panel.state = panel_state::hidden;

    return;
}

// panel_collapse
//   function: collapses a panel to its title bar.
D_INLINE void
panel_collapse(
    window_panel& _panel
)
{
    _panel.state = panel_state::collapsed;

    return;
}

// panel_detach
//   function: detaches a panel into a floating window.
D_INLINE void
panel_detach(
    window_panel& _panel
)
{
    _panel.state = panel_state::detached;
    _panel.dock  = dock_position::floating;

    return;
}

// panel_has_view
//   function: returns whether the panel has a bound view.
D_INLINE bool
panel_has_view(
    const window_panel& _panel
) noexcept
{
    return (_panel.view_ptr != nullptr);
}

// panel_is_visible
//   function: returns whether the panel is in a drawable state.
D_INLINE bool
panel_is_visible(
    const window_panel& _panel
) noexcept
{
    return ( (_panel.state == panel_state::visible) ||
             (_panel.state == panel_state::detached) );
}


// =============================================================================
//  4.  TRAITS
// =============================================================================

namespace window_panel_traits
{
NS_INTERNAL

    // has_name_member
    //   trait: detects a name field.
    template<typename _Type,
             typename = void>
    struct has_name_member : std::false_type
    {};

    template<typename _Type>
    struct has_name_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().name)>>
        : std::true_type
    {};

    // has_view_ptr_member
    //   trait: detects a view_ptr field.
    template<typename _Type,
             typename = void>
    struct has_view_ptr_member : std::false_type
    {};

    template<typename _Type>
    struct has_view_ptr_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().view_ptr)>>
        : std::true_type
    {};

    // has_on_draw_member
    //   trait: detects an on_draw callback field.
    template<typename _Type,
             typename = void>
    struct has_on_draw_member : std::false_type
    {};

    template<typename _Type>
    struct has_on_draw_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().on_draw)>>
        : std::true_type
    {};

NS_END  // internal

// is_window_panel
//   trait: true if a type satisfies the window_panel structural interface.
template<typename _Type>
struct is_window_panel : std::conjunction<
    internal::has_name_member<_Type>,
    internal::has_view_ptr_member<_Type>,
    internal::has_on_draw_member<_Type>>
{};

template<typename _Type>
D_INLINE constexpr bool is_window_panel_v =
    is_window_panel<_Type>::value;

}   // namespace window_panel_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_WINDOW_PANEL_
