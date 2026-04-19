/*******************************************************************************
* uxoxo [component]                                           autosuggest.hpp
*
* Autosuggest component:
*   A framework-agnostic, presentation-neutral autosuggest template.
* Connects an input source to a suggest_adapter on the data side, and
* exposes a minimal state surface that any framework can map to its
* own UI — dropdown list, inline ghost text, floating panel, or
* anything else.
*
*   Autosuggest is the "live suggestion" pattern: as the user types,
* a list of candidate completions appears.  The user may navigate the
* list (up/down), accept a suggestion, or dismiss the list and keep
* typing.  The suggestions are non-destructive — accepting one does
* not alter the input until the user explicitly confirms.
*
*   This template prescribes NOTHING about rendering.  It owns:
*     - A reference to the backing suggest_adapter (via type erasure
*       or direct CRTP pointer — the framework decides)
*     - The current suggestion results (a container of _Suggest)
*     - A selected-index cursor
*     - Visibility / active toggles
*     - A display mode hint (enum)
*
*   The data flow is:
*     user input -> as_update() -> suggest_adapter::suggest() ->
*     results stored in autosuggest -> framework reads & renders
*
*   The suggest_adapter is NOT owned by autosuggest.  The adapter
* is a separate object (often shared between autosuggest and
* autocomplete instances, or between multiple input fields).
* autosuggest holds a non-owning pointer to it.
*
*   Type parameters:
*     _Input      input type fed to the adapter (e.g. std::string)
*     _Suggest    suggestion element type (e.g. std::string, rich_suggestion)
*     _Container  result container (e.g. std::vector<_Suggest>)
*     _Policy     framework-supplied presentation policy (default: empty)
*
*   Shared operations (show, hide, toggle_visible, activate,
* deactivate) are provided by the ADL-dispatched free functions
* in component_common.hpp.  Legacy as_-prefixed wrappers are
* retained for backward compatibility.
*
* Contents:
*   1  Display mode enum
*   2  Default policy
*   3  autosuggest struct
*   4  Domain-specific free functions
*   5  Legacy free functions (as_-prefixed, thin wrappers)
*   6  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/autosuggest.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.09
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_AUTOSUGGEST_
#define  UXOXO_COMPONENT_AUTOSUGGEST_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "component_traits.hpp"
#include "component_common.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  DISPLAY MODE
// ===============================================================================

enum class suggest_display_mode : std::uint8_t
{
    // the framework decides.
    automatic,

    // a dropdown / popup list below or above the input.
    dropdown,

    // inline ghost text (dimmed completion after the cursor).
    ghost,

    // a floating panel anchored to the input but rendered
    // independently (e.g. a tooltip-style overlay).
    floating_panel,

    // invisible — suggestions are computed but not rendered.
    // Useful when the consumer wants programmatic access only.
    hidden
};


/*****************************************************************************/

// ===============================================================================
//  2  DEFAULT POLICY
// ===============================================================================

struct autosuggest_default_policy
{};


/*****************************************************************************/

// ===============================================================================
//  3  AUTOSUGGEST
// ===============================================================================
//   The suggest_fn type-erases the adapter connection.  The user
// sets it to a lambda or std::function that calls their concrete
// suggest_adapter derivative.  This keeps autosuggest decoupled
// from any specific adapter type at the template level while
// still supporting zero-overhead CRTP adapters at the call site.

template <typename _Input     = std::string,
          typename _Suggest   = std::string,
          typename _Container = std::vector<_Suggest>,
          typename _Policy    = autosuggest_default_policy>
struct autosuggest
{
    using input_type     = _Input;
    using suggest_type   = _Suggest;
    using container_type = _Container;
    using policy_type    = _Policy;
    using size_type      = std::size_t;
    using suggest_fn     = std::function<
                               _Container(const _Input&)>;

    // -- adapter connection -------------------------------------------
    //   The user provides a callable that maps input -> suggestions.
    // Typically wraps a suggest_adapter::suggest() call.
    suggest_fn  source;

