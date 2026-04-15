/*******************************************************************************
* uxoxo [component]                                                 toolbar.hpp
*
*   A framework-agnostic toolbar that holds a sequence of type-erased
* button entries.  Because buttons are templates parameterised on feature
* flags, the toolbar erases them through a small polymorphic wrapper so
* that a single toolbar can contain buttons with different feature sets.
*
*   The toolbar also supports separators and flexible spacers as entries,
* enabling layouts like:  [Save] [Undo] [Redo]  |  <spacer>  |  [Settings]
*
*   The toolbar itself has no render() method.  It is positioned by the
* renderer based on the dock field (top, bottom, left, right, floating).
*
* Contents:
*   1  Enums (toolbar_dock, entry_kind)
*   2  toolbar_entry (type-erased wrapper)
*   3  toolbar struct
*   4  Free functions
*   5  Traits
*
*
* path:      /inc/uxoxo/templates/component/toolbar/toolbar.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_COMPONENT_TOOLBAR_
#define UXOXO_COMPONENT_TOOLBAR_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../view_common.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  ENUMS
// ===============================================================================

// toolbar_dock
//   enum: edge of the parent window where the toolbar is anchored.
enum class toolbar_dock : std::uint8_t
{
    top,
    bottom,
    left,
    right,
    floating
};

// entry_kind
//   enum: discriminator for toolbar entries.
enum class entry_kind : std::uint8_t
{
    button,
    separator,
    spacer
};


/*****************************************************************************/

// ===============================================================================
//  2  TOOLBAR ENTRY (type-erased button wrapper)
// ===============================================================================
//   A toolbar can hold buttons with different template arguments.  Each
// button is erased behind a small interface that exposes the fields every
// renderer needs: label, enabled, visible, pressed, hovered, plus a
// generic draw dispatch.
//
//   Separators and spacers carry no payload — just the entry_kind tag.

// toolbar_button_iface
//   class: abstract interface for a type-erased button in the toolbar.
class toolbar_button_iface
{
public:
    virtual ~toolbar_button_iface() = default;

    // identity
    virtual const std::string& label()   const = 0;
    virtual bool               enabled() const = 0;
    virtual bool               visible() const = 0;

    // interaction state (mutable — set by renderer)
    virtual bool  pressed()  const = 0;
    virtual bool  hovered()  const = 0;

    // click dispatch
    virtual void  click() = 0;

    // reset per-frame transient state
    virtual void  reset_frame() = 0;

    // raw pointer for renderer to static_cast if needed
    virtual void* raw_ptr() = 0;

    // tooltip (empty if not supported)
    virtual const std::string& tooltip_text() const = 0;
};


// toolbar_button_impl
//   class: concrete implementation wrapping a specific button<_F, _I>.
template <unsigned _F,
          typename _I>
class toolbar_button_impl : public toolbar_button_iface
{
public:
    using button_type = button<_F, _I>;

    explicit toolbar_button_impl(
            button_type _btn
        ) noexcept
            : m_btn(std::move(_btn))
        {}

    const std::string& label()   const override { return m_btn.label;   }
    bool               enabled() const override { return m_btn.enabled; }
    bool               visible() const override { return m_btn.visible; }
    bool               pressed() const override { return m_btn.pressed; }
    bool               hovered() const override { return m_btn.hovered; }

    void click() override
    {
        btn_click(m_btn);

        return;
    }

    void reset_frame() override
    {
        btn_reset_pressed(m_btn);
        m_btn.hovered = false;

        return;
    }

    void* raw_ptr() override
    {
        return static_cast<void*>(&m_btn);
    }

    const std::string& tooltip_text() const override
    {
        if constexpr (has_bf(_F, bf_tooltip))
        {
            return m_btn.tooltip;
        }
        else
        {
            static const std::string empty;

            return empty;
        }
    }

    // direct access for typed renderers
    button_type&       get()       { return m_btn; }
    const button_type& get() const { return m_btn; }

private:
    button_type m_btn;
};


// toolbar_entry
//   struct: a single item in the toolbar — button, separator, or spacer.
struct toolbar_entry
{
    entry_kind                              kind = entry_kind::button;
    std::unique_ptr<toolbar_button_iface>   btn;

    // convenience queries
    [[nodiscard]] bool
    is_button() const noexcept
    {
        return (kind == entry_kind::button);
    }

    [[nodiscard]] bool
    is_separator() const noexcept
    {
        return (kind == entry_kind::separator);
    }

    [[nodiscard]] bool
    is_spacer() const noexcept
    {
        return (kind == entry_kind::spacer);
    }
};


/*****************************************************************************/

// ===============================================================================
//  3  TOOLBAR
// ===============================================================================

// toolbar
//   struct: an ordered sequence of buttons, separators, and spacers.
struct toolbar
{
    // component identity
    static constexpr bool focusable = false;

    // layout
    toolbar_dock dock    = toolbar_dock::bottom;
    float        height  = 36.0f;       // horizontal toolbar height
    float        width   = 36.0f;       // vertical toolbar width
    float        padding = 4.0f;
    float        spacing = 4.0f;        // gap between entries

