/*******************************************************************************
* uxoxo [component]                                            autocomplete.hpp
*
* Autocomplete component:
*   A framework-agnostic, presentation-neutral autocomplete template.
* Like autosuggest, it connects to a suggest_adapter on the data side
* and exposes a state surface for the framework to render.  The key
* distinction:
*
*   autosuggest  — NON-DESTRUCTIVE.  Shows candidates alongside the
*                  input.  The user explicitly picks one.  The input
*                  is not modified until confirmation.
*
*   autocomplete — DESTRUCTIVE.  Automatically extends the input with
*                  the best (or only) match.  The user's input is
*                  modified in-place.  The extended portion may be
*                  shown differently (selected, dimmed, etc.) so the
*                  user can accept (Tab/Enter), reject (keep typing),
*                  or cycle through alternatives.
*
*   Concrete examples:
*     autosuggest:   Google search dropdown, IDE quick-fix popup
*     autocomplete:  bash Tab completion, IDE inline ghost completion,
*                    Excel cell auto-fill, browser URL bar
*
*   Like autosuggest, this template prescribes NOTHING about how the
* completion is rendered.  It could be:
*     - Ghost text appended to the cursor (fish shell, GitHub Copilot)
*     - Selected/highlighted suffix in the input field (bash, Excel)
*     - A cycling indicator (zsh menu-complete)
*     - Nothing visible — programmatic completion only
*
*   The data flow is:
*     user input -> autocomplete_update() -> suggest_adapter::suggest() ->
*     best match selected -> completion suffix computed ->
*     framework reads & renders/applies
*
*   Type parameters:
*     _Input      input type fed to the adapter
*     _Suggest    suggestion element type
*     _Container  result container
*     _Policy     framework-supplied presentation policy
*
* Contents:
*   1.  completion mode enum
*   2.  default policy
*   3.  autocomplete struct
*   4.  free functions
*   5.  traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/autocomplete.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.04.09
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_AUTOCOMPLETE_
#define  UXOXO_COMPONENT_AUTOCOMPLETE_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  COMPLETION MODE
// ===============================================================================
//   Hints to the framework about how the completion is applied.

enum class completion_mode : std::uint8_t
{
    // the framework decides.
    automatic,

    // complete to the longest common prefix of all matches,
    // then let the user cycle through remaining candidates.
    // (bash Tab behavior)
    common_prefix,

    // complete to the first/best match immediately.
    // (fish shell, GitHub Copilot ghost text)
    first_match,

    // cycle through all matches on repeated invocation.
    // (zsh menu-complete)
    cycle,

    // no automatic application — the consumer calls
    // autocomplete_apply() explicitly.
    manual
};

// ===============================================================================
//  2.  DEFAULT POLICY
// ===============================================================================

// autocomplete_default_policy
//   struct: empty default policy placeholder.
struct autocomplete_default_policy
{};

// ===============================================================================
//  3.  AUTOCOMPLETE
// ===============================================================================
//   _Matcher is a type-erased callable that extracts the completion
// suffix from a suggestion given the current input.  Signature:
//   _Input (_Input current_input, _Suggest selected_suggestion)
//           -> completed input (the full replacement value)
//
//   If no matcher is set, the default behavior is identity — the
// suggestion IS the completed value.  Frameworks override this to
// implement prefix-aware completion (e.g. only appending the
// suffix that extends beyond the user's current input).

// autocomplete
//   struct: framework-agnostic autocomplete state template.
template<typename _Input     = std::string,
         typename _Suggest   = std::string,
         typename _Container = std::vector<_Suggest>,
         typename _Policy    = autocomplete_default_policy>
struct autocomplete
{
    using input_type     = _Input;
    using suggest_type   = _Suggest;
    using container_type = _Container;
    using policy_type    = _Policy;
    using size_type      = std::size_t;
    using suggest_fn     = std::function<
                               _Container(const _Input&)>;
    using matcher_fn     = std::function<
                               _Input(const _Input&,
                                      const _Suggest&)>;

    // -- adapter connection -------------------------------------------
    suggest_fn  source;

    // -- matcher ------------------------------------------------------
    //   transforms (input, suggestion) -> completed input.
    // If unset, the suggestion is used directly as the completed
    // value.
    matcher_fn  matcher;

    // -- results ------------------------------------------------------
    _Container  candidates {};
    size_type   selected    = 0;

    // -- completion state ---------------------------------------------
    //   completed_value holds the result of applying the matcher
    // to the current input and selected candidate.  The framework
    // reads this to know what to display / insert.
    _Input      completed_value {};
    bool        has_completion   = false;

    // -- display ------------------------------------------------------
    completion_mode mode    = completion_mode::automatic;
    bool            active  = true;
    bool            visible = false;

    // -- construction -------------------------------------------------
    autocomplete() = default;

    explicit autocomplete(
        suggest_fn _source
    )
        : source(std::move(_source))
    {}

    autocomplete(
        suggest_fn      _source,
        matcher_fn      _matcher
    )
        : source(std::move(_source)),
            matcher(std::move(_matcher))
    {}

    autocomplete(
        suggest_fn      _source,
        matcher_fn      _matcher,
        completion_mode _mode
    )
        : source(std::move(_source)),
            matcher(std::move(_matcher)),
            mode(_mode)
    {}

    // -- queries ------------------------------------------------------
    D_NODISCARD bool
    has_source() const noexcept
    {
        return static_cast<bool>(source);
    }

    D_NODISCARD bool
    has_candidates() const noexcept
    {
        return !candidates.empty();
    }

    D_NODISCARD bool
    has_matcher() const noexcept
    {
        return static_cast<bool>(matcher);
    }
};


// ===============================================================================
//  4.  FREE FUNCTIONS
// ===============================================================================

// autocomplete_set_source
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
void autocomplete_set_source(
    autocomplete<_I, _S, _C, _P>&                     _ac,
    typename autocomplete<_I, _S, _C, _P>::suggest_fn _fn)
{
    _ac.source = std::move(_fn);

    return;
}

// autocomplete_set_matcher
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
void autocomplete_set_matcher(
    autocomplete<_I, _S, _C, _P>&                      _ac,
    typename autocomplete<_I, _S, _C, _P>::matcher_fn   _fn)
{
    _ac.matcher = std::move(_fn);

    return;
}

// autocomplete_update
//   regenerates the candidate list from the given input and
// computes the completion for the first/selected candidate.
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
void autocomplete_update(
    autocomplete<_I, _S, _C, _P>& _ac,
    const _I&                      _input)
{
    if ( (!_ac.active) ||
         (!_ac.source) )
    {
        _ac.candidates      = _C{};
        _ac.has_completion   = false;
        _ac.visible          = false;

        return;
    }

    _ac.candidates = _ac.source(_input);
    _ac.selected   = 0;

    if (_ac.candidates.empty())
    {
        _ac.has_completion = false;
        _ac.visible        = false;

        return;
    }

    // apply matcher to produce the completed value.
    if (_ac.matcher)
    {
        _ac.completed_value = _ac.matcher(
            _input,
            _ac.candidates[_ac.selected]);
    }
    else
    {
        // default: the suggestion IS the completed value.
        // This requires _Suggest to be convertible to _Input.
        _ac.completed_value = static_cast<_I>(
            _ac.candidates[_ac.selected]);
    }

    _ac.has_completion = true;
    _ac.visible        = true;

    return;
}

// autocomplete_cycle_next
//   advances to the next candidate and recomputes the
// completion.
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
bool autocomplete_cycle_next(
    autocomplete<_I, _S, _C, _P>& _ac,
    const _I&                      _input)
{
    if (_ac.candidates.empty())
    {
        return false;
    }

    _ac.selected = (_ac.selected + 1) % _ac.candidates.size();

    // recompute completion for new selection.
    if (_ac.matcher)
    {
        _ac.completed_value = _ac.matcher(
            _input,
            _ac.candidates[_ac.selected]);
    }
    else
    {
        _ac.completed_value = static_cast<_I>(
            _ac.candidates[_ac.selected]);
    }

    _ac.has_completion = true;

    return true;
}

// autocomplete_cycle_prev
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
bool autocomplete_cycle_prev(
    autocomplete<_I, _S, _C, _P>& _ac,
    const _I&                      _input)
{
    if (_ac.candidates.empty())
    {
        return false;
    }

    _ac.selected = (_ac.selected + _ac.candidates.size() - 1)
                   % _ac.candidates.size();

    if (_ac.matcher)
    {
        _ac.completed_value = _ac.matcher(
            _input,
            _ac.candidates[_ac.selected]);
    }
    else
    {
        _ac.completed_value = static_cast<_I>(
            _ac.candidates[_ac.selected]);
    }

    _ac.has_completion = true;

    return true;
}

// autocomplete_accept
//   returns the completed value for the current selection.
// The caller applies it to the input control.  Returns
// nullptr if no completion is available.
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
D_NODISCARD const _I*
autocomplete_accept(
    const autocomplete<_I, _S, _C, _P>& _ac)
{
    if (!_ac.has_completion)
    {
        return nullptr;
    }

    return &(_ac.completed_value);
}

// autocomplete_dismiss
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
void autocomplete_dismiss(
    autocomplete<_I, _S, _C, _P>& _ac)
{
    _ac.visible        = false;
    _ac.has_completion = false;

    return;
}

// autocomplete_clear
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
void autocomplete_clear(
    autocomplete<_I, _S, _C, _P>& _ac)
{
    _ac.candidates      = _C{};
    _ac.selected        = 0;
    _ac.completed_value = _I{};
    _ac.has_completion  = false;
    _ac.visible         = false;

    return;
}

// autocomplete_activate
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
void autocomplete_activate(
    autocomplete<_I, _S, _C, _P>& _ac)
{
    _ac.active = true;

    return;
}

// autocomplete_deactivate
template<typename _I,
         typename _S,
         typename _C,
         typename _P>
void autocomplete_deactivate(
    autocomplete<_I, _S, _C, _P>& _ac)
{
    _ac.active         = false;
    _ac.visible        = false;
    _ac.has_completion = false;

    return;
}

// ===============================================================================
//  5.  TRAITS
// ===============================================================================

namespace autocomplete_traits {
NS_INTERNAL

    // has_source_member
    //   trait: detects presence of a `source` member.
    template<typename,
             typename = void>
    struct has_source_member : std::false_type {};
    template<typename _Type>
    struct has_source_member<_Type, std::void_t<
        decltype(std::declval<_Type>().source)
    >> : std::true_type {};

    // has_candidates_member
    //   trait: detects presence of a `candidates` member.
    template<typename,
             typename = void>
    struct has_candidates_member : std::false_type {};
    template<typename _Type>
    struct has_candidates_member<_Type, std::void_t<
        decltype(std::declval<_Type>().candidates)
    >> : std::true_type {};

    // has_completed_value_member
    //   trait: detects presence of a `completed_value` member.
    template<typename,
             typename = void>
    struct has_completed_value_member : std::false_type {};
    template<typename _Type>
    struct has_completed_value_member<_Type, std::void_t<
        decltype(std::declval<_Type>().completed_value)
    >> : std::true_type {};

    // has_has_completion_member
    //   trait: detects presence of a `has_completion` member.
    template<typename,
             typename = void>
    struct has_has_completion_member : std::false_type {};
    template<typename _Type>
    struct has_has_completion_member<_Type, std::void_t<
        decltype(std::declval<_Type>().has_completion)
    >> : std::true_type {};

    // has_matcher_member
    //   trait: detects presence of a `matcher` member.
    template<typename,
             typename = void>
    struct has_matcher_member : std::false_type {};
    template<typename _Type>
    struct has_matcher_member<_Type, std::void_t<
        decltype(std::declval<_Type>().matcher)
    >> : std::true_type {};

    // has_mode_member
    //   trait: detects presence of a `mode` member.
    template<typename,
             typename = void>
    struct has_mode_member : std::false_type {};
    template<typename _Type>
    struct has_mode_member<_Type, std::void_t<
        decltype(std::declval<_Type>().mode)
    >> : std::true_type {};

    // has_active_member
    //   trait: detects presence of an `active` member.
    template<typename,
             typename = void>
    struct has_active_member : std::false_type {};
    template<typename _Type>
    struct has_active_member<_Type, std::void_t<
        decltype(std::declval<_Type>().active)
    >> : std::true_type {};

    // has_visible_member
    //   trait: detects presence of a `visible` member.
    template<typename,
             typename = void>
    struct has_visible_member : std::false_type {};
    template<typename _Type>
    struct has_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type>().visible)
    >> : std::true_type {};

NS_END  // internal

// has_source_v
//   constant: true if _Type has a `source` member.
template<typename _Type>
D_INLINE constexpr bool has_source_v = internal::has_source_member<_Type>::value;

// has_candidates_v
//   constant: true if _Type has a `candidates` member.
template<typename _Type>
D_INLINE constexpr bool has_candidates_v = internal::has_candidates_member<_Type>::value;

// has_completed_value_v
//   constant: true if _Type has a `completed_value` member.
template<typename _Type>
D_INLINE constexpr bool has_completed_value_v = internal::has_completed_value_member<_Type>::value;

// has_matcher_v
//   constant: true if _Type has a `matcher` member.
template<typename _Type>
D_INLINE constexpr bool has_matcher_v = internal::has_matcher_member<_Type>::value;

// has_mode_v
//   constant: true if _Type has a `mode` member.
template<typename _Type>
D_INLINE constexpr bool has_mode_v = internal::has_mode_member<_Type>::value;

// is_autocomplete
//   trait: has source + candidates + completed_value +
// has_completion + active + visible.
template<typename _Type>
struct is_autocomplete : std::conjunction<
    internal::has_source_member<_Type>,
    internal::has_candidates_member<_Type>,
    internal::has_completed_value_member<_Type>,
    internal::has_has_completion_member<_Type>,
    internal::has_active_member<_Type>,
    internal::has_visible_member<_Type>
>
{};

// is_autocomplete_v
//   constant: true if _Type satisfies the autocomplete interface.
template<typename _Type>
D_INLINE constexpr bool is_autocomplete_v = is_autocomplete<_Type>::value;

// is_matcher_autocomplete
//   trait: autocomplete with a custom matcher.
template<typename _Type>
struct is_matcher_autocomplete : std::conjunction<
    is_autocomplete<_Type>,
    internal::has_matcher_member<_Type>
>
{};

// is_matcher_autocomplete_v
//   constant: true if _Type is an autocomplete with a matcher.
template<typename _Type>
D_INLINE constexpr bool is_matcher_autocomplete_v = is_matcher_autocomplete<_Type>::value;


NS_END  // namespace autocomplete_traits
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_AUTOCOMPLETE_
