/*******************************************************************************
* uxoxo [component]                                        component_common.hpp
*
* Shared component operations:
*   ADL-dispatched free functions that work on any structurally conforming
* component.  These replace the per-component prefixed functions for
* operations that are semantically identical across component types:
*
*     ic_enable(my_input)   →   enable(my_input)
*     oc_enable(my_output)  →   enable(my_output)
*     ic_disable(my_input)  →   disable(my_input)
*     oc_show(my_output)    →   show(my_output)
*     as_show(my_suggest)   →   show(my_suggest)
*
*   Each function is constrained via SFINAE so it only matches types
* that have the required members.  No base class, no tags — just
* structural conformance.  This is the same approach the STL uses
* with std::begin / std::end / std::swap.
*
*   Component-specific operations (ti_insert_char, to_append,
* as_update, etc.) remain in their respective headers with their
* existing prefixes.  Only truly shared operations are centralized
* here.
*
*   Compositional forwarding:
*   The `sub_component` accessor template enables composite components
* (like dev_console) to expose their sub-components for direct use
* with these shared functions, without writing per-operation
* forwarding wrappers.
*
* Contents:
*   1  Enable / disable
*   2  Show / hide / toggle visibility
*   3  Activate / deactivate
*   4  Read-only
*   5  Value access
*   6  Clear (requires clearable mixin)
*   7  Undo (requires undo mixin)
*   8  Copy request (requires copyable mixin)
*   9  Commit (input-like components)
*   10 Compositional forwarding utilities
*
*
* path:      /inc/uxoxo/templates/component/component_common.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.15
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_COMMON_
#define  UXOXO_COMPONENT_COMMON_ 1

// std
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "./component_traits.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  ENABLE / DISABLE
// ===============================================================================
//   Constrained to types with an `enabled` member.

// enable
//   function: sets the component's enabled flag to true.
template <typename _T,
          std::enable_if_t<
              component_traits::has_enabled_v<_T>,
              int> = 0>
void enable(_T& _c)
{
    _c.enabled = true;

    return;
}

// disable
//   function: sets the component's enabled flag to false.
template <typename _T,
          std::enable_if_t<
              component_traits::has_enabled_v<_T>,
              int> = 0>
void disable(_T& _c)
{
    _c.enabled = false;

    return;
}

// is_enabled (non-member query)
//   function: returns the component's enabled state.
// For input-like components with read_only, returns
// enabled && !read_only.
template <typename _T,
          std::enable_if_t<
              ( component_traits::has_enabled_v<_T> &&
                component_traits::has_read_only_v<_T> ),
              int> = 0>
[[nodiscard]] bool
is_enabled(const _T& _c) noexcept
{
    return _c.enabled && !_c.read_only;
}

// is_enabled (no read_only member)
template <typename _T,
          std::enable_if_t<
              ( component_traits::has_enabled_v<_T> &&
                !component_traits::has_read_only_v<_T> ),
              int> = 0>
[[nodiscard]] bool
is_enabled(const _T& _c) noexcept
{
    return _c.enabled;
}


/*****************************************************************************/

// ===============================================================================
//  2  SHOW / HIDE / TOGGLE VISIBILITY
// ===============================================================================
//   Constrained to types with a `visible` member.

// show
template <typename _T,
          std::enable_if_t<
              component_traits::has_visible_v<_T>,
              int> = 0>
void show(_T& _c)
{
    _c.visible = true;

    return;
}

// hide
template <typename _T,
          std::enable_if_t<
              component_traits::has_visible_v<_T>,
              int> = 0>
void hide(_T& _c)
{
    _c.visible = false;

    return;
}

// toggle_visible
template <typename _T,
          std::enable_if_t<
              component_traits::has_visible_v<_T>,
              int> = 0>
void toggle_visible(_T& _c)
{
    _c.visible = !_c.visible;

    return;
}

// is_visible (non-member query)
template <typename _T,
          std::enable_if_t<
              component_traits::has_visible_v<_T>,
              int> = 0>
[[nodiscard]] bool
is_visible(const _T& _c) noexcept
{
    return _c.visible;
}


