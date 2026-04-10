/*******************************************************************************
* uxoxo [component]                                                menu_bar.hpp
*
*   Horizontal menu bar component.  Owns a list of top-level menus and
* manages focus, selection, and drop-down state.  Pure data aggregate.
*
*   The menu_bar is the topmost row in a Midnight Commander layout:
*     ------------------------------------------------------------
*     |  Left   File   Command   Options   Right                 |
*     ------------------------------------------------------------
*
*   Each top-level entry may have an associated drop-down menu (from
* menu.hpp).  The menu_bar manages which entry is focused, whether a
* drop-down is open, and navigating within the open drop-down.
*
*   Follows the component pattern:
*     - Pure data aggregate (no base class, no vtable)
*     - Free-function operations
*     - menu_bar_traits:: namespace with SFINAE detection
*
*
* file:      /inc/uxoxo/component/menu/menu_bar.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.31
*******************************************************************************/

#ifndef UXOXO_COMPONENT_MENU_BAR_
#define UXOXO_COMPONENT_MENU_BAR_ 1

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

//#include <uxoxo>
#include <component/view_common.hpp>
#include <component/menu/menu.hpp>


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  MENU BAR ENTRY
// ===============================================================================
//   A single top-level item in the bar.  Holds a label and an optional
// drop-down menu.

// menu_bar_entry
//   struct: a single top-level item in the menu bar with an optional dropdown.
template <typename _Data = std::string,
          unsigned _Feat = mf_none,
          typename _Icon = int>
struct menu_bar_entry
{
    using menu_type = menu<_Data, _Feat, _Icon>;
    using data_type = _Data;

    _Data                       label;
    std::unique_ptr<menu_type>  dropdown;
    bool                        enabled = true;

    menu_bar_entry() = default;

    explicit menu_bar_entry(
            _Data _lbl
        )
            : label(std::move(_lbl))
        {}

    menu_bar_entry(
            _Data                       _lbl,
            std::unique_ptr<menu_type>  _dd
        )
            : label(std::move(_lbl)),
              dropdown(std::move(_dd))
        {}

    [[nodiscard]] bool has_dropdown() const noexcept
    {
        return ( (dropdown != nullptr) &&
                 (!dropdown->empty()) );
    }
};


// ===============================================================================
//  2. MENU BAR
// ===============================================================================

// menu_bar
//   struct: horizontal menu bar component managing top-level entries, focus,
// and drop-down state.
template <typename _Data = std::string,
          unsigned _Feat = mf_none,
          typename _Icon = int>
struct menu_bar
{
    using entry_type = menu_bar_entry<_Data, _Feat, _Icon>;
    using menu_type  = menu<_Data, _Feat, _Icon>;
    using item_type  = menu_item<_Data, _Feat, _Icon>;
    using data_type  = _Data;

    static constexpr bool focusable  = true;
    static constexpr bool scrollable = false;

    // -- data -------------------------------------------------------------
    std::vector<entry_type> entries;

    // bar-level navigation
    std::size_t focused  = 0;       // currently highlighted top-level entry

    // drop-down state
    bool        active   = false;   // true --> a drop-down is visible
    std::size_t dd_cursor = 0;      // cursor within the open drop-down

    // -- add entries ------------------------------------------------------

    entry_type& add(entry_type e)
    {
        entries.push_back(std::move(e));

        return entries.back();
    }

    entry_type& emplace(non_deduced<_Data> label)
    {
        entries.emplace_back(std::move(label));

        return entries.back();
    }

    entry_type& emplace(non_deduced<_Data>              label,
                        std::unique_ptr<menu_type> dd)
    {
        entries.emplace_back(std::move(label), std::move(dd));

        return entries.back();
    }

    // -- queries ----------------------------------------------------------

    [[nodiscard]] std::size_t count() const noexcept { return entries.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries.empty(); }

    [[nodiscard]] entry_type* focused_entry()
    {
        return (focused < entries.size()) ? &entries[focused] : nullptr;
    }

    [[nodiscard]] const entry_type* focused_entry() const
    {
        return (focused < entries.size()) ? &entries[focused] : nullptr;
    }

    // active_menu
    //   Returns a pointer to the currently open drop-down, or nullptr.
    [[nodiscard]] menu_type* active_menu()
    {
        if ( (!active) ||
             (focused >= entries.size()) )
        {
            return nullptr;
        }

        return entries[focused].dropdown.get();
    }

    [[nodiscard]] const menu_type* active_menu() const
    {
        if ( (!active) ||
             (focused >= entries.size()) )
        {
            return nullptr;
        }

        return entries[focused].dropdown.get();
    }

    // active_item
    //   Returns the item under the drop-down cursor, or nullptr.
    [[nodiscard]] item_type* active_item()
    {
        auto* m = active_menu();
        if ( (!m) ||
             (dd_cursor >= m->size()) )
        {
            return nullptr;
        }

        return &(*m)[dd_cursor];
    }