    // visibility
    bool visible = true;

    // entries
    std::vector<toolbar_entry> entries;
};


/*****************************************************************************/

// ===============================================================================
//  4  FREE FUNCTIONS
// ===============================================================================

// tb_add_button
//   function: adds a button to the toolbar.  The button is moved into
// a type-erased wrapper.  Returns a reference to the toolbar_entry.
template <unsigned _F,
          typename _I>
toolbar_entry&
tb_add_button(
    toolbar&        _tb,
    button<_F, _I>  _btn
)
{
    toolbar_entry entry;
    entry.kind = entry_kind::button;
    entry.btn  = std::make_unique<toolbar_button_impl<_F, _I>>(
        std::move(_btn));

    _tb.entries.push_back(std::move(entry));

    return _tb.entries.back();
}

// tb_add_separator
//   function: adds a visual separator to the toolbar.
D_INLINE toolbar_entry&
tb_add_separator(
    toolbar& _tb
)
{
    toolbar_entry entry;
    entry.kind = entry_kind::separator;

    _tb.entries.push_back(std::move(entry));

    return _tb.entries.back();
}

// tb_add_spacer
//   function: adds a flexible spacer that pushes subsequent entries
// to the opposite end.
D_INLINE toolbar_entry&
tb_add_spacer(
    toolbar& _tb
)
{
    toolbar_entry entry;
    entry.kind = entry_kind::spacer;

    _tb.entries.push_back(std::move(entry));

    return _tb.entries.back();
}

// tb_clear
//   function: removes all entries from the toolbar.
D_INLINE void
tb_clear(
    toolbar& _tb
)
{
    _tb.entries.clear();

    return;
}

// tb_entry_count
//   function: returns the number of entries.
D_INLINE std::size_t
tb_entry_count(
    const toolbar& _tb
) noexcept
{
    return _tb.entries.size();
}

// tb_button_count
//   function: returns the number of button entries.
D_INLINE std::size_t
tb_button_count(
    const toolbar& _tb
)
{
    std::size_t count = 0;

    for (const auto& e : _tb.entries)
    {
        if (e.is_button())
        {
            ++count;
        }
    }

    return count;
}

// tb_reset_frame
//   function: resets per-frame transient state on all buttons.
// Call at the start of each frame before rendering.
D_INLINE void
tb_reset_frame(
    toolbar& _tb
)
{
    for (auto& e : _tb.entries)
    {
        if ( (e.is_button()) && (e.btn) )
        {
            e.btn->reset_frame();
        }
    }

    return;
}

// tb_show
D_INLINE void
tb_show(
    toolbar& _tb
)
{
    _tb.visible = true;

    return;
}

// tb_hide
D_INLINE void
tb_hide(
    toolbar& _tb
)
{
    _tb.visible = false;

    return;
}

// tb_find_button_by_label
//   function: finds the first button entry matching a label.
// Returns nullptr if not found.
D_INLINE toolbar_button_iface*
tb_find_button_by_label(
    toolbar&           _tb,
    const std::string& _label
)
{
    for (auto& e : _tb.entries)
    {
        if ( (e.is_button()) &&
             (e.btn)         &&
             (e.btn->label() == _label) )
        {
            return e.btn.get();
        }
    }

    return nullptr;
}

// tb_for_each_button
//   function: calls a visitor for each button entry.
template <typename _Fn>
void
tb_for_each_button(
    toolbar& _tb,
    _Fn&&    _fn
)
{
    for (auto& e : _tb.entries)
    {
        if ( (e.is_button()) && (e.btn) )
        {
            _fn(*e.btn);
        }
    }

    return;
}


/*****************************************************************************/

// ===============================================================================
//  5  TRAITS
// ===============================================================================

namespace toolbar_traits {
namespace detail {

    template <typename, typename = void>
    struct has_entries_member : std::false_type {};
    template <typename _Type>
    struct has_entries_member<_Type, std::void_t<
        decltype(std::declval<_Type>().entries)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_dock_member : std::false_type {};
    template <typename _Type>
    struct has_dock_member<_Type, std::void_t<
        decltype(std::declval<_Type>().dock)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_height_member : std::false_type {};
    template <typename _Type>
    struct has_height_member<_Type, std::void_t<
        decltype(std::declval<_Type>().height)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_visible_member : std::false_type {};
    template <typename _Type>
    struct has_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type>().visible)
    >> : std::true_type {};

}   // namespace detail

template <typename _Type>
inline constexpr bool has_entries_v =
    detail::has_entries_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_dock_v =
    detail::has_dock_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_height_v =
    detail::has_height_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_visible_v =
    detail::has_visible_member<_Type>::value;

// is_toolbar
//   type trait: has entries + dock + height + visible.
template <typename _Type>
struct is_toolbar : std::conjunction<
    detail::has_entries_member<_Type>,
    detail::has_dock_member<_Type>,
    detail::has_height_member<_Type>,
    detail::has_visible_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_toolbar_v =
    is_toolbar<_Type>::value;

}   // namespace toolbar_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_TOOLBAR_
