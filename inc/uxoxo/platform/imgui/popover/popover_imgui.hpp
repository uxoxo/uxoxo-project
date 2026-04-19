/*******************************************************************************
* uxoxo [imgui]                                               imgui_popover.hpp
*
* ImGui backend for popover<>:
*   Renders popover<_Content, _Source, _Features, _Policy> as an ImGui
* window positioned relative to a caller-supplied anchor rect.  The
* popover template itself is framework-agnostic; this header is the
* thin adapter layer that maps its declarative state (direction,
* alignment, sizing hints, lifecycle flags) onto ImGui primitives.
*
*   What this file provides:
*     - popover_render(pop, anchor_min, anchor_max, content_fn)
*         Full-control entry point.  Caller supplies the anchor
*         rectangle and a callable that renders `_Content` inside
*         the popover window.
*     - popover_render_at_last_item(pop, content_fn)
*         Convenience overload that reads the anchor rect from the
*         last ImGui item (GetItemRectMin / GetItemRectMax), so
*         `ImGui::Button("x"); popover_render_at_last_item(pop, …);`
*         just works.
*     - popover_handle_nav_keys(pop)
*         Arrow / Home / End / PageUp / PageDown / Enter key
*         forwarding to drop_container-shaped content.  SFINAE-gated;
*         only compiles when `_Content` looks like a drop_container.
*         Content types with their own nav flow skip this helper.
*     - render_drop_container(dc, labeler)
*         Canned content renderer for drop_container<_Item>.  Each
*         item rendered via ImGui::Selectable with the highlighted
*         index shown as selected.  Labeler maps `_Item -> const char*`
*         (or anything printable via `%s`).
*
*   Input routing model:
*     ImGui is immediate-mode and focus-sensitive.  An autocomplete
*   popover usually lives BELOW a focused text input — the text input
*   owns focus, the popover is drawn but does not steal it.  To make
*   this work, popover_render applies `NoFocusOnAppearing` and does
*   NOT process keyboard input itself.  The caller drives navigation
*   either by calling `popover_handle_nav_keys(pop)` after the
*   textbox consumes its own keys, or by wiring keys directly to
*   drop_* / content-specific verbs.
*
*     Context-menu style popovers (right-click menus, standalone
*   pickers) WILL take focus — the renderer the caller passes in is
*   free to call Begin/End child widgets that grab focus, and
*   popover_handle_nav_keys can be called unconditionally.
*
*   Scrolling:
*     The shell window uses `AlwaysAutoResize`; the canned
*   drop_container renderer wraps its Selectable list in a
*   BeginChild sized by `max_visible_rows * row_height`.  When the
*   content exceeds that, ImGui draws a scrollbar inside the child
*   and the outer window stays the same size.  Custom content
*   renderers that want this behavior should do the same.
*
*   What this file does NOT handle:
*     - Modal enforcement.  `pop.modal` is a hint; the caller is
*       responsible for not drawing interactive widgets behind the
*       popover when modal is set (e.g. by checking it before
*       drawing the rest of the frame).
*     - Animations / transitions.  ImGui has no built-in animation
*       system; add your own if needed by interpolating the
*       anchor_offset_{x,y} fields.
*     - Focus trap.  `pop.focus_trap` is a hint only.
*
*
* path:      /inc/uxoxo/platform/imgui/popover/imgui_popover.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_IMGUI_COMPONENT_POPOVER_DRAW_
#define  UXOXO_IMGUI_COMPONENT_POPOVER_DRAW_ 1

// std
#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/popover/popover.hpp"
#include "../../../templates/component/container/drop_container.hpp"


NS_UXOXO
NS_COMPONENT
NS_IMGUI

// ===============================================================================
//  1  PLACEMENT HELPERS
// ===============================================================================
//   Internal math shared by the main render function.  These are
// `inline` so the header is self-contained and ODR-safe.

// compute_pivot
//   function: maps (direction, alignment) to the ImVec2 pivot that
// ImGui::SetNextWindowPos uses to interpret the position point.
// The pivot is in unit coordinates where (0, 0) is the window's
// top-left and (1, 1) is its bottom-right.
inline ImVec2
compute_pivot(
    DPopoverDirection _dir,
    DPopoverAlignment _align
) noexcept
{
    float px = 0.0f;
    float py = 0.0f;

    // cross-axis pivot from alignment
    float cross = (_align == DPopoverAlignment::start)  ? 0.0f
                : (_align == DPopoverAlignment::center) ? 0.5f
                :                                         1.0f;

    // map axis according to direction
    switch (_dir)
    {
        case DPopoverDirection::down:
            px = cross;
            py = 0.0f;
            break;

        case DPopoverDirection::up:
            px = cross;
            py = 1.0f;
            break;

        case DPopoverDirection::right:
            px = 0.0f;
            py = cross;
            break;

        case DPopoverDirection::left:
            px = 1.0f;
            py = cross;
            break;

        case DPopoverDirection::automatic:
        default:
            // should have been resolved upstream; fall back to down/start
            px = 0.0f;
            py = 0.0f;
            break;
    }

    return ImVec2(px, py);
}

// compute_anchor_point
//   function: picks the point on the anchor rect from which the
// popover extends.  Paired with compute_pivot so the popover's
// pivot-corner lands at this point.
inline ImVec2
compute_anchor_point(
    const ImVec2&     _anchor_min,
    const ImVec2&     _anchor_max,
    DPopoverDirection _dir,
    DPopoverAlignment _align
) noexcept
{
    // cross-axis coordinate from alignment
    auto cross_x = [&]() -> float
    {
        return (_align == DPopoverAlignment::start)  ? _anchor_min.x
             : (_align == DPopoverAlignment::center) ? 0.5f * (_anchor_min.x + _anchor_max.x)
             :                                         _anchor_max.x;
    };

    auto cross_y = [&]() -> float
    {
        return (_align == DPopoverAlignment::start)  ? _anchor_min.y
             : (_align == DPopoverAlignment::center) ? 0.5f * (_anchor_min.y + _anchor_max.y)
             :                                         _anchor_max.y;
    };

    switch (_dir)
    {
        case DPopoverDirection::down:  return ImVec2(cross_x(), _anchor_max.y);
        case DPopoverDirection::up:    return ImVec2(cross_x(), _anchor_min.y);
        case DPopoverDirection::right: return ImVec2(_anchor_max.x, cross_y());
        case DPopoverDirection::left:  return ImVec2(_anchor_min.x, cross_y());
        default:                       return ImVec2(_anchor_min.x, _anchor_max.y);
    }
}

// resolve_automatic_direction
//   function: chooses down / up / right / left based on available
// space around the anchor.  Prefers down, then up, then right, then
// left when a size hint fits; otherwise returns the direction with
// the most raw space.
inline DPopoverDirection
resolve_automatic_direction(
    const ImVec2& _anchor_min,
    const ImVec2& _anchor_max,
    float         _estimated_w,
    float         _estimated_h
) noexcept
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;

    const float space_down  = disp.y - _anchor_max.y;
    const float space_up    = _anchor_min.y;
    const float space_right = disp.x - _anchor_max.x;
    const float space_left  = _anchor_min.x;

    // first pass: prefer canonical order if it fits the hint
    if (_estimated_h > 0.0f)
    {
        if (space_down >= _estimated_h) return DPopoverDirection::down;
        if (space_up   >= _estimated_h) return DPopoverDirection::up;
    }

    if (_estimated_w > 0.0f)
    {
        if (space_right >= _estimated_w) return DPopoverDirection::right;
        if (space_left  >= _estimated_w) return DPopoverDirection::left;
    }

    // second pass: no hint fit — pick the direction with the most space
    float             best_space = space_down;
    DPopoverDirection best_dir   = DPopoverDirection::down;

    if (space_up > best_space)
    {
        best_space = space_up;
        best_dir   = DPopoverDirection::up;
    }

    if (space_right > best_space)
    {
        best_space = space_right;
        best_dir   = DPopoverDirection::right;
    }

    if (space_left > best_space)
    {
        best_space = space_left;
        best_dir   = DPopoverDirection::left;
    }

    return best_dir;
}




// ===============================================================================
//  2  CORE RENDER
// ===============================================================================

// popover_render
//   function: draws `_pop` as an ImGui window anchored against the
// given rectangle.  No-op when `!_pop.visible`.  The caller supplies
// a `_ContentFn` callable invoked as `_content_fn(_pop.content)`
// from inside the popover window body.
//
//   Side effects on `_pop`:
//     - resolved_direction is updated when direction == automatic
//     - active is synced to ImGui::IsWindowFocused() for the popover
//     - visible may be flipped to false via popover_dismiss when
//       pf_dismissable is set AND (Esc pressed | click outside)
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy,
          typename      _ContentFn>
void
popover_render(
    popover<_Content, _Source, _Features, _Policy>& _pop,
    const ImVec2&                                   _anchor_min,
    const ImVec2&                                   _anchor_max,
    _ContentFn&&                                    _content_fn
)
{
    if (!_pop.visible)
    {
        return;
    }

    // ------------------------------------------------------------
    // 1. resolve direction (automatic -> concrete)
    // ------------------------------------------------------------
    DPopoverDirection dir = _pop.direction;

    if (dir == DPopoverDirection::automatic)
    {
        // estimate size for automatic resolution — use explicit hints
        // when provided, else a sensible default based on row count
        const float row_h = ImGui::GetFrameHeightWithSpacing();
        const float est_h = (_pop.desired_height > 0)
                            ? static_cast<float>(_pop.desired_height)
                          : (_pop.max_visible_entries > 0)
                            ? static_cast<float>(_pop.max_visible_entries) * row_h
                            : row_h * 4.0f;
        const float est_w = (_pop.desired_width > 0)
                            ? static_cast<float>(_pop.desired_width)
                            : 240.0f;

        dir = resolve_automatic_direction(_anchor_min,
                                          _anchor_max,
                                          est_w,
                                          est_h);
    }

    _pop.resolved_direction = dir;

    // ------------------------------------------------------------
    // 2. position
    // ------------------------------------------------------------
    ImVec2 anchor_pt = compute_anchor_point(_anchor_min,
                                            _anchor_max,
                                            dir,
                                            _pop.alignment);
    ImVec2 pivot     = compute_pivot(dir, _pop.alignment);

    // apply user offset
    anchor_pt.x += static_cast<float>(_pop.anchor_offset_x);
    anchor_pt.y += static_cast<float>(_pop.anchor_offset_y);

    ImGui::SetNextWindowPos(anchor_pt, ImGuiCond_Always, pivot);

    // ------------------------------------------------------------
    // 3. size constraints
    // ------------------------------------------------------------
    //   When both dimensions are fixed, set explicit size.  Otherwise
    // let the window auto-resize within a constraint box derived
    // from the hints and max_visible_entries.
    if ((_pop.desired_width > 0) && (_pop.desired_height > 0))
    {
        ImGui::SetNextWindowSize(
            ImVec2(static_cast<float>(_pop.desired_width),
                   static_cast<float>(_pop.desired_height)),
            ImGuiCond_Always);
    }
    else
    {
        const float row_h = ImGui::GetFrameHeightWithSpacing();
        const float cap_h = (_pop.max_visible_entries > 0)
                            ? static_cast<float>(_pop.max_visible_entries) * row_h
                            : FLT_MAX;

        const ImVec2 min_sz(
            (_pop.desired_width > 0)
                ? static_cast<float>(_pop.desired_width)
                : 0.0f,
            (_pop.min_visible_entries > 0)
                ? static_cast<float>(_pop.min_visible_entries) * row_h
                : 0.0f);

        const ImVec2 max_sz(
            (_pop.desired_width > 0)
                ? static_cast<float>(_pop.desired_width)
                : FLT_MAX,
            (_pop.desired_height > 0)
                ? static_cast<float>(_pop.desired_height)
                : cap_h);

        ImGui::SetNextWindowSizeConstraints(min_sz, max_sz);
    }

    // ------------------------------------------------------------
    // 4. window flags
    // ------------------------------------------------------------
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar      |
        ImGuiWindowFlags_NoResize        |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing;

    if ((_pop.desired_width == 0) || (_pop.desired_height == 0))
    {
        flags |= ImGuiWindowFlags_AlwaysAutoResize;
    }

    // ------------------------------------------------------------
    // 5. stable ID — anchor_id is the user's disambiguator
    // ------------------------------------------------------------
    //   If the user didn't set one, fall back to the popover's
    // address; not ideal for stable IDs across frames with moving
    // objects but workable for the common case.
    const std::string window_id =
        _pop.anchor_id.empty()
            ? std::string("##popover_") +
                  std::to_string(reinterpret_cast<std::uintptr_t>(&_pop))
            : std::string("##popover_") + _pop.anchor_id;

    // ------------------------------------------------------------
    // 6. draw — remember rect for outside-click detection
    // ------------------------------------------------------------
    //   `was_drawn` tracks whether Begin returned true.  When false
    // (window clipped / fully obscured), win_pos and win_size stay
    // zero and the click-outside check below must be skipped —
    // otherwise every click outside the anchor would dismiss a
    // popover that's merely hidden behind another window.
    ImVec2 win_pos   (0.0f, 0.0f);
    ImVec2 win_size  (0.0f, 0.0f);
    bool   focused    = false;
    bool   was_drawn  = false;

    if (ImGui::Begin(window_id.c_str(), nullptr, flags))
    {
        was_drawn = true;
        win_pos   = ImGui::GetWindowPos();
        win_size  = ImGui::GetWindowSize();
        focused   = ImGui::IsWindowFocused(
                        ImGuiFocusedFlags_RootAndChildWindows);

        // -- header (labeled popovers only) --------------------------
        if constexpr ((_Features & pf_labeled) != 0)
        {
            if (!_pop.label.empty())
            {
                ImGui::TextDisabled("%s", _pop.label.c_str());
                ImGui::Separator();
            }
        }

        // -- loading state overrides content -------------------------
        if (_pop.is_loading)
        {
            const char* msg = _pop.loading_message.empty()
                              ? "Loading..."
                              : _pop.loading_message.c_str();

            ImGui::TextDisabled("%s", msg);
        }
        else
        {
            // delegate content rendering to the caller
            std::forward<_ContentFn>(_content_fn)(_pop.content);
        }

        // -- Esc dismissal (dismissable popovers only) ---------------
        if constexpr ((_Features & pf_dismissable) != 0)
        {
            if (focused && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                popover_dismiss(_pop);
            }
        }
    }
    ImGui::End();

    // ------------------------------------------------------------
    // 7. sync active flag (reflects previous-frame focus)
    // ------------------------------------------------------------
    _pop.active = focused;

    // ------------------------------------------------------------
    // 8. click-outside dismissal (dismissable popovers only)
    // ------------------------------------------------------------
    //   Triggered on a fresh mouse-down that is both outside the
    // popover's rect AND outside the anchor's rect — clicking the
    // anchor is a toggle gesture, not a dismissal.  Skipped when
    // the window wasn't drawn this frame (see `was_drawn` note).
    if constexpr ((_Features & pf_dismissable) != 0)
    {
        if ( _pop.visible                                    &&
             was_drawn                                       &&
             ImGui::IsMouseClicked(ImGuiMouseButton_Left) )
        {
            const ImVec2 mp = ImGui::GetMousePos();

            const bool in_window =
                ( (mp.x >= win_pos.x)              &&
                  (mp.x <= win_pos.x + win_size.x) &&
                  (mp.y >= win_pos.y)              &&
                  (mp.y <= win_pos.y + win_size.y) );

            const bool in_anchor =
                ( (mp.x >= _anchor_min.x) &&
                  (mp.x <= _anchor_max.x) &&
                  (mp.y >= _anchor_min.y) &&
                  (mp.y <= _anchor_max.y) );

            if ( (!in_window) && (!in_anchor) )
            {
                popover_dismiss(_pop);
            }
        }
    }

    return;
}




// ===============================================================================
//  3  ANCHOR-TO-LAST-ITEM CONVENIENCE
// ===============================================================================

// popover_render_at_last_item
//   function: convenience overload that pulls the anchor rectangle
// from the last ImGui item drawn.  Call immediately after the
// anchor widget (Button, InputText, etc.) has been drawn.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy,
          typename      _ContentFn>
void
popover_render_at_last_item(
    popover<_Content, _Source, _Features, _Policy>& _pop,
    _ContentFn&&                                    _content_fn
)
{
    const ImVec2 amin = ImGui::GetItemRectMin();
    const ImVec2 amax = ImGui::GetItemRectMax();

    popover_render(_pop,
                   amin,
                   amax,
                   std::forward<_ContentFn>(_content_fn));

    return;
}




// ===============================================================================
//  4  NAV KEY FORWARDING (drop_container content)
// ===============================================================================

NS_INTERNAL

    // has_drop_nav_shape
    //   trait: structural detector for drop_container-like content
    // (has both `.items` and `.highlighted` data members).  Used
    // to gate popover_handle_nav_keys so it only compiles for
    // compatible content types.
    template <typename, typename = void>
    struct has_drop_nav_shape : std::false_type
    {};

    template <typename _Type>
    struct has_drop_nav_shape<_Type, std::void_t<
        decltype(std::declval<_Type&>().items),
        decltype(std::declval<_Type&>().highlighted)
    >> : std::true_type
    {};

}   // NS_INTERNAL

// popover_handle_nav_keys
//   function: forwards standard navigation keys (arrow / home / end /
// pageup / pagedown / enter) to a drop_container-shaped content via
// the drop_* free functions.  SFINAE-gated: only compiles when
// `_Content` looks like a drop_container.
//
//   The caller decides when to invoke this — typically after the
// focused textbox has processed its own key bindings.  No-op when
// `!pop.visible`.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy,
          std::enable_if_t<
              internal::has_drop_nav_shape<_Content>::value,
              int> = 0>
void
popover_handle_nav_keys(
    popover<_Content, _Source, _Features, _Policy>& _pop
)
{
    if (!_pop.visible)
    {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
    {
        drop_highlight_next(_pop.content);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
    {
        drop_highlight_prev(_pop.content);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true))
    {
        drop_page_down(_pop.content);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true))
    {
        drop_page_up(_pop.content);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
    {
        drop_highlight_first(_pop.content);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_End, false))
    {
        drop_highlight_last(_pop.content);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false))
    {
        if (drop_select(_pop.content))
        {
            popover_notify_selected(_pop);
        }
    }

    return;
}




// ===============================================================================
//  5  CANNED CONTENT RENDERERS
// ===============================================================================

// render_drop_container
//   function: renders a drop_container<_Item, _F> as a vertical list
// of ImGui::Selectable rows.  The highlighted item is drawn as
// selected.  Clicking a row sets it as highlighted and fires
// drop_select — which invokes on_select if installed.  The rendered
// rows live inside a BeginChild sized by max_visible_rows so the
// list scrolls internally instead of ballooning the outer window.
//
//   `_labeler` maps `const _Item&` to something printable via `%s`.
// Typical implementations:
//     [](const std::string& s)   { return s.c_str(); }
//     [](const my_suggest& s)    { return s.display.c_str(); }
//
//   When the highlighted index changes (detected by a navigation
// key this frame), the list auto-scrolls to keep the highlighted
// row in view via ImGui::SetScrollHereY.
template <typename      _Item,
          std::uint32_t _F,
          typename      _Labeler>
void
render_drop_container(
    drop_container<_Item, _F>& _dc,
    _Labeler&&                 _labeler
)
{
    using size_type = typename drop_container<_Item, _F>::size_type;

    if (drop_is_empty(_dc))
    {
        ImGui::TextDisabled("(no items)");

        return;
    }

    // sized scroll region — keeps outer window from growing unbounded
    const float row_h = ImGui::GetFrameHeightWithSpacing();
    const float child_h =
        (_dc.max_visible_rows > 0)
            ? static_cast<float>(_dc.max_visible_rows) * row_h
            : 0.0f;

    // child id derived from address for uniqueness across multiple
    // drop_containers in the same frame
    char child_id[32];
    std::snprintf(child_id,
                  sizeof(child_id),
                  "##dc_%p",
                  static_cast<const void*>(&_dc));

    // detect nav key presses this frame so we can scroll-to-current
    const bool nav_fired =
        ( ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) ||
          ImGui::IsKeyPressed(ImGuiKey_UpArrow,   true) ||
          ImGui::IsKeyPressed(ImGuiKey_PageDown,  true) ||
          ImGui::IsKeyPressed(ImGuiKey_PageUp,    true) ||
          ImGui::IsKeyPressed(ImGuiKey_Home,      false) ||
          ImGui::IsKeyPressed(ImGuiKey_End,       false) );

    if (ImGui::BeginChild(child_id,
                          ImVec2(0.0f, child_h),
                          false,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        for (size_type i = 0; i < _dc.items.size(); ++i)
        {
            const bool highlighted = (i == _dc.highlighted);

            // push index to keep Selectable IDs unique
            ImGui::PushID(static_cast<int>(i));

            if (ImGui::Selectable(_labeler(_dc.items[i]), highlighted))
            {
                drop_highlight_set(_dc, i);
                drop_select(_dc);
            }

            // auto-scroll to the highlighted row when nav keys moved it
            if (highlighted && nav_fired)
            {
                ImGui::SetScrollHereY(0.5f);
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    return;
}

// render_drop_container (std::string convenience)
//   function: overload for drop_container<std::string> that uses
// the string itself as the label.  Spares the caller from writing
// a one-line lambda for the most common case.
template <std::uint32_t _F>
void
render_drop_container(
    drop_container<std::string, _F>& _dc
)
{
    render_drop_container(_dc,
                          [](const std::string& _s) -> const char*
                          {
                              return _s.c_str();
                          });

    return;
}


NS_END  // imgui
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_POPOVER_DRAW_