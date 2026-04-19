/*******************************************************************************
* uxoxo [imgui]                                         imgui_menu_bar_draw.hpp
*
*   Dear ImGui draw handler for the menu_bar component.  Renders a
* horizontal menu bar using ImGui::BeginMainMenuBar / BeginMenuBar,
* with full support for:
*
*   - top-level entries with click-to-open dropdowns
*   - nested submenus (mf_submenu) via recursive ImGui::BeginMenu
*   - per-item keyboard shortcuts display (mf_shortcuts)
*   - per-item icons (mf_icons) rendered as text prefix
*   - checkable items (mf_checkable) with ImGui::MenuItem selected param
*   - separators and non-selectable group headers
*   - disabled / greyed-out items
*   - synchronisation between ImGui's active state and the menu_bar's
*     focus / active / dd_cursor model
*
*   Two entry points are provided:
*     imgui_draw_main_menu_bar   uses ImGui::BeginMainMenuBar (viewport top)
*     imgui_draw_menu_bar        uses ImGui::BeginMenuBar (within a window)
*
*   Both delegate to a shared implementation that recursively renders
* menus and menu items.
*
*   Structure:
*     1.  style constants
*     2.  internal helpers (recursive menu/item rendering)
*     3.  imgui_draw_main_menu_bar
*     4.  imgui_draw_menu_bar
*
*   REQUIRES: C++17 or later.  Dear ImGui headers must be included
* before this header.
*
*
* path:      /inc/uxoxo/platform/imgui/menu/imgui_menu_bar_draw.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_IMGUI_COMPONENT_MENU_BAR_DRAW_
#define UXOXO_IMGUI_COMPONENT_MENU_BAR_DRAW_ 1

// std
#include <cstddef>
#include <string>
#include <type_traits>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/menu/menu.hpp"
#include "../../../templates/component/menu/menu_bar.hpp"
#include "../../../templates/render_context.hpp"


NS_UXOXO
NS_COMPONENT
NS_IMGUI

// =============================================================================
//  1.  STYLE CONSTANTS
// =============================================================================

namespace imgui_menu_style
{
    D_INLINE const ImVec4 header_text     = ImVec4(0.55f, 0.58f, 0.65f, 1.0f);
    D_INLINE const ImVec4 disabled_text   = ImVec4(0.40f, 0.40f, 0.43f, 0.70f);
    D_INLINE const ImVec4 shortcut_text   = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
    D_INLINE const ImVec4 check_mark      = ImVec4(0.40f, 0.72f, 0.45f, 1.0f);

}   // namespace imgui_menu_style


// =============================================================================
//  2.  INTERNAL HELPERS
// =============================================================================

NS_INTERNAL

    // imgui_menu_item_label
    //   function: composes the display label for a menu item,
    // optionally including an icon prefix.
    template<typename _Item>
    std::string
    imgui_menu_item_label(
        const _Item& _item
    )
    {
        std::string result;

        if constexpr (_Item::has_icons)
        {
            if constexpr (std::is_convertible_v<
                              typename _Item::icon_type,
                              std::string>)
            {
                std::string icon_str = _item.icon;

                if (!icon_str.empty())
                {
                    result += icon_str;
                    result += " ";
                }
            }
        }

        if constexpr (std::is_convertible_v<
                          typename _Item::data_type,
                          std::string>)
        {
            result += _item.label;
        }
        else
        {
            result += "?";
        }

        return result;
    }

    // imgui_menu_item_shortcut
    //   function: returns the shortcut string for an item, or
    // nullptr if none.
    template<typename _Item>
    const char*
    imgui_menu_item_shortcut(
        const _Item& _item
    )
    {
        if constexpr (_Item::has_shortcuts)
        {
            if (!_item.shortcut.empty())
            {
                return _item.shortcut.c_str();
            }
        }

        (void)_item;

        return nullptr;
    }

    // imgui_draw_menu_items
    //   function: recursively renders all items in a menu.
    // Returns true if any item was activated.
    template<typename _Menu>
    bool
    imgui_draw_menu_items(
        _Menu&      _menu,
        bool&       _interacted
    )
    {
        using item_type = typename _Menu::value_type;

        for (auto& item : _menu.items)
        {
            // separator
            if (item.is_separator())
            {
                ImGui::Separator();

                continue;
            }

            // group header
            if (item.is_header())
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      imgui_menu_style::header_text);

                std::string label = imgui_menu_item_label(item);
                ImGui::TextUnformatted(label.c_str());

                ImGui::PopStyleColor();

                continue;
            }

            // disabled text color
            if (!item.enabled)
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      imgui_menu_style::disabled_text);
            }

            // submenu
            if constexpr (item_type::has_submenus)
            {
                if (item.submenu)
                {
                    std::string label = imgui_menu_item_label(item);

                    if (ImGui::BeginMenu(label.c_str(),
                                         item.enabled))
                    {
                        imgui_draw_menu_items(*item.submenu,
                                              _interacted);

                        ImGui::EndMenu();
                    }

                    if (!item.enabled)
                    {
                        ImGui::PopStyleColor();
                    }

                    continue;
                }
            }

            // normal item
            std::string label    = imgui_menu_item_label(item);
            const char* shortcut = imgui_menu_item_shortcut(item);

            // checkable items
            if constexpr (item_type::is_checkable)
            {
                bool checked = item.checked;

                if (ImGui::MenuItem(label.c_str(),
                                    shortcut,
                                    &checked,
                                    item.enabled))
                {
                    item.checked = checked;
                    _interacted  = true;
                }
            }
            else
            {
                if (ImGui::MenuItem(label.c_str(),
                                    shortcut,
                                    false,
                                    item.enabled))
                {
                    _interacted = true;
                }
            }

            if (!item.enabled)
            {
                ImGui::PopStyleColor();
            }
        }

        return _interacted;
    }

    // imgui_draw_menu_bar_entries
    //   function: renders all top-level entries of a menu bar.
    // Called between BeginMainMenuBar/EndMainMenuBar or
    // BeginMenuBar/EndMenuBar.  Returns true if any item was
    // activated.
    template<typename _MenuBar>
    bool
    imgui_draw_menu_bar_entries(
        _MenuBar& _bar
    )
    {
        bool interacted = false;

        for (std::size_t i = 0; i < _bar.entries.size(); ++i)
        {
            auto& entry = _bar.entries[i];

            // compose label
            std::string label;

            if constexpr (std::is_convertible_v<
                              typename _MenuBar::data_type,
                              std::string>)
            {
                label = entry.label;
            }
            else
            {
                label = "?";
            }

            // entries without dropdowns render as plain menu items
            if (!entry.has_dropdown())
            {
                if (ImGui::MenuItem(label.c_str(),
                                    nullptr,
                                    false,
                                    entry.enabled))
                {
                    _bar.focused = i;
                    interacted   = true;
                }

                continue;
            }

            // entries with dropdowns render as BeginMenu
            if (ImGui::BeginMenu(label.c_str(),
                                 entry.enabled))
            {
                // sync bar state
                _bar.focused = i;
                _bar.active  = true;

                imgui_draw_menu_items(*entry.dropdown,
                                      interacted);

                ImGui::EndMenu();
            }
            else
            {
                // if this was the active menu and it closed, sync
                if ( (_bar.active) &&
                     (_bar.focused == i) )
                {
                    _bar.active = false;
                }
            }
        }

        return interacted;
    }

NS_END  // internal


// =============================================================================
//  3.  IMGUI DRAW MAIN MENU BAR
// =============================================================================
//   Renders the menu bar at the top of the viewport using
// ImGui::BeginMainMenuBar.

// imgui_draw_main_menu_bar
//   function: renders a menu_bar as the ImGui main menu bar
// (viewport-level).  Returns true if any item was activated.
template<typename _MenuBar>
bool
imgui_draw_main_menu_bar(
    _MenuBar&       _bar,
    render_context& _ctx
)
{
    (void)_ctx;

    if (_bar.empty())
    {
        return false;
    }

    bool interacted = false;

    if (ImGui::BeginMainMenuBar())
    {
        interacted = internal::imgui_draw_menu_bar_entries(_bar);

        ImGui::EndMainMenuBar();
    }

    return interacted;
}


// =============================================================================
//  4.  IMGUI DRAW MENU BAR (window-level)
// =============================================================================
//   Renders the menu bar inside an ImGui window that has the
// ImGuiWindowFlags_MenuBar flag.

// imgui_draw_menu_bar
//   function: renders a menu_bar inside the current ImGui window.
// The window must have been created with ImGuiWindowFlags_MenuBar.
// Returns true if any item was activated.
template<typename _MenuBar>
bool
imgui_draw_menu_bar(
    _MenuBar&       _bar,
    render_context& _ctx
)
{
    (void)_ctx;

    if (_bar.empty())
    {
        return false;
    }

    bool interacted = false;

    if (ImGui::BeginMenuBar())
    {
        interacted = internal::imgui_draw_menu_bar_entries(_bar);

        ImGui::EndMenuBar();
    }

    return interacted;
}


// =============================================================================
//  5.  STANDALONE MENU POPUP
// =============================================================================
//   Renders a menu as a standalone popup (e.g. right-click context
// menu).  The caller is responsible for calling ImGui::OpenPopup
// with the provided id.

// imgui_draw_menu_popup
//   function: renders a menu as an ImGui popup.  Call
// ImGui::OpenPopup(_id) before this to trigger the popup.
// Returns true if any item was activated.
template<typename _Menu>
bool
imgui_draw_menu_popup(
    _Menu&      _menu,
    const char* _id
)
{
    bool interacted = false;

    if (ImGui::BeginPopup(_id))
    {
        internal::imgui_draw_menu_items(_menu,
                                        interacted);

        ImGui::EndPopup();
    }

    return interacted;
}


// =============================================================================
//  6.  CONTEXT MENU POPUP
// =============================================================================
//   Convenience wrapper that opens a popup on right-click and
// renders a menu inside it.

// imgui_draw_context_menu
//   function: opens a context menu popup on right-click of the
// last item and renders the given menu inside it.  Returns true
// if any item was activated.
template<typename _Menu>
bool
imgui_draw_context_menu(
    _Menu&      _menu,
    const char* _id = "##ctx"
)
{
    bool interacted = false;

    if (ImGui::BeginPopupContextItem(_id))
    {
        internal::imgui_draw_menu_items(_menu,
                                        interacted);

        ImGui::EndPopup();
    }

    return interacted;
}


NS_END  // imgui
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_COMPONENT_MENU_BAR_DRAW_