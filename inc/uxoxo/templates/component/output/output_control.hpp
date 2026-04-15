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
*   Shared operations (enable, disable, show, hide, set_value, clear,
* request_copy) are provided by the ADL-dispatched free functions in
* component_common.hpp.  Legacy oc_-prefixed wrappers are retained
* for backward compatibility.
*
* Contents:
*   1.  Feature flags (output_control_feat)
*   2.  output_control struct
*   3.  Legacy free functions (oc_-prefixed, thin wrappers)
*   4.  Traits (SFINAE detection, delegates to component_traits)
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
#include "../../component_mixin.hpp"
#include "../../component_traits.hpp"
#include "../../component_common.hpp"


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
//  2.  OUTPUT CONTROL
// ===============================================================================
//   _Value     the type of the control's displayed payload.
//   _Feat      bitwise OR of output_control_feat flags.
//
//   The on_change callback is invoked whenever set_value()
// modifies the value.  The callback is optional.
//
//   EBO mixins are sourced from the shared component_mixin
// namespace.  This eliminates the output_mixin duplication — the
// same mixin definitions are shared with input_control and any
// future component.

template <typename _Value,
          unsigned _Feat = ocf_none>
struct output_control
    : component_mixin::label_data     <has_ocf(_Feat, ocf_labeled)>
    , component_mixin::clearable_data <has_ocf(_Feat, ocf_clearable), _Value>
    , component_mixin::copyable_data  <has_ocf(_Feat, ocf_copyable)>
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


/*****************************************************************************/

// ===============================================================================
//  3.  LEGACY FREE FUNCTIONS
// ===============================================================================
//   These oc_-prefixed functions are retained for backward
// compatibility.  New code should prefer the ADL-dispatched
// equivalents in component_common.hpp:
//
//     oc_set_value()     →  set_value(oc, val)
//     oc_clear()         →  clear(oc)
//     oc_request_copy()  →  request_copy(oc)
//     oc_enable()        →  enable(oc)
//     oc_disable()       →  disable(oc)
//     oc_show()          →  show(oc)
//     oc_hide()          →  hide(oc)

// oc_set_value
template <typename _V, unsigned _F>
void oc_set_value(output_control<_V, _F>& _oc,
                  _V                      _val)
{
    set_value(_oc, std::move(_val));

    return;
}

// oc_clear
template <typename _V, unsigned _F>
void oc_clear(output_control<_V, _F>& _oc)
{
    static_assert(has_ocf(_F, ocf_clearable),
                  "requires ocf_clearable");

    clear(_oc);

    return;
}

// oc_request_copy
template <typename _V, unsigned _F>
void oc_request_copy(output_control<_V, _F>& _oc)
{
    static_assert(has_ocf(_F, ocf_copyable),
                  "requires ocf_copyable");

    request_copy(_oc);

    return;
}

// oc_enable
template <typename _V, unsigned _F>
void oc_enable(output_control<_V, _F>& _oc)
{
    enable(_oc);

    return;
}

// oc_disable
template <typename _V, unsigned _F>
void oc_disable(output_control<_V, _F>& _oc)
{
    disable(_oc);

    return;
}

// oc_show
template <typename _V, unsigned _F>
void oc_show(output_control<_V, _F>& _oc)
{
    show(_oc);

    return;
}

// oc_hide
template <typename _V, unsigned _F>
void oc_hide(output_control<_V, _F>& _oc)
{
    hide(_oc);

    return;
}


/*****************************************************************************/

// ===============================================================================
//  4.  TRAITS
// ===============================================================================
//   Shared detectors delegate to component_traits.  Only the
// output-specific composite traits (is_output_control, etc.) are
// defined here.  The _v aliases are kept for backward
// compatibility but now forward to the shared versions.

namespace output_control_traits {

// -- value aliases (delegate to component_traits) -------------------------
template <typename _Type>
inline constexpr bool has_value_v =
    component_traits::has_value_v<_Type>;
template <typename _Type>
inline constexpr bool has_enabled_v =
    component_traits::has_enabled_v<_Type>;
template <typename _Type>
inline constexpr bool has_visible_v =
    component_traits::has_visible_v<_Type>;
template <typename _Type>
inline constexpr bool has_on_change_v =
    component_traits::has_on_change_v<_Type>;
template <typename _Type>
inline constexpr bool has_label_v =
    component_traits::has_label_v<_Type>;
template <typename _Type>
inline constexpr bool has_default_value_v =
    component_traits::has_default_value_v<_Type>;
template <typename _Type>
inline constexpr bool has_copy_requested_v =
    component_traits::has_copy_requested_v<_Type>;

// -- output-specific composite traits -------------------------------------

// is_output_control
//   trait: has value + enabled + visible + on_change.
template <typename _Type>
struct is_output_control : component_traits::is_output_like<_Type>
{};

template <typename _Type>
inline constexpr bool is_output_control_v =
    is_output_control<_Type>::value;

// is_labeled_output
template <typename _Type>
struct is_labeled_output : std::conjunction<
    is_output_control<_Type>,
    component_traits::detail::has_label_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_labeled_output_v =
    is_labeled_output<_Type>::value;

// is_clearable_output
template <typename _Type>
struct is_clearable_output : std::conjunction<
    is_output_control<_Type>,
    component_traits::detail::has_default_value_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_clearable_output_v =
    is_clearable_output<_Type>::value;

// is_copyable_output
template <typename _Type>
struct is_copyable_output : std::conjunction<
    is_output_control<_Type>,
    component_traits::detail::has_copy_requested_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_copyable_output_v =
    is_copyable_output<_Type>::value;


}   // namespace output_control_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_OUTPUT_CONTROL_
