/*******************************************************************************
* uxoxo [imgui]                                          imgui_render_event.hpp
*
* Shared per-frame event-report base for ImGui renderers:
*   `combo_box_event`, `magnification_control_event`, and
* `message_box_event` each independently expose a flat set of `bool`
* flags (committed, changed, opened, dismissed, ...) summarising what
* the user did during the frame.  This header consolidates the shared
* surface into a single base struct so callers can write generic
* event-handling code, and so future renderers reach for the same
* flag vocabulary instead of inventing new names.
*
*   The base carries only flags that genuinely apply to every
* interactive renderer:
*
*     - any_change   any flag below is set
*     - committed    a commit-style action fired (Enter, OK, default)
*     - dismissed    the component was closed without a commit (Esc)
*     - changed      the underlying value mutated this frame
*     - focus_gained the component took focus this frame
*     - focus_lost   the component lost focus this frame
*
*   Component-specific event types derive from `render_event` and add
* their own fields (combo_box_event::filter_changed, message_box_event
* ::triggered_bit, etc.).  The base is an aggregate; derivation does
* not break aggregate initialization for the derived type as long as
* the derived type follows suit (no user-declared constructor) - this
* matches how every existing event type is currently written.
*
*   The free function `summarise(_evt)` updates `any_change` from the
* OR of the other flags.  Renderers call it once at the end of the
* draw to spare callers from writing the OR themselves.
*
* Contents:
*   1.  render_event           (base struct)
*   2.  summarise              (recompute any_change)
*   3.  has_any                (free predicate)
*
*
* path:      /inc/uxoxo/platform/imgui/core/imgui_render_event.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.08
*******************************************************************************/

#ifndef  UXOXO_IMGUI_RENDER_EVENT_
#define  UXOXO_IMGUI_RENDER_EVENT_ 1

// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  1.  RENDER EVENT
// ===========================================================================

// render_event
//   struct: shared per-frame event-report base for ImGui renderers.
// Every interactive component renderer that returns an event struct
// derives from this base so callers can query the universal flags
// (any_change, committed, dismissed, changed, focus_*) without
// knowing the concrete event type.  Derived event structs add their
// own fields and remain aggregate-initializable.
struct render_event
{
    bool any_change   = false;  // OR of every other flag below
    bool committed    = false;  // commit-style action fired this frame
    bool dismissed    = false;  // closed without a commit (Esc / X)
    bool changed      = false;  // underlying value mutated this frame
    bool focus_gained = false;  // component took focus this frame
    bool focus_lost   = false;  // component lost focus this frame
};


// ===========================================================================
//  2.  SUMMARISE
// ===========================================================================

/*
summarise
  Recomputes `_evt.any_change` from the OR of every other flag in
the base.  Called by renderers at the end of the draw so callers
can probe a single bool to ask "did anything happen this frame?".

  Derived event structs that add their own bool flags should override
this with their own summarise overload that ORs the derived flags
into any_change as well.  The base overload is intentionally narrow -
it only sees the base flags.

Parameter(s):
  _evt: the render_event (or derived) to update.  Mutated in place.
Return:
  none.
*/
inline void
summarise(
    render_event& _evt
) noexcept
{
    _evt.any_change = ( (_evt.committed)    ||
                        (_evt.dismissed)    ||
                        (_evt.changed)      ||
                        (_evt.focus_gained) ||
                        (_evt.focus_lost) );

    return;
}


// ===========================================================================
//  3.  HAS ANY
// ===========================================================================

/*
has_any
  Free predicate form of `_evt.any_change`.  Useful in generic code
that does not want to take a reference to a specific event type and
does not want to call summarise() first.

Parameter(s):
  _evt: the render_event (or derived) to inspect.
Return:
  true iff any flag in the base is set.  Does NOT inspect derived
  flags - callers querying derived events should call the derived
  type's own summarise overload first if they want the full OR.
*/
[[nodiscard]] inline bool
has_any(
    const render_event& _evt
) noexcept
{
    return ( (_evt.committed)    ||
             (_evt.dismissed)    ||
             (_evt.changed)      ||
             (_evt.focus_gained) ||
             (_evt.focus_lost) );
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_RENDER_EVENT_
