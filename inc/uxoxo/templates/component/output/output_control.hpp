/*******************************************************************************
* uxoxo [component]                                         output_control.hpp
*
* Generic output control:
*   A framework-agnostic, pure-data output control template.  This is the
* read-only counterpart to input_control.  Where input_control accepts
* user input, output_control displays computed or received values — log
* windows, status displays, read-only fields, terminal output panes.
*
*   The template prescribes no rendering.  All mutation is via free
* functions; the struct stays a plain data aggregate.
*
*   output_control captures the invariants shared by read-only display
* elements:
*     - A value of arbitrary type _Value
*     - Visible / enabled gates
*     - An optional label (compile-time)
*     - An on_change callback (notified when the value is set externally)
*     - Scrollable flag (for multi-line / streaming outputs)
*
*   Feature composition follows the same EBO-mixin bitfield pattern used
* throughout the uxoxo component layer.
*
* Contents:
*   1.  Feature flags (output_control_feat)
*   2.  EBO mixins
*   3.  output_control struct
*   4.  Free functions
*   5.  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/output/output_control.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_OUTPUT_CONTROL_
#define  UXOXO_COMPONENT_OUTPUT_CONTROL_ 1

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
#include "../view_common.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  OUTPUT CONTROL FEATURE FLAGS
// ===============================================================================
//   Start at bit 16 to avoid colliding with view_feat (0–7)
// and text_output_feat (8–15).

enum output_control_feat : unsigned
{
    ocf_none       = 0,
    ocf_labeled    = 1u << 16,     // control has an associated label
    ocf_clearable  = 1u << 17,     // control can be cleared to default
    ocf_copyable   = 1u << 18,     // value can be copied to clipboard

    ocf_all        = ocf_labeled | ocf_clearable | ocf_copyable
};

constexpr unsigned operator|(output_control_feat _a,
                             output_control_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool has_ocf(unsigned            _f,
                       output_control_feat  _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}

// ===============================================================================
//  2.  EBO MIXINS
// ===============================================================================

namespace output_mixin {

    // -- label --------------------------------------------------------
    template <bool _Enable>
    struct label_data
    {};

    template <>
    struct label_data<true>
    {
        std::string label;
    };

    // -- clearable ----------------------------------------------------
    template <bool _Enable, typename _Value>
    struct clearable_data
    {};

    template <typename _Value>
    struct clearable_data<true, _Value>
    {
        _Value default_value {};
    };

    // -- copyable -----------------------------------------------------
    template <bool _Enable>
    struct copyable_data
    {};

    template <>
    struct copyable_data<true>
    {
        bool copy_requested = false;
    };

}   // namespace output_mixin

// ===============================================================================
//  3.  OUTPUT CONTROL
// ===============================================================================
//   _Value     the type of the control's displayed payload.
//   _Feat      bitwise OR of output_control_feat flags.
//
//   The on_change callback is invoked whenever oc_set_value()
// modifies the value.  The callback is optional.

template <typename _Value,
          unsigned _Feat = ocf_none>
struct output_control
    : output_mixin::label_data     <has_ocf(_Feat, ocf_labeled)>
    , output_mixin::clearable_data <has_ocf(_Feat, ocf_clearable), _Value>
    , output_mixin::copyable_data  <has_ocf(_Feat, ocf_copyable)>
{
    using value_type   = _Value;
    using change_fn    = std::function<void(const _Value&)>;

    static constexpr unsigned features    = _Feat;
    static constexpr bool has_label       = has_ocf(_Feat, ocf_labeled);
    static constexpr bool is_clearable    = has_ocf(_Feat, ocf_clearable);
    static constexpr bool is_copyable     = has_ocf(_Feat, ocf_copyable);
    static constexpr bool focusable       = false;
    static constexpr bool scrollable      = false;

    // -- core state ---------------------------------------------------
    _Value  value    {};
    bool    enabled  = true;
    bool    visible  = true;

    // -- change callback ----------------------------------------------
    change_fn  on_change;

    // -- construction -------------------------------------------------
    output_control() = default;

    explicit output_control(
            _Value _initial
        )
            : value(std::move(_initial))
        {}
};

// ===============================================================================
//  4.  FREE FUNCTIONS
// ===============================================================================

// oc_set_value
//   replaces the control's value and notifies the change callback.
template <typename _V, unsigned _F>
void oc_set_value(output_control<_V, _F>& _oc,
                  _V                      _val)
{
    _oc.value = std::move(_val);

    if (_oc.on_change)
    {
        _oc.on_change(_oc.value);
    }

    return;
}

// oc_clear
//   resets the control to its default value (requires ocf_clearable).
template <typename _V, unsigned _F>
void oc_clear(output_control<_V, _F>& _oc)
{
    static_assert(has_ocf(_F, ocf_clearable),
                  "requires ocf_clearable");

    _oc.value = _oc.default_value;

    if (_oc.on_change)
    {
        _oc.on_change(_oc.value);
    }

    return;
}

// oc_request_copy
//   signals the renderer to copy the value to the clipboard
// (requires ocf_copyable).
template <typename _V, unsigned _F>
void oc_request_copy(output_control<_V, _F>& _oc)
{
    static_assert(has_ocf(_F, ocf_copyable),
                  "requires ocf_copyable");

    _oc.copy_requested = true;

    return;
}

// oc_enable / oc_disable
template <typename _V, unsigned _F>
void oc_enable(output_control<_V, _F>& _oc)
{
    _oc.enabled = true;

    return;
}

template <typename _V, unsigned _F>
void oc_disable(output_control<_V, _F>& _oc)
{
    _oc.enabled = false;

    return;
}

// oc_show / oc_hide
template <typename _V, unsigned _F>
void oc_show(output_control<_V, _F>& _oc)
{
    _oc.visible = true;

    return;
}

template <typename _V, unsigned _F>
void oc_hide(output_control<_V, _F>& _oc)
{
    _oc.visible = false;

    return;
}

// ===============================================================================
//  5.  TRAITS
// ===============================================================================

namespace output_control_traits {
NS_INTERNAL

    template <typename, typename = void>
    struct has_value_member : std::false_type {};
    template <typename _Type>
    struct has_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().value)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_enabled_member : std::false_type {};
    template <typename _Type>
    struct has_enabled_member<_Type, std::void_t<
        decltype(std::declval<_Type>().enabled)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_visible_member : std::false_type {};
    template <typename _Type>
    struct has_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type>().visible)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_on_change_member : std::false_type {};
    template <typename _Type>
    struct has_on_change_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_change)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_label_member : std::false_type {};
    template <typename _Type>
    struct has_label_member<_Type, std::void_t<
        decltype(std::declval<_Type>().label)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_default_value_member : std::false_type {};
    template <typename _Type>
    struct has_default_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().default_value)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_copy_requested_member : std::false_type {};
    template <typename _Type>
    struct has_copy_requested_member<_Type, std::void_t<
        decltype(std::declval<_Type>().copy_requested)
    >> : std::true_type {};

}   // NS_INTERNAL

template <typename _Type>
inline constexpr bool has_value_v =
    internal::has_value_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_enabled_v =
    internal::has_enabled_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_visible_v =
    internal::has_visible_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_on_change_v =
    internal::has_on_change_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_label_v =
    internal::has_label_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_default_value_v =
    internal::has_default_value_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_copy_requested_v =
    internal::has_copy_requested_member<_Type>::value;

// is_output_control
//   type trait: has value + enabled + visible + on_change.
template <typename _Type>
struct is_output_control : std::conjunction<
    internal::has_value_member<_Type>,
    internal::has_enabled_member<_Type>,
    internal::has_visible_member<_Type>,
    internal::has_on_change_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_output_control_v =
    is_output_control<_Type>::value;

// is_labeled_output
template <typename _Type>
struct is_labeled_output : std::conjunction<
    is_output_control<_Type>,
    internal::has_label_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_labeled_output_v =
    is_labeled_output<_Type>::value;

// is_clearable_output
template <typename _Type>
struct is_clearable_output : std::conjunction<
    is_output_control<_Type>,
    internal::has_default_value_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_clearable_output_v =
    is_clearable_output<_Type>::value;

// is_copyable_output
template <typename _Type>
struct is_copyable_output : std::conjunction<
    is_output_control<_Type>,
    internal::has_copy_requested_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_copyable_output_v =
    is_copyable_output<_Type>::value;


NS_END  // namespace output_control_traits
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_OUTPUT_CONTROL_