/*****************************************************************************/

// ===============================================================================
//  3  ACTIVATE / DEACTIVATE
// ===============================================================================
//   Constrained to types with an `active` member (autosuggest,
// autocomplete, etc.).

// activate
template <typename _T,
          std::enable_if_t<
              component_traits::has_active_v<_T>,
              int> = 0>
void activate(_T& _c)
{
    _c.active = true;

    return;
}

// deactivate
//   Also hides the component if it has a visible member.
template <typename _T,
          std::enable_if_t<
              component_traits::has_active_v<_T>,
              int> = 0>
void deactivate(_T& _c)
{
    _c.active = false;

    if constexpr (component_traits::has_visible_v<_T>)
    {
        _c.visible = false;
    }

    return;
}


/*****************************************************************************/

// ===============================================================================
//  4  READ-ONLY
// ===============================================================================
//   Constrained to types with a `read_only` member.

// set_read_only
template <typename _T,
          std::enable_if_t<
              component_traits::has_read_only_v<_T>,
              int> = 0>
void set_read_only(_T& _c,
                   bool _ro)
{
    _c.read_only = _ro;

    return;
}


/*****************************************************************************/

// ===============================================================================
//  5  VALUE ACCESS
// ===============================================================================
//   Constrained to types with a `value` member.

// set_value
//   function: replaces the component's value.  If the component is
// undoable (has previous_value + has_previous), stores the old
// value before replacing.  If the component has an on_change
// callback, invokes it after replacement.
template <typename _T,
          std::enable_if_t<
              component_traits::has_value_v<_T>,
              int> = 0>
void set_value(_T& _c,
               typename std::remove_reference_t<
                   decltype(std::declval<_T&>().value)> _val)
{
    // store undo state if available
    if constexpr (component_traits::is_undoable_v<_T>)
    {
        _c.previous_value = _c.value;
        _c.has_previous   = true;
    }

    _c.value = std::move(_val);

    // notify on_change if present
    if constexpr (component_traits::has_on_change_v<_T>)
    {
        if (_c.on_change)
        {
            _c.on_change(_c.value);
        }
    }

    return;
}

// get_value
//   function: returns a const reference to the component's value.
template <typename _T,
          std::enable_if_t<
              component_traits::has_value_v<_T>,
              int> = 0>
[[nodiscard]] const auto&
get_value(const _T& _c) noexcept
{
    return _c.value;
}


/*****************************************************************************/

// ===============================================================================
//  6  CLEAR  (requires clearable mixin)
// ===============================================================================

// clear
//   function: resets the component's value to its default_value.
// If undoable, stores the current value before clearing.
// If on_change is present, notifies after clearing.
template <typename _T,
          std::enable_if_t<
              component_traits::is_clearable_v<_T>,
              int> = 0>
void clear(_T& _c)
{
    if constexpr (component_traits::is_undoable_v<_T>)
    {
        _c.previous_value = _c.value;
        _c.has_previous   = true;
    }

    _c.value = _c.default_value;

    if constexpr (component_traits::has_on_change_v<_T>)
    {
        if (_c.on_change)
        {
            _c.on_change(_c.value);
        }
    }

    return;
}


/*****************************************************************************/

// ===============================================================================
//  7  UNDO  (requires undo mixin)
// ===============================================================================

// undo
//   function: restores the component's previous value.
// Returns true if undo was performed, false if no previous
// value was stored.
template <typename _T,
          std::enable_if_t<
              component_traits::is_undoable_v<_T>,
              int> = 0>
bool undo(_T& _c)
{
    if (!_c.has_previous)
    {
        return false;
    }

    _c.value        = _c.previous_value;
    _c.has_previous = false;

    if constexpr (component_traits::has_on_change_v<_T>)
    {
        if (_c.on_change)
        {
            _c.on_change(_c.value);
        }
    }

    return true;
}


/*****************************************************************************/

// ===============================================================================
//  8  COPY REQUEST  (requires copyable mixin)
// ===============================================================================

// request_copy
//   function: signals the renderer to copy the component's value
// to the clipboard.
template <typename _T,
          std::enable_if_t<
              component_traits::is_copyable_v<_T>,
              int> = 0>
