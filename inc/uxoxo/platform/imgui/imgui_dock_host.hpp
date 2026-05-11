/*******************************************************************************
* uxoxo [imgui]                                             imgui_dock_host.hpp
*
* Dear ImGui renderer for the dock_host<_Feat> composite:
*   Owns the layout pass for a host instantiation - resolves any
* policy-driven pending dock requests against the current frame's
* screen geometry, performs the host's reconcile() to commit pending
* docks, allocates space for each supported zone via that zone's
* split_ratio (read off the composite's slot), and yields each zone's
* rectangle back to the caller through a user-supplied content
* callback so the caller renders whatever it wants in each zone.
*
*   Design tenets:
*
*   1.  Zone gating compiles out.  `dock_host<_Feat>` carries only
*       the slots for zones whose bit is set in _Feat; every loop
*       below over the five candidate zones tests
*       _host.template has_field<_Tag>() at compile time, so an
*       instance with D_DOCK_HOST_LEFT | D_DOCK_HOST_RIGHT generates
*       code for two zones and nothing else.
*
*   2.  Layout is single-pass and allocation-free.  The host's
*       interior rectangle is divided into the cardinal sides per
*       split_ratio (top + bottom claim from the height, left + right
*       claim from the width remaining after top/bottom); center
*       fills whatever's left.  No per-zone allocation, no
*       intermediate vectors.
*
*   3.  Policy resolution lives here.  The dockable subsystem
*       deliberately separates "request" (in dockable_common::dock_*)
*       from "resolution" because resolving DDockPolicy::nearest
*       needs layout info.  This is the place where layout info
*       exists - rectangle bounds, current zone occupancy - so the
*       resolver runs in resolve_pending_policies() before the
*       host's reconcile().
*
*   4.  The content callback receives the zone's rectangle as an
*       ImGui ScreenRect plus the dock_zone_state for that zone.
*       Callers iterate _host.items themselves to find the children
*       belonging to that zone (or use for_each_in_zone()), giving
*       them full control over per-zone layout (tabs vs splits vs
*       stack).  The renderer doesn't impose a presentation.
*
*   5.  Picker is opt-in.  When _host.picker_open is true after
*       reconcile, render_dock_host() draws the picker via the
*       reusable widget in imgui_dock_picker.hpp.  The picker's
*       result is dispatched back through the canonical
*       dock()/close_dock_popup() verbs.
*
*   Two entry points are provided:
*
*     render_dock_host(_host, _content_fn, _ctx)
*         The single-call all-in-one form.  Resolves policies,
*         reconciles, lays out zones, invokes _content_fn for each
*         enabled zone, draws the picker, returns an event report.
*
*     Per-step composable forms (resolve_pending_policies,
*     compute_zone_rects, draw_zone_splitters):
*         For callers that want to interleave their own logic
*         between policy resolution and zone iteration (e.g. to
*         drag-resize the splitters in the same frame).
*
*   Migration note (2026.05.10): new module.  Built on the
* consolidated palette + scope + render_event infrastructure from
* steps 1-8.
*
* Contents:
*   1.  dock_host_event                   (derives render_event)
*   2.  dock_host_zone_rects              (per-zone layout output)
*   3.  internal helpers                  (zone iteration, policy)
*   4.  resolve_pending_policies          - layout-aware policy
*   5.  compute_zone_rects                - layout-only step
*   6.  draw_zone_splitters               - optional drag affordance
*   7.  render_dock_host                  - canonical all-in-one
*
*
* path:      /inc/uxoxo/platform/imgui/dockable/imgui_dock_host.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.10
*******************************************************************************/

#ifndef  UXOXO_IMGUI_DOCK_HOST_
#define  UXOXO_IMGUI_DOCK_HOST_ 1

// std
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/dockable/dock_handle.hpp"
#include "../../../templates/component/dockable/dock_host.hpp"
#include "../../../templates/component/dockable/dock_host_common.hpp"
#include "../../../templates/component/dockable/dock_host_types.hpp"
#include "../../../templates/component/dockable/dockable_common.hpp"
#include "../../../templates/component/dockable/dockable_types.hpp"
#include "../../../templates/render_context.hpp"
#include "../core/imgui_palette.hpp"
#include "../core/imgui_render_event.hpp"
#include "../core/imgui_scope.hpp"
#include "./imgui_dock_picker.hpp"


