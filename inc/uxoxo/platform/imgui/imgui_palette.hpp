/*******************************************************************************
* uxoxo [imgui]                                               imgui_palette.hpp
*
* Tag-keyed central palette for the ImGui platform layer:
*   Replaces the eight per-component style namespaces (imgui_table_style,
* imgui_toolbar_style, imgui_db_toolbar_style, imgui_mariadb_toolbar_style,
* imgui_mysql_toolbar_style, imgui_tab_style, imgui_menu_style,
* imgui_console_style) with a single tag-indexed table.  Renderers query
* values via `palette::get<Tag>()`; applications swap themes by setting
* the inline-static fields via `palette::set<Tag>(value)`.
*
*   The file defines the SHARED visual vocabulary - colors and dimensions
* used across two or more components.  Vendor-specific accents (the
* MySQL ANALYZE / OPTIMIZE / CHECK button colors, MariaDB Galera tints,
* etc.) remain in their vendor headers as additional tags layered on
* top of this palette; they are out of scope here because they have a
* single consumer.
*
*   Each `entry<_Tag>` specialization holds a `static inline` value so
* the header is ODR-safe and the palette can be mutated at run-time
* without recompilation.  Tags are empty structs (zero overhead) and
* live in the `palette` namespace; mis-typing a tag yields a clean
* compile error against the primary template.
*
* Contents:
*   1.  surface tags          (toolbar / status / window backgrounds)
*   2.  button tags           (bg / hover / active / disabled / text)
*   3.  text tags             (header, body, muted, disabled)
*   4.  indicator tags        (ok / warn / error / disconn / dirty / stale)
*   5.  selection tags        (cursor border, selection bg)
*   6.  badge / separator tags
*   7.  table region tags     (header / footer / total / row striping)
*   8.  console color tags    (output_color_tag mapping)
*   9.  dimension tags        (heights, paddings, rounding)
*  10.  entry<_Tag>           (primary template + specializations)
*  11.  get<_Tag> / set<_Tag> (public accessors)
*
*
* path:      /inc/uxoxo/platform/imgui/core/imgui_palette.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.08
*******************************************************************************/

#ifndef  UXOXO_IMGUI_PALETTE_
#define  UXOXO_IMGUI_PALETTE_ 1

// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"


NS_UXOXO
NS_IMGUI


