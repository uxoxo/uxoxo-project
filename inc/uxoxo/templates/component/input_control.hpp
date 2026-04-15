/*******************************************************************************
* uxoxo [component]                                           input_control.hpp
*
* Generic input control:
*   A framework-agnostic, pure-data input control template.  This is the
* abstract base for any UI element that accepts user input — text fields,
* search bars, command lines, spinners, sliders, and so on.  The template
* prescribes no rendering, no observer coupling, and no concrete input
* modality.  All mutation is via free functions; the struct stays a plain
* data aggregate.
*
*   input_control captures the minimal invariants shared by ALL input
* elements:
*     - An enabled/disabled gate
*     - A read-only gate
*     - A focusable flag (compile-time)
*     - A value of arbitrary type _Value
*     - An "accepted" callback type (the thing that happens on commit)
*
*   Concrete controls (text_input, numeric_input, dropdown, etc.)
* derive from this and add domain-specific state.  The trait system
* in input_control_traits detects conforming types structurally, so
* no inheritance is actually required — it exists for convenience
* and documentation, not dispatch.
*
*   Feature composition follows the same EBO-mixin bitfield pattern
* used by tree_node, list_entry, and text_input.
*
*   Shared operations (enable, disable, set_value, clear, undo,
* commit, set_read_only) are provided by the ADL-dispatched free
* functions in component_common.hpp.  Legacy ic_-prefixed wrappers
* are retained for backward compatibility.
*
* Contents:
*   1.  Feature flags (input_control_feat)
*   2.  input_control struct
*   3.  Legacy free functions (ic_-prefixed, thin wrappers)
*   4.  Traits (SFINAE detection, delegates to component_traits)
*
*
* path:      /inc/uxoxo/templates/component/input/input_control.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.09
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_INPUT_CONTROL_
#define  UXOXO_COMPONENT_INPUT_CONTROL_ 1

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
//  1.  INPUT CONTROL FEATURE FLAGS
// ===============================================================================
//   Start at bit 16 to avoid colliding with view_feat (0–7)
// and text_input_feat (8–15).

enum input_control_feat : unsigned
{
    icf_none       = 0,
    icf_labeled    = 1u << 16,     // control has an associated label
    icf_clearable  = 1u << 17,     // control can be cleared to default
    icf_undoable   = 1u << 18,     // single-level undo of last commit

    icf_all        = icf_labeled | icf_clearable | icf_undoable
};

constexpr unsigned operator|(input_control_feat a,
                             input_control_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_icf(unsigned           f,
                       input_control_feat bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}

// ===============================================================================
//  2.  INPUT CONTROL
// ===============================================================================
//   _Value     the type of the control's payload (std::string for text,
//              int for a spinner, bool for a checkbox, etc.)
//   _Feat      bitwise OR of input_control_feat flags
//
//   The commit callback type is std::function<void(const _Value&)>.
// It is invoked by commit() when the user confirms the current
// value.  The callback is optional (empty by default).
//
//   EBO mixins are sourced from the shared component_mixin
// namespace.  This eliminates the input_mixin duplication — the
// same mixin definitions are shared with output_control and any
// future component.

template <typename _Value,
          unsigned _Feat = icf_none>
struct input_control
    : component_mixin::label_data     <has_icf(_Feat, icf_labeled)>
    , component_mixin::clearable_data <has_icf(_Feat, icf_clearable), _Value>
    , component_mixin::undo_data      <has_icf(_Feat, icf_undoable),  _Value>
{
    using value_type    = _Value;
    using commit_fn     = std::function<void(const _Value&)>;

    static constexpr unsigned features    = _Feat;
    static constexpr bool has_label       = has_icf(_Feat, icf_labeled);
    static constexpr bool is_clearable    = has_icf(_Feat, icf_clearable);
    static constexpr bool is_undoable     = has_icf(_Feat, icf_undoable);
    static constexpr bool focusable       = true;

    // -- core state ---------------------------------------------------
    _Value  value    {};
    bool    enabled  = true;
    bool    read_only = false;

    // -- commit callback ----------------------------------------------
    commit_fn  on_commit;

    // -- construction -------------------------------------------------
    input_control() = default;

    explicit input_control(
            _Value initial
        )
            : value(std::move(initial))
        {}

    // -- queries ------------------------------------------------------
    [[nodiscard]] bool
    is_enabled() const noexcept
    {
        return enabled && !read_only;
    }
};


/*****************************************************************************/

