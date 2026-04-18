/******************************************************************************
* uxoxo [component]                                           color_picker.hpp
*
*   Vendor-agnostic color picker component template for the uxoxo framework. 
* Parameterized on a djinterp color model type (rgb, hsl, hsv, cmyk, ycbcr, 
* cie_lab) and exposes the structural members required for uxoxo trait 
* detection (value_type, model_tag, size_type). Holds a committed color, a 
* staged preview color, a bounded history ring of previously committed colors, 
* a change callback, a picker-style enum, and a flag mask.
*   Only the pure component state lives in this header; actual rendering is 
* performed by vendor-specific bindings in the `uxoxo::platform` namespace 
* (e.g. imgui or glfw adapters), which read the exposed state and drive it 
* through get/set methods.
*
*
* path:      /inc/uxoxo/templates/component/dialog/color/color_picker.hpp
* link(s):   TBA
* author(s): Sam 'teer' Neal-Blim                             date: 2026.04.17
******************************************************************************/

/*
TABLE OF CONTENTS
=================
I.    ENUMERATIONS
      ---------------
      a. picker_style
      b. picker_flag

II.   CHANGE CALLBACK
      ------------------
      a. color_change_callback_t

III.  COLOR HISTORY RING
      ----------------------
      i.    color_history
            a. value_type, size_type, capacity_value
            b. color_history()                   (default constructor)
            c. push, clear
            d. size, capacity, empty
            e. operator[]

IV.   COLOR PICKER COMPONENT TEMPLATE
      ---------------------------------
      i.    color_picker
            a. value_type, model_tag, size_type
            b. callback_type, history_type
            c. color_picker()                    (default constructor)
            d. color_picker(initial, style, flags)
            e. get_color / get_staged
            f. set_color / set_staged
            g. commit / revert
            h. set_on_change / clear_on_change
            i. set_style / get_style
            j. set_flags / get_flags / has_flag
            k. history
            l. operator==

V.    DEDUCTION GUIDES  (C++17+)
      ---------------------------
      a. color_picker(initial)
      b. color_picker(initial, style)
      c. color_picker(initial, style, flags)
*/

#ifndef UXOXO_COMPONENT_DIALOG_COLOR_PICKER_
#define UXOXO_COMPONENT_DIALOG_COLOR_PICKER_ 1

// std
#include <cstddef>
#include <functional>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/util/color/color.hpp>
// uxoxo
#include "../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


///////////////////////////////////////////////////////////////////////////////
///                     I.   ENUMERATIONS                                   ///
///////////////////////////////////////////////////////////////////////////////

// picker_style
//   enum: selects the visual variant a platform adapter should render
// when binding a color_picker. Bindings are free to fall back to
// `sliders` when a requested style is unsupported.
enum class picker_style : unsigned
{
    sliders    = 0,  // one slider per channel
    wheel      = 1,  // hue wheel with saturation / value triangle
    square     = 2,  // saturation / value square plus hue strip
    swatches   = 3,  // grid of preset swatches
    eyedropper = 4   // pick from screen
};

// picker_flag
//   enum: bitmask flags controlling picker behavior. Combined with
// bitwise-or; tested via color_picker::has_flag.
enum picker_flag : unsigned
{
    no_flags        = 0,
    show_alpha      = 1u << 0,  // expose alpha channel when model carries one
    show_preview    = 1u << 1,  // render before/after swatch
    show_hex        = 1u << 2,  // show hex input field (RGB-family only)
    show_history    = 1u << 3,  // render history ring
    clamp_on_set    = 1u << 4,  // clamp out-of-gamut values on set_color
    live_update     = 1u << 5,  // fire on_change during drag (staged updates)
    commit_on_close = 1u << 6   // only commit when the picker closes
};


///////////////////////////////////////////////////////////////////////////////
///                   II.   CHANGE CALLBACK                                 ///
///////////////////////////////////////////////////////////////////////////////

// color_change_callback_t
//   alias template: signature invoked by a color_picker when its
// stored color changes. Parameterized on the underlying color model
// so adapters receive the exact model without conversion.
template<typename _ColorModel>
using color_change_callback_t =
    std::function<void (const _ColorModel& _previous,
                        const _ColorModel& _current)>;


///////////////////////////////////////////////////////////////////////////////
///                III.   COLOR HISTORY RING                                ///
///////////////////////////////////////////////////////////////////////////////

