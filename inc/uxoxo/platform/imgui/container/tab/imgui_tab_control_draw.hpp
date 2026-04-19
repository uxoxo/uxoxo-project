/*******************************************************************************
* uxoxo [imgui]                                      imgui_tab_control_draw.hpp
*
*   Dear ImGui draw handler for the tab_control component.  Renders a
* tab bar with full support for the tab_control feature set:
*
*   - Placement: top, bottom, left, right edges
*   - Per-tab close buttons (always, selected-only, hover-only, never)
*   - Per-tab icons (text prefix), tooltips, badge counters
*   - Pinned tabs (rendered first, unclosable)
*   - Scrollable bar with prev/next arrows on overflow
*   - "+" add button
*   - Overflow menu dropdown for hidden tabs
*   - Multi-row wrapping
*   - Drag-to-reorder via ImGui drag/drop payloads
*   - Inline rename via popup text input
*   - Per-tab color overrides
*
*   Vertical placements (left/right) render tabs stacked vertically
* with rotated labels.  Horizontal placements (top/bottom) render
* the standard ImGui-style tab strip.
*
*   Structure:
*     1.  style constants
*     2.  internal helpers (tab measurement, label composition)
*     3.  single tab rendering
*     4.  horizontal tab bar rendering
*     5.  vertical tab bar rendering
*     6.  overflow menu
*     7.  rename popup
*     8.  imgui_draw_tab_control (main entry point)
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included
* before this header.
*
*
* path:      /inc/uxoxo/platform/imgui/container/tab/imgui_tab_control_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.15
*******************************************************************************/

#ifndef UXOXO_COMPONENT_IMGUI_TAB_CONTROL_DRAW_
#define UXOXO_COMPONENT_IMGUI_TAB_CONTROL_DRAW_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../../uxoxo.hpp"
#include "../../../../templates/component/container/tab/tab_control.hpp"
#include "../../../../templates/render_context.hpp"


NS_UXOXO
NS_COMPONENT
NS_IMGUI


// =============================================================================
//  1.  STYLE CONSTANTS
// =============================================================================

namespace imgui_tab_style
{
    // tab bar background
    D_INLINE const ImVec4 bar_bg           = ImVec4(0.13f, 0.14f, 0.17f, 1.0f);
    D_INLINE const ImVec4 bar_border       = ImVec4(0.22f, 0.23f, 0.26f, 1.0f);

    // tab defaults
    D_INLINE const ImVec4 tab_bg           = ImVec4(0.18f, 0.19f, 0.22f, 1.0f);
    D_INLINE const ImVec4 tab_hover        = ImVec4(0.28f, 0.30f, 0.36f, 1.0f);
    D_INLINE const ImVec4 tab_selected     = ImVec4(0.22f, 0.45f, 0.68f, 1.0f);
    D_INLINE const ImVec4 tab_disabled     = ImVec4(0.14f, 0.15f, 0.17f, 0.60f);

    // text
    D_INLINE const ImVec4 tab_text         = ImVec4(0.75f, 0.76f, 0.79f, 1.0f);
    D_INLINE const ImVec4 tab_text_selected= ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    D_INLINE const ImVec4 tab_text_disabled= ImVec4(0.45f, 0.45f, 0.48f, 0.70f);

    // close button
    D_INLINE const ImVec4 close_bg         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    D_INLINE const ImVec4 close_bg_hover   = ImVec4(0.72f, 0.26f, 0.26f, 0.85f);
    D_INLINE const ImVec4 close_text       = ImVec4(0.80f, 0.80f, 0.82f, 0.80f);

