/*******************************************************************************
* uxoxo [component]                                              renderer.hpp
*
*   Framework-agnostic renderer interface with a component registry.  A
* renderer translates view structs (table_view, future list_view, etc.) into
* draw calls for a specific backend.  The component registry maps view types
* (identified via std::type_index) to type-erased draw handlers, so new view
* types can be registered without modifying the renderer itself.
*
*   Concrete renderers (imgui_renderer, qt_renderer, tui_renderer) inherit
* from renderer and register their own draw handlers at construction time.
*
*   Structure:
*     1.  draw_handler (type-erased draw callback)
*     2.  component_registry (type_index → draw_handler map)
*     3.  renderer (abstract base)
*     4.  free functions (render_component dispatch)
*     5.  traits
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/component/renderer/renderer.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_COMPONENT_RENDERER_
#define UXOXO_COMPONENT_RENDERER_ 1

// std
#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
// imgui
#include "imgui.h"
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../uxoxo.hpp"
#include "./renderer.hpp"
#include "./render_context.hpp"


NS_UXOXO
NS_COMPONENT


// =============================================================================
//  1.  DRAW HANDLER
// =============================================================================
//   A draw_handler is a type-erased callable that draws a single component.
// The renderer casts the void* back to the concrete view type inside the
// registered lambda.

// draw_handler
//   type: signature for a type-erased component draw callback.
// Parameters are: (void* view, render_context& ctx).
// Returns true if the component was drawn successfully.
using draw_handler = std::function<bool(void*,
                                        render_context&)>;


// =============================================================================
//  2.  COMPONENT REGISTRY
// =============================================================================
//   Maps std::type_index to draw_handler.  Each renderer backend populates
// this with its own handlers at construction time.

// component_registry
//   class: type-indexed registry of component draw handlers.
class component_registry
{
public:
    // register_handler
    //   registers a draw handler for a specific view type.  The
    // handler receives a void* that it must cast to _ViewType*.
    // Overwrites any previously registered handler for the same
    // type.
    template<typename _ViewType>
    void
    register_handler(
        draw_handler _handler
    )
    {
        m_handlers[std::type_index(typeid(_ViewType))] =
            std::move(_handler);

        return;
    }

    // register_typed_handler
    //   convenience wrapper that accepts a callable taking a
    // typed reference instead of void*.  Wraps it in the
    // type-erased draw_handler signature.
    template<typename _ViewType,
             typename _Fn>
    void
    register_typed_handler(
        _Fn _fn
    )
    {
        register_handler<_ViewType>(
            [fn = std::move(_fn)](void*            _view_ptr,
                                  render_context&  _ctx) -> bool
            {
                if (!_view_ptr)
                {
                    return false;
                }

                return fn(*static_cast<_ViewType*>(_view_ptr),
                          _ctx);
            });

        return;
    }

    // find_handler
    //   returns a pointer to the registered draw_handler for a
    // type, or nullptr if none is registered.
    template<typename _ViewType>
    const draw_handler*
    find_handler() const
    {
        auto it = m_handlers.find(
            std::type_index(typeid(_ViewType)));

        if (it == m_handlers.end())
        {
            return nullptr;
        }

        return &(it->second);
    }

    // has_handler
    //   returns whether a draw handler is registered for a type.
    template<typename _ViewType>
    bool
    has_handler() const
    {
        return (m_handlers.count(
            std::type_index(typeid(_ViewType))) > 0);
    }

    // handler_count
    //   returns the number of registered handlers.
    std::size_t
    handler_count() const noexcept
    {
        return m_handlers.size();
    }

    // clear
    //   removes all registered handlers.
    void
    clear()
    {
        m_handlers.clear();

        return;
    }

private:
    std::unordered_map<std::type_index,
                       draw_handler> m_handlers;
};


// =============================================================================
//  3.  RENDERER (abstract base)
// =============================================================================
//   A renderer owns a component_registry and provides the frame lifecycle:
// begin_frame → draw components → end_frame.  Concrete backends override
// the virtual methods to set up and tear down their per-frame state.

// renderer
//   class: abstract base for all rendering backends.
class renderer
{
public:
    virtual ~renderer() = default;

    // frame lifecycle
    virtual bool begin_frame(render_context& _ctx) = 0;
    virtual void end_frame(render_context&   _ctx) = 0;

    // registry access
    component_registry&
    registry() noexcept
    {
        return m_registry;
    }

    const component_registry&
    registry() const noexcept
    {
        return m_registry;
    }

    // draw_component
    //   dispatches a view to its registered draw handler.
    // Returns false if no handler is registered for the type.
    template<typename _ViewType>
    bool
    draw_component(
        _ViewType&      _view,
        render_context& _ctx
    )
    {
        const auto* handler = m_registry.find_handler<_ViewType>();

        if (!handler)
        {
            return false;
        }

        return (*handler)(static_cast<void*>(&_view),
                          _ctx);
    }

protected:
    // derived renderers register handlers in their constructors
    renderer() = default;

private:
    component_registry m_registry;
};


// =============================================================================
//  4.  FREE FUNCTIONS
// =============================================================================

// render_component
//   function: convenience free function that dispatches a view through
// a renderer's registry.
template<typename _ViewType>
bool
render_component(
    renderer&       _renderer,
    _ViewType&      _view,
    render_context& _ctx
)
{
    return _renderer.draw_component(_view,
                                    _ctx);
}


// =============================================================================
//  5.  TRAITS
// =============================================================================

namespace renderer_traits
{
NS_INTERNAL

    // has_registry_member
    //   trait: detects a registry() method.
    template<typename _Type,
             typename = void>
    struct has_registry_member : std::false_type
    {};

    template<typename _Type>
    struct has_registry_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().registry())>>
        : std::true_type
    {};

    // has_begin_frame
    //   trait: detects a begin_frame method.
    template<typename _Type,
             typename = void>
    struct has_begin_frame : std::false_type
    {};

    template<typename _Type>
    struct has_begin_frame<
        _Type,
        std::void_t<decltype(std::declval<_Type>().begin_frame(
            std::declval<render_context&>()))>>
        : std::true_type
    {};

    // has_end_frame
    //   trait: detects an end_frame method.
    template<typename _Type,
             typename = void>
    struct has_end_frame : std::false_type
    {};

    template<typename _Type>
    struct has_end_frame<
        _Type,
        std::void_t<decltype(std::declval<_Type>().end_frame(
            std::declval<render_context&>()))>>
        : std::true_type
    {};

NS_END  // internal

// is_renderer
//   trait: true if a type satisfies the renderer structural interface.
template<typename _Type>
struct is_renderer : std::conjunction<
    internal::has_registry_member<_Type>,
    internal::has_begin_frame<_Type>,
    internal::has_end_frame<_Type>>
{};

template<typename _Type>
D_INLINE constexpr bool is_renderer_v =
    is_renderer<_Type>::value;

}   // namespace renderer_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_RENDERER_