namespace palette {

// ===========================================================================
//  1.  SURFACE TAGS
// ===========================================================================
//   Background and border colors for the major chrome surfaces:
// toolbars, status bars, popup / floating windows.

// toolbar_bg_tag
//   tag: background fill for any toolbar surface.
struct toolbar_bg_tag
{};

// toolbar_border_tag
//   tag: border / separator stroke between a toolbar and its content area.
struct toolbar_border_tag
{};

// status_bar_bg_tag
//   tag: background fill for status / footer bars.
struct status_bar_bg_tag
{};

// window_bg_tag
//   tag: background fill for floating / popup windows (console, popovers).
struct window_bg_tag
{};

// window_border_tag
//   tag: border stroke for floating / popup windows.
struct window_border_tag
{};


// ===========================================================================
//  2.  BUTTON TAGS
// ===========================================================================
//   Default button surface colors.  Vendor-specific accent buttons
// (e.g. MySQL OPTIMIZE) layer their own tags on top of this palette
// in their vendor headers.

// btn_bg_tag
//   tag: default button background fill.
struct btn_bg_tag
{};

// btn_hover_tag
//   tag: button background fill while the cursor is over the button.
struct btn_hover_tag
{};

// btn_active_tag
//   tag: button background fill while the button is pressed.
struct btn_active_tag
{};

// btn_disabled_tag
//   tag: button background fill in the disabled state.  Drives
// ImGuiCol_Button, ImGuiCol_ButtonHovered, and ImGuiCol_ButtonActive
// uniformly so disabled buttons stay inert under hover / press.
struct btn_disabled_tag
{};

// btn_text_tag
//   tag: button text color in the enabled state.
struct btn_text_tag
{};

// btn_text_disabled_tag
//   tag: button text color in the disabled state.
struct btn_text_disabled_tag
{};

// btn_toggle_on_bg_tag
//   tag: button background fill when the button is in the toggled-on
// state (toolbar buttons with bf_toggle).
struct btn_toggle_on_bg_tag
{};

// btn_toggle_on_border_tag
//   tag: border stroke around toggled-on buttons.
struct btn_toggle_on_border_tag
{};


// ===========================================================================
//  3.  TEXT TAGS
// ===========================================================================
//   Text colors used outside button labels.  fg_tag (in imgui_style.hpp)
// remains the canonical foreground tag for style-readable components;
// these are bulk-text categories the chrome layer needs.

// text_header_tag
//   tag: section header text (toolbar headers, table column headers).
struct text_header_tag
{};

// text_body_tag
//   tag: default body / data text.
struct text_body_tag
{};

// text_muted_tag
//   tag: secondary / muted text (status messages, footers).
struct text_muted_tag
{};

// text_disabled_tag
//   tag: disabled or grayed-out text.
struct text_disabled_tag
{};

// text_shortcut_tag
//   tag: keyboard shortcut hint text in menus and tooltips.
struct text_shortcut_tag
{};


// ===========================================================================
//  4.  INDICATOR TAGS
// ===========================================================================
//   Status dot / pill colors used by status bars across the database
// table family, the console, and the connection chrome.

// indicator_ok_tag
//   tag: positive / connected / committed indicator color.
struct indicator_ok_tag
{};

// indicator_warn_tag
//   tag: caution indicator (dirty, unsaved, partial).
struct indicator_warn_tag
{};

// indicator_error_tag
//   tag: error indicator (failed, disconnected, fatal).
struct indicator_error_tag
{};

// indicator_disconn_tag
//   tag: disconnected indicator (database down, no session).
struct indicator_disconn_tag
{};

// indicator_stale_tag
//   tag: stale-data indicator (refresh needed, server-side change).
struct indicator_stale_tag
{};


// ===========================================================================
//  5.  SELECTION TAGS
// ===========================================================================

// cursor_border_tag
//   tag: border stroke around the active cursor cell / row.
struct cursor_border_tag
{};

// selection_bg_tag
//   tag: background fill for selected items.
struct selection_bg_tag
{};


// ===========================================================================
//  6.  BADGE / SEPARATOR TAGS
// ===========================================================================

// badge_bg_tag
//   tag: badge circle / pill background.
struct badge_bg_tag
{};

// badge_text_tag
//   tag: badge counter text.
struct badge_text_tag
{};

// separator_tag
//   tag: thin separator line color (toolbar separators, list dividers).
struct separator_tag
{};


// ===========================================================================
//  7.  TABLE REGION TAGS
// ===========================================================================
//   Table-specific region backgrounds.  These were previously the
// imgui_table_style namespace; promoted to the palette so that any
// renderer that wants table-styled rows (data grids, listboxes,
// tree-table hybrids) reuses the same vocabulary.

// table_header_bg_tag
//   tag: header-row background.
struct table_header_bg_tag
{};

// table_footer_bg_tag
//   tag: footer-row background.
struct table_footer_bg_tag
{};

// table_total_bg_tag
//   tag: total-row background.
struct table_total_bg_tag
{};

// table_data_bg_even_tag
//   tag: data-row background, even index (zebra striping).
struct table_data_bg_even_tag
{};

// table_data_bg_odd_tag
//   tag: data-row background, odd index (zebra striping).
struct table_data_bg_odd_tag
{};

// table_edit_bg_tag
//   tag: background fill for an in-progress cell edit.
struct table_edit_bg_tag
{};

// table_edit_border_tag
//   tag: border stroke around an in-progress cell edit.
struct table_edit_border_tag
{};

// table_sort_active_tag
//   tag: sort indicator color for the actively-sorted column.
struct table_sort_active_tag
{};


// ===========================================================================
//  8.  CONSOLE COLOR TAGS
// ===========================================================================
//   One tag per output_color_tag enumerator in the dev_console.  Lets
// callers retint individual log levels without forking the entire
// console renderer.

// console_normal_tag
//   tag: default output line color.
struct console_normal_tag
{};

// console_info_tag
//   tag: info-level output color.
struct console_info_tag
{};

// console_warning_tag
//   tag: warning-level output color.
struct console_warning_tag
{};

// console_error_tag
//   tag: error-level output color.
struct console_error_tag
{};

// console_debug_tag
//   tag: debug-level output color.
struct console_debug_tag
{};

// console_success_tag
//   tag: success / completed output color.
struct console_success_tag
{};

// console_highlight_tag
//   tag: highlight / matched-search output color.
struct console_highlight_tag
{};


// ===========================================================================
//  9.  DIMENSION TAGS
// ===========================================================================
//   Pixel dimensions and unitless scalars used to lay out chrome.
// Stored as `float` rather than ImVec4 so the same `entry<_Tag>` shape
// works for both - the value type is whatever the specialization
// declares.

// toolbar_height_tag
//   tag: pixel height of a toolbar surface.
struct toolbar_height_tag
{};

// status_bar_height_tag
//   tag: pixel height of a status bar surface.
struct status_bar_height_tag
{};

// badge_radius_tag
//   tag: pixel radius of badge circles drawn over toolbar buttons.
struct badge_radius_tag
{};

// default_rounding_tag
//   tag: default frame-rounding radius for buttons and panels.
struct default_rounding_tag
{};

// pill_rounding_tag
//   tag: rounding radius for pill-shaped buttons.
struct pill_rounding_tag
{};

// separator_thickness_tag
//   tag: pixel thickness of separator lines.
struct separator_thickness_tag
{};

// grid_alpha_tag
//   tag: alpha multiplier for table grid lines (0..1).
struct grid_alpha_tag
{};


// ===========================================================================
//  10. ENTRY
// ===========================================================================
//   Primary template is intentionally undefined so unknown tags
// produce a clean "incomplete type" diagnostic at the call site.  Each
// specialization holds a `static inline` value (C++17) initialized
// from the default theme; applications mutate it directly via
// palette::set<Tag>(...) for runtime theming.

// entry
//   trait: tag-to-value table.  Specialized for every tag declared
// above; missing specializations are a compile error.
template<typename _Tag>
struct entry;


// ---- 10a. surface entries ------------------------------------------------

template<>
struct entry<toolbar_bg_tag>
{
    static inline ImVec4 value { 0.13f, 0.14f, 0.17f, 1.0f };
};

template<>
struct entry<toolbar_border_tag>
{
    static inline ImVec4 value { 0.22f, 0.23f, 0.26f, 1.0f };
};

template<>
struct entry<status_bar_bg_tag>
{
    static inline ImVec4 value { 0.11f, 0.12f, 0.14f, 1.0f };
};

template<>
struct entry<window_bg_tag>
{
    static inline ImVec4 value { 0.10f, 0.10f, 0.12f, 0.94f };
};

template<>
struct entry<window_border_tag>
{
    static inline ImVec4 value { 0.22f, 0.22f, 0.26f, 0.80f };
};


// ---- 10b. button entries -------------------------------------------------

template<>
struct entry<btn_bg_tag>
{
    static inline ImVec4 value { 0.20f, 0.21f, 0.24f, 1.0f };
};

template<>
struct entry<btn_hover_tag>
{
    static inline ImVec4 value { 0.28f, 0.30f, 0.36f, 1.0f };
};

template<>
struct entry<btn_active_tag>
{
    static inline ImVec4 value { 0.18f, 0.19f, 0.22f, 1.0f };
};

template<>
struct entry<btn_disabled_tag>
{
    static inline ImVec4 value { 0.30f, 0.30f, 0.32f, 0.50f };
};

template<>
struct entry<btn_text_tag>
{
    static inline ImVec4 value { 0.82f, 0.82f, 0.85f, 1.0f };
};

template<>
struct entry<btn_text_disabled_tag>
{
    static inline ImVec4 value { 0.45f, 0.45f, 0.48f, 0.70f };
};

template<>
struct entry<btn_toggle_on_bg_tag>
{
    static inline ImVec4 value { 0.22f, 0.45f, 0.68f, 0.80f };
};

template<>
struct entry<btn_toggle_on_border_tag>
{
    static inline ImVec4 value { 0.35f, 0.60f, 0.85f, 0.60f };
};


// ---- 10c. text entries ---------------------------------------------------

template<>
struct entry<text_header_tag>
{
    static inline ImVec4 value { 0.85f, 0.87f, 0.90f, 1.0f };
};

template<>
struct entry<text_body_tag>
{
    static inline ImVec4 value { 0.78f, 0.78f, 0.80f, 1.0f };
};

template<>
struct entry<text_muted_tag>
{
    static inline ImVec4 value { 0.55f, 0.55f, 0.58f, 1.0f };
};

template<>
struct entry<text_disabled_tag>
{
    static inline ImVec4 value { 0.40f, 0.40f, 0.43f, 0.70f };
};

template<>
struct entry<text_shortcut_tag>
{
    static inline ImVec4 value { 0.50f, 0.50f, 0.55f, 1.0f };
};


// ---- 10d. indicator entries ----------------------------------------------

template<>
struct entry<indicator_ok_tag>
{
    static inline ImVec4 value { 0.35f, 0.70f, 0.40f, 1.0f };
};

template<>
struct entry<indicator_warn_tag>
{
    static inline ImVec4 value { 0.95f, 0.75f, 0.20f, 1.0f };
};

template<>
struct entry<indicator_error_tag>
{
    static inline ImVec4 value { 0.95f, 0.35f, 0.30f, 1.0f };
};

template<>
struct entry<indicator_disconn_tag>
{
    static inline ImVec4 value { 0.75f, 0.25f, 0.25f, 1.0f };
};

template<>
struct entry<indicator_stale_tag>
{
    static inline ImVec4 value { 0.70f, 0.45f, 0.20f, 1.0f };
};


// ---- 10e. selection entries ----------------------------------------------

template<>
struct entry<cursor_border_tag>
{
    static inline ImVec4 value { 0.40f, 0.65f, 1.00f, 0.90f };
};

template<>
struct entry<selection_bg_tag>
{
    static inline ImVec4 value { 0.25f, 0.42f, 0.70f, 0.35f };
};


// ---- 10f. badge / separator entries --------------------------------------

template<>
struct entry<badge_bg_tag>
{
    static inline ImVec4 value { 0.85f, 0.25f, 0.20f, 1.0f };
};

template<>
struct entry<badge_text_tag>
{
    static inline ImVec4 value { 1.00f, 1.00f, 1.00f, 1.0f };
};

template<>
struct entry<separator_tag>
{
    static inline ImVec4 value { 0.30f, 0.30f, 0.34f, 0.60f };
};


// ---- 10g. table region entries -------------------------------------------

template<>
struct entry<table_header_bg_tag>
{
    static inline ImVec4 value { 0.18f, 0.20f, 0.25f, 1.0f };
};

template<>
struct entry<table_footer_bg_tag>
{
    static inline ImVec4 value { 0.16f, 0.18f, 0.22f, 1.0f };
};

template<>
struct entry<table_total_bg_tag>
{
    static inline ImVec4 value { 0.22f, 0.24f, 0.28f, 1.0f };
};

template<>
struct entry<table_data_bg_even_tag>
{
    static inline ImVec4 value { 0.12f, 0.12f, 0.14f, 1.0f };
};

template<>
struct entry<table_data_bg_odd_tag>
{
    static inline ImVec4 value { 0.14f, 0.14f, 0.16f, 1.0f };
};

template<>
struct entry<table_edit_bg_tag>
{
    static inline ImVec4 value { 0.10f, 0.10f, 0.12f, 1.0f };
};

template<>
struct entry<table_edit_border_tag>
{
    static inline ImVec4 value { 0.50f, 0.75f, 1.00f, 0.80f };
};

template<>
struct entry<table_sort_active_tag>
{
    static inline ImVec4 value { 0.60f, 0.80f, 1.00f, 1.0f };
};


// ---- 10h. console color entries ------------------------------------------

template<>
struct entry<console_normal_tag>
{
    static inline ImVec4 value { 0.80f, 0.80f, 0.82f, 1.0f };
};

template<>
struct entry<console_info_tag>
{
    static inline ImVec4 value { 0.55f, 0.75f, 0.95f, 1.0f };
};

template<>
struct entry<console_warning_tag>
{
    static inline ImVec4 value { 0.95f, 0.80f, 0.30f, 1.0f };
};

template<>
struct entry<console_error_tag>
{
    static inline ImVec4 value { 0.95f, 0.35f, 0.30f, 1.0f };
};

template<>
struct entry<console_debug_tag>
{
    static inline ImVec4 value { 0.55f, 0.55f, 0.58f, 1.0f };
};

template<>
struct entry<console_success_tag>
{
    static inline ImVec4 value { 0.35f, 0.80f, 0.45f, 1.0f };
};

template<>
struct entry<console_highlight_tag>
{
    static inline ImVec4 value { 1.00f, 1.00f, 0.60f, 1.0f };
};


// ---- 10i. dimension entries ----------------------------------------------

template<>
struct entry<toolbar_height_tag>
{
    static inline float value = 32.0f;
};

template<>
struct entry<status_bar_height_tag>
{
    static inline float value = 22.0f;
};

template<>
struct entry<badge_radius_tag>
{
    static inline float value = 8.0f;
};

template<>
struct entry<default_rounding_tag>
{
    static inline float value = 4.0f;
};

template<>
struct entry<pill_rounding_tag>
{
    static inline float value = 14.0f;
};

template<>
struct entry<separator_thickness_tag>
{
    static inline float value = 1.0f;
};

template<>
struct entry<grid_alpha_tag>
{
    static inline float value = 0.12f;
};


// ===========================================================================
//  11. PUBLIC ACCESSORS
// ===========================================================================

/*
get
  Returns the palette value associated with `_Tag` by reference.
The reference is to the inline-static `entry<_Tag>::value`, so callers
that hold the reference across a `set` will observe the new value.

Parameter(s):
  none.
Return:
  A const reference to the palette entry's value.  The value type is
  whatever the entry specialization declares - typically ImVec4 for
  colors and float for dimensions.
*/
template<typename _Tag>
[[nodiscard]] inline const auto&
get() noexcept
{
    return entry<_Tag>::value;
}

/*
set
  Mutates the palette value associated with `_Tag`.  Intended for
runtime theme switching: an application can call `palette::set<...>`
once at startup (or in response to a settings change) and every
subsequent renderer call resolves the new value through `get<...>`.

Parameter(s):
  _value: the new palette value.  The type is deduced from the entry
          specialization - implicit conversions apply.
Return:
  none.
*/
template<typename _Tag,
          typename _Value>
inline void
set(
    const _Value& _value
) noexcept
{
    entry<_Tag>::value = _value;

    return;
}


}   // namespace palette


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_PALETTE_