    // badge
    D_INLINE const ImVec4 badge_bg         = ImVec4(0.72f, 0.25f, 0.25f, 1.0f);
    D_INLINE const ImVec4 badge_text       = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);

    // pinned marker
    D_INLINE const ImVec4 pinned_marker    = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);

    // drag indicator
    D_INLINE const ImVec4 drag_line        = ImVec4(0.40f, 0.70f, 1.00f, 0.90f);

    // add button
    D_INLINE const ImVec4 add_btn_text     = ImVec4(0.60f, 0.65f, 0.72f, 1.0f);
    D_INLINE const ImVec4 add_btn_hover    = ImVec4(0.28f, 0.30f, 0.36f, 1.0f);

    // sizing
    D_INLINE constexpr float tab_height    = 26.0f;
    D_INLINE constexpr float tab_v_width   = 28.0f;    // vertical-placement width
    D_INLINE constexpr float tab_padding_x = 10.0f;
    D_INLINE constexpr float tab_padding_y = 4.0f;
    D_INLINE constexpr float close_btn_sz  = 14.0f;
    D_INLINE constexpr float scroll_arrow_w = 20.0f;
    D_INLINE constexpr float add_btn_w      = 24.0f;
    D_INLINE constexpr float overflow_btn_w = 24.0f;
    D_INLINE constexpr float badge_radius   = 8.0f;
    D_INLINE constexpr float drag_line_thk  = 2.0f;

}   // namespace imgui_tab_style


// =============================================================================
//  2.  INTERNAL HELPERS
// =============================================================================

NS_INTERNAL

    // imgui_tab_is_horizontal
    //   helper: returns true if the placement draws tabs in a
    // horizontal row.
    D_INLINE bool
    imgui_tab_is_horizontal(
        tab_placement _placement
    ) noexcept
    {
        return ( (_placement == tab_placement::top) ||
                 (_placement == tab_placement::bottom) );
    }

    // imgui_tab_compose_label
    //   helper: composes the display label for a tab, optionally
    // including an icon prefix.
    template<typename _Tab>
    std::string
    imgui_tab_compose_label(
        const _Tab& _tab
    )
    {
        std::string result;

        if constexpr (_Tab::has_icon)
        {
            if constexpr (std::is_convertible_v<
                              typename _Tab::icon_type,
                              std::string>)
            {
                std::string icon_str = _tab.icon;

                if (!icon_str.empty())
                {
                    result += icon_str;
                    result += " ";
                }
            }
        }

        result += _tab.label;

        return result;
    }

    // imgui_tab_measure_width
    //   helper: returns the pixel width a tab would occupy when
    // rendered horizontally.
    template<typename _TabControl>
    float
    imgui_tab_measure_width(
        const typename _TabControl::entry_type& _tab,
        const _TabControl&                       _tc
    )
    {
        std::string label = imgui_tab_compose_label(_tab);

        ImVec2 text_size = ImGui::CalcTextSize(label.c_str());

        float width = text_size.x +
                      (2.0f * imgui_tab_style::tab_padding_x);

        // close button contributes width when visible
        if constexpr (_TabControl::tabs_closable)
        {
            if constexpr (_TabControl::tabs_pinnable)
            {
                if (!_tab.pinned)
                {
                    width += imgui_tab_style::close_btn_sz +
                             imgui_tab_style::tab_padding_x;
                }
            }
            else
            {
                width += imgui_tab_style::close_btn_sz +
                         imgui_tab_style::tab_padding_x;
            }
        }

        // badge contributes width
        if constexpr (_TabControl::tabs_have_badges)
        {
            if (_tab.badge_visible)
            {
                width += (2.0f * imgui_tab_style::badge_radius) +
                         imgui_tab_style::tab_padding_x;
            }
        }

        // clamp to min/max
        width = std::max(width, _tc.min_tab_width);
        width = std::min(width, _tc.max_tab_width);

        return width;
    }

    // imgui_tab_should_show_close
    //   helper: returns true if the close button for a tab should
    // render given the current policy and state.
    template<typename _Tab>
    bool
    imgui_tab_should_show_close(
        const _Tab& _tab,
        bool        _is_selected,
        bool        _is_hovered
    )
    {
        if constexpr (!_Tab::is_closable)
        {
            (void)_tab; (void)_is_selected; (void)_is_hovered;

            return false;
        }
        else
        {
            // pinned tabs never show close
            if constexpr (_Tab::is_pinnable)
            {
                if (_tab.pinned)
                {
                    return false;
                }
            }

            switch (_tab.close_policy)
            {
                case tab_close_policy::always:
                    return true;
                case tab_close_policy::selected_only:
                    return _is_selected;
                case tab_close_policy::hover_only:
                    return _is_hovered;
                case tab_close_policy::never:
                    return false;
            }

            return true;
        }
    }

    // imgui_tab_draw_badge
    //   helper: overlays a badge counter on a tab.
    template<typename _Tab>
    void
    imgui_tab_draw_badge(
        const _Tab& _tab,
        ImVec2      _tab_min,
        ImVec2      _tab_max
    )
    {
        if constexpr (!_Tab::has_badge)
        {
            (void)_tab; (void)_tab_min; (void)_tab_max;

            return;
        }
        else
        {
            if (!_tab.badge_visible)
            {
                return;
            }

            const float r = imgui_tab_style::badge_radius;

            ImVec2 center(_tab_max.x - r - 4.0f,
                          _tab_min.y + r + 2.0f);

            ImDrawList* dl = ImGui::GetWindowDrawList();

            dl->AddCircleFilled(
                center,
                r,
                ImGui::ColorConvertFloat4ToU32(
                    imgui_tab_style::badge_bg));

            char buf[16];
            if (_tab.badge_count > 99)
            {
                std::snprintf(buf, sizeof(buf), "99+");
            }
            else
            {
                std::snprintf(buf, sizeof(buf), "%d",
                              _tab.badge_count);
            }

            ImVec2 text_sz = ImGui::CalcTextSize(buf);
            ImVec2 text_pos(center.x - (text_sz.x * 0.5f),
                            center.y - (text_sz.y * 0.5f));

            dl->AddText(
                text_pos,
                ImGui::ColorConvertFloat4ToU32(
                    imgui_tab_style::badge_text),
                buf);

            return;
        }
    }

    // imgui_tab_draw_pinned_marker
    //   helper: draws a small indicator that a tab is pinned.
    template<typename _Tab>
    void
    imgui_tab_draw_pinned_marker(
        const _Tab& _tab,
        ImVec2      _tab_min
    )
    {
        if constexpr (!_Tab::is_pinnable)
        {
            (void)_tab; (void)_tab_min;

            return;
        }
        else
        {
            if (!_tab.pinned)
            {
                return;
            }

            ImDrawList* dl = ImGui::GetWindowDrawList();

            dl->AddCircleFilled(
                ImVec2(_tab_min.x + 4.0f,
                       _tab_min.y + 4.0f),
                2.5f,
                ImGui::ColorConvertFloat4ToU32(
                    imgui_tab_style::pinned_marker));

            return;
        }
    }