    // -- results ------------------------------------------------------
    _Container  suggestions {};
    size_type   selected     = 0;

    // -- display ------------------------------------------------------
    suggest_display_mode display_mode =
        suggest_display_mode::automatic;
    bool        active       = true;
    bool        visible      = false;
    size_type   max_visible  = 10;

    // -- construction -------------------------------------------------
    autosuggest() = default;

    explicit autosuggest(
            suggest_fn _source
        )
            : source(std::move(_source))
        {}

    autosuggest(
            suggest_fn           _source,
            suggest_display_mode _mode
        )
            : source(std::move(_source)),
              display_mode(_mode)
        {}

    // -- queries ------------------------------------------------------
    [[nodiscard]] bool
    has_source() const noexcept
    {
        return static_cast<bool>(source);
    }

    [[nodiscard]] bool
    has_suggestions() const noexcept
    {
        return !suggestions.empty();
    }
};


/*****************************************************************************/

// ===============================================================================
//  4  DOMAIN-SPECIFIC FREE FUNCTIONS
// ===============================================================================
//   These operations are unique to autosuggest and have no
// shared equivalent in component_common.hpp.

// as_set_source
//   connects the autosuggest to a suggest_adapter via a callable.
template <typename _I, typename _S,
          typename _C, typename _P>
void as_set_source(
    autosuggest<_I, _S, _C, _P>&                       _as,
    typename autosuggest<_I, _S, _C, _P>::suggest_fn    _fn)
{
    _as.source = std::move(_fn);

    return;
}

// as_update
//   regenerates the suggestion list from the given input.
// Call after every input change.
template <typename _I, typename _S,
          typename _C, typename _P>
void as_update(autosuggest<_I, _S, _C, _P>& _as,
               const _I&                    _input)
{
    if (!_as.active || !_as.source)
    {
        _as.suggestions = _C{};
        _as.visible     = false;

        return;
    }

    _as.suggestions = _as.source(_input);
    _as.selected    = 0;
    _as.visible     = !_as.suggestions.empty();

    return;
}

// as_next
//   moves the selection cursor to the next suggestion.
template <typename _I, typename _S,
          typename _C, typename _P>
bool as_next(autosuggest<_I, _S, _C, _P>& _as)
{
    if (_as.suggestions.empty())
    {
        return false;
    }

    _as.selected = (_as.selected + 1) % _as.suggestions.size();

    return true;
}

// as_prev
//   moves the selection cursor to the previous suggestion.
template <typename _I, typename _S,
          typename _C, typename _P>
bool as_prev(autosuggest<_I, _S, _C, _P>& _as)
{
    if (_as.suggestions.empty())
    {
        return false;
    }

    _as.selected = (_as.selected + _as.suggestions.size() - 1)
                   % _as.suggestions.size();

    return true;
}

// as_selected_value
//   returns a pointer to the currently selected suggestion,
// or nullptr if no suggestions exist.
template <typename _I, typename _S,
          typename _C, typename _P>
const _S* as_selected_value(
    const autosuggest<_I, _S, _C, _P>& _as)
{
    if ( _as.suggestions.empty()           ||
         _as.selected >= _as.suggestions.size() )
    {
        return nullptr;
    }

    return &(_as.suggestions[_as.selected]);
}

// as_dismiss
//   hides the suggestion list without clearing results.
// Semantically distinct from hide() — "dismiss" conveys user
// intent (pressed Escape, clicked away), where hide() is a
// generic visibility toggle.  Retained as a named operation
// for framework code that distinguishes the two.
template <typename _I, typename _S,
          typename _C, typename _P>
void as_dismiss(autosuggest<_I, _S, _C, _P>& _as)
{
    _as.visible = false;

    return;
}

// as_clear
//   clears all suggestions and resets the cursor.
// NOTE: this is NOT the shared clear() from component_common.hpp,
// which resets a component's value to its default_value.  This
// clears the suggestions container — a domain-specific operation.
template <typename _I, typename _S,
          typename _C, typename _P>