NS_UXOXO
NS_IMGUI


using uxoxo::component::DDockZone;
using uxoxo::component::DDockPolicy;
using uxoxo::component::DDockPresentation;
using uxoxo::component::dock_handle;
using uxoxo::component::dock_host;
using uxoxo::component::dock_zone_state;
using uxoxo::component::dock_zone_bit;
using uxoxo::component::dock_zone_left_tag;
using uxoxo::component::dock_zone_right_tag;
using uxoxo::component::dock_zone_top_tag;
using uxoxo::component::dock_zone_bottom_tag;
using uxoxo::component::dock_zone_center_tag;
using uxoxo::component::render_context;


// ===========================================================================
//  1.  DOCK HOST EVENT
// ===========================================================================

// dock_host_event
//   struct: per-frame event report from rendering the host.
// Inherits render_event for the shared flags; adds host-specific
// surfaces:
//
//   - resolved_pending:    one or more pending policy-driven docks
//                           were resolved this frame
//   - confirmed_docks:     count of children whose dock_requested
//                           was confirmed this frame
//   - picker_committed:    the picker fired (the user picked a
//                           zone in the cross widget)
//   - picker_dismissed:    the picker was closed without a pick
//   - splitter_dragged:    a zone splitter was dragged (only set
//                           when draw_zone_splitters runs and a
//                           drag actually occurred)
//
//   The base `committed` flag mirrors `picker_committed` so generic
// callers can probe the base.  The base `changed` flag is set when
// any host-side state mutated (resolved or confirmed or split-
// dragged).
struct dock_host_event : render_event
{
    bool        resolved_pending = false;
    std::size_t confirmed_docks  = 0;
    bool        picker_committed = false;
    bool        picker_dismissed = false;
    bool        splitter_dragged = false;
    DDockZone   picked_zone      = DDockZone::none;
};


// ===========================================================================
//  2.  DOCK HOST ZONE RECTS
// ===========================================================================

// dock_zone_rect
//   struct: per-zone rectangle output from compute_zone_rects.
// Carries screen-space coordinates plus a flag indicating whether
// the zone is enabled on this host instantiation (so callers
// iterating over the five-element array can skip disabled zones
// without consulting the host's _Feat directly).
struct dock_zone_rect
{
    DDockZone zone   = DDockZone::none;
    bool      active = false;        // zone is supported AND has area
    ImVec2    tl     {0.0f, 0.0f};   // top-left in screen coords
    ImVec2    br     {0.0f, 0.0f};   // bottom-right in screen coords

    [[nodiscard]] ImVec2 size() const noexcept
    {
        return ImVec2(br.x - tl.x, br.y - tl.y);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return ( (br.x <= tl.x) ||
                 (br.y <= tl.y) );
    }
};

// dock_host_zone_rects
//   struct: aggregated layout output.  Indexed by DDockZone (cast
// to underlying type); slot 0 (DDockZone::none) is unused and
// always inactive.
struct dock_host_zone_rects
{
    dock_zone_rect zones[6];      // indexed by DDockZone underlying

    [[nodiscard]] const dock_zone_rect&
    operator[](DDockZone _z) const noexcept
    {
        return zones[static_cast<unsigned>(_z)];
    }

    [[nodiscard]] dock_zone_rect&
    operator[](DDockZone _z) noexcept
    {
        return zones[static_cast<unsigned>(_z)];
    }
};


// ===========================================================================
//  3.  INTERNAL HELPERS
// ===========================================================================

