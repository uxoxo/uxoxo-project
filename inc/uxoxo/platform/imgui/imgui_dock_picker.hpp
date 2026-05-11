/*******************************************************************************
* uxoxo [imgui]                                           imgui_dock_picker.hpp
*
* Reusable cross-shaped dock picker widget:
*   The visual affordance that materializes when a dockable component
* has its `popup_open` flag raised AND its `presentation` is set to
* DDockPresentation::popup.  Draws a centered cross of five buttons
* (left, right, top, bottom, center) and reports which zone (if any)
* the user clicked this frame.
*
*   Pure UI - no state of its own.  Callers pass a bitmask describing
* which zones the host supports; arms for unsupported zones are
* skipped entirely.  The picker is dispatch-only - it does not mutate
* the requesting component, it does not write to the host.  The
* caller wires its return value into `dock(_c, picked_zone)` (or
* equivalent) to keep all state mutations flowing through the
* canonical verb surface.
*
*   The picker compiles in three independent flavors that share the
* same drawing code:
*
*     render_dock_picker_at(_origin, _supported)
*         Draw the picker centered at the given screen position.
*         Caller has already computed the host's interior rectangle.
*
*     render_dock_picker_in_window(_supported)
*         Convenience wrapper that takes the current ImGui window's
*         work area as the host rectangle.
*
*     render_dock_picker_for_handle(_supported, _handle)
*         Full-service: draws the picker AND dispatches the picked
*         zone onto the dock_handle's underlying component via its
*         vtable.  Closes the picker automatically on selection or
*         on Escape / click-outside.
*
*   Zero-overhead pathway:
*   The supported mask is a plain `unsigned` (D_DOCK_HOST_* bits)
* and the per-zone draw loop early-exits on unset bits, so a host
* that supports only LEFT+RIGHT pays for drawing exactly two
* buttons.  No per-zone allocation, no virtual dispatch beyond the
* one through `dock_handle::confirm` that the host already pays.
*
*   Migration note (2026.05.10): new module.  Built on the
* consolidated palette + scope + render_event infrastructure from
* steps 1-8 of the platform refactor.
*
* Contents:
*   1.  dock_picker_event              (derives render_event)
*   2.  internal helpers               (geometry, arm drawing)
*   3.  render_dock_picker_at          - canonical positional form
*   4.  render_dock_picker_in_window   - convenience overload
*   5.  render_dock_picker_for_handle  - full-service overload
*
*
* path:      /inc/uxoxo/platform/imgui/dockable/imgui_dock_picker.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.10
*******************************************************************************/

#ifndef  UXOXO_IMGUI_DOCK_PICKER_
#define  UXOXO_IMGUI_DOCK_PICKER_ 1

// std
#include <cstdint>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/dockable/dock_handle.hpp"
#include "../../../templates/component/dockable/dock_host_types.hpp"
#include "../../../templates/component/dockable/dockable_common.hpp"
#include "../../../templates/component/dockable/dockable_types.hpp"
#include "../core/imgui_palette.hpp"
#include "../core/imgui_render_event.hpp"
#include "../core/imgui_scope.hpp"


NS_UXOXO
NS_IMGUI


using uxoxo::component::DDockZone;
using uxoxo::component::dock_handle;
using uxoxo::component::dock_zone_bit;


// ===========================================================================
//  1.  DOCK PICKER EVENT
// ===========================================================================

// dock_picker_event
//   struct: per-frame event report from rendering the dock picker.
// Inherits render_event for the shared (committed / dismissed /
// changed / focus_*) flags.  When a zone is picked, `committed`
// fires (the user made a selection) and `picked_zone` carries the
// result; when the picker is dismissed via Escape or click-outside,
// `dismissed` fires with `picked_zone == DDockZone::none`.
//
//   Hosts that wire the picker into their reconcile pass can probe
// `committed` to know whether to dispatch `dock(owner, picked_zone)`
// or `dismissed` to know whether to call `close_dock_popup(owner)`.
struct dock_picker_event : render_event
{
    DDockZone picked_zone = DDockZone::none;
    bool      hover_arm   = false;   // cursor is over any arm
};