    [[nodiscard]] const item_type* active_item() const
    {
        auto* m = active_menu();
        if ( (!m) ||
             (dd_cursor >= m->size()) )
        {
            return nullptr;
        }

        return &(*m)[dd_cursor];
    }

    // -- bar-level navigation ---------------------------------------------

    // next
    //   Moves focus to the next enabled top-level entry.  Wraps around.
    //   If a drop-down is open, opens the new entry's drop-down.
    bool next()
    {
        if (entries.empty())
        {
            return false;
        }

        std::size_t n = entries.size();
        for (std::size_t i = 1; i <= n; ++i)
        {
            std::size_t idx = (focused + i) % n;
            if (entries[idx].enabled)
            {
                focused = idx;
                if (active)
                {
                    open_dropdown();
                }

                return true;
            }
        }

        return false;
    }

    // prev
    bool prev()
    {
        if (entries.empty())
        {
            return false;
        }

        std::size_t n = entries.size();
        for (std::size_t i = 1; i <= n; ++i)
        {
            std::size_t idx = (focused + n - i) % n;
            if (entries[idx].enabled)
            {
                focused = idx;
                if (active)
                {
                    open_dropdown();
                }

                return true;
            }
        }

        return false;
    }

    // -- drop-down management ---------------------------------------------

    // open_dropdown
    //   Opens the drop-down for the currently focused entry.
    //   Positions dd_cursor on the first selectable item.
    bool open_dropdown()
    {
        if (focused >= entries.size())
        {
            return false;
        }

        auto* dd = entries[focused].dropdown.get();
        if ( (!dd) ||
             (dd->empty()) )
        {
            return false;
        }

        active    = true;
        dd_cursor = dd->first_selectable();

        return true;
    }

    // close_dropdown
    void close_dropdown()
    {
        active = false;
        dd_cursor = 0;

        return;
    }

    // toggle_dropdown
    void toggle_dropdown()
    {
        if (active)
        {
            close_dropdown();
        }
        else
        {
            open_dropdown();
        }

        return;
    }

    // -- drop-down cursor navigation --------------------------------------

    // dd_next
    //   Moves the drop-down cursor to the next selectable item.
    bool dd_next()
    {
        auto* m = active_menu();
        if ( (!m) ||
             (m->empty()) )
        {
            return false;
        }

        dd_cursor = m->next_selectable(dd_cursor);

        return true;
    }

    // dd_prev
    bool dd_prev()
    {
        auto* m = active_menu();
        if ( (!m) ||
             (m->empty()) )
        {
            return false;
        }

        dd_cursor = m->prev_selectable(dd_cursor);

        return true;
    }

    // dd_home
    bool dd_home()
    {
        auto* m = active_menu();
        if ( (!m) ||
             (m->empty()) )
        {
            return false;
        }

        dd_cursor = m->first_selectable();

        return true;
    }

    // dd_end
    bool dd_end()
    {
        auto* m = active_menu();
        if ( (!m) ||
             (m->empty()) )
        {
            return false;
        }

        // find last selectable
        for (std::size_t i = m->size(); i > 0; --i)
        {
            if ((*m)[i - 1].is_selectable())
            {
                dd_cursor = i - 1;

                return true;
            }
        }

        return false;
    }

    // -- activation -------------------------------------------------------

    // activate
    //   "Presses" the currently focused top-level entry.  If the entry
    // has a drop-down, toggles it.  Returns the entry that was activated.
    entry_type* activate()
    {
        if (focused >= entries.size())
        {
            return nullptr;
        }

        auto& e = entries[focused];
        if (!e.enabled)
        {
            return nullptr;
        }

        if (e.has_dropdown())
        {
            toggle_dropdown();
        }

        return &e;
    }

    // activate_dd_item
    //   "Presses" the item under the drop-down cursor.  Closes the
    // drop-down.  Returns the item, or nullptr if not selectable.
    item_type* activate_dd_item()
    {
        auto* item = active_item();
        if ( (!item) ||
             (!item->is_selectable()) )
        {
            return nullptr;
        }

        // if the item has a submenu, we don't close - the renderer
        // would open the submenu.  For normal items, close.
        if (!item->has_submenu())
        {
            close_dropdown();
        }

        return item;
    }

    // -- search -----------------------------------------------------------

    // search_bar
    //   Finds the first top-level entry matching a predicate on its label.
    template <typename _Match>
    bool search_bar(_Match match)
    {
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            if ( (entries[i].enabled) &&
                 (match(entries[i].label)) )
            {
                focused = i;
                if (active)
                {
                    open_dropdown();
                }

                return true;
            }
        }