void request_copy(_T& _c)
{
    _c.copy_requested = true;

    return;
}


/*****************************************************************************/

// ===============================================================================
//  9  COMMIT  (input-like components with on_commit)
// ===============================================================================

// commit
//   function: invokes the component's on_commit callback with
// its current value.  Returns true if the callback was invoked,
// false if the component is disabled or no callback is set.
template <typename _T,
          std::enable_if_t<
              ( component_traits::has_on_commit_v<_T> &&
                component_traits::has_value_v<_T>     &&
                component_traits::has_enabled_v<_T> ),
              int> = 0>
bool commit(_T& _c)
{
    if constexpr (component_traits::has_read_only_v<_T>)
    {
        if (!_c.enabled || _c.read_only)
        {
            return false;
        }
    }
    else
    {
        if (!_c.enabled)
        {
            return false;
        }
    }

    if (_c.on_commit)
    {
        _c.on_commit(_c.value);

        return true;
    }

    return false;
}


/*****************************************************************************/

// ===============================================================================
//  10  COMPOSITIONAL FORWARDING UTILITIES
// ===============================================================================
//   These utilities support composite components (like dev_console)
// that contain sub-components.  Instead of writing:
//
//     void dc_enable_output(dev_console<F>& dc) { dc.output.enabled = true; }
//
// the framework consumer writes:
//
//     enable(dc.output());
//
// using the same shared free functions.  The composite just needs
// to expose accessor functions for its sub-components.
//
//   For cases where the composite wants to forward an operation to
// ALL applicable sub-components (e.g. disable(console) disables
// the console AND its output AND its autosuggest), the
// for_each_sub template provides a compile-time visitation
// mechanism.
//
//   Usage:
//
//     // in the composite struct:
//     template <typename _Fn>
//     void visit_components(_Fn&& fn)
//     {
//         fn(this->input);
//         if constexpr (has_output) { fn(this->output); }
//         if constexpr (has_suggest) { fn(this->suggest); }
//     }
//
//     // in client code:
//     for_each_sub(my_console, [](auto& sub) { disable(sub); });

// for_each_sub
//   function: calls the given callable on each sub-component that
// the composite exposes via its visit_components method.
// Requires the composite to define a visit_components member.

namespace detail {

    template <typename, typename = void>
    struct has_visit_components : std::false_type {};

    template <typename _T>
    struct has_visit_components<_T, std::void_t<
        decltype(std::declval<_T&>().visit_components(
            std::declval<void(*)(int&)>()
        ))
    >> : std::true_type {};

}   // namespace detail

template <typename _T>
inline constexpr bool has_visit_components_v =
    detail::has_visit_components<_T>::value;

template <typename _T,
          typename _Fn,
          std::enable_if_t<
              has_visit_components_v<_T>,
              int> = 0>
void for_each_sub(_T&   _composite,
                  _Fn&& _fn)
{
    _composite.visit_components(std::forward<_Fn>(_fn));

    return;
}

// enable_all
//   function: enables the composite and all its sub-components.
template <typename _T,
          std::enable_if_t<
              ( component_traits::has_enabled_v<_T> &&
                has_visit_components_v<_T> ),
              int> = 0>
void enable_all(_T& _c)
{
    _c.enabled = true;
    _c.visit_components([](auto& _sub)
    {
        if constexpr (component_traits::has_enabled_v<
                          std::remove_reference_t<decltype(_sub)>>)
        {
            _sub.enabled = true;
        }
    });

    return;
}

// disable_all
//   function: disables the composite and all its sub-components.
template <typename _T,
          std::enable_if_t<
              ( component_traits::has_enabled_v<_T> &&
                has_visit_components_v<_T> ),
              int> = 0>
void disable_all(_T& _c)
{
    _c.enabled = false;
    _c.visit_components([](auto& _sub)
    {
        if constexpr (component_traits::has_enabled_v<
                          std::remove_reference_t<decltype(_sub)>>)
        {
            _sub.enabled = false;
        }
    });

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_COMMON_