// ===========================================================================
//  2.  INTERNAL HELPERS
// ===========================================================================

NS_INTERNAL

    // arm_size
    //   constant: pixel size of each cross arm.  Local to the picker
    // because it's the only consumer; promote to the palette if a
    // second picker emerges with the same geometry.
    inline constexpr float dock_picker_arm_size    = 36.0f;
    inline constexpr float dock_picker_arm_gap     = 4.0f;
    inline constexpr float dock_picker_arm_rounding = 4.0f;

    // zone_offset
    //   function: returns the (dx, dy) screen offset of an arm's
    // top-left corner relative to the picker center, in units of
    // (arm_size + arm_gap).
    //
    //   Layout:
    //
    //                  [ TOP   ]
    //       [ LEFT  ] [ CENTER ] [ RIGHT ]
    //                  [ BOTTOM]
    //
    D_NODISCARD D_INLINE ImVec2
    zone_offset(
        DDockZone _zone
    ) noexcept
    {
        const float step = (dock_picker_arm_size + dock_picker_arm_gap);

        switch (_zone)
        {
            case DDockZone::left:
                return ImVec2(-step, 0.0f);

            case DDockZone::right:
                return ImVec2(step, 0.0f);

            case DDockZone::top:
                return ImVec2(0.0f, -step);

            case DDockZone::bottom:
                return ImVec2(0.0f, step);

            case DDockZone::center:
                return ImVec2(0.0f, 0.0f);

            case DDockZone::none:
            default:
                return ImVec2(0.0f, 0.0f);
        }
    }

    // arm_glyph
    //   function: returns a static glyph string for an arm's label.
    // ASCII-only to avoid font-fallback assumptions; renderers that
    // ship an icon font can substitute their own glyphs through a
    // future style hook without changing this header.
    D_NODISCARD D_INLINE const char*
    arm_glyph(
        DDockZone _zone
    ) noexcept
    {
        switch (_zone)
        {
            case DDockZone::left:   return "<";
            case DDockZone::right:  return ">";
            case DDockZone::top:    return "^";
            case DDockZone::bottom: return "v";
            case DDockZone::center: return "+";
            case DDockZone::none:
            default:                return "?";
        }
    }

    // draw_arm
    //   function: renders a single arm as a colored rectangle with
    // a centered glyph.  Returns true iff the user clicked the arm
    // this frame.  Hover state is reflected in the fill color and
    // also written into `_evt.hover_arm` so callers can dim the
    // host content when the picker is interactive.
    D_INLINE bool
    draw_arm(
        const ImVec2&      _origin,
        DDockZone          _zone,
        dock_picker_event& _evt
    ) noexcept
    {
        ImDrawList* dl;
        ImVec2      tl;
        ImVec2      br;
        bool        hovered;
        bool        clicked;
        ImU32       fill;
        ImU32       border;
        const char* glyph;
        ImVec2      glyph_size;
        ImVec2      mouse;

        tl = _origin;
        br = ImVec2(_origin.x + dock_picker_arm_size,
                    _origin.y + dock_picker_arm_size);

        // -- hit test ---------------------------------------------------
        mouse   = ImGui::GetIO().MousePos;
        hovered = ( (mouse.x >= tl.x) && (mouse.x <= br.x) &&
                    (mouse.y >= tl.y) && (mouse.y <= br.y) );

        clicked = ( (hovered) &&
                    (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) );

        if (hovered)
        {
            _evt.hover_arm = true;
        }

        // -- colors -----------------------------------------------------
        //   Hovered arms light up with the toggle-on background tag
        // so the picker visually echoes a toolbar toggle button;
        // unhovered arms use the standard button background tag.
        fill = ImGui::GetColorU32(
                   hovered
                       ? palette::get<palette::btn_toggle_on_bg_tag>()
                       : palette::get<palette::btn_bg_tag>());

        border = ImGui::GetColorU32(
                   hovered
                       ? palette::get<palette::btn_toggle_on_border_tag>()
                       : palette::get<palette::toolbar_border_tag>());

        // -- draw -------------------------------------------------------
        dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(tl, br, fill, dock_picker_arm_rounding);
        dl->AddRect(tl, br, border, dock_picker_arm_rounding,
                    0, hovered ? 1.5f : 1.0f);

        glyph      = arm_glyph(_zone);
        glyph_size = ImGui::CalcTextSize(glyph);

        dl->AddText(
            ImVec2(tl.x + ((dock_picker_arm_size - glyph_size.x) * 0.5f),
                   tl.y + ((dock_picker_arm_size - glyph_size.y) * 0.5f)),
            ImGui::GetColorU32(palette::get<palette::btn_text_tag>()),
            glyph);

        return clicked;
    }

