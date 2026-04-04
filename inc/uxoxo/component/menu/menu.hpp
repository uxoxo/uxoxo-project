/*******************************************************************************
* uxoxo [component]                                                    menu.hpp
*
*   Concrete menu item types and menu container.  Pure data aggregates - no
* observer coupling, no renderer knowledge.  These types are designed to
* satisfy the abstract detectors in menu_traits.hpp.
*
*   Structure:
*     1.  Menu feature flags
*     2.  Menu item EBO mixins (shortcut, submenu, checked)
*     3.  menu_item<_Data, _Feat, _Icon> - a single menu entry
*     4.  menu<_Data, _Feat, _Icon> - a flat or nested container of items
*     5.  Free-function helpers
*     6.  static_menu<_Type, N> - compile-time fixed-size menu
*
*   The menu container is deliberately separate from menu_bar.  A menu is
* just a collection of items; a menu_bar is a UI component that owns menus
* and manages navigation/focus/drop-down state.
*
*
* file:      /inc/uxoxo/component/menu/menu.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.31
*******************************************************************************/

#ifndef UXOXO_COMPONENT_MENU_
#define UXOXO_COMPONENT_MENU_ 1

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

//#include <djinterp>
#include <container/view_common.hpp>
#include <container/menu_traits.hpp>


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  MENU FEATURE FLAGS
// ===============================================================================
//   Use the high bits to avoid colliding with view_feat / text_input_feat.

enum menu_feat : unsigned
{
    mf_none        = 0,
    mf_shortcuts   = 1u << 12,    // per-item shortcut string
    mf_icons       = 1u << 13,    // per-item icon
    mf_checkable   = 1u << 14,    // per-item check mark
    mf_submenu     = 1u << 15,    // items may have child menus

    mf_all         = mf_shortcuts | mf_icons | mf_checkable | mf_submenu
};

