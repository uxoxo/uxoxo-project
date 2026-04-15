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
* Contents:
*   1.  Feature flags (input_control_feat)
*   2.  EBO mixins
*   3.  input_control struct
*   4.  Free functions (enable, disable, commit)
*   5.  Traits (SFINAE detection)
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

constexpr bool has_icf(unsigned       f,
                       input_control_feat bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}

// ===============================================================================
//  2.  EBO MIXINS
// ===============================================================================

namespace input_mixin {

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

    // -- undoable -----------------------------------------------------
    template <bool _Enable, typename _Value>
    struct undo_data
    {};

    template <typename _Value>
    struct undo_data<true, _Value>
    {
        _Value   previous_value {};
        bool     has_previous = false;
    };

}   // namespace input_mixin

// ===============================================================================
//  3.  INPUT CONTROL
// ===============================================================================
//   _Value     the type of the control's payload (std::string for text,
//              int for a spinner, bool for a checkbox, etc.)
//   _Feat      bitwise OR of input_control_feat flags
//
//   The commit callback type is std::function<void(const _Value&)>.
// It is invoked by ic_commit() when the user confirms the current
// value.  The callback is optional (empty by default).

template <typename _Value,
          unsigned _Feat = icf_none>
struct input_control
    : input_mixin::label_data     <has_icf(_Feat, icf_labeled)>
    , input_mixin::clearable_data <has_icf(_Feat, icf_clearable), _Value>
    , input_mixin::undo_data      <has_icf(_Feat, icf_undoable),  _Value>
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

// ===============================================================================
//  4.  FREE FUNCTIONS
// ===============================================================================

// ic_enable
template <typename _V, unsigned _F>
void ic_enable(input_control<_V, _F>& ic)
{
    ic.enabled = true;

    return;
}

// ic_disable
template <typename _V, unsigned _F>
void ic_disable(input_control<_V, _F>& ic)
{
    ic.enabled = false;

    return;
}

// ic_set_read_only
template <typename _V, unsigned _F>
void ic_set_read_only(input_control<_V, _F>& ic,
                      bool                   ro)
{
    ic.read_only = ro;

    return;
}

// ic_set_value
//   replaces the control's value.  Stores the previous value
// if the control is undoable.
template <typename _V, unsigned _F>
void ic_set_value(input_control<_V, _F>& ic,
                  _V                     val)
{
    if constexpr (has_icf(_F, icf_undoable))
    {
        ic.previous_value = ic.value;
        ic.has_previous   = true;
    }

    ic.value = std::move(val);

    return;
}

// ic_commit
//   invokes the commit callback with the current value.
// Returns true if the callback was invoked.
template <typename _V, unsigned _F>
bool ic_commit(input_control<_V, _F>& ic)
{
    if (!ic.is_enabled())
    {
        return false;
    }

    if (ic.on_commit)
    {
        ic.on_commit(ic.value);
    }

    return true;
}

// ic_clear
//   resets the control to its default value (requires icf_clearable).
template <typename _V, unsigned _F>
void ic_clear(input_control<_V, _F>& ic)
{
    static_assert(has_icf(_F, icf_clearable),
                  "requires icf_clearable");

    if constexpr (has_icf(_F, icf_undoable))
    {
        ic.previous_value = ic.value;
        ic.has_previous   = true;
    }

    ic.value = ic.default_value;

    return;
}

// ic_undo
//   restores the previous value (requires icf_undoable).
// Returns true if undo was performed.
template <typename _V, unsigned _F>
bool ic_undo(input_control<_V, _F>& ic)
{
    static_assert(has_icf(_F, icf_undoable),
                  "requires icf_undoable");

    if (!ic.has_previous)
    {
        return false;
    }

    ic.value        = ic.previous_value;
    ic.has_previous = false;

    return true;
}

// ===============================================================================
//  5.  TRAITS
// ===============================================================================

namespace input_control_traits {
namespace detail {

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
    struct has_read_only_member : std::false_type {};
    template <typename _Type>
    struct has_read_only_member<_Type, std::void_t<
        decltype(std::declval<_Type>().read_only)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_focusable_flag : std::false_type {};
    template <typename _Type>
    struct has_focusable_flag<_Type,
        std::enable_if_t<_Type::focusable>
    > : std::true_type {};

    template <typename, typename = void>
    struct has_on_commit_member : std::false_type {};
    template <typename _Type>
    struct has_on_commit_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_commit)
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
    struct has_previous_value_member : std::false_type {};
    template <typename _Type>
    struct has_previous_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().previous_value)
    >> : std::true_type {};

}   // namespace detail

template <typename _Type>
inline constexpr bool has_value_v =
    detail::has_value_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_enabled_v =
    detail::has_enabled_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_read_only_v =
    detail::has_read_only_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_on_commit_v =
    detail::has_on_commit_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_label_v =
    detail::has_label_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_default_value_v =
    detail::has_default_value_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_previous_value_v =
    detail::has_previous_value_member<_Type>::value;

// is_input_control
//   type trait: has value + enabled + read_only + focusable.
template <typename _Type>
struct is_input_control : std::conjunction<
    detail::has_value_member<_Type>,
    detail::has_enabled_member<_Type>,
    detail::has_read_only_member<_Type>,
    detail::has_focusable_flag<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_input_control_v =
    is_input_control<_Type>::value;

// is_labeled_input
template <typename _Type>
struct is_labeled_input : std::conjunction<
    is_input_control<_Type>,
    detail::has_label_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_labeled_input_v =
    is_labeled_input<_Type>::value;

// is_clearable_input
template <typename _Type>
struct is_clearable_input : std::conjunction<
    is_input_control<_Type>,
    detail::has_default_value_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_clearable_input_v =
    is_clearable_input<_Type>::value;

// is_undoable_input
template <typename _Type>
struct is_undoable_input : std::conjunction<
    is_input_control<_Type>,
    detail::has_previous_value_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_undoable_input_v =
    is_undoable_input<_Type>::value;


NS_END  // namespace input_control_traits
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_INPUT_CONTROL_