/*******************************************************************************
* uxoxo [imgui]                                                imgui_chrome.hpp
*
* Shared toolbar / status bar chrome primitives:
*   The database, MySQL, MariaDB, dev console, and (to a lesser degree)
* table renderers all draw a fixed-height "chrome" surface above and / or
* below their main content - a toolbar at the top, a status bar at the
* bottom, sometimes both.  The geometry, background fill, border, and
* button-row layout were duplicated across each renderer; this header
* consolidates them.
*
*   The chrome primitives are RAII-style scopes that wrap ImGui::BeginChild
* / EndChild with the shared palette colors and dimensions baked in:
*
*     toolbar_scope     A horizontally-laid child window of fixed height
*                       (toolbar_height_tag) using the toolbar_bg_tag and
*                       toolbar_border_tag palette colors.  The constructor
*                       calls BeginChild; the destructor calls EndChild.
*
*     status_bar_scope  Same shape as toolbar_scope but anchored at the
*                       bottom of the parent region with status_bar_bg_tag.
*
*   Within either scope, callers freely use ImGui::SameLine() to lay out
* button rows, indicator dots via `status_indicator(...)`, and arbitrary
* widgets.  The scopes do not push / pop ID - callers should bracket
* their content with `scoped_id(some_key)` from imgui_scope.hpp when
* needed.
*
*   `status_indicator(label, ok, warn)` is the third primitive: a small
* circle drawn with the appropriate palette indicator tag followed by
* the label text, with no extra layout.  Used to surface
* connection / dirty / stale state in database table status bars.
*
*   `chrome_separator()` draws a vertical separator between toolbar
* button groups using the shared separator_tag and separator_thickness_tag
* palette entries.
*
* Contents:
*   1.  toolbar_scope
*   2.  status_bar_scope
*   3.  status_indicator
*   4.  chrome_separator
*
*
* path:      /inc/uxoxo/platform/imgui/core/imgui_chrome.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.08
*******************************************************************************/

#ifndef  UXOXO_IMGUI_CHROME_
#define  UXOXO_IMGUI_CHROME_ 1

// std
#include <cstdint>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./imgui_palette.hpp"
#include "./imgui_scope.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  1.  TOOLBAR SCOPE
// ===========================================================================

// toolbar_scope
//   class: RAII guard wrapping ImGui::BeginChild / EndChild for a
// fixed-height toolbar surface.  Pushes the shared toolbar background
// color in the constructor and pops it in the destructor.  Width
// defaults to ImGui's content region width; height comes from
// `palette::get<palette::toolbar_height_tag>()` unless overridden.
//
//   The scope is `if (begun)`-truthy: callers should test the result
// of `is_visible()` before drawing content, because BeginChild may
// return false when the parent region is too small.  Either way, the
// destructor still calls EndChild - mirroring ImGui's own contract.
class toolbar_scope
{
public:
    explicit toolbar_scope(
            const char* _id,
            float       _height = -1.0f
        ) noexcept
            : m_visible(false)
        {
            float h;

            h = (_height > 0.0f)
                    ? _height
                    : palette::get<palette::toolbar_height_tag>();

            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                palette::get<palette::toolbar_bg_tag>());

            m_visible = ImGui::BeginChild(
                _id,
                ImVec2(0.0f, h),
                false,
                ImGuiWindowFlags_NoScrollbar);
        }

    ~toolbar_scope() noexcept
        {
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

    toolbar_scope(const toolbar_scope&)            = delete;
    toolbar_scope& operator=(const toolbar_scope&) = delete;
    toolbar_scope(toolbar_scope&&)                 = delete;
    toolbar_scope& operator=(toolbar_scope&&)      = delete;

    // is_visible
    //   function: true iff BeginChild produced a drawable region.
    [[nodiscard]] bool is_visible() const noexcept
        { return m_visible; }

private:
    bool m_visible;
};


// ===========================================================================
//  2.  STATUS BAR SCOPE
// ===========================================================================

// status_bar_scope
//   class: RAII guard for a fixed-height status / footer bar.  Mirror
// of toolbar_scope using palette::status_bar_bg_tag and
// palette::status_bar_height_tag.  The destination region is
// flagged with ImGuiWindowFlags_NoScrollbar | NoScrollWithMouse to
// keep the footer flat and immune to nested scrolling.
class status_bar_scope
{
public:
    explicit status_bar_scope(
            const char* _id,
            float       _height = -1.0f
        ) noexcept
            : m_visible(false)
        {
            float h;

            h = (_height > 0.0f)
                    ? _height
                    : palette::get<palette::status_bar_height_tag>();

            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                palette::get<palette::status_bar_bg_tag>());

            m_visible = ImGui::BeginChild(
                _id,
                ImVec2(0.0f, h),
                false,
                ( ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse ));
        }

    ~status_bar_scope() noexcept
        {
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

    status_bar_scope(const status_bar_scope&)            = delete;
    status_bar_scope& operator=(const status_bar_scope&) = delete;
    status_bar_scope(status_bar_scope&&)                 = delete;
    status_bar_scope& operator=(status_bar_scope&&)      = delete;

    [[nodiscard]] bool is_visible() const noexcept
        { return m_visible; }

private:
    bool m_visible;
};