// =============================================================================
//  3.  SINGLE TAB RENDERING
// =============================================================================

    // imgui_draw_tab_button
    //   helper: renders one tab as a selectable button.  Handles
    // selection, close button, badge, pinned marker, context
    // menu, and drag-drop source/target.  Returns true if any
    // interaction occurred.
    template<typename _TabControl>
    bool
    imgui_draw_tab_button(
        _TabControl&  _tc,
        std::size_t   _index,
        float         _width
    )
    {
        using entry_type = typename _TabControl::entry_type;

        entry_type& tab = _tc.tabs[_index];

        if (!tab.visible)
        {
            return false;
        }

        bool interacted = false;
        bool is_selected = (_index == _tc.selected);

        // -- unique id for this tab ---------------------------------
        ImGui::PushID(static_cast<int>(_index));

        // -- style colors based on state ----------------------------
        int color_pushes = 0;

        if (!tab.enabled)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  imgui_tab_style::tab_disabled);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  imgui_tab_style::tab_disabled);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  imgui_tab_style::tab_disabled);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_tab_style::tab_text_disabled);
            color_pushes = 4;
        }
        else if (is_selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  imgui_tab_style::tab_selected);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  imgui_tab_style::tab_selected);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  imgui_tab_style::tab_selected);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_tab_style::tab_text_selected);
            color_pushes = 4;
        }
        else
        {
            // per-tab color override
            bool has_custom_bg = false;

            if constexpr (entry_type::has_color)
            {
                has_custom_bg = (tab.tab_a > 0.0f);
            }

            if (has_custom_bg)
            {
                if constexpr (entry_type::has_color)
                {
                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImVec4(tab.tab_r, tab.tab_g,
                               tab.tab_b, tab.tab_a));
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      imgui_tab_style::tab_bg);
            }

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  imgui_tab_style::tab_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  imgui_tab_style::tab_bg);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_tab_style::tab_text);
            color_pushes = 4;
        }

        // -- compose label ------------------------------------------
        std::string display = imgui_tab_compose_label(tab);

        // -- capture position for overlays --------------------------
        ImVec2 tab_min = ImGui::GetCursorScreenPos();

        // -- the tab itself -----------------------------------------
        ImVec2 button_size(_width,
                           imgui_tab_style::tab_height);

        bool clicked = ImGui::Button(display.c_str(),
                                     button_size);

        bool is_hovered = ImGui::IsItemHovered();

        ImVec2 tab_max(tab_min.x + _width,
                       tab_min.y + imgui_tab_style::tab_height);

        // update tab's interaction state (read by application)
        tab.hovered = is_hovered;
        tab.pressed = clicked;

        // -- handle selection ---------------------------------------
        if (clicked && tab.enabled)
        {
            tc_select(_tc, _index);
            interacted = true;
        }

        // -- tooltip ------------------------------------------------
        if constexpr (entry_type::has_tooltip)
        {
            if (is_hovered && !tab.tooltip.empty())
            {
                ImGui::SetTooltip("%s", tab.tooltip.c_str());
            }
        }

        // -- overlays -----------------------------------------------
        imgui_tab_draw_pinned_marker(tab, tab_min);
        imgui_tab_draw_badge(tab, tab_min, tab_max);

        // -- close button overlay -----------------------------------
        if (imgui_tab_should_show_close(tab, is_selected, is_hovered))
        {
            ImGui::SameLine(0.0f, 0.0f);

            float close_x = tab_max.x -
                            imgui_tab_style::close_btn_sz - 4.0f;

            ImGui::SetCursorScreenPos(
                ImVec2(close_x,
                       tab_min.y +
                       (imgui_tab_style::tab_height -
                        imgui_tab_style::close_btn_sz) * 0.5f));

            ImGui::PushStyleColor(ImGuiCol_Button,
                                  imgui_tab_style::close_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  imgui_tab_style::close_bg_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  imgui_tab_style::close_bg_hover);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  imgui_tab_style::close_text);

            if (ImGui::Button("x",
                              ImVec2(imgui_tab_style::close_btn_sz,
                                     imgui_tab_style::close_btn_sz)))
            {
                tc_close_tab(_tc, _index);
                interacted = true;

                ImGui::PopStyleColor(4);
                ImGui::PopStyleColor(color_pushes);
                ImGui::PopID();

                return interacted;
            }

            ImGui::PopStyleColor(4);
        }

        // -- drag source (reorderable) ------------------------------
        if constexpr (_TabControl::is_reorderable)
        {
            // pinned tabs cannot be dragged
            bool can_drag = true;

            if constexpr (entry_type::is_pinnable)
            {
                can_drag = !tab.pinned;
            }

            if ( (can_drag) &&
                 (ImGui::BeginDragDropSource(
                      ImGuiDragDropFlags_SourceNoDisableHover)) )
            {
                std::size_t payload_index = _index;

                ImGui::SetDragDropPayload(
                    "UXOXO_TAB",
                    &payload_index,
                    sizeof(payload_index));

                ImGui::TextUnformatted(tab.label.c_str());

                _tc.drag       = tab_drag_state::dragging;
                _tc.drag_index = _index;

                ImGui::EndDragDropSource();
            }
        }

        // -- drop target (reorderable) ------------------------------
        if constexpr (_TabControl::is_reorderable)
        {
            if (ImGui::BeginDragDropTarget())
            {
                const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("UXOXO_TAB");

                if (payload)
                {
                    std::size_t src_index = 0;
                    std::memcpy(&src_index,
                                payload->Data,
                                sizeof(src_index));

                    if (src_index != _index)
                    {
                        tc_move_tab(_tc, src_index, _index);
                        interacted = true;
                    }

                    _tc.drag = tab_drag_state::idle;
                }

                ImGui::EndDragDropTarget();
            }
        }

        // -- context menu -------------------------------------------
        if constexpr (entry_type::has_context)
        {
            if (ImGui::BeginPopupContextItem("##tab_ctx"))
            {
                if (ImGui::MenuItem("Close"))
                {
                    tc_close_tab(_tc, _index);
                    interacted = true;
                }

                if constexpr (entry_type::is_pinnable)
                {
                    if (tab.pinned)
                    {
                        if (ImGui::MenuItem("Unpin"))
                        {
                            tab_unpin(tab);
                            interacted = true;
                        }
                    }
                    else
                    {
                        if (ImGui::MenuItem("Pin"))
                        {
                            tab_pin(tab);
                            interacted = true;
                        }
                    }
                }

                if constexpr (entry_type::is_renamable)
                {
                    if (ImGui::MenuItem("Rename"))
                    {
                        tc_begin_rename(_tc, _index);
                        interacted = true;
                    }
                }

                if constexpr (_TabControl::is_detachable)
                {
                    if (ImGui::MenuItem("Detach"))
                    {
                        if (_tc.on_detach)
                        {
                            _tc.on_detach(_index);
                        }
                        interacted = true;
                    }
                }

                // invoke user context handler for custom actions
                if (_tc.on_context)
                {
                    _tc.on_context(_index, 0);
                }

                ImGui::EndPopup();
            }
        }

        // -- double-click to begin rename ---------------------------
        if constexpr (entry_type::is_renamable)
        {
            if ( (is_hovered) &&
                 (ImGui::IsMouseDoubleClicked(
                      ImGuiMouseButton_Left)) )
            {
                if (tab.renamable)
                {
                    tc_begin_rename(_tc, _index);
                    interacted = true;
                }
            }
        }

        ImGui::PopStyleColor(color_pushes);
        ImGui::PopID();

        return interacted;
    }




