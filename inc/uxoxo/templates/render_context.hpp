/*******************************************************************************
* uxoxo [component]                                          render_context.hpp
*
*   Per-frame rendering context passed to component draw handlers by the
* renderer.  Contains frame timing, available viewport dimensions, and an
* opaque pointer to backend-specific state (e.g. an ImGui context for the
* imgui renderer).
*
*   This struct is framework-agnostic — it carries only the information that
* every draw handler needs regardless of backend.  Backend-specific renderers
* extend it by populating the backend_context pointer.
*
*   Structure:
*     1.  render_context struct
*     2.  traits
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/templates/component/renderer/render_context.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_COMPONENT_RENDER_CONTEXT_
#define UXOXO_COMPONENT_RENDER_CONTEXT_ 1

// std
#include <cstddef>
#include <cstdint>
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// render_context
//   struct: per-frame state supplied to every component draw handler.
struct render_context
{
    // frame timing
    float    delta_time   = 0.0f;
    uint64_t frame_number = 0;

    // available viewport (pixels)
    float viewport_width  = 0.0f;
    float viewport_height = 0.0f;

    // backend-specific context (e.g. ImGuiContext*)
    //   the renderer populates this before dispatching draw calls.
    // draw handlers that need backend-specific API cast this to the
    // expected type.
    void* backend_context = nullptr;
};


// =============================================================================
//  2.  TRAITS
// =============================================================================

namespace render_context_traits
{
NS_INTERNAL

    // has_delta_time_member
    //   trait: detects render_context's delta_time field.
    template<typename _Type,
             typename = void>
    struct has_delta_time_member : std::false_type
    {};

    template<typename _Type>
    struct has_delta_time_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().delta_time)>>
        : std::true_type
    {};

    // has_backend_context_member
    //   trait: detects render_context's backend_context field.
    template<typename _Type,
             typename = void>
    struct has_backend_context_member : std::false_type
    {};

    template<typename _Type>
    struct has_backend_context_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().backend_context)>>
        : std::true_type
    {};

NS_END  // internal

// is_render_context
//   trait: true if a type satisfies the render_context structural interface.
template<typename _Type>
struct is_render_context : std::conjunction<
    internal::has_delta_time_member<_Type>,
    internal::has_backend_context_member<_Type>>
{};

template<typename _Type>
D_INLINE constexpr bool is_render_context_v =
    is_render_context<_Type>::value;

}   // namespace render_context_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_RENDER_CONTEXT_