NS_INTERNAL

    // host_zone_state
    //   function: pulls the dock_zone_state for a given zone tag off
    // the host's composite.  Compile-time gated on has_field<>() so
    // unsupported zones get a default-constructed fallback (which
    // every caller below either guards via an `if constexpr` check
    // or treats as a zero-size zone).
    //
    //   Two overloads cover the const/non-const cases so reconcile-
    // adjacent code can mutate split ratios while read-only paths
    // stay const-safe.
    template<typename _Tag,
              unsigned _Feat>
    D_INLINE dock_zone_state&
    host_zone_state(
        dock_host<_Feat>& _host
    ) noexcept
    {
        return _host.template get_field<_Tag>();
    }

    template<typename _Tag,
              unsigned _Feat>
    D_INLINE const dock_zone_state&
    host_zone_state_c(
        const dock_host<_Feat>& _host
    ) noexcept
    {
        return _host.template get_field<_Tag>();
    }

    // nearest_zone
    //   function: maps a screen point to the closest supported
    // cardinal side of `_rect`.  Used by policy resolution when a
    // child requests DDockPolicy::nearest without specifying a
    // preferred zone (or specifies one that isn't supported).
    //
    //   Distance is measured to each cardinal edge of the rect;
    // the edge with the smallest distance wins.  Center is never
    // returned because nearest is by definition a cardinal-side
    // request - callers wanting center should use
    // DDockZone::center directly.
    D_NODISCARD inline DDockZone
    nearest_zone(
        const ImVec2& _point,
        const ImVec2& _rect_tl,
        const ImVec2& _rect_br,
        unsigned      _supported
    ) noexcept
    {
        constexpr DDockZone cardinals[4] =
        {
            DDockZone::left,
            DDockZone::right,
            DDockZone::top,
            DDockZone::bottom
        };

        const float distances[4] =
        {
            std::abs(_point.x - _rect_tl.x),  // left
            std::abs(_rect_br.x - _point.x),  // right
            std::abs(_point.y - _rect_tl.y),  // top
            std::abs(_rect_br.y - _point.y)   // bottom
        };

        DDockZone best     = DDockZone::none;
        float     best_d   = 1.0e30f;

        for (int i = 0; i < 4; ++i)
        {
            const unsigned bit = dock_zone_bit(cardinals[i]);

            if ((_supported & bit) == 0u)
            {
                continue;
            }

            if (distances[i] < best_d)
            {
                best_d = distances[i];
                best   = cardinals[i];
            }
        }

        return best;
    }

    // flow_resolve
    //   function: walks the cardinal-then-center preference order
    // and returns the first supported zone that has not already
    // been claimed by another docked child.  Used by
    // DDockPolicy::flow.  Occupancy is computed from the host's
    // current handles (children whose current_zone equals the
    // candidate), so newly-confirmed docks earlier in the same
    // reconcile tick correctly mark their zones as occupied.
    //
    //   The preference order (left, right, top, bottom, center)
    // matches the dockable.hpp model description; hosts wanting
    // a different order can fork this helper into their own
    // resolver.
    template<unsigned _Feat>
    D_NODISCARD DDockZone
    flow_resolve(
        const dock_host<_Feat>& _host,
        unsigned                _supported
    ) noexcept
    {
        constexpr DDockZone preference[5] =
        {
            DDockZone::left,
            DDockZone::right,
            DDockZone::top,
            DDockZone::bottom,
            DDockZone::center
        };

        for (DDockZone z : preference)
        {
            const unsigned bit = dock_zone_bit(z);

            if ((_supported & bit) == 0u)
            {
                continue;
            }

            const std::size_t occupants =
                uxoxo::component::count_in_zone(_host, z);

            if (occupants == 0)
            {
                return z;
            }
        }

        return DDockZone::none;
    }

NS_END  // internal


// ===========================================================================
//  4.  RESOLVE PENDING POLICIES
// ===========================================================================

/*
resolve_pending_policies
  Walks the host's children and, for each child whose
dock_requested is true AND whose policy is not DDockPolicy::fixed,
rewrites its target_zone to a concrete supported zone using the
current host layout as input.

  - DDockPolicy::nearest: if target_zone is already a supported
    cardinal side, kept; otherwise resolved to the nearest
    supported cardinal side by distance from the host's center.
  - DDockPolicy::flow: target_zone is overwritten with
    flow_resolve()'s preference-order walk.

  After this runs, every pending child has a concrete target_zone
that the host's reconcile() will then commit via confirm().  This
function does NOT call confirm() itself - it only rewrites
target_zone, leaving the canonical handshake intact.

  This is the layout-info-dependent step that dock_host_common.hpp
deliberately omitted: it requires knowledge of the host's screen
geometry, which lives only at render time.

Parameter(s):
  _host:    the dock_host to process.
  _rect_tl: top-left of the host's interior in screen coordinates.
  _rect_br: bottom-right of the host's interior in screen coords.
Return:
  The number of pending requests whose policy was resolved this
  frame.  Returns 0 if no pending requests existed or all of them
  used DDockPolicy::fixed.

Note:
  The vtable in dock_handle.hpp does not currently expose a
  target-zone setter.  Policy resolution therefore needs concrete
  type access to mutate `target_zone`; this function is a no-op
  on hosts that store only generic handles - the user is expected
  to call resolve_pending_policies on their typed children
  individually OR to extend the vtable.  See the typed-children
  overload below for the common case.
*/
template<unsigned _Feat>
std::size_t
resolve_pending_policies(
    dock_host<_Feat>& _host,
    const ImVec2&     _rect_tl,
    const ImVec2&     _rect_br
)
{
    // The base overload runs over erased handles and can only
    // ACT on pending requests through the dock_handle's narrow
    // surface.  Since target_zone is not in that surface, this
    // overload is informational only - it returns 0 and leaves
    // resolution to the typed overload below.  Callers using
    // the picker-driven workflow (where the user's pick is
    // already a concrete zone) never reach DDockPolicy::nearest
    // resolution because the picker writes a concrete
    // target_zone directly.
    (void)_host;
    (void)_rect_tl;
    (void)_rect_br;

    return 0;
}