// =============================================================================
//  4.  HORIZONTAL TAB BAR
// =============================================================================

    // imgui_draw_tabs_horizontal
    //   helper: renders tabs in a single horizontal row (or wrapped
    // into multiple rows).  Returns true if any interaction
    // occurred.
    template<typename _TabControl>
    bool
    imgui_draw_tabs_horizontal(
        _TabControl& _tc
    )
    {
        bool interacted = false;

        float avail_w = ImGui::GetContentRegionAvail().x;
        float cursor_x = 0.0f;
        std::size_t current_row = 0;

        // reserve space for add button and overflow menu
        float reserved = 0.0f;

        if constexpr (_TabControl::has_add_button)
        {
            reserved += imgui_tab_style::add_btn_w +
                        imgui_tab_style::tab_padding_x;
        }

        if constexpr (_TabControl::has_overflow)
        {
            reserved += imgui_tab_style::overflow_btn_w +
                        imgui_tab_style::tab_padding_x;
        }

        float usable_w = avail_w - reserved;

        // clear overflow indices each frame
        if constexpr (_TabControl::has_overflow)
        {
            _tc.overflow_indices.clear();
        }

        // -- pinned tabs first --------------------------------------
        std::vector<std::size_t> draw_order;
        draw_order.reserve(_tc.tabs.size());

        if constexpr (_TabControl::tabs_pinnable)
        {
            // pinned
            for (std::size_t i = 0; i < _tc.tabs.size(); ++i)
            {
                if (_tc.tabs[i].pinned)
                {
                    draw_order.push_back(i);
                }
            }

            // unpinned
            for (std::size_t i = 0; i < _tc.tabs.size(); ++i)
            {
                if (!_tc.tabs[i].pinned)
                {
                    draw_order.push_back(i);
                }
            }
        }
        else
        {
            for (std::size_t i = 0; i < _tc.tabs.size(); ++i)
            {
                draw_order.push_back(i);
            }
        }

        // -- draw each tab ------------------------------------------
        bool first_in_row = true;

        for (std::size_t ord = 0; ord < draw_order.size(); ++ord)
        {
            std::size_t i = draw_order[ord];
            auto& tab = _tc.tabs[i];

            if (!tab.visible)
            {
                continue;
            }

            float tab_w = imgui_tab_measure_width(tab, _tc);

            // multi-row wrap check
            if constexpr (_TabControl::is_multirow)
            {
                if ( (!first_in_row) &&
                     (cursor_x + tab_w > usable_w) &&
                     (current_row + 1 < _tc.max_rows) )
                {
                    ImGui::NewLine();
                    cursor_x = 0.0f;
                    ++current_row;
                    first_in_row = true;
                }

                _tc.current_rows = current_row + 1;
            }

            // overflow check (when not multirow)
            if constexpr (_TabControl::has_overflow)
            {
                if constexpr (!_TabControl::is_multirow)
                {
                    if ( (!first_in_row) &&
                         (cursor_x + tab_w > usable_w) )
                    {
                        _tc.overflow_indices.push_back(i);

                        continue;
                    }
                }
            }

            if (!first_in_row)
            {
                ImGui::SameLine(0.0f,
                                imgui_tab_style::tab_padding_y);
            }

            if (imgui_draw_tab_button(_tc, i, tab_w))
            {
                interacted = true;
            }

            cursor_x += tab_w + imgui_tab_style::tab_padding_y;
            first_in_row = false;
        }

        // -- overflow menu button -----------------------------------
        if constexpr (_TabControl::has_overflow)
        {
            if (!_tc.overflow_indices.empty())
            {
                ImGui::SameLine();

                if (ImGui::Button(">>",
                                  ImVec2(imgui_tab_style::overflow_btn_w,
                                         imgui_tab_style::tab_height)))
                {
                    _tc.overflow_active = true;
                    ImGui::OpenPopup("##tab_overflow");
                }

                if (ImGui::BeginPopup("##tab_overflow"))
                {
                    for (std::size_t ovf_idx : _tc.overflow_indices)
                    {
                        if (ovf_idx >= _tc.tabs.size())
                        {
                            continue;
                        }

                        const auto& ovf_tab = _tc.tabs[ovf_idx];
                        std::string ovf_label =
                            imgui_tab_compose_label(ovf_tab);

                        if (ImGui::MenuItem(ovf_label.c_str(),
                                            nullptr,
                                            ovf_idx == _tc.selected,
                                            ovf_tab.enabled))
                        {
                            tc_select(_tc, ovf_idx);
                            interacted = true;
                        }
                    }

                    ImGui::EndPopup();
                }
                else
                {
                    _tc.overflow_active = false;
                }
            }
        }

        // -- add button ---------------------------------------------
        if constexpr (_TabControl::has_add_button)
        {
            if (_tc.add_enabled)
            {
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Text,
                                      imgui_tab_style::add_btn_text);

                if (ImGui::Button("+",
                                  ImVec2(imgui_tab_style::add_btn_w,
                                         imgui_tab_style::tab_height)))
                {
                    if (_tc.on_add)
                    {
                        _tc.on_add();
                    }
                    interacted = true;
                }

                ImGui::PopStyleColor();

                _tc.add_hovered = ImGui::IsItemHovered();

                if ( (_tc.add_hovered) &&
                     (!_tc.add_tooltip.empty()) )
                {
                    ImGui::SetTooltip("%s",
                                      _tc.add_tooltip.c_str());
                }
            }
        }

        return interacted;
    }