// color_history
//   struct: fixed-capacity ring buffer of recently committed colors.
// Oldest-drop eviction once capacity is reached. Exposes value_type,
// size_type, size(), capacity(), and operator[] for uxoxo's sized /
// array-like trait detectors.
template<typename    _ColorModel,
         std::size_t _Capacity = 16>
struct color_history
{
    using value_type = _ColorModel;
    using size_type  = std::size_t;

    // capacity_value
    //   constant: the compile-time ring capacity.
    D_STATIC_CONSTEXPR size_type capacity_value = _Capacity;

    // color_history (default)
    //   constructor: initializes an empty history ring. Element
    // slots are value-initialized.
    color_history()
        : m_slots(),
          m_count(0),
          m_head(0)
    {}

    // push
    //   function: appends `_c` to the ring. When the ring is at
    // capacity the oldest entry is evicted.
    void
    push(
        const value_type& _c
    )
    {
        m_slots[m_head] = _c;
        m_head = (m_head + 1) % _Capacity;

        if (m_count < _Capacity)
        {
            ++m_count;
        }
    }

    // clear
    //   function: drops every stored entry.
    void
    clear()
    {
        m_count = 0;
        m_head  = 0;
    }

    // size
    //   function: returns the number of populated entries.
    D_CONSTEXPR_INLINE size_type
    size() const
    {
        return m_count;
    }

    // capacity
    //   function: returns the fixed ring capacity.
    D_STATIC_CONSTEXPR_INLINE size_type
    capacity()
    {
        return _Capacity;
    }

    // empty
    //   function: returns true if no entries are stored.
    D_CONSTEXPR_INLINE bool
    empty() const
    {
        return (m_count == 0);
    }

    // operator[]
    //   function: indexed access in insertion order where index 0
    // is the oldest surviving entry and index size()-1 is the most
    // recent. Behavior is undefined when `_i >= size()`.
    D_CONSTEXPR_INLINE const value_type&
    operator[](
        size_type _i
    ) const
    {
        return m_slots[( ( (m_count == _Capacity) 
                           ? m_head 
                           : size_type(0) ) +
                       _i) % _Capacity];
    }

private:
    value_type m_slots[_Capacity];
    size_type  m_count;
    size_type  m_head;
};


///////////////////////////////////////////////////////////////////////////////
///             IV.   COLOR PICKER COMPONENT TEMPLATE                       ///
///////////////////////////////////////////////////////////////////////////////

// color_picker
//   class: vendor-agnostic color picker component. Holds a single
// committed color of model type `_ColorModel`, a staged "preview"
// value for commit-on-close workflows, a bounded history ring, an
// optional change callback, a picker-style hint, and a flag mask.
// Platform adapters read the exposed state and drive it through the
// public set_color / set_staged / commit surface.
//
//   Template parameters:
//     _ColorModel       color model type (must satisfy
//                        djinterp::color::is_color_model).
//     _HistoryCapacity  size of the history ring (default 16).
template<typename    _ColorModel      = djinterp::color::rgb,
         std::size_t _HistoryCapacity = 16>
class color_picker
{
public:
    using value_type    = _ColorModel;
    using model_tag     = typename _ColorModel::model_tag;
    using size_type     = std::size_t;
    using callback_type = color_change_callback_t<_ColorModel>;
    using history_type  = color_history<_ColorModel, _HistoryCapacity>;

    // color_picker (default)
    //   constructor: initializes with a default-constructed color
    // (opaque black for RGB-family models), `sliders` style, and
    // the `show_preview` flag.
    color_picker()
        : m_committed(),
          m_staged(),
          m_style(picker_style::sliders),
          m_flags(show_preview),
          m_on_change(),
          m_history()
    {}

    // color_picker (parameterized)
    //   constructor: initializes from an explicit initial color,
    // visual style, and flag mask. The committed and staged slots
    // both start at `_initial`.
    color_picker(
        const value_type& _initial,
        picker_style      _style = picker_style::sliders,
        unsigned          _flags = show_preview
    )
        : m_committed(_initial),
          m_staged(_initial),
          m_style(_style),
          m_flags(_flags),
          m_on_change(),
          m_history()
    {}

    // get_color
    //   function: returns a const reference to the currently
    // committed color.
    const value_type&
    get_color() const
    {
        return m_committed;
    }