/*
resolve_pending_policies (typed children overload)
  Variant that takes the list of typed dockable children directly
so the resolver can write target_zone on each.  Pass the same
container of pointers the caller used to call register_dockable
on the host.

  This overload is the practical one - it is the typical entry
point for code that owns its dockable children directly and just
wants the policy layer to do the right thing before reconcile.

Parameter(s):
  _host:     the dock_host (used for occupancy queries in flow).
  _children: span of pointers to dockable components.  Each
             must satisfy is_dockable_v.
  _rect_tl:  top-left of the host's interior.
  _rect_br:  bottom-right of the host's interior.
Return:
  The number of pending requests resolved.
*/
template<unsigned _Feat,
          typename _Iterator>
std::size_t
resolve_pending_policies(
    dock_host<_Feat>& _host,
    _Iterator         _begin,
    _Iterator         _end,
    const ImVec2&     _rect_tl,
    const ImVec2&     _rect_br
)
{
    std::size_t  resolved;
    const ImVec2 center = ImVec2((_rect_tl.x + _rect_br.x) * 0.5f,
                                  (_rect_tl.y + _rect_br.y) * 0.5f);

    resolved = 0;

    for (auto it = _begin; it != _end; ++it)
    {
        auto& dockable = **it;

        if (!dockable.dock_requested)
        {
            continue;
        }

        if (dockable.policy == DDockPolicy::fixed)
        {
            continue;
        }

        if (dockable.policy == DDockPolicy::nearest)
        {
            // honor the supplied preference if supported
            const unsigned pref_bit =
                dock_zone_bit(dockable.target_zone);

            if ( (dockable.target_zone != DDockZone::none) &&
                 ((_host.features & pref_bit) != 0u) )
            {
                continue;
            }

            dockable.target_zone = internal::nearest_zone(
                center, _rect_tl, _rect_br, _host.features);
            ++resolved;
        }
        else if (dockable.policy == DDockPolicy::flow)
        {
            dockable.target_zone = internal::flow_resolve(
                _host, _host.features);
            ++resolved;
        }
    }

    return resolved;
}


// ===========================================================================
//  5.  COMPUTE ZONE RECTS
// ===========================================================================