        return false;
    }

    // search_dropdown
    //   Finds the next item in the open drop-down matching a predicate.
    template <typename _Match>
    bool search_dropdown(_Match match)
    {
        auto* m = active_menu();
        if ( (!m) ||
             (m->empty()) )
        {
            return false;
        }

        std::size_t n = m->size();
        for (std::size_t i = 1; i <= n; ++i)
        {
            std::size_t idx = (dd_cursor + i) % n;
            if ( ((*m)[idx].is_selectable()) &&
                 (match((*m)[idx].label)) )
            {
                dd_cursor = idx;

                return true;
            }
        }

        return false;
    }
};


// ===============================================================================
//  3.  FREE-FUNCTION HELPERS
// ===============================================================================

// attach_dropdown
template <typename _D,
          unsigned _F,
          typename _I>
void attach_dropdown(
    menu_bar_entry<_D, _F, _I>&       entry,
    std::unique_ptr<menu<_D, _F, _I>> dd)
{
    entry.dropdown = std::move(dd);

    return;
}


// ===============================================================================
//  4.  MENU BAR TRAITS
// ===============================================================================

namespace menu_bar_traits {
namespace detail
{
    // has_entries_member
    //   trait: detects a public .entries member.
    template <typename,
              typename = void>
    struct has_entries_member : std::false_type
    {};

    template <typename _Type>
    struct has_entries_member<_Type, std::void_t<
        decltype(std::declval<_Type>().entries)
    >> : std::true_type
    {};

    // has_focused_member
    //   trait: detects a public .focused member.
    template <typename,
              typename = void>
    struct has_focused_member : std::false_type
    {};

    template <typename _Type>
    struct has_focused_member<_Type, std::void_t<
        decltype(std::declval<_Type>().focused)
    >> : std::true_type
    {};

    // has_active_member
    //   trait: detects a public .active member.
    template <typename,
              typename = void>
    struct has_active_member : std::false_type
    {};

    template <typename _Type>
    struct has_active_member<_Type, std::void_t<
        decltype(std::declval<_Type>().active)
    >> : std::true_type
    {};

    // has_dd_cursor_member
    //   trait: detects a public .dd_cursor member.
    template <typename,
              typename = void>
    struct has_dd_cursor_member : std::false_type
    {};

    template <typename _Type>
    struct has_dd_cursor_member<_Type, std::void_t<
        decltype(std::declval<_Type>().dd_cursor)
    >> : std::true_type
    {};

    // has_dropdown_member
    //   trait: detects a public .dropdown member (on entry type).
    template <typename,
              typename = void>
    struct has_dropdown_member : std::false_type
    {};

    template <typename _Type>
    struct has_dropdown_member<_Type, std::void_t<
        decltype(std::declval<_Type>().dropdown)
    >> : std::true_type
    {};

    // has_entry_label
    //   trait: detects a public .label member (on entry type).
    template <typename,
              typename = void>
    struct has_entry_label : std::false_type
    {};

    template <typename _Type>
    struct has_entry_label<_Type, std::void_t<
        decltype(std::declval<_Type>().label)
    >> : std::true_type
    {};

}   // namespace detail

// convenience aliases
template <typename _T> inline constexpr bool has_entries_v    = detail::has_entries_member<_T>::value;
template <typename _T> inline constexpr bool has_focused_v    = detail::has_focused_member<_T>::value;
template <typename _T> inline constexpr bool has_active_v     = detail::has_active_member<_T>::value;
template <typename _T> inline constexpr bool has_dd_cursor_v  = detail::has_dd_cursor_member<_T>::value;
template <typename _T> inline constexpr bool has_dropdown_v   = detail::has_dropdown_member<_T>::value;
template <typename _T> inline constexpr bool has_entry_label_v= detail::has_entry_label<_T>::value;

// is_menu_bar_entry
//   trait: has label + dropdown + data_type.
template <typename _Type>
struct is_menu_bar_entry : std::conjunction<
    detail::has_entry_label<_Type>,
    detail::has_dropdown_member<_Type>,
    view_traits::detail::has_data_type_alias<_Type>
>
{};

template <typename _T>
inline constexpr bool is_menu_bar_entry_v = is_menu_bar_entry<_T>::value;

// is_menu_bar
//   trait: has entries + focused + active + dd_cursor + focusable.
template <typename _Type>
struct is_menu_bar : std::conjunction<
    detail::has_entries_member<_Type>,
    detail::has_focused_member<_Type>,
    detail::has_active_member<_Type>,
    detail::has_dd_cursor_member<_Type>,
    view_traits::detail::has_focusable_flag<_Type>
>
{};

template <typename _T>
inline constexpr bool is_menu_bar_v = is_menu_bar<_T>::value;

// has_open_dropdown
//   trait: the menu bar can have a visible drop-down.
template <typename _Type>
struct has_open_dropdown : std::conjunction<
    is_menu_bar<_Type>,
    detail::has_active_member<_Type>
>
{};

template <typename _T>
inline constexpr bool has_open_dropdown_v = has_open_dropdown<_T>::value;


}   // namespace menu_bar_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MENU_BAR_