// =============================================================================
//  5.  VERTICAL TAB BAR
// =============================================================================

    // imgui_draw_tabs_vertical
    //   helper: renders tabs in a vertical stack (left or right
    // placement).  Uses full-width buttons with no rotation.
    // Returns true if any interaction occurred.
    template<typename _TabControl>
    bool
    imgui_draw_tabs_vertical(
        _TabControl& _tc
    )
    {
        using entry_type = typename _TabControl::entry_type;

        bool interacted = false;

        float avail_w = ImGui::GetContentRegionAvail().x;
        float tab_w   = avail_w;

        // -- pinned tabs first --------------------------------------
        std::vector<std::size_t> draw_order;
        draw_order.reserve(_tc.tabs.size());

        if constexpr (_TabControl::tabs_pinnable)
        {
            for (std::size_t i = 0; i < _tc.tabs.size(); ++i)
            {
                if (_tc.tabs[i].pinned)
                {
                    draw_order.push_back(i);
                }
            }

            for (std::size_t i = 0; i < _tc.tabs.size(); ++i)
            {
                if (!_tc.tabs[i].pinned)
                {
                    draw_order.push_back(i);
                }
            }
        }
        else
        {
            for (std::size_t i = 0; i < _tc.tabs.size(); ++i)
            {
                draw_order.push_back(i);
            }
        }

        // -- draw each tab ------------------------------------------
        for (std::size_t i : draw_order)
        {
            entry_type& tab = _tc.tabs[i];

            if (!tab.visible)
            {
                continue;
            }

            if (imgui_draw_tab_button(_tc, i, tab_w))
            {
                interacted = true;
            }
        }

        // -- add button (vertical) ----------------------------------
        if constexpr (_TabControl::has_add_button)
        {
            if (_tc.add_enabled)
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      imgui_tab_style::add_btn_text);

                if (ImGui::Button("+",
                                  ImVec2(tab_w,
                                         imgui_tab_style::tab_height)))
                {
                    if (_tc.on_add)
                    {
                        _tc.on_add();
                    }
                    interacted = true;
                }

                ImGui::PopStyleColor();

                _tc.add_hovered = ImGui::IsItemHovered();
            }
        }

        return interacted;
    }