/*
compute_zone_rects
  Divides `_rect` among the host's supported zones according to
each zone's split_ratio.  Top and bottom claim from the full
height; left and right claim from the height that remains after
top and bottom; center fills whatever interior is left.

  Disabled zones are skipped entirely (their slot doesn't exist on
the composite), so the corresponding entry in the output has
`active == false` and the surviving zones expand to fill the
freed space.

  The function only computes geometry; it does not draw anything
and does not mutate the host's zone states.  Callers can use the
output rectangles to draw zone backgrounds, route children into
zones, run hit-tests for splitter drags, etc.

Parameter(s):
  _host: the host whose split_ratios drive the division.
  _rect_tl: top-left of the host's interior.
  _rect_br: bottom-right of the host's interior.
Return:
  A dock_host_zone_rects struct populated for every supported zone.
  Unsupported zones have active == false and a zero-size rect.
*/
template<unsigned _Feat>
[[nodiscard]] dock_host_zone_rects
compute_zone_rects(
    const dock_host<_Feat>& _host,
    const ImVec2&           _rect_tl,
    const ImVec2&           _rect_br
)
{
    dock_host_zone_rects out;
    float                cursor_top;
    float                cursor_bot;
    float                cursor_left;
    float                cursor_right;
    const float          full_w = (_rect_br.x - _rect_tl.x);
    const float          full_h = (_rect_br.y - _rect_tl.y);

    // -- top zone ------------------------------------------------------
    cursor_top = _rect_tl.y;

    if constexpr (_host.template has_field<dock_zone_top_tag>())
    {
        const auto& st = internal::host_zone_state_c<dock_zone_top_tag>(_host);

        if (!st.collapsed)
        {
            const float h = (full_h * st.split_ratio);
            auto& slot    = out[DDockZone::top];
            slot.zone     = DDockZone::top;
            slot.active   = true;
            slot.tl       = ImVec2(_rect_tl.x, _rect_tl.y);
            slot.br       = ImVec2(_rect_br.x, _rect_tl.y + h);
            cursor_top    = slot.br.y;
        }
    }

    // -- bottom zone ---------------------------------------------------
    cursor_bot = _rect_br.y;

    if constexpr (_host.template has_field<dock_zone_bottom_tag>())
    {
        const auto& st = internal::host_zone_state_c<dock_zone_bottom_tag>(_host);

        if (!st.collapsed)
        {
            const float h = (full_h * st.split_ratio);
            auto& slot    = out[DDockZone::bottom];
            slot.zone     = DDockZone::bottom;
            slot.active   = true;
            slot.tl       = ImVec2(_rect_tl.x, _rect_br.y - h);
            slot.br       = ImVec2(_rect_br.x, _rect_br.y);
            cursor_bot    = slot.tl.y;
        }
    }

    // -- left zone -----------------------------------------------------
    //   Left and right occupy the height remaining after top+bottom
    // (cursor_top..cursor_bot) so they don't visually overlap the
    // horizontal bars.
    cursor_left = _rect_tl.x;

    if constexpr (_host.template has_field<dock_zone_left_tag>())
    {
        const auto& st = internal::host_zone_state_c<dock_zone_left_tag>(_host);

        if (!st.collapsed)
        {
            const float w = (full_w * st.split_ratio);
            auto& slot    = out[DDockZone::left];
            slot.zone     = DDockZone::left;
            slot.active   = true;
            slot.tl       = ImVec2(_rect_tl.x, cursor_top);
            slot.br       = ImVec2(_rect_tl.x + w, cursor_bot);
            cursor_left   = slot.br.x;
        }
    }

    // -- right zone ----------------------------------------------------
    cursor_right = _rect_br.x;

    if constexpr (_host.template has_field<dock_zone_right_tag>())
    {
        const auto& st = internal::host_zone_state_c<dock_zone_right_tag>(_host);

        if (!st.collapsed)
        {
            const float w = (full_w * st.split_ratio);
            auto& slot    = out[DDockZone::right];
            slot.zone     = DDockZone::right;
            slot.active   = true;
            slot.tl       = ImVec2(_rect_br.x - w, cursor_top);
            slot.br       = ImVec2(_rect_br.x, cursor_bot);
            cursor_right  = slot.tl.x;
        }
    }

    // -- center zone (fills whatever interior remains) -----------------
    if constexpr (_host.template has_field<dock_zone_center_tag>())
    {
        auto& slot  = out[DDockZone::center];
        slot.zone   = DDockZone::center;
        slot.tl     = ImVec2(cursor_left,  cursor_top);
        slot.br     = ImVec2(cursor_right, cursor_bot);
        slot.active = !slot.empty();
    }

    return out;
}


// ===========================================================================
//  6.  DRAW ZONE SPLITTERS
// ===========================================================================

