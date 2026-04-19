/*******************************************************************************
* uxoxo [component]                                                  button.hpp
*
*   A framework-agnostic button template with zero-cost optional features
* controlled by a compile-time bitfield.  Follows the same EBO-mixin
* pattern used by tree_node and login_form.
*
*   The user pays for exactly what they enable:
*
*     button<>                                      ~40 bytes (label+callback)
*     button<bf_icon>                               +sizeof(_Icon)
*     button<bf_icon | bf_tooltip | bf_toggle>      +tooltip string +1 bool
*
*   Template parameters:
*     _Feat:   bitwise OR of button_feat flags  (default: bf_none)
*     _Icon:   icon storage type when bf_icon is set  (default: int)
*
*   A button is a pure data aggregate.  It has no render() method.
* A renderer discovers its capabilities via button_traits:: and
* dispatches with if constexpr.
*
*   Shared operations (enable, disable, show, hide) are provided by
* the ADL-dispatched free functions in component_common.hpp.  Legacy
* btn_-prefixed wrappers are retained for backward compatibility.
*
* Contents:
*   1  Feature flags (button_feat)
*   2  Shape and state enums
*   3  EBO mixins
*   4  button struct
*   5  Domain-specific free functions
*   6  Legacy free functions (btn_-prefixed, thin wrappers)
*   7  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/button/button.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef UXOXO_COMPONENT_BUTTON_
#define UXOXO_COMPONENT_BUTTON_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../component_traits.hpp"
#include "../component_common.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  FEATURE FLAGS
// ===============================================================================

enum button_feat : unsigned
{
    bf_none     = 0,
    bf_icon     = 1u << 0,      // icon display (type parameterised)
    bf_tooltip  = 1u << 1,      // hover tooltip text
    bf_shape    = 1u << 2,      // non-default shape (circle, pill, etc.)
    bf_toggle   = 1u << 3,      // toggle / sticky state
    bf_badge    = 1u << 4,      // badge counter overlay
    bf_shortcut = 1u << 5,      // keyboard shortcut label
    bf_color    = 1u << 6,      // per-button color override

    bf_standard = bf_icon | bf_tooltip,
    bf_all      = bf_icon    | bf_tooltip | bf_shape | bf_toggle
                | bf_badge   | bf_shortcut | bf_color
};

constexpr unsigned operator|(button_feat _a,
                             button_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool has_bf(unsigned     _f,
                      button_feat  _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}




// ===============================================================================
//  2  SHAPE AND STATE ENUMS
// ===============================================================================

// button_shape
//   enum: visual shape of the button.
enum class button_shape : std::uint8_t
{
    rect,           // sharp corners
    rounded,        // small corner radius
    circle,         // circular (icon-only buttons)
    pill,           // fully rounded capsule
    custom          // renderer-specific shape
};

// button_size
//   enum: semantic size hint for the renderer.
enum class button_size : std::uint8_t
{
    small,
    medium,
    large
};




// ===============================================================================
//  3  EBO MIXINS
// ===============================================================================

namespace button_mixin {

    // -- icon -------------------------------------------------------------
    template <bool _Enable,
              typename _Icon = int>
    struct icon_data
    {};

    template <typename _Icon>
    struct icon_data<true, _Icon>
    {
        _Icon icon{};
        bool  icon_only = false;    // hide label, show only icon
    };

    // -- tooltip ----------------------------------------------------------
    template <bool _Enable>
    struct tooltip_data
    {};

    template <>
    struct tooltip_data<true>
    {
        std::string tooltip;
    };

    // -- shape ------------------------------------------------------------
    template <bool _Enable>
    struct shape_data
    {};

    template <>
    struct shape_data<true>
    {
        button_shape shape        = button_shape::rounded;
        float        corner_radius = 4.0f;      // for rounded / custom
    };

    // -- toggle -----------------------------------------------------------
    template <bool _Enable>
    struct toggle_data
    {};

    template <>
    struct toggle_data<true>
    {
        bool toggled = false;
    };

    // -- badge ------------------------------------------------------------
    template <bool _Enable>
    struct badge_data
    {};

    template <>
    struct badge_data<true>
    {
        int         badge_count   = 0;
        bool        badge_visible = false;
    };

    // -- shortcut ---------------------------------------------------------
    template <bool _Enable>
    struct shortcut_data
    {};

    template <>
    struct shortcut_data<true>
    {
        std::string shortcut_label;     // e.g. "Ctrl+S"
    };

    // -- color ------------------------------------------------------------
    template <bool _Enable>
    struct color_data
    {};

    template <>
    struct color_data<true>
    {
        // stored as RGBA floats (0–1)
        float bg_r = 0.0f;
        float bg_g = 0.0f;
        float bg_b = 0.0f;
        float bg_a = 0.0f;     // 0 = use theme default

        float fg_r = 0.0f;
        float fg_g = 0.0f;
        float fg_b = 0.0f;
        float fg_a = 0.0f;     // 0 = use theme default
    };

}   // namespace button_mixin




// ===============================================================================
//  4  BUTTON
// ===============================================================================

template <unsigned _Feat = bf_none,
          typename _Icon = int>
struct button
    : button_mixin::icon_data     <has_bf(_Feat, bf_icon), _Icon>
    , button_mixin::tooltip_data  <has_bf(_Feat, bf_tooltip)>
    , button_mixin::shape_data    <has_bf(_Feat, bf_shape)>
    , button_mixin::toggle_data   <has_bf(_Feat, bf_toggle)>
    , button_mixin::badge_data    <has_bf(_Feat, bf_badge)>
    , button_mixin::shortcut_data <has_bf(_Feat, bf_shortcut)>
    , button_mixin::color_data    <has_bf(_Feat, bf_color)>
{
    using self_type = button<_Feat, _Icon>;
    using icon_type = _Icon;
    using click_fn  = std::function<void(self_type&)>;

    static constexpr unsigned features = _Feat;

    // compile-time feature queries
    static constexpr bool has_icon     = has_bf(_Feat, bf_icon);
    static constexpr bool has_tooltip  = has_bf(_Feat, bf_tooltip);
    static constexpr bool has_shape    = has_bf(_Feat, bf_shape);
    static constexpr bool has_toggle   = has_bf(_Feat, bf_toggle);
    static constexpr bool has_badge    = has_bf(_Feat, bf_badge);
    static constexpr bool has_shortcut = has_bf(_Feat, bf_shortcut);
    static constexpr bool has_color    = has_bf(_Feat, bf_color);

    // component identity
    static constexpr bool focusable = true;

    // -- core state -------------------------------------------------------
    std::string label;
    bool        enabled  = true;
    bool        visible  = true;
    button_size size     = button_size::medium;

    // -- interaction state (set by renderer, read by application) ---------
    bool        pressed  = false;    // true on the frame the button was clicked
    bool        hovered  = false;    // true while the cursor is over the button
    bool        focused  = false;    // true while the button has keyboard focus

    // -- callback ---------------------------------------------------------
    click_fn    on_click;

    // -- construction -----------------------------------------------------
    button() = default;

    explicit button(std::string _label)
        : label(std::move(_label))
    {}
};




// ===============================================================================
//  5  DOMAIN-SPECIFIC FREE FUNCTIONS
// ===============================================================================
//   These operations are unique to button and have no shared
// equivalent in component_common.hpp.

// btn_click
//   function: simulates a click.  Sets pressed, handles toggle,
// invokes the callback.
template <unsigned _F,
          typename _I>
void
btn_click(
    button<_F, _I>& _btn
)
{
    if (!_btn.enabled)
    {
        return;
    }

    _btn.pressed = true;

    // toggle state
    if constexpr (has_bf(_F, bf_toggle))
    {
        _btn.toggled = !_btn.toggled;
    }

    // invoke callback
    if (_btn.on_click)
    {
        _btn.on_click(_btn);
    }

    return;
}

// btn_reset_pressed
//   function: clears the pressed flag.  Call at the start of each
// frame before rendering.
template <unsigned _F,
          typename _I>
void
btn_reset_pressed(
    button<_F, _I>& _btn
)
{
    _btn.pressed = false;

    return;
}

// btn_set_label
template <unsigned _F,
          typename _I>
void
btn_set_label(
    button<_F, _I>& _btn,
    std::string      _label
)
{
    _btn.label = std::move(_label);

    return;
}

// btn_set_tooltip
template <unsigned _F,
          typename _I>
void
btn_set_tooltip(
    button<_F, _I>& _btn,
    std::string      _text
)
{
    static_assert(has_bf(_F, bf_tooltip),
                  "btn_set_tooltip requires bf_tooltip");

    _btn.tooltip = std::move(_text);

    return;
}

// btn_set_icon
template <unsigned _F,
          typename _I>
void
btn_set_icon(
    button<_F, _I>&                                _btn,
    typename button<_F, _I>::icon_type              _icon
)
{
    static_assert(has_bf(_F, bf_icon),
                  "btn_set_icon requires bf_icon");

    _btn.icon = std::move(_icon);

    return;
}

// btn_set_icon_only
//   function: sets the button to icon-only mode (hides label).
template <unsigned _F,
          typename _I>
void
btn_set_icon_only(
    button<_F, _I>& _btn,
    bool             _icon_only
)
{
    static_assert(has_bf(_F, bf_icon),
                  "btn_set_icon_only requires bf_icon");

    _btn.icon_only = _icon_only;

    return;
}

// btn_set_shape
template <unsigned _F,
          typename _I>
void
btn_set_shape(
    button<_F, _I>& _btn,
    button_shape     _shape,
    float            _radius = 4.0f
)
{
    static_assert(has_bf(_F, bf_shape),
                  "btn_set_shape requires bf_shape");

    _btn.shape         = _shape;
    _btn.corner_radius = _radius;

    return;
}

// btn_set_badge
template <unsigned _F,
          typename _I>
void
btn_set_badge(
    button<_F, _I>& _btn,
    int              _count
)
{
    static_assert(has_bf(_F, bf_badge),
                  "btn_set_badge requires bf_badge");

    _btn.badge_count   = _count;
    _btn.badge_visible = (_count > 0);

    return;
}

// btn_set_shortcut_label
template <unsigned _F,
          typename _I>
void
btn_set_shortcut_label(
    button<_F, _I>& _btn,
    std::string      _shortcut
)
{
    static_assert(has_bf(_F, bf_shortcut),
                  "btn_set_shortcut_label requires bf_shortcut");

    _btn.shortcut_label = std::move(_shortcut);

    return;
}

// btn_set_color
//   function: sets the button's per-instance color override.
template <unsigned _F,
          typename _I>
void
btn_set_color(
    button<_F, _I>& _btn,
    float _bg_r, float _bg_g, float _bg_b, float _bg_a,
    float _fg_r, float _fg_g, float _fg_b, float _fg_a
)
{
    static_assert(has_bf(_F, bf_color),
                  "btn_set_color requires bf_color");

    _btn.bg_r = _bg_r;  _btn.bg_g = _bg_g;
    _btn.bg_b = _bg_b;  _btn.bg_a = _bg_a;
    _btn.fg_r = _fg_r;  _btn.fg_g = _fg_g;
    _btn.fg_b = _fg_b;  _btn.fg_a = _fg_a;

    return;
}

// btn_is_toggled
template <unsigned _F,
          typename _I>
bool
btn_is_toggled(
    const button<_F, _I>& _btn
)
{
    static_assert(has_bf(_F, bf_toggle),
                  "btn_is_toggled requires bf_toggle");

    return _btn.toggled;
}




// ===============================================================================
//  6  LEGACY FREE FUNCTIONS
// ===============================================================================
//   These btn_-prefixed functions are retained for backward
// compatibility.  New code should prefer the ADL-dispatched
// equivalents in component_common.hpp:
//
//     btn_enable(btn)   ->  enable(btn)
//     btn_disable(btn)  ->  disable(btn)
//                          show(btn)      (no legacy equivalent existed)
//                          hide(btn)      (no legacy equivalent existed)

// btn_enable
template <unsigned _F,
          typename _I>
void
btn_enable(
    button<_F, _I>& _btn
)
{
    enable(_btn);

    return;
}

// btn_disable
template <unsigned _F,
          typename _I>
void
btn_disable(
    button<_F, _I>& _btn
)
{
    disable(_btn);

    return;
}




// ===============================================================================
//  7  TRAITS
// ===============================================================================
//   Shared detectors (has_enabled_member, has_label_member,
// has_focusable_flag) delegate to component_traits.  Button-
// specific detectors remain here.

namespace button_traits 
{
NS_INTERNAL
    // -- button-specific detectors ----------------------------------------

    template <typename, typename = void>
    struct has_on_click_member : std::false_type {};
    template <typename _Type>
    struct has_on_click_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_click)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_pressed_member : std::false_type {};
    template <typename _Type>
    struct has_pressed_member<_Type, std::void_t<
        decltype(std::declval<_Type>().pressed)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_icon_member : std::false_type {};
    template <typename _Type>
    struct has_icon_member<_Type, std::void_t<
        decltype(std::declval<_Type>().icon)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_tooltip_member : std::false_type {};
    template <typename _Type>
    struct has_tooltip_member<_Type, std::void_t<
        decltype(std::declval<_Type>().tooltip)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_shape_member : std::false_type {};
    template <typename _Type>
    struct has_shape_member<_Type, std::void_t<
        decltype(std::declval<_Type>().shape)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_toggled_member : std::false_type {};
    template <typename _Type>
    struct has_toggled_member<_Type, std::void_t<
        decltype(std::declval<_Type>().toggled)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_badge_count_member : std::false_type {};
    template <typename _Type>
    struct has_badge_count_member<_Type, std::void_t<
        decltype(std::declval<_Type>().badge_count)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_shortcut_label_member : std::false_type {};
    template <typename _Type>
    struct has_shortcut_label_member<_Type, std::void_t<
        decltype(std::declval<_Type>().shortcut_label)
    >> : std::true_type {};

NS_END  // internal

// -- shared aliases (delegate to component_traits) ------------------------
template <typename _Type>
inline constexpr bool has_label_v =
    has_label_v<_Type>;
template <typename _Type>
inline constexpr bool has_enabled_v =
    has_enabled_v<_Type>;

// -- button-specific aliases ----------------------------------------------
template <typename _Type>
inline constexpr bool has_on_click_v =
    internal::has_on_click_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_pressed_v =
    internal::has_pressed_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_icon_v =
    internal::has_icon_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_tooltip_v =
    internal::has_tooltip_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_shape_v =
    internal::has_shape_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_toggled_v =
    internal::has_toggled_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_badge_count_v =
    internal::has_badge_count_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_shortcut_label_v =
    internal::has_shortcut_label_member<_Type>::value;

// -- composite traits -----------------------------------------------------

// is_button
//   trait: has label + enabled + on_click + pressed + focusable.
template <typename _Type>
struct is_button : std::conjunction<
    ::uxoxo::component::internal::has_label_member<_Type>,
    ::uxoxo::component::internal::has_enabled_member<_Type>,
    internal::has_on_click_member<_Type>,
    internal::has_pressed_member<_Type>,
    ::uxoxo::component::internal::has_focusable_flag<_Type>
    >
{};

template <typename _Type>
inline constexpr bool is_button_v =
    is_button<_Type>::value;

// is_toggle_button
template <typename _Type>
struct is_toggle_button : std::conjunction<
    is_button<_Type>,
    internal::has_toggled_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_toggle_button_v =
    is_toggle_button<_Type>::value;

// is_icon_button
template <typename _Type>
struct is_icon_button : std::conjunction<
    is_button<_Type>,
    internal::has_icon_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_icon_button_v =
    is_icon_button<_Type>::value;

}   // namespace button_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_BUTTON_
