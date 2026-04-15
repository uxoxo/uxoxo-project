/*******************************************************************************
* uxoxo [component]                                        glfw_app_window.hpp
*
*   Concrete app_window subclass backed by GLFW + OpenGL 3.  Handles
* platform window creation, event polling, buffer swapping, and Dear ImGui
* platform/renderer backend initialisation.
*
*   Usage:
*     glfw_app_window win("uxoxo", 1280, 720);
*     win.create();
*     while (app_window_run_frame(win, ctx)) { ... }
*     win.destroy();
*
*   The caller provides an imgui_renderer (or any renderer subclass)
* via set_renderer() after create().  ImGui context setup and backend
* binding happen inside create().
*
*   Structure:
*     1.  glfw_app_window class
*     2.  error callback
*
*   REQUIRES: C++17 or later.  GLFW3, OpenGL 3.3+, Dear ImGui with
* GLFW + OpenGL3 backends.
*
*
* path:      /inc/uxoxo/platform/glfw/glfw_app_window.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.12
*******************************************************************************/
#ifndef UXOXO_GLFW_APP_WINDOW_
#define UXOXO_GLFW_APP_WINDOW_ 1

#pragma comment(lib, "opengl32.lib")

// std
#include <cstdio>
#include <string>
// platform
// glfw
#include <GLFW/glfw3.h>
// imgui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../templates/component/window/app_window.hpp"
#include "../../templates/component/window/app_window.hpp"


NS_UXOXO
NS_PLATFORM
NS_GLFW


// =============================================================================
//  1.  GLFW ERROR CALLBACK
// =============================================================================

NS_INTERNAL

    D_INLINE void
    glfw_error_callback(
        int         _error,
        const char* _description
    )
    {
        std::fprintf(stderr,
                     "[uxoxo] GLFW error %d: %s\n",
                     _error,
                     _description);

        return;
    }
NS_END  // internal


// =============================================================================
//  2.  GLFW APP WINDOW
// =============================================================================

// glfw_app_window
//   class: concrete app_window backed by GLFW + OpenGL 3.
class glfw_app_window : public uxoxo::component::app_window
{
public:
    glfw_app_window()
        : app_window("uxoxo", 1280, 720),
          m_window(nullptr)
    {
    }

    glfw_app_window(
		const std::string& _title,
		int                _width,
		int                _height
	)
		: app_window(_title, _width, _height),
		  m_window(nullptr)
	{
	}

    ~glfw_app_window() override
    {
        if (m_window)
        {
            destroy();
        }
    }

    // -- platform lifecycle ------------------------------------------

    /*
    create
        Initialises GLFW, creates an OpenGL window, and sets up the
    Dear ImGui context with GLFW + OpenGL3 backends.

    Parameter(s):
        (none)
    Return:
        true if window creation succeeded, false otherwise.
    */
    bool
    create() override
    {
        glfwSetErrorCallback(internal::glfw_error_callback);

        if (!glfwInit())
        {
            return false;
        }

        // OpenGL 3.3 core
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,
                       GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        m_window = glfwCreateWindow(width(),
                                     height(),
                                     title().c_str(),
                                     nullptr,
                                     nullptr);

        if (!m_window)
        {
            glfwTerminate();

            return false;
        }

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);    // vsync

        // Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // dark theme
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding    = 4.0f;
        style.FrameRounding     = 3.0f;
        style.GrabRounding      = 2.0f;
        style.ScrollbarRounding = 4.0f;

        // init backends
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");

        m_state = uxoxo::component::window_state::visible;

        return true;
    }

    /*
    destroy
        Shuts down ImGui backends, destroys the GLFW window, and
    terminates GLFW.

    Parameter(s):
        (none)
    Return:
        none.
    */
    void
    destroy() override
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }

        glfwTerminate();

        m_state = uxoxo::component::window_state::closed;

        return;
    }

    /*
    poll_events
        Pumps the GLFW event queue and starts a new ImGui frame
    for the GLFW + OpenGL3 backends.

    Parameter(s):
        (none)
    Return:
        true (always succeeds).
    */
    bool
    poll_events() override
    {
        glfwPollEvents();

        // update window size from GLFW (handles resize)
        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(m_window, &w, &h);
        set_size(w, h);

        // new ImGui frame for backends
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        return true;
    }

    /*
    should_close
        Returns whether the GLFW window has been asked to close.

    Parameter(s):
        (none)
    Return:
        true if the window should close.
    */
    bool
    should_close() const override
    {
        return (glfwWindowShouldClose(m_window) != 0);
    }

    /*
    swap_buffers
        Presents the rendered frame by swapping the OpenGL back
    buffer, and clears the viewport for the next frame.

    Parameter(s):
        (none)
    Return:
        none.
    */
    void
    swap_buffers() override
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);

        // clear for next frame
        glViewport(0, 0, width(), height());
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        return;
    }

    // -- accessors ---------------------------------------------------

    GLFWwindow*
    handle() noexcept
    {
        return m_window;
    }

    const GLFWwindow*
    handle() const noexcept
    {
        return m_window;
    }

private:
    GLFWwindow* m_window;
};


NS_END  // glfw
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_GLFW_APP_WINDOW_