/*
draw_zone_splitters
  Optional drag affordance: draws an invisible thin splitter on the
inner boundary of every active cardinal zone, captures mouse drags
on those splitters, and updates the corresponding zone's
split_ratio.  Callers that want fixed (non-resizable) zone layouts
simply omit this call.

  Splitter behavior:
    - left zone splitter:   vertical, on the zone's right edge,
                              drag dx adjusts split_ratio along x.
    - right zone splitter:  vertical, on the zone's left edge,
                              drag dx adjusts split_ratio along -x.
    - top zone splitter:    horizontal, on the zone's bottom edge,
                              drag dy adjusts split_ratio along y.
    - bottom zone splitter: horizontal, on the zone's top edge,
                              drag dy adjusts split_ratio along -y.

  Split ratios are clamped to [0.05, 0.80] so a drag can neither
collapse a zone (use the `collapsed` field for that) nor crowd
the center out.  The clamp values are local; if a future host
wants different limits, fork or parameterize.

  Returns true iff any splitter was dragged this frame.  Callers
that want to report the drag through the host event probe the
return.

Parameter(s):
  _host:  the dock_host whose zone states are mutated.
  _rects: the layout output from compute_zone_rects (used to
          locate splitter positions).
  _rect_tl: host's interior top-left (used for ratio computation).
  _rect_br: host's interior bottom-right.
Return:
  true iff a splitter was dragged this frame.
*/
template<unsigned _Feat>
bool
draw_zone_splitters(
    dock_host<_Feat>&           _host,
    const dock_host_zone_rects& _rects,
    const ImVec2&               _rect_tl,
    const ImVec2&               _rect_br
)
{
    constexpr float thickness = 4.0f;
    constexpr float min_ratio = 0.05f;
    constexpr float max_ratio = 0.80f;

    bool dragged = false;

    const float full_w = (_rect_br.x - _rect_tl.x);
    const float full_h = (_rect_br.y - _rect_tl.y);

    auto run_splitter = [&](const char* _id,
                            const ImVec2& _bar_tl,
                            const ImVec2& _bar_br,
                            bool          _vertical,
                            float&        _ratio,
                            float         _denom,
                            bool          _sign_negate)
    {
        scoped_id id(_id);

        ImGui::SetCursorScreenPos(_bar_tl);

        const ImVec2 sz(_bar_br.x - _bar_tl.x,
                        _bar_br.y - _bar_tl.y);

        ImGui::InvisibleButton("##split", sz);

        // visual hint when hovered or active
        if ( (ImGui::IsItemHovered()) ||
             (ImGui::IsItemActive()) )
        {
            ImGui::GetWindowDrawList()->AddRectFilled(
                _bar_tl, _bar_br,
                ImGui::GetColorU32(
                    palette::get<palette::cursor_border_tag>()));

            ImGui::SetMouseCursor(
                _vertical
                    ? ImGuiMouseCursor_ResizeEW
                    : ImGuiMouseCursor_ResizeNS);
        }

        if (ImGui::IsItemActive())
        {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            const float  d     = _vertical ? delta.x : delta.y;

            if (d != 0.0f && _denom > 0.0f)
            {
                const float adj = (_sign_negate ? -d : d) / _denom;
                _ratio = std::clamp(_ratio + adj, min_ratio, max_ratio);
                dragged = true;
            }
        }
    };

    // left splitter (right edge of left zone)
    if constexpr (_host.template has_field<dock_zone_left_tag>())
    {
        const auto& r = _rects[DDockZone::left];

        if (r.active)
        {
            auto& st = internal::host_zone_state<dock_zone_left_tag>(_host);
            run_splitter(
                "left_split",
                ImVec2(r.br.x - (thickness * 0.5f), r.tl.y),
                ImVec2(r.br.x + (thickness * 0.5f), r.br.y),
                true, st.split_ratio, full_w, false);
        }
    }

    // right splitter (left edge of right zone)
    if constexpr (_host.template has_field<dock_zone_right_tag>())
    {
        const auto& r = _rects[DDockZone::right];

        if (r.active)
        {
            auto& st = internal::host_zone_state<dock_zone_right_tag>(_host);
            run_splitter(
                "right_split",
                ImVec2(r.tl.x - (thickness * 0.5f), r.tl.y),
                ImVec2(r.tl.x + (thickness * 0.5f), r.br.y),
                true, st.split_ratio, full_w, true);
        }
    }

    // top splitter (bottom edge of top zone)
    if constexpr (_host.template has_field<dock_zone_top_tag>())
    {
        const auto& r = _rects[DDockZone::top];

        if (r.active)
        {
            auto& st = internal::host_zone_state<dock_zone_top_tag>(_host);
            run_splitter(
                "top_split",
                ImVec2(r.tl.x, r.br.y - (thickness * 0.5f)),
                ImVec2(r.br.x, r.br.y + (thickness * 0.5f)),
                false, st.split_ratio, full_h, false);
        }
    }

    // bottom splitter (top edge of bottom zone)
    if constexpr (_host.template has_field<dock_zone_bottom_tag>())
    {
        const auto& r = _rects[DDockZone::bottom];

        if (r.active)
        {
            auto& st = internal::host_zone_state<dock_zone_bottom_tag>(_host);
            run_splitter(
                "bottom_split",
                ImVec2(r.tl.x, r.tl.y - (thickness * 0.5f)),
                ImVec2(r.br.x, r.tl.y + (thickness * 0.5f)),
                false, st.split_ratio, full_h, true);
        }
    }

    return dragged;
}


