/*******************************************************************************
* uxoxo [imgui]                                     imgui_stacked_view_draw.hpp
*
* ImGui renderer for the `stacked_view` container:
*   Renders only the page at the currently selected index.  Out-of-range
* or empty-stack cases draw nothing - no placeholder, no warning.
* Navigation between pages is driven externally (a button, menu_bar,
* state machine, etc.) and the stacked_view's `on_select` and
* `on_change` callbacks are fired by that driver, not by this
* renderer.  This module is pure presentation.
*
*   The caller supplies a `_render_fn` callable that knows how to draw
* a single page.  Keeping rendering through a callback decouples this
* module from the broader render pipeline and makes it usable both
* inside the framework's component_registry-based renderer and in
* standalone test harnesses.
*
*   The function is a template over the stacked_view's _PageFeat,
* _CtrlFeat, and _Icon parameters.  The callback receives a
* `const stacked_page<_PageFeat, _Icon>&` matching the view's
* instantiation.
*
*   ImGui ID scoping: the renderer pushes the view's address as an
* ImGui ID before invoking the callback so that sibling
* stacked_views in the same window don't collide on the widget
* identities used inside their page bodies.
*
*
* path:      /inc/uxoxo/platform/imgui/container/stacked/
                 imgui_stacked_view_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.04.17
*******************************************************************************/

#ifndef  UXOXO_IMGUI_COMPONENT_STACKED_VIEW_DRAW_
#define  UXOXO_IMGUI_COMPONENT_STACKED_VIEW_DRAW_ 1

// std
#include <utility>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../../uxoxo.hpp"
#include "../../../../templates/component/container/stacked/stacked_view.hpp"


NS_UXOXO
NS_IMGUI


// =============================================================================
//  1  PUBLIC DRAW FUNCTION
// =============================================================================

/*
imgui_draw_stacked_view
  Renders a stacked_view by drawing only the page at the currently
  selected index.  Safe against empty page lists and out-of-range
  selected indices - both produce a silent no-op.  No border, no
  chrome - the stacked_view is purely a page-switching container;
  any visible framing is the job of a surrounding `frame` or
  `panel`.

  The view's address is pushed onto the ImGui ID stack for the
  duration of the callback so sibling stacked_views in the same
  window don't collide on the widget identities used inside their
  page bodies.

Parameter(s):
  _sv:        the stacked_view to render.
  _render_fn: callable invoked on the page corresponding to the
              current selection.  Called as
              _render_fn(_sv.pages[_sv.selected]) and receives a
              `const stacked_page<_PageFeat, _Icon>&`.
Return:
  none.
*/
template <unsigned _PageFeat,
          unsigned _CtrlFeat,
          typename _Icon,
          typename _RenderFn>
void
imgui_draw_stacked_view(
    const component::stacked_view<_PageFeat, _CtrlFeat, _Icon>& _sv,
    _RenderFn&&                                                 _render_fn
)
{
    // empty stack - draw nothing
    if (_sv.pages.empty())
    {
        return;
    }

    // out-of-range selected index - draw nothing.  `selected` is
    // unsigned, so no negative-value check is required.
    if (_sv.selected >= _sv.pages.size())
    {
        return;
    }

    // scope a fresh ImGui ID region so sibling stacked_views don't
    // collide on widget identities inside their page bodies
    ImGui::PushID(static_cast<const void*>(&_sv));

    std::forward<_RenderFn>(_render_fn)(_sv.pages[_sv.selected]);

    ImGui::PopID();

    return;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_STACKED_VIEW_DRAW_