void as_clear(autosuggest<_I, _S, _C, _P>& _as)
{
    _as.suggestions = _C{};
    _as.selected    = 0;
    _as.visible     = false;

    return;
}


/*****************************************************************************/

// ===============================================================================
//  5  LEGACY FREE FUNCTIONS
// ===============================================================================
//   These as_-prefixed functions are retained for backward
// compatibility.  New code should prefer the ADL-dispatched
// equivalents in component_common.hpp:
//
//     as_show(as)        ->  show(as)
//     as_hide(as)        ->  hide(as)
//     as_toggle(as)      ->  toggle_visible(as)
//     as_activate(as)    ->  activate(as)
//     as_deactivate(as)  ->  deactivate(as)

// as_show
template <typename _I, typename _S,
          typename _C, typename _P>
void as_show(autosuggest<_I, _S, _C, _P>& _as)
{
    show(_as);

    return;
}

// as_hide
template <typename _I, typename _S,
          typename _C, typename _P>
void as_hide(autosuggest<_I, _S, _C, _P>& _as)
{
    hide(_as);

    return;
}

// as_toggle
template <typename _I, typename _S,
          typename _C, typename _P>
void as_toggle(autosuggest<_I, _S, _C, _P>& _as)
{
    toggle_visible(_as);

    return;
}

// as_activate
template <typename _I, typename _S,
          typename _C, typename _P>
void as_activate(autosuggest<_I, _S, _C, _P>& _as)
{
    activate(_as);

    return;
}

// as_deactivate
template <typename _I, typename _S,
          typename _C, typename _P>
void as_deactivate(autosuggest<_I, _S, _C, _P>& _as)
{
    deactivate(_as);

    return;
}


/*****************************************************************************/

// ===============================================================================
//  6  TRAITS
// ===============================================================================
//   Shared detectors (has_visible, has_active) delegate to
// component_traits.  Autosuggest-specific detectors remain here.

namespace autosuggest_traits {
namespace detail {

    // -- autosuggest-specific detectors -------------------------------

    template <typename, typename = void>
    struct has_source_member : std::false_type {};
    template <typename _Type>
    struct has_source_member<_Type, std::void_t<
        decltype(std::declval<_Type>().source)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_suggestions_member : std::false_type {};
    template <typename _Type>
    struct has_suggestions_member<_Type, std::void_t<
        decltype(std::declval<_Type>().suggestions)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_selected_member : std::false_type {};
    template <typename _Type>
    struct has_selected_member<_Type, std::void_t<
        decltype(std::declval<_Type>().selected)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_display_mode_member : std::false_type {};
    template <typename _Type>
    struct has_display_mode_member<_Type, std::void_t<
        decltype(std::declval<_Type>().display_mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_max_visible_member : std::false_type {};
    template <typename _Type>
    struct has_max_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type>().max_visible)
    >> : std::true_type {};

}   // namespace detail

// -- autosuggest-specific aliases -----------------------------------------
template <typename _Type>
inline constexpr bool has_source_v =
    detail::has_source_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_suggestions_v =
    detail::has_suggestions_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_selected_v =
    detail::has_selected_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_max_visible_v =
    detail::has_max_visible_member<_Type>::value;

// -- shared aliases (delegate to component_traits) ------------------------
template <typename _Type>
inline constexpr bool has_visible_v =
    component_traits::has_visible_v<_Type>;
template <typename _Type>
inline constexpr bool has_active_v =
    component_traits::has_active_v<_Type>;

// -- composite traits -----------------------------------------------------

// is_autosuggest
//   trait: has source + suggestions + selected + visible + active.
template <typename _Type>
struct is_autosuggest : std::conjunction<
    detail::has_source_member<_Type>,
    detail::has_suggestions_member<_Type>,
    detail::has_selected_member<_Type>,
    component_traits::detail::has_visible_member<_Type>,
    component_traits::detail::has_active_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_autosuggest_v =
    is_autosuggest<_Type>::value;

}   // namespace autosuggest_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_AUTOSUGGEST_