NS_END  // internal


// ===========================================================================
//  3.  RENDER_DOCK_PICKER_AT  (canonical positional form)
// ===========================================================================

/*
render_dock_picker_at
  Renders the cross-shaped dock picker centered at `_center`.  Each
arm corresponds to one bit in `_supported`; arms whose bit is unset
in the mask are skipped (the host doesn't support that zone, so it's
visually wrong to offer it).

  The picker is drawn as five rectangles arranged in a plus pattern.
A hovered arm tints with the toggle-on palette tag; the center cell
is offered when D_DOCK_HOST_CENTER is set.  Clicks dispatch via the
returned event; Escape sets `dismissed` true with picked_zone =
DDockZone::none.

  This function does NOT close itself - callers control the picker's
visibility by mutating the underlying component's popup_open flag
through close_dock_popup().  Doing it externally keeps the picker
stateless and reusable.

Parameter(s):
  _center:    screen-space center point of the cross.
  _supported: bitmask of D_DOCK_HOST_* bits.  Use the host's
              `features` constant or the equivalent.
Return:
  A dock_picker_event with `picked_zone` set when an arm was
  clicked, or `dismissed` set when Escape was pressed.
*/
[[nodiscard]] inline dock_picker_event
render_dock_picker_at(
    const ImVec2& _center,
    unsigned      _supported
) noexcept
{
    dock_picker_event evt;

    // -- escape dismisses the picker --------------------------------
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        evt.dismissed = true;
        evt.any_change = true;
        return evt;
    }

    // -- iterate the five candidate zones ---------------------------
    //   Order matters only for hit-testing precedence when arms
    // overlap (they don't, by geometry, but the order is stable
    // anyway for testability).
    constexpr DDockZone zones[5] =
    {
        DDockZone::left,
        DDockZone::right,
        DDockZone::top,
        DDockZone::bottom,
        DDockZone::center
    };

    const float half = (internal::dock_picker_arm_size * 0.5f);

    for (DDockZone z : zones)
    {
        const unsigned bit = dock_zone_bit(z);

        if ((_supported & bit) == 0u)
        {
            continue;
        }

        const ImVec2 off    = internal::zone_offset(z);
        const ImVec2 origin = ImVec2(_center.x + off.x - half,
                                     _center.y + off.y - half);

        if (internal::draw_arm(origin, z, evt))
        {
            evt.committed   = true;
            evt.picked_zone = z;
            evt.any_change  = true;
        }
    }

    // -- click-outside dismisses (if no arm was clicked) ------------
    //   Only when the click landed and no arm consumed it - hover
    // detection already runs in draw_arm; if hover_arm is false at
    // the end of the loop, the click was outside the picker.
    if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) &&
         (!evt.hover_arm)                               &&
         (!evt.committed) )
    {
        evt.dismissed  = true;
        evt.any_change = true;
    }

    return evt;
}


// ===========================================================================
//  4.  RENDER_DOCK_PICKER_IN_WINDOW  (convenience overload)
// ===========================================================================

