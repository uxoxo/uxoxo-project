/*******************************************************************************
* uxoxo [component]                                          imgui_renderer.hpp
*
*   Concrete renderer backend for Dear ImGui.  Subclasses the framework-
* agnostic renderer and:
*
*   - Registers draw handlers for table_view and database_table_view_pair
*     in the component registry at construction time
*   - Implements begin_frame / end_frame around ImGui::NewFrame /
*     ImGui::Render
*   - Provides imgui_render_panel, which wraps a window_panel in an
*     ImGui::Begin / ImGui::End window, translating panel flags (closable,
*     resizable, movable) to ImGuiWindowFlags
*   - Stores the ImGuiContext* in the render_context's backend_context
*     field so that draw handlers can access it if needed
*
*   Platform integration (GLFW, SDL, etc.) is NOT handled here.  The
* caller is responsible for initialising the ImGui platform and rendering
* backends before constructing imgui_renderer, and for calling the
* platform-specific render hooks between begin_frame and end_frame (or
* after end_frame as appropriate for the backend).
*
*   Structure:
*     1.  imgui_renderer class
*     2.  panel rendering helper
*     3.  convenience: imgui_render_all_panels
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included before
* this header.
*
*
* path:      /inc/uxoxo/platform/imgui/imgui_renderer.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_COMPONENT_IMGUI_RENDERER_
#define UXOXO_COMPONENT_IMGUI_RENDERER_ 1

// std
#include <cstddef>
// imgui
#include "imgui.h"
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../templates/renderer.hpp"
#include "../../templates/render_context.hpp"
#include "../../templates/component/table/table_view.hpp"
#include "../../templates/component/table/database/database_table_view.hpp"
#include "../../templates/component/window/window_panel.hpp"
#include "./table/imgui_table_draw.hpp"
#include "./table/database/imgui_database_table_draw.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI


using uxoxo::component::renderer;
using uxoxo::component::render_context;
using uxoxo::component::window_panel;

// =============================================================================
//  1.  IMGUI RENDERER
// =============================================================================

// imgui_renderer
//   class: concrete Dear ImGui rendering backend.  Registers component
// draw handlers and drives the ImGui frame lifecycle.
class imgui_renderer : public renderer
{
public:
    // constructor
    //   registers all built-in component draw handlers.
    imgui_renderer()
    {
        register_builtin_handlers();

        return;
    }

    ~imgui_renderer() override = default;

    // begin_frame
    //   starts a new ImGui frame.  Stores the current ImGuiContext
    // in the render_context for draw handlers that need it.
    bool
    begin_frame(
        render_context& _ctx
    ) override
    {
        _ctx.backend_context = ImGui::GetCurrentContext();

        if (!_ctx.backend_context)
        {
            return false;
        }

        ImGui::NewFrame();

        return true;
    }

    // end_frame
    //   finalises the ImGui frame.  The caller is responsible for
    // calling the platform-specific render backend (e.g.
    // ImGui_ImplOpenGL3_RenderDrawData) after this.
    void
    end_frame(
        render_context& _ctx
    ) override
    {
        (void)_ctx;

        ImGui::Render();

        return;
    }

private:
    // register_builtin_handlers
    //   wires the built-in draw handlers into the component registry.
    void
    register_builtin_handlers()
    {
        // table_view handler
        registry().register_typed_handler<table_view>(
            [](table_view&      _view,
               render_context&  _ctx) -> bool
            {
                return imgui_draw_table_view(_view,
                                             _ctx);
            });

        // database_table_view_pair handler
        registry().register_typed_handler<database_table_view_pair>(
            [](database_table_view_pair& _pair,
               render_context&           _ctx) -> bool
            {
                return imgui_draw_database_table_view(_pair,
                                                       _ctx);
            });

        return;
    }
};


// =============================================================================
//  2.  PANEL RENDERING HELPER
// =============================================================================

// imgui_render_panel
//   function: wraps a window_panel in an ImGui::Begin / ImGui::End
// window, translating panel properties to ImGuiWindowFlags.  Calls
// the panel's on_draw callback inside the ImGui window.  Returns
// true if the panel was drawn and interaction occurred.
D_INLINE bool
imgui_render_panel(
    window_panel&   _panel,
    render_context& _ctx
)
{
    // skip non-visible panels
    if (!panel_is_visible(_panel))
    {
        return false;
    }

    // skip panels with no bound view
    if (!_panel.on_draw)
    {
        return false;
    }

    // translate panel flags to ImGuiWindowFlags
    ImGuiWindowFlags flags = 0;

    if (!_panel.resizable)
    {
        flags |= ImGuiWindowFlags_NoResize;
    }

    if (!_panel.movable)
    {
        flags |= ImGuiWindowFlags_NoMove;
    }

    if (!_panel.scrollable)
    {
        flags |= ImGuiWindowFlags_NoScrollbar;
    }

    // set initial size and position if specified
    if ( (_panel.width > 0.0f) && (_panel.height > 0.0f) )
    {
        ImGui::SetNextWindowSize(
            ImVec2(_panel.width,
                   _panel.height),
            ImGuiCond_FirstUseEver);
    }

    if ( (_panel.x > 0.0f) || (_panel.y > 0.0f) )
    {
        ImGui::SetNextWindowPos(
            ImVec2(_panel.x,
                   _panel.y),
            ImGuiCond_FirstUseEver);
    }

    // closable window
    bool open = true;
    bool* p_open = _panel.closable ? &open : nullptr;

    bool result = false;

    if (ImGui::Begin(_panel.title.c_str(),
                     p_open,
                     flags))
    {
        result = _panel.on_draw(_ctx);
    }

    ImGui::End();

    // handle close
    if ( (_panel.closable) && (!open) )
    {
        panel_hide(_panel);
    }

    return result;
}


// =============================================================================
//  3.  CONVENIENCE: RENDER ALL PANELS
// =============================================================================

// imgui_render_all_panels
//   function: iterates over a panel collection and renders each one
// as an ImGui window.  Suitable for calling from app_window::tick
// or a custom render loop.
template<typename _PanelContainer>
void
imgui_render_all_panels(
    _PanelContainer& _panels,
    render_context&  _ctx
)
{
    for (auto& panel : _panels)
    {
        imgui_render_panel(panel,
                           _ctx);
    }

    return;
}


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_RENDERER_