// ===============================================================================
//  3.  LEGACY FREE FUNCTIONS
// ===============================================================================
//   These ic_-prefixed functions are retained for backward
// compatibility.  New code should prefer the ADL-dispatched
// equivalents in component_common.hpp:
//
//     ic_enable(ic)       →  enable(ic)
//     ic_disable(ic)      →  disable(ic)
//     ic_set_read_only()  →  set_read_only(ic, ro)
//     ic_set_value()      →  set_value(ic, val)
//     ic_commit()         →  commit(ic)
//     ic_clear()          →  clear(ic)
//     ic_undo()           →  undo(ic)

// ic_enable
template <typename _V, unsigned _F>
void ic_enable(input_control<_V, _F>& _ic)
{
    enable(_ic);

    return;
}

// ic_disable
template <typename _V, unsigned _F>
void ic_disable(input_control<_V, _F>& _ic)
{
    disable(_ic);

    return;
}

// ic_set_read_only
template <typename _V, unsigned _F>
void ic_set_read_only(input_control<_V, _F>& _ic,
                      bool                   _ro)
{
    set_read_only(_ic, _ro);

    return;
}

// ic_set_value
template <typename _V, unsigned _F>
void ic_set_value(input_control<_V, _F>& _ic,
                  _V                     _val)
{
    set_value(_ic, std::move(_val));

    return;
}

// ic_commit
template <typename _V, unsigned _F>
bool ic_commit(input_control<_V, _F>& _ic)
{
    return commit(_ic);
}

// ic_clear
template <typename _V, unsigned _F>
void ic_clear(input_control<_V, _F>& _ic)
{
    static_assert(has_icf(_F, icf_clearable),
                  "requires icf_clearable");

    clear(_ic);

    return;
}

// ic_undo
template <typename _V, unsigned _F>
bool ic_undo(input_control<_V, _F>& _ic)
{
    static_assert(has_icf(_F, icf_undoable),
                  "requires icf_undoable");

    return undo(_ic);
}


/*****************************************************************************/

// ===============================================================================
//  4.  TRAITS
// ===============================================================================
//   Shared detectors delegate to component_traits.  Only the
// input-specific composite traits (is_input_control, etc.) are
// defined here.  The _v aliases are kept for backward
// compatibility but now forward to the shared versions.

namespace input_control_traits {

// -- value aliases (delegate to component_traits) -------------------------
template <typename _Type>
inline constexpr bool has_value_v =
    component_traits::has_value_v<_Type>;
template <typename _Type>
inline constexpr bool has_enabled_v =
    component_traits::has_enabled_v<_Type>;
template <typename _Type>
inline constexpr bool has_read_only_v =
    component_traits::has_read_only_v<_Type>;
template <typename _Type>
inline constexpr bool has_on_commit_v =
    component_traits::has_on_commit_v<_Type>;
template <typename _Type>
inline constexpr bool has_label_v =
    component_traits::has_label_v<_Type>;
template <typename _Type>
inline constexpr bool has_default_value_v =
    component_traits::has_default_value_v<_Type>;
template <typename _Type>
inline constexpr bool has_previous_value_v =
    component_traits::has_previous_value_v<_Type>;

// -- input-specific composite traits --------------------------------------

// is_input_control
//   trait: has value + enabled + read_only + focusable(true).
template <typename _Type>
struct is_input_control : component_traits::is_input_like<_Type>
{};

template <typename _Type>
inline constexpr bool is_input_control_v =
    is_input_control<_Type>::value;

// is_labeled_input
template <typename _Type>
struct is_labeled_input : std::conjunction<
    is_input_control<_Type>,
    component_traits::detail::has_label_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_labeled_input_v =
    is_labeled_input<_Type>::value;

// is_clearable_input
template <typename _Type>
struct is_clearable_input : std::conjunction<
    is_input_control<_Type>,
    component_traits::detail::has_default_value_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_clearable_input_v =
    is_clearable_input<_Type>::value;

// is_undoable_input
template <typename _Type>
struct is_undoable_input : std::conjunction<
    is_input_control<_Type>,
    component_traits::detail::has_previous_value_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_undoable_input_v =
    is_undoable_input<_Type>::value;


}   // namespace input_control_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_INPUT_CONTROL_
