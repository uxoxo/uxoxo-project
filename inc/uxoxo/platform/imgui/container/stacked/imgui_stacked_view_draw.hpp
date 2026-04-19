/*******************************************************************************
* uxoxo [imgui]                                     imgui_stacked_view_draw.hpp
*
* ImGui renderer for the `stacked_view` container:
*   Renders only the child at the currently selected index.  Out-of-range,
* negative, or empty-stack cases draw nothing — no placeholder, no warning.
* Navigation between pages is driven externally (a button, menu_bar, state
* machine, etc.) and the stacked_view's `page_changed` signal is emitted by
* that driver, not by this renderer.  This module is pure presentation.
*
*   The caller supplies a `node_render_fn` callback that knows how to
* dispatch over a node's component variant.  Keeping the recursion through
* a callback decouples this module from the broader render pipeline and
* makes it usable both inside the framework's component_registry-based
* renderer and in standalone test harnesses.
*
*   This header also defines `node_render_fn`, the shared child-render
* callback type reused by `imgui_frame_draw.hpp` and any other container
* drawer that recurses into children.
*
*
* path:      /inc/uxoxo/platform/imgui/container/stacked/
                 imgui_stacked_view_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.17
*******************************************************************************/

#ifndef  UXOXO_IMGUI_COMPONENT_STACKED_VIEW_DRAW_
#define  UXOXO_IMGUI_COMPONENT_STACKED_VIEW_DRAW_ 1

// std
#include <cstddef>
#include <functional>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../../uxoxo.hpp"
#include "../../../../templates/component/components.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI


// ===============================================================================
//  I.   SHARED CALLBACK TYPE
// ===============================================================================

// node_render_fn
//   type: callback invoked to render a single child node.  The integrating
// layer (typically the component_registry dispatcher) supplies an
// implementation that visits the node's component variant and dispatches
// to the appropriate draw handler.  Shared across all container drawers
// in this directory.
using node_render_fn = std::function<void(const node&)>;


// ===============================================================================
//  II.  PUBLIC DRAW FUNCTION
// ===============================================================================

/*
imgui_draw_stacked_view
  Renders a stacked_view by drawing only the child at the currently
selected index.  Safe against: empty children, negative or out-of-range
selected index, and null child entries.  No border, no chrome — the
stacked_view is purely a page-switching container; any visible framing
is the job of a surrounding `frame` or `panel`.

Parameter(s):
  _sv:        the stacked_view to render.
  _render_fn: callback invoked on the child node corresponding to the
              current selection.  If null, or if no child is eligible,
              the callback is not invoked.
Return:
  none.
*/
inline void
imgui_draw_stacked_view(
    const stacked_view&   _sv,
    const node_render_fn& _render_fn
)
{
    std::size_t count;
    std::size_t idx;

    // parameter validation
    if (!_render_fn)
    {
        return;
    }

    // initialize
    count = _sv.children.size();

    // empty stack — draw nothing
    if (count == 0)
    {
        return;
    }

    // negative or out-of-range index — draw nothing
    if ( (_sv.selected < 0) ||
         (static_cast<std::size_t>(_sv.selected) >= count) )
    {
        return;
    }

    idx = static_cast<std::size_t>(_sv.selected);

    // null child — draw nothing
    if (!_sv.children[idx])
    {
        return;
    }

    _render_fn(*_sv.children[idx]);

    return;
}


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_STACKED_VIEW_DRAW_