    // get_staged
    //   function: returns a const reference to the staged
    // (uncommitted, preview) color. Tracks the committed value
    // when `commit_on_close` is unset.
    const value_type&
    get_staged() const
    {
        return m_staged;
    }

    // set_color
    //   function: sets both the committed and staged color, pushes
    // the previous committed value into the history ring, and
    // fires the change callback when one is registered.
    void
    set_color(
        const value_type& _c
    )
    {
        value_type previous = m_committed;

        m_history.push(previous);
        m_committed = _c;
        m_staged    = _c;

        if (m_on_change)
        {
            m_on_change(previous, m_committed);
        }
    }

    // set_staged
    //   function: updates only the staged value. When the
    // `live_update` flag is set and a callback is registered, the
    // callback fires with the prior staged value as `_previous`;
    // otherwise callers must invoke `commit()` to publish.
    void
    set_staged(
        const value_type& _c
    )
    {
        value_type previous = m_staged;

        m_staged = _c;

        if ( has_flag(live_update) &&
             m_on_change )
        {
            m_on_change(previous, m_staged);
        }
    }

    // commit
    //   function: promotes the staged value to the committed slot,
    // pushes the old committed value to history, and fires the
    // change callback. Intended for `commit_on_close` workflows.
    // No-op when staged equals committed.
    void
    commit()
    {
        if (m_staged == m_committed)
        {
            return;
        }

        value_type previous = m_committed;

        m_history.push(previous);
        m_committed = m_staged;

        if (m_on_change)
        {
            m_on_change(previous, m_committed);
        }
    }

    // revert
    //   function: discards any staged change, restoring the staged
    // slot to the committed color. Does not fire the callback.
    void
    revert()
    {
        m_staged = m_committed;
    }

    // set_on_change
    //   function: installs a callback invoked on commit (and on
    // every staged update when `live_update` is set).
    void
    set_on_change(
        callback_type _cb
    )
    {
        m_on_change = std::move(_cb);
    }

    // clear_on_change
    //   function: drops the installed change callback, if any.
    void
    clear_on_change()
    {
        m_on_change = callback_type();
    }

    // set_style
    //   function: selects the visual variant for platform adapters.
    void
    set_style(
        picker_style _s
    )
    {
        m_style = _s;
    }

    // get_style
    //   function: returns the current visual variant.
    D_CONSTEXPR_INLINE picker_style
    get_style() const
    {
        return m_style;
    }

    // set_flags
    //   function: assigns the full flag mask.
    void
    set_flags(
        unsigned _f
    )
    {
        m_flags = _f;
    }

    // get_flags
    //   function: returns the current flag mask.
    D_CONSTEXPR_INLINE unsigned
    get_flags() const
    {
        return m_flags;
    }

    // has_flag
    //   function: returns true when every bit in `_mask` is set in
    // the current flag mask.
    D_CONSTEXPR_INLINE bool
    has_flag(
        unsigned _mask
    ) const
    {
        return ( (m_flags & _mask) == _mask );
    }

    // history
    //   function: returns a const reference to the history ring.
    const history_type&
    history() const
    {
        return m_history;
    }

    // operator==
    //   function: equality compares committed color, staged color,
    // style, and flag mask. The callback and history are not
    // considered.
    bool
    operator==(
        const color_picker& _other
    ) const
    {
        return ( (m_committed == _other.m_committed) &&
                 (m_staged    == _other.m_staged)    &&
                 (m_style     == _other.m_style)     &&
                 (m_flags     == _other.m_flags) );
    }

private:
    value_type    m_committed;
    value_type    m_staged;
    picker_style  m_style;
    unsigned      m_flags;
    callback_type m_on_change;
    history_type  m_history;
};


///////////////////////////////////////////////////////////////////////////////
///                V.   DEDUCTION GUIDES  (C++17+)                          ///
///////////////////////////////////////////////////////////////////////////////

#if D_ENV_LANG_IS_CPP17_OR_HIGHER

    template<typename _ColorModel>
    color_picker(const _ColorModel&) -> color_picker<_ColorModel>;

    template<typename _ColorModel>
    color_picker(const _ColorModel&,
                 picker_style) -> color_picker<_ColorModel>;

    template<typename _ColorModel>
    color_picker(const _ColorModel&,
                 picker_style,
                 unsigned) -> color_picker<_ColorModel>;
#endif  // D_ENV_LANG_IS_CPP17_OR_HIGHER


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DIALOG_COLOR_PICKER_