// =============================================================================
//  6.  RENAME POPUP
// =============================================================================

    // imgui_draw_tab_rename_popup
    //   helper: when _tc.renaming is true, renders an inline text
    // input popup for the tab at _tc.rename_index.
    template<typename _TabControl>
    bool
    imgui_draw_tab_rename_popup(
        _TabControl& _tc
    )
    {
        if constexpr (!_TabControl::tabs_renamable)
        {
            (void)_tc;

            return false;
        }
        else
        {
            if (!_tc.renaming)
            {
                return false;
            }

            bool interacted = false;

            if (!ImGui::IsPopupOpen("##tab_rename"))
            {
                ImGui::OpenPopup("##tab_rename");
            }

            if (ImGui::BeginPopup("##tab_rename"))
            {
                // seed buffer
                char buf[256];
                std::strncpy(buf,
                             _tc.rename_buffer.c_str(),
                             sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';

                ImGui::SetKeyboardFocusHere();

                if (ImGui::InputText(
                        "##rename_input",
                        buf,
                        sizeof(buf),
                        ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    _tc.rename_buffer = buf;
                    tc_commit_rename(_tc);
                    interacted = true;

                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    _tc.rename_buffer = buf;
                }

                ImGui::SameLine();

                if (ImGui::Button("OK"))
                {
                    tc_commit_rename(_tc);
                    interacted = true;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    tc_cancel_rename(_tc);
                    interacted = true;

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            else
            {
                // popup closed externally (e.g. click away)
                if (_tc.renaming)
                {
                    tc_cancel_rename(_tc);
                }
            }

            return interacted;
        }
    }

NS_END  // internal




// =============================================================================
//  7.  IMGUI DRAW TAB CONTROL
// =============================================================================

// imgui_draw_tab_control
//   function: renders a tab_control using Dear ImGui.  Dispatches
// to horizontal or vertical rendering based on placement.  Returns
// true if any user interaction occurred this frame.
template<typename _TabControl>
bool
imgui_draw_tab_control(
    _TabControl&    _tc,
    render_context& _ctx
)
{
    (void)_ctx;

    // respect visibility / enabled gates
    if (!_tc.visible)
    {
        return false;
    }

    if (_tc.tabs.empty())
    {
        // even an empty tab bar may show an add button
        if constexpr (_TabControl::has_add_button)
        {
            if (_tc.add_enabled)
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      imgui_tab_style::add_btn_text);

                bool clicked = ImGui::Button(
                    "+",
                    ImVec2(imgui_tab_style::add_btn_w,
                           imgui_tab_style::tab_height));

                ImGui::PopStyleColor();

                if (clicked && _tc.on_add)
                {
                    _tc.on_add();

                    return true;
                }
            }
        }

        return false;
    }

    bool interacted = false;

    // disable interaction when the whole control is disabled
    if (!_tc.enabled)
    {
        ImGui::BeginDisabled();
    }

    // frame background
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          imgui_tab_style::bar_bg);

    // frame rounding and spacing
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(imgui_tab_style::tab_padding_y,
                               imgui_tab_style::tab_padding_y));

    // dispatch by placement
    if (internal::imgui_tab_is_horizontal(_tc.placement))
    {
        interacted = internal::imgui_draw_tabs_horizontal(_tc);
    }
    else
    {
        interacted = internal::imgui_draw_tabs_vertical(_tc);
    }

    // rename popup (if active)
    if constexpr (_TabControl::tabs_renamable)
    {
        if (internal::imgui_draw_tab_rename_popup(_tc))
        {
            interacted = true;
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    if (!_tc.enabled)
    {
        ImGui::EndDisabled();
    }

    return interacted;
}




// =============================================================================
//  8.  KEYBOARD INPUT
// =============================================================================

// imgui_tab_control_handle_input
//   function: handles keyboard navigation for the tab control.
// Call while the tab bar has focus.  Returns true if any input
// was consumed.
//
//   Bindings:
//     Ctrl+Tab         next tab
//     Ctrl+Shift+Tab   previous tab
//     Ctrl+W           close selected tab
//     Ctrl+T           trigger add (if enabled)
//     F2               rename selected tab
//     Esc              cancel rename (if active)
template<typename _TabControl>
bool
imgui_tab_control_handle_input(
    _TabControl& _tc
)
{
    if ( (!_tc.enabled) ||
         (_tc.tabs.empty()) )
    {
        return false;
    }

    bool consumed = false;

    ImGuiIO& io = ImGui::GetIO();
    bool ctrl  = io.KeyCtrl;
    bool shift = io.KeyShift;

    // cancel rename on Esc
    if constexpr (_TabControl::tabs_renamable)
    {
        if (_tc.renaming)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                tc_cancel_rename(_tc);

                return true;
            }

            // don't process other bindings while renaming
            return false;
        }
    }

    // Ctrl+Tab / Ctrl+Shift+Tab
    if ( (ctrl) &&
         (ImGui::IsKeyPressed(ImGuiKey_Tab)) )
    {
        if (shift)
        {
            tc_select_prev(_tc);
        }
        else
        {
            tc_select_next(_tc);
        }
        consumed = true;
    }

    // Ctrl+W - close selected
    if constexpr (_TabControl::tabs_closable)
    {
        if ( (ctrl) &&
             (!shift) &&
             (ImGui::IsKeyPressed(ImGuiKey_W)) )
        {
            if (tc_close_tab(_tc, _tc.selected))
            {
                consumed = true;
            }
        }
    }

    // Ctrl+T - add
    if constexpr (_TabControl::has_add_button)
    {
        if ( (ctrl) &&
             (!shift) &&
             (ImGui::IsKeyPressed(ImGuiKey_T)) )
        {
            if ( (_tc.add_enabled) &&
                 (_tc.on_add) )
            {
                _tc.on_add();
                consumed = true;
            }
        }
    }

    // F2 - rename selected
    if constexpr (_TabControl::tabs_renamable)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_F2))
        {
            tc_begin_rename(_tc, _tc.selected);
            consumed = true;
        }
    }

    return consumed;
}


NS_END
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_TAB_CONTROL_DRAW_