// ===========================================================================
//  7.  RENDER DOCK HOST  (canonical all-in-one)
// ===========================================================================

/*
render_dock_host
  Single-call entry point.  Performs the full per-frame work:
  1. Computes the host's interior rectangle from the current
     ImGui window's content region.
  2. Calls _host's reconcile() to apply pending confirmations and
     refresh picker state.  Policy resolution is NOT performed
     here because the typed-children resolver overload requires
     access to concrete dockables; callers wanting policy
     resolution should call resolve_pending_policies() with
     typed pointers BEFORE this function.
  3. Computes per-zone rectangles via compute_zone_rects().
  4. Optionally draws zone splitters (controlled by
     `_draw_splitters`).
  5. Invokes the caller's _content_fn(zone, rect, state) for each
     active zone so the caller renders whatever it wants there.
  6. Draws the picker if host.picker_open is true.  Picker results
     are dispatched back onto the picker_owner handle through the
     existing handle vtable - but because the vtable cannot mutate
     target_zone on the underlying component, the actual dispatch
     happens via the returned event: the caller observes
     `picker_committed` and `picked_zone` and routes them onto the
     typed component itself with `dock(owner, picked_zone)`.

  The _content_fn callable receives, for each enabled zone:
    - DDockZone:                the zone identifier
    - const dock_zone_rect&:    screen rectangle
    - dock_zone_state&:         the zone's runtime state (split,
                                collapsed) for in-callback mutation

  This is the same shape as the dockable.hpp invariants prescribe:
the host renders the geometry; clients fill the geometry.

Parameter(s):
  _host:           the host to render.
  _content_fn:     callable invoked once per active zone.
  _ctx:            render context (unused currently).
  _draw_splitters: when true, enables drag-to-resize on zone
                   boundaries.  Default true.
Return:
  A dock_host_event populated with the per-frame report.
*/
template<unsigned _Feat,
          typename _ContentFn>