/*
render_dock_picker_in_window
  Convenience wrapper that computes the picker's center from the
current ImGui window's work area (the region inside any title bar,
menu bar, and scrollbars).  Useful when the picker should appear
centered in the host window without the caller computing screen
coordinates explicitly.

Parameter(s):
  _supported: bitmask of D_DOCK_HOST_* bits.
Return:
  A dock_picker_event identical to render_dock_picker_at.
*/
[[nodiscard]] inline dock_picker_event
render_dock_picker_in_window(
    unsigned _supported
) noexcept
{
    const ImVec2 work_pos  = ImGui::GetWindowPos();
    const ImVec2 work_size = ImGui::GetWindowSize();
    const ImVec2 center    = ImVec2(work_pos.x + (work_size.x * 0.5f),
                                    work_pos.y + (work_size.y * 0.5f));

    return render_dock_picker_at(center, _supported);
}


// ===========================================================================
//  5.  RENDER_DOCK_PICKER_FOR_HANDLE  (full-service overload)
// ===========================================================================

/*
render_dock_picker_for_handle
  Full-service overload that draws the picker AND dispatches the
selected zone onto the dock_handle's underlying component.  Use
when the calling code has a dock_handle in hand (typically from a
host's picker_owner field after reconcile()) and wants to wire the
user's pick straight back to the dockable.

  Dispatch path:
    - on commit: writes target_zone via the handle's vtable, raises
      dock_requested on the underlying component, and lowers the
      popup_open flag through the same vtable.  The next host
      reconcile() will then call confirm() and the chosen zone
      becomes current_zone.
    - on dismiss: lowers popup_open without raising dock_requested.

  The vtable in dock_handle.hpp does not currently expose a
target-zone setter (its public surface is intentionally narrow -
get_current_zone, get_target_zone, is_dock_pending, is_popup_open,
confirm) so this overload is provided as a TEMPLATE on the concrete
dockable type that the caller still has access to.  When you only
have an erased handle, fall back to render_dock_picker_at and
handle dispatch yourself.

Parameter(s):
  _supported: bitmask of D_DOCK_HOST_* bits.
  _center:    screen-space center point of the cross.
  _dockable:  the dockable component whose target_zone and
              popup_open are mutated on commit / dismiss.  Must
              satisfy is_dockable_v with popup support.
Return:
  A dock_picker_event identical to render_dock_picker_at.
*/
template<typename _Dockable>
dock_picker_event
render_dock_picker_for_handle(
    unsigned      _supported,
    const ImVec2& _center,
    _Dockable&    _dockable
)
{
    static_assert(
        uxoxo::component::is_dockable_v<_Dockable>,
        "render_dock_picker_for_handle requires a type satisfying "
        "is_dockable_v - see dockable_traits.hpp");

    dock_picker_event evt = render_dock_picker_at(_center, _supported);

    if (evt.committed)
    {
        // route the commit through the canonical verbs so any
        // future decoration in dock() (logging, telemetry,
        // validation) applies uniformly to picker-driven docks
        uxoxo::component::dock(_dockable, evt.picked_zone);

        if constexpr (uxoxo::component::has_dock_popup_v<_Dockable>)
        {
            uxoxo::component::close_dock_popup(_dockable);
        }
    }
    else if (evt.dismissed)
    {
        if constexpr (uxoxo::component::has_dock_popup_v<_Dockable>)
        {
            uxoxo::component::close_dock_popup(_dockable);
        }
    }

    return evt;
}


NS_END  // imgui
NS_END  // uxoxo


// =============================================================================
//  BACKWARD-COMPAT BRIDGE
// =============================================================================
//   The picker's canonical home is `uxoxo::imgui`.  The bridge re-
// exports its public symbols into `uxoxo::component` so call sites
// outside the imgui layer that want to use the picker can write the
// shorter qualification.

NS_UXOXO
NS_COMPONENT

using uxoxo::imgui::dock_picker_event;
using uxoxo::imgui::render_dock_picker_at;
using uxoxo::imgui::render_dock_picker_in_window;
using uxoxo::imgui::render_dock_picker_for_handle;

NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_DOCK_PICKER_