// ===========================================================================
//  3.  STATUS INDICATOR
// ===========================================================================

/*
status_indicator
  Draws a small filled circle followed by `_label` text.  The circle
color is selected from the palette indicator tags by truth-table:

      _ok    _warn    _error    color resolved from
      ----   ----     ----      -----------------------
      true   false    false     indicator_ok_tag
      false  true     false     indicator_warn_tag
      false  false    true      indicator_error_tag
      false  false    false     indicator_disconn_tag

  The convention matches the database-table status bar: connected and
clean is "ok", connected and dirty is "warn", recently failed is
"error", and absence of any state is "disconnected".  Callers
needing custom colors should use the explicit `status_indicator_ex`
overload below.

Parameter(s):
  _label: text to draw after the indicator dot.  Pass nullptr or
          empty for a dot-only indicator.
  _ok:    when true, resolves to indicator_ok_tag.
  _warn:  when true (and _ok false), resolves to indicator_warn_tag.
  _error: when true (and _ok and _warn false), resolves to
          indicator_error_tag.
Return:
  none.
*/
inline void
status_indicator(
    const char* _label,
    bool        _ok,
    bool        _warn  = false,
    bool        _error = false
) noexcept
{
    ImVec4      color;
    ImDrawList* dl;
    ImVec2      origin;
    float       radius;

    if (_ok)
    {
        color = palette::get<palette::indicator_ok_tag>();
    }
    else if (_warn)
    {
        color = palette::get<palette::indicator_warn_tag>();
    }
    else if (_error)
    {
        color = palette::get<palette::indicator_error_tag>();
    }
    else
    {
        color = palette::get<palette::indicator_disconn_tag>();
    }

    dl     = ImGui::GetWindowDrawList();
    origin = ImGui::GetCursorScreenPos();
    radius = 5.0f;

    // dot
    dl->AddCircleFilled(
        ImVec2(origin.x + radius,
               origin.y + (ImGui::GetTextLineHeight() * 0.5f)),
        radius,
        ImGui::GetColorU32(color));

    // advance cursor past the dot and draw label
    ImGui::Dummy(ImVec2((radius * 2.0f) + 4.0f, 0.0f));
    ImGui::SameLine();

    if ( (_label) && (*_label) )
    {
        scoped_color label_color {
            { ImGuiCol_Text,
                  palette::get<palette::text_muted_tag>() }
        };

        ImGui::TextUnformatted(_label);
    }

    return;
}

/*
status_indicator_ex
  Explicit-color overload of status_indicator for callers whose
indicator color does not map onto the four canonical tags.

Parameter(s):
  _label: text to draw after the indicator dot.
  _color: explicit indicator color.
Return:
  none.
*/
inline void
status_indicator_ex(
    const char*   _label,
    const ImVec4& _color
) noexcept
{
    ImDrawList* dl;
    ImVec2      origin;
    float       radius;

    dl     = ImGui::GetWindowDrawList();
    origin = ImGui::GetCursorScreenPos();
    radius = 5.0f;

    dl->AddCircleFilled(
        ImVec2(origin.x + radius,
               origin.y + (ImGui::GetTextLineHeight() * 0.5f)),
        radius,
        ImGui::GetColorU32(_color));

    ImGui::Dummy(ImVec2((radius * 2.0f) + 4.0f, 0.0f));
    ImGui::SameLine();

    if ( (_label) && (*_label) )
    {
        scoped_color label_color {
            { ImGuiCol_Text,
                  palette::get<palette::text_muted_tag>() }
        };

        ImGui::TextUnformatted(_label);
    }

    return;
}


// ===========================================================================
//  4.  CHROME SEPARATOR
// ===========================================================================

/*
chrome_separator
  Draws a thin vertical separator line between toolbar button groups.
Pulls color from palette::separator_tag and thickness from
palette::separator_thickness_tag.  Advances the cursor with SameLine
on entry and exit so the separator inserts cleanly into a horizontal
button row:

      [ Refresh ] [ Commit ]   |   [ Analyze ] [ Optimize ]

  The separator's height matches the current line height; padding
above and below is contributed via Dummy items so adjacent buttons
do not visually crowd the line.

Parameter(s):
  _padding: pixels of horizontal whitespace placed on each side of
            the separator.  Default 6.0f matches the toolbar's
            existing convention.
Return:
  none.
*/
inline void
chrome_separator(
    float _padding = 6.0f
) noexcept
{
    ImDrawList* dl;
    ImVec2      origin;
    float       line_h;
    float       thickness;
    ImU32       color;

    ImGui::SameLine(0.0f, _padding);

    dl        = ImGui::GetWindowDrawList();
    origin    = ImGui::GetCursorScreenPos();
    line_h    = ImGui::GetTextLineHeight();
    thickness = palette::get<palette::separator_thickness_tag>();
    color     = ImGui::GetColorU32(palette::get<palette::separator_tag>());

    dl->AddLine(
        ImVec2(origin.x, origin.y + 2.0f),
        ImVec2(origin.x, origin.y + line_h - 2.0f),
        color,
        thickness);

    ImGui::Dummy(ImVec2(thickness, line_h));
    ImGui::SameLine(0.0f, _padding);

    return;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_CHROME_
