/*******************************************************************************
* uxoxo [component]                                             app_window.hpp
*
*   Platform-level window that owns a collection of panels and drives the
* render loop through an abstract renderer.  An app_window represents a
* single OS-visible window — it has a title, dimensions, position, and a
* lifecycle (create → show → tick → close).
*
*   The window is framework-agnostic at this level.  Platform-specific
* subclasses (e.g. glfw_app_window, sdl_app_window) implement the virtual
* methods to create an actual OS window, pump events, and manage the
* graphics context.  The base class provides panel management and the
* render dispatch loop.
*
*   Structure:
*     1.  enums (window_state)
*     2.  app_window (abstract base)
*     3.  panel management (free functions)
*     4.  render loop helpers
*     5.  traits
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/component/window/app_window.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_COMPONENT_APP_WINDOW_
#define UXOXO_COMPONENT_APP_WINDOW_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../renderer.hpp"
#include "../../render_context.hpp"
#include "./window_panel.hpp"


NS_UXOXO
NS_COMPONENT


// =============================================================================
//  1.  ENUMS
// =============================================================================

// window_state
//   enum: lifecycle state of the platform window.
enum class window_state : std::uint8_t
{
    created,
    visible,
    minimized,
    maximized,
    hidden,
    closing,
    closed
};


// =============================================================================
//  2.  APP WINDOW
// =============================================================================

// app_window
//   class: abstract platform window owning panels and a renderer.
class app_window
{
public:
    using size_type = std::size_t;

    virtual ~app_window() = default;

    // platform lifecycle (implemented by concrete subclass)
    virtual bool create()  = 0;
    virtual void destroy() = 0;
    virtual bool poll_events() = 0;
    virtual bool should_close() const = 0;
    virtual void swap_buffers() = 0;

    // window properties
    const std::string&
    title() const noexcept
    {
        return m_title;
    }

    void
    set_title(
        const std::string& _title
    )
    {
        m_title = _title;

        return;
    }

    int  width()  const noexcept { return m_width;  }
    int  height() const noexcept { return m_height; }

    void
    set_size(
        int _width,
        int _height
    )
    {
        m_width  = _width;
        m_height = _height;

        return;
    }

    window_state
    state() const noexcept
    {
        return m_state;
    }

    // renderer access
    renderer*
    get_renderer() noexcept
    {
        return m_renderer;
    }

    void
    set_renderer(
        renderer* _renderer
    )
    {
        m_renderer = _renderer;

        return;
    }

    // panel management
    std::vector<window_panel>&
    panels() noexcept
    {
        return m_panels;
    }

    const std::vector<window_panel>&
    panels() const noexcept
    {
        return m_panels;
    }

    // tick
    //   drives one frame: begin_frame → draw all panels → end_frame.
    // Returns false if the renderer is not set or begin_frame fails.
    bool
    tick(
        render_context& _ctx
    )
    {
        if (!m_renderer)
        {
            return false;
        }

        // update context viewport from window dimensions
        _ctx.viewport_width  = static_cast<float>(m_width);
        _ctx.viewport_height = static_cast<float>(m_height);

        // begin frame
        if (!m_renderer->begin_frame(_ctx))
        {
            return false;
        }

        // draw each visible panel
        for (auto& panel : m_panels)
        {
            panel_draw(panel,
                       _ctx);
        }

        // end frame
        m_renderer->end_frame(_ctx);

        return true;
    }

protected:
    app_window() = default;

    app_window(
            const std::string& _title,
            int                _width,
            int                _height
        ) noexcept
            : m_title(_title),
              m_width(_width),
              m_height(_height),
              m_state(window_state::created),
              m_renderer(nullptr)
        {}

    window_state m_state = window_state::created;

private:
    std::string               m_title;
    int                       m_width    = 800;
    int                       m_height   = 600;
    renderer*                 m_renderer = nullptr;
    std::vector<window_panel> m_panels;
};


// =============================================================================
//  3.  PANEL MANAGEMENT (free functions)
// =============================================================================

// app_window_add_panel
//   function: adds a panel to the window and returns a reference to
// it.  The panel is appended to the end of the panel list.
D_INLINE window_panel&
app_window_add_panel(
    app_window&        _window,
    const std::string& _name,
    const std::string& _title
)
{
    window_panel panel;
    panel.name  = _name;
    panel.title = _title;

    _window.panels().push_back(std::move(panel));

    return _window.panels().back();
}

// app_window_find_panel
//   function: finds a panel by name.  Returns nullptr if not found.
D_INLINE window_panel*
app_window_find_panel(
    app_window&        _window,
    const std::string& _name
)
{
    for (auto& panel : _window.panels())
    {
        if (panel.name == _name)
        {
            return &panel;
        }
    }

    return nullptr;
}

// app_window_find_panel (const overload)
//   function: finds a panel by name (const).  Returns nullptr if not
// found.
D_INLINE const window_panel*
app_window_find_panel(
    const app_window&  _window,
    const std::string& _name
)
{
    for (const auto& panel : _window.panels())
    {
        if (panel.name == _name)
        {
            return &panel;
        }
    }

    return nullptr;
}

// app_window_remove_panel
//   function: removes a panel by name.  Returns true if a panel was
// removed.
D_INLINE bool
app_window_remove_panel(
    app_window&        _window,
    const std::string& _name
)
{
    auto& panels = _window.panels();

    auto it = std::remove_if(
        panels.begin(),
        panels.end(),
        [&_name](const window_panel& _p)
        {
            return (_p.name == _name);
        });

    if (it == panels.end())
    {
        return false;
    }

    panels.erase(it,
                 panels.end());

    return true;
}

// app_window_panel_count
//   function: returns the number of panels.
D_INLINE std::size_t
app_window_panel_count(
    const app_window& _window
) noexcept
{
    return _window.panels().size();
}

// app_window_visible_panel_count
//   function: returns the number of visible panels.
D_INLINE std::size_t
app_window_visible_panel_count(
    const app_window& _window
)
{
    std::size_t count = 0;

    for (const auto& panel : _window.panels())
    {
        if (panel_is_visible(panel))
        {
            ++count;
        }
    }

    return count;
}


// =============================================================================
//  4.  RENDER LOOP HELPERS
// =============================================================================

// app_window_run_frame
//   function: drives a single frame — polls events, ticks the
// renderer, and swaps buffers.  Returns false if the window should
// close.
D_INLINE bool
app_window_run_frame(
    app_window&     _window,
    render_context& _ctx
)
{
    // poll platform events
    if ( (!_window.poll_events()) ||
         (_window.should_close()) )
    {
        return false;
    }

    // render
    _window.tick(_ctx);

    // present
    _window.swap_buffers();

    return true;
}


// =============================================================================
//  5.  TRAITS
// =============================================================================

namespace app_window_traits
{
NS_INTERNAL

    // has_title_method
    //   trait: detects a title() method.
    template<typename _Type,
             typename = void>
    struct has_title_method : std::false_type
    {};

    template<typename _Type>
    struct has_title_method<
        _Type,
        std::void_t<decltype(std::declval<const _Type>().title())>>
        : std::true_type
    {};

    // has_panels_method
    //   trait: detects a panels() method.
    template<typename _Type,
             typename = void>
    struct has_panels_method : std::false_type
    {};

    template<typename _Type>
    struct has_panels_method<
        _Type,
        std::void_t<decltype(std::declval<_Type>().panels())>>
        : std::true_type
    {};

    // has_tick_method
    //   trait: detects a tick() method.
    template<typename _Type,
             typename = void>
    struct has_tick_method : std::false_type
    {};

    template<typename _Type>
    struct has_tick_method<
        _Type,
        std::void_t<decltype(std::declval<_Type>().tick(
            std::declval<render_context&>()))>>
        : std::true_type
    {};

    // has_should_close_method
    //   trait: detects a should_close() method.
    template<typename _Type,
             typename = void>
    struct has_should_close_method : std::false_type
    {};

    template<typename _Type>
    struct has_should_close_method<
        _Type,
        std::void_t<decltype(std::declval<const _Type>().should_close())>>
        : std::true_type
    {};

NS_END  // internal

// is_app_window
//   trait: true if a type satisfies the app_window structural interface.
template<typename _Type>
struct is_app_window : std::conjunction<
    internal::has_title_method<_Type>,
    internal::has_panels_method<_Type>,
    internal::has_tick_method<_Type>,
    internal::has_should_close_method<_Type>>
{};

template<typename _Type>
D_INLINE constexpr bool is_app_window_v =
    is_app_window<_Type>::value;

}   // namespace app_window_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_APP_WINDOW_