constexpr menu_feat operator|(menu_feat a, menu_feat b) noexcept
{
    return static_cast<menu_feat>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr bool has_mf(unsigned f, menu_feat bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


// ===============================================================================
//  2.  MENU ITEM EBO MIXINS
// ===============================================================================

// forward-declare menu for submenu pointer
template <typename _Data, unsigned _Feat, typename _Icon>
struct menu;

namespace menu_mixin {

    // -- shortcut ---------------------------------------------------------
    template <bool _Enable>
    struct shortcut_data {};

    template <>
    struct shortcut_data<true>
    {
        std::string shortcut;       // display string e.g. "Ctrl+S"
    };

    // -- icon -------------------------------------------------------------
    template <bool _Enable, typename _Icon = int>
    struct icon_data {};

    template <typename _Icon>
    struct icon_data<true, _Icon>
    {
        _Icon icon{};
    };

    // -- checkable --------------------------------------------------------
    template <bool _Enable>
    struct checkable_data {};

    template <>
    struct checkable_data<true>
    {
        bool checked = false;
    };

    // -- submenu ----------------------------------------------------------
    template <bool _Enable, typename _Data, unsigned _Feat, typename _Icon>
    struct submenu_data {};

    template <typename _Data, unsigned _Feat, typename _Icon>
    struct submenu_data<true, _Data, _Feat, _Icon>
    {
        std::unique_ptr<menu<_Data, _Feat, _Icon>> submenu;
    };

}   // namespace menu_mixin



// ===============================================================================
//  3.  MENU ITEM
// ===============================================================================

// menu_item_type
//   Distinguishes normal items, separators, and group headers.
enum class menu_item_type : std::uint8_t
{
    normal,
    separator,
    header          // non-selectable group header
};

template <typename _Data   = std::string,
          unsigned _Feat   = mf_none,
          typename _Icon   = int>
struct menu_item
    : menu_mixin::shortcut_data  <has_mf(_Feat, mf_shortcuts)>
    , menu_mixin::icon_data      <has_mf(_Feat, mf_icons), _Icon>
    , menu_mixin::checkable_data <has_mf(_Feat, mf_checkable)>
    , menu_mixin::submenu_data   <has_mf(_Feat, mf_submenu), _Data, _Feat, _Icon>
{
    using data_type = _Data;
    using icon_type = _Icon;

    static constexpr unsigned features = _Feat;
    static constexpr bool has_shortcuts  = has_mf(_Feat, mf_shortcuts);
    static constexpr bool has_icons      = has_mf(_Feat, mf_icons);
    static constexpr bool is_checkable   = has_mf(_Feat, mf_checkable);
    static constexpr bool has_submenus   = has_mf(_Feat, mf_submenu);

    _Data           label;
    menu_item_type  type    = menu_item_type::normal;
    bool            enabled = true;

    // -- construction -----------------------------------------------------
    menu_item() = default;

    explicit menu_item(_Data lbl)
        : label(std::move(lbl))
    {}

    menu_item(_Data lbl, menu_item_type t)
        : label(std::move(lbl)), type(t)
    {}

    // -- queries ----------------------------------------------------------
    [[nodiscard]] bool is_separator() const noexcept 
    { 
        return type == menu_item_type::separator; 
    }

    [[nodiscard]] bool is_header() const noexcept 
    { 
        return type == menu_item_type::header; 
    }

    [[nodiscard]] bool is_selectable() const noexcept
    {
        return type == menu_item_type::normal && enabled;
    }

    [[nodiscard]] bool has_submenu() const noexcept
    {
        if constexpr (has_submenus)
            return this->submenu != nullptr;
        else
            return false;
    }
};

// convenience: separator factory
template <typename _D = std::string, unsigned _F = mf_none, typename _I = int>
menu_item<_D, _F, _I> make_separator()
{
    return menu_item<_D, _F, _I>(_D{}, menu_item_type::separator);
}

// convenience: header factory
template <typename _D = std::string, unsigned _F = mf_none, typename _I = int>
menu_item<_D, _F, _I> make_header(non_deduced<_D> label)
{
    return menu_item<_D, _F, _I>(std::move(label), menu_item_type::header);
}


// ===============================================================================
//  4.  MENU CONTAINER
// ===============================================================================
//   A menu is an ordered collection of menu_items.  It satisfies the
// abstract is_menu trait (has value_type, begin/end, size).

template <typename _Data   = std::string,
          unsigned _Feat   = mf_none,
          typename _Icon   = int>
struct menu
{
    using value_type      = menu_item<_Data, _Feat, _Icon>;
    using iterator        = typename std::vector<value_type>::iterator;
    using const_iterator  = typename std::vector<value_type>::const_iterator;

    // -- data -------------------------------------------------------------
    std::string             title;
    std::vector<value_type> items;

    // -- construction -----------------------------------------------------
    menu() = default;
    explicit menu(std::string t) : title(std::move(t)) {}

    menu(std::string t, std::initializer_list<value_type> init)
        : title(std::move(t)), items(init)
    {}

    // -- container interface (satisfies menu_traits::is_menu) -------------
    iterator        begin()       noexcept { return items.begin(); }
    const_iterator  begin() const noexcept { return items.begin(); }
    const_iterator  cbegin()const noexcept { return items.cbegin(); }
    iterator        end()         noexcept { return items.end(); }
    const_iterator  end()   const noexcept { return items.end(); }
    const_iterator  cend()  const noexcept { return items.cend(); }

    [[nodiscard]] std::size_t size()  const noexcept { return items.size(); }
    [[nodiscard]] bool        empty() const noexcept { return items.empty(); }

    value_type&       operator[](std::size_t i)       { return items[i]; }
    const value_type& operator[](std::size_t i) const { return items[i]; }

    // -- mutation ---------------------------------------------------------

    value_type& add(value_type item)
    {
        items.push_back(std::move(item));
        return items.back();
    }

    value_type& emplace(non_deduced<_Data> label)
    {
        items.emplace_back(std::move(label));
        return items.back();
    }

    void add_separator()
    {
        items.push_back(make_separator<_Data, _Feat, _Icon>());
    }

    value_type& add_header(non_deduced<_Data> label)
    {
        items.push_back(make_header<_Data, _Feat, _Icon>(std::move(label)));
        return items.back();
    }

    bool remove(std::size_t idx)
    {
        if (idx >= items.size()) return false;
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(idx));
        return true;
    }

    void clear() { items.clear(); }

    // -- queries ----------------------------------------------------------

    // selectable_count
    //   Number of items that are normal and enabled.
    [[nodiscard]] std::size_t selectable_count() const
    {
        std::size_t n = 0;
        for (const auto& item : items)
            if (item.is_selectable()) ++n;
        return n;
    }

    // find_by_label
    //   Returns index of first item matching predicate on label, or -1.
    template <typename _Pred>
    std::size_t find_by(_Pred pred) const
    {
        for (std::size_t i = 0; i < items.size(); ++i)
            if (pred(items[i].label)) return i;
        return static_cast<std::size_t>(-1);
    }

    // next_selectable
    //   Returns the index of the next selectable item after `from`.
    //   Wraps around.  Returns from if none found.
    [[nodiscard]] std::size_t next_selectable(std::size_t from) const
    {
        if (items.empty()) return from;
        std::size_t n = items.size();
        for (std::size_t i = 1; i <= n; ++i) {
            std::size_t idx = (from + i) % n;
            if (items[idx].is_selectable()) return idx;
        }
        return from;
    }

    // prev_selectable
    [[nodiscard]] std::size_t prev_selectable(std::size_t from) const
    {
        if (items.empty()) return from;
        std::size_t n = items.size();
        for (std::size_t i = 1; i <= n; ++i) {
            std::size_t idx = (from + n - i) % n;
            if (items[idx].is_selectable()) return idx;
        }
        return from;
    }

    // first_selectable
    [[nodiscard]] std::size_t first_selectable() const
    {
        for (std::size_t i = 0; i < items.size(); ++i)
            if (items[i].is_selectable()) return i;
        return 0;
    }
};



// ===============================================================================
//  5.  FREE-FUNCTION HELPERS
// ===============================================================================

// set_shortcut  (menu_item)
template <typename _D, unsigned _F, typename _I>
void set_shortcut(menu_item<_D, _F, _I>& item, std::string sc)
{
    static_assert(has_mf(_F, mf_shortcuts), "requires mf_shortcuts");
    item.shortcut = std::move(sc);
}

// set_icon  (menu_item)
template <typename _D, unsigned _F, typename _I>
void set_icon(menu_item<_D, _F, _I>& item, non_deduced<_I> icon)
{
    static_assert(has_mf(_F, mf_icons), "requires mf_icons");
    item.icon = std::move(icon);
}

// set_checked  (menu_item)
template <typename _D, unsigned _F, typename _I>
void set_checked(menu_item<_D, _F, _I>& item, bool c)
{
    static_assert(has_mf(_F, mf_checkable), "requires mf_checkable");
    item.checked = c;
}

// toggle_checked  (menu_item)
template <typename _D, unsigned _F, typename _I>
void toggle_checked(menu_item<_D, _F, _I>& item)
{
    static_assert(has_mf(_F, mf_checkable), "requires mf_checkable");
    item.checked = !item.checked;
}

// attach_submenu
template <typename _D, unsigned _F, typename _I>
void attach_submenu(menu_item<_D, _F, _I>& item,
                    std::unique_ptr<menu<_D, _F, _I>> sub)
{
    static_assert(has_mf(_F, mf_submenu), "requires mf_submenu");
    item.submenu = std::move(sub);
}

// make_submenu_item
//   Convenience: creates a normal item with an attached submenu.
template <typename _D, unsigned _F, typename _I>
menu_item<_D, _F, _I> make_submenu_item(
    non_deduced<_D> label,
    std::unique_ptr<menu<_D, _F, _I>> sub)
{
    static_assert(has_mf(_F, mf_submenu), "requires mf_submenu");
    menu_item<_D, _F, _I> item(std::move(label));
    item.submenu = std::move(sub);
    return item;
}

// enable_all
template <typename _D, unsigned _F, typename _I>
void enable_all(menu<_D, _F, _I>& m)
{
    for (auto& item : m.items) item.enabled = true;
}

// disable_all
template <typename _D, unsigned _F, typename _I>
void disable_all(menu<_D, _F, _I>& m)
{
    for (auto& item : m.items)
        if (item.type == menu_item_type::normal) item.enabled = false;
}

// -- traversal ------------------------------------------------------------

// menu_for_each
//   Calls fn(item, depth) for every item in the menu.  Recurses into
// submenus when mf_submenu is enabled.
template <typename _D, unsigned _F, typename _I, typename _Fn>
void menu_for_each(menu<_D, _F, _I>& m, _Fn&& fn, std::size_t depth = 0)
{
    for (auto& item : m.items) {
        fn(item, depth);
        if constexpr (has_mf(_F, mf_submenu)) {
            if (item.submenu)
                menu_for_each(*item.submenu, fn, depth + 1);
        }
    }
}

// menu_for_each (const)
template <typename _D, unsigned _F, typename _I, typename _Fn>
void menu_for_each(const menu<_D, _F, _I>& m, _Fn&& fn, std::size_t depth = 0)
{
    for (const auto& item : m.items) {
        fn(item, depth);
        if constexpr (has_mf(_F, mf_submenu)) {
            if (item.submenu)
                menu_for_each(*item.submenu, fn, depth + 1);
        }
    }
}

// menu_count_all
//   Total item count including submenus.
template <typename _D, unsigned _F, typename _I>
std::size_t menu_count_all(const menu<_D, _F, _I>& m)
{
    std::size_t n = 0;
    menu_for_each(m, [&n](const auto&, std::size_t) { ++n; });
    return n;
}

// menu_find_item
//   Finds the first item matching a predicate (depth-first).
template <typename _D, unsigned _F, typename _I, typename _Pred>
menu_item<_D, _F, _I>* menu_find_item(menu<_D, _F, _I>& m, _Pred pred)
{
    for (auto& item : m.items) {
        if (pred(item)) return &item;
        if constexpr (has_mf(_F, mf_submenu)) {
            if (item.submenu) {
                auto* found = menu_find_item(*item.submenu, pred);
                if (found) return found;
            }
        }
    }
    return nullptr;
}



// ===============================================================================
//  6.  STATIC MENU
// ===============================================================================
//   Compile-time fixed-size menu.  Satisfies menu_traits::is_menu because
// it has value_type + begin()/end().

template <typename _Type,
          std::size_t N>
class static_menu
{
public:
    using value_type             = _Type;
    using reference              = _Type&;
    using const_reference        = const _Type&;
    using pointer                = _Type*;
    using const_pointer          = const _Type*;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using iterator               = _Type*;
    using const_iterator         = const _Type*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static_menu() = default;

    constexpr static_menu(std::initializer_list<_Type> init) 
    {
        auto it = init.begin();
        for (size_type i = 0; i < N && i < init.size(); ++i)
            items_[i] = *(it++);
    }

    template <typename... _Args,
              typename = std::enable_if_t<sizeof...(_Args) <= N>>
    constexpr static_menu(_Args&&... args)
        : items_{ static_cast<_Type>(std::forward<_Args>(args))... }
    {}

    // iterators
    iterator       begin()       noexcept { return std::begin(items_); }
    const_iterator begin() const noexcept { return std::begin(items_); }
    const_iterator cbegin()const noexcept { return std::begin(items_); }
    iterator       end()         noexcept { return std::end(items_); }
    const_iterator end()   const noexcept { return std::end(items_); }
    const_iterator cend()  const noexcept { return std::end(items_); }

    reverse_iterator       rbegin()       noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator       rend()         noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend()   const noexcept { return const_reverse_iterator(begin()); }

    // element access
    reference       operator[](size_type i)       { return items_[i]; }
    const_reference operator[](size_type i) const { return items_[i]; }

    reference at(size_type i)
    {
        if (i >= N) throw std::out_of_range("static_menu index out of range");
        return items_[i];
    }

    const_reference at(size_type i) const
    {
        if (i >= N) throw std::out_of_range("static_menu index out of range");
        return items_[i];
    }

    reference       front()       { return items_[0]; }
    const_reference front() const { return items_[0]; }
    reference       back()        { return items_[N - 1]; }
    const_reference back()  const { return items_[N - 1]; }

    // capacity
    [[nodiscard]] constexpr bool      empty()    const noexcept { return N == 0; }
    [[nodiscard]] constexpr size_type size()     const noexcept { return N; }
    [[nodiscard]] constexpr size_type max_size() const noexcept { return N; }

    _Type*       data()       noexcept { return items_; }
    const _Type* data() const noexcept { return items_; }

private:
    _Type items_[N];
};

// deduction guides
template <typename    _T, 
          typename... _U>
static_menu(_T, _U...) -> static_menu<_T, 1 + sizeof...(_U)>;


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MENU_