dock_host_event
render_dock_host(
    dock_host<_Feat>& _host,
    _ContentFn&&      _content_fn,
    render_context&   _ctx,
    bool              _draw_splitters = true
)
{
    dock_host_event evt;

    (void)_ctx;

    if (!_host.visible)
    {
        return evt;
    }

    scoped_disabled dis(!_host.enabled);

    // -- interior rectangle -------------------------------------------
    const ImVec2 rect_tl = ImGui::GetCursorScreenPos();
    const ImVec2 avail   = ImGui::GetContentRegionAvail();
    const ImVec2 rect_br = ImVec2(rect_tl.x + avail.x,
                                  rect_tl.y + avail.y);

    // -- pre-reconcile pending count ----------------------------------
    //   We count pending handles BEFORE reconcile so the reported
    // confirmed_docks accurately reflects what reconcile cleared.
    std::size_t pre_pending = 0;

    for (const auto& h : _host.items)
    {
        if (h.is_dock_pending())
        {
            ++pre_pending;
        }
    }

    // -- reconcile (confirms pending + refreshes picker state) --------
    uxoxo::component::reconcile(_host);

    evt.confirmed_docks = pre_pending;

    // -- layout -------------------------------------------------------
    const dock_host_zone_rects rects =
        compute_zone_rects(_host, rect_tl, rect_br);

    // -- draw zone backgrounds (subtle tint per zone) -----------------
    //   Each active zone gets a thin background fill from the palette
    // so empty zones are visually distinguishable from unsupported
    // ones.  Background tag varies by zone for slight visual cueing.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (unsigned i = 1; i <= 5; ++i)
        {
            const DDockZone z = static_cast<DDockZone>(i);
            const auto&     r = rects[z];

            if (!r.active)
            {
                continue;
            }

            const ImU32 fill = (z == DDockZone::center)
                ? ImGui::GetColorU32(
                      palette::get<palette::window_bg_tag>())
                : ImGui::GetColorU32(
                      palette::get<palette::toolbar_bg_tag>());

            dl->AddRectFilled(r.tl, r.br, fill);
            dl->AddRect(r.tl, r.br,
                        ImGui::GetColorU32(
                            palette::get<palette::toolbar_border_tag>()),
                        0.0f, 0, 1.0f);
        }
    }

    // -- invoke user content callback per zone ------------------------
    //   Each zone gets a child window pinned to its rect so the
    // caller's ImGui draw calls clip correctly.  The callback runs
    // inside the child window; on return we close it before moving
    // to the next zone.
    auto invoke_zone = [&](DDockZone _z, dock_zone_state& _state)
    {
        const auto& r = rects[_z];

        if (!r.active)
        {
            return;
        }

        ImGui::SetCursorScreenPos(r.tl);

        const ImVec2 sz = r.size();

        char id_buf[24];
        std::snprintf(id_buf, sizeof(id_buf),
                      "##dock_zone_%u",
                      static_cast<unsigned>(_z));

        if (ImGui::BeginChild(id_buf, sz, false))
        {
            _content_fn(_z, r, _state);
        }

        ImGui::EndChild();
    };

    if constexpr (_host.template has_field<dock_zone_top_tag>())
    {
        invoke_zone(
            DDockZone::top,
            internal::host_zone_state<dock_zone_top_tag>(_host));
    }
    if constexpr (_host.template has_field<dock_zone_bottom_tag>())
    {
        invoke_zone(
            DDockZone::bottom,
            internal::host_zone_state<dock_zone_bottom_tag>(_host));
    }
    if constexpr (_host.template has_field<dock_zone_left_tag>())
    {
        invoke_zone(
            DDockZone::left,
            internal::host_zone_state<dock_zone_left_tag>(_host));
    }
    if constexpr (_host.template has_field<dock_zone_right_tag>())
    {
        invoke_zone(
            DDockZone::right,
            internal::host_zone_state<dock_zone_right_tag>(_host));
    }
    if constexpr (_host.template has_field<dock_zone_center_tag>())
    {
        invoke_zone(
            DDockZone::center,
            internal::host_zone_state<dock_zone_center_tag>(_host));
    }

    // -- splitters (after content so they overlay on the boundaries) --
    if (_draw_splitters)
    {
        if (draw_zone_splitters(_host, rects, rect_tl, rect_br))
        {
            evt.splitter_dragged = true;
        }
    }

    // -- picker (only when a child requested it via popup_open) -------
    if (_host.picker_open)
    {
        const ImVec2 center = ImVec2((rect_tl.x + rect_br.x) * 0.5f,
                                      (rect_tl.y + rect_br.y) * 0.5f);

        const dock_picker_event pe =
            render_dock_picker_at(center, _host.features);

        if (pe.committed)
        {
            evt.picker_committed = true;
            evt.committed        = true;
            evt.picked_zone      = pe.picked_zone;
        }

        if (pe.dismissed)
        {
            evt.picker_dismissed = true;
            evt.dismissed        = true;
        }
    }

    // -- assemble the event report ------------------------------------
    evt.changed = ( (evt.confirmed_docks > 0u) ||
                    (evt.resolved_pending)     ||
                    (evt.splitter_dragged)     ||
                    (evt.picker_committed) );

    evt.any_change = ( (evt.changed)          ||
                       (evt.committed)        ||
                       (evt.dismissed)        ||
                       (evt.focus_gained)     ||
                       (evt.focus_lost)       ||
                       (evt.picker_committed) ||
                       (evt.picker_dismissed) );

    return evt;
}


NS_END  // imgui
NS_END  // uxoxo


// =============================================================================
//  BACKWARD-COMPAT BRIDGE
// =============================================================================
//   The host renderer's canonical home is `uxoxo::imgui`.  The
// bridge re-exports its public symbols into `uxoxo::component` so
// generic dockable code outside the imgui layer can write the
// shorter qualification when convenient.

NS_UXOXO
NS_COMPONENT

using uxoxo::imgui::dock_host_event;
using uxoxo::imgui::dock_zone_rect;
using uxoxo::imgui::dock_host_zone_rects;
using uxoxo::imgui::resolve_pending_policies;
using uxoxo::imgui::compute_zone_rects;
using uxoxo::imgui::draw_zone_splitters;
using uxoxo::imgui::render_dock_host;

NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_DOCK_HOST_
