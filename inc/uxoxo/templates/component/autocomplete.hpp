/*******************************************************************************
* uxoxo [component]                                          autocomplete.hpp
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
*     user input → ac_update() → suggest_adapter::suggest() →
*     best match selected → completion suffix computed →
*     framework reads & renders/applies
*
*   Type parameters:
*     _Input      input type fed to the adapter
*     _Suggest    suggestion element type
*     _Container  result container
*     _Policy     framework-supplied presentation policy
*
* Contents:
*   §1  Completion mode enum
*   §2  Default policy
*   §3  autocomplete struct
*   §4  Free functions
*   §5  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/autocomplete.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.09
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_AUTOCOMPLETE_
#define  UXOXO_COMPONENT_AUTOCOMPLETE_ 1

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include <uxoxo>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  COMPLETION MODE
// ═══════════════════════════════════════════════════════════════════════════════
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
    // ac_apply() explicitly.
    manual
};

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  DEFAULT POLICY
// ═══════════════════════════════════════════════════════════════════════════════

struct autocomplete_default_policy
{};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  AUTOCOMPLETE
// ═══════════════════════════════════════════════════════════════════════════════
//   _Matcher is a type-erased callable that extracts the completion
// suffix from a suggestion given the current input.  Signature:
//   _Input (_Input current_input, _Suggest selected_suggestion)
//           → completed input (the full replacement value)
//
//   If no matcher is set, the default behavior is identity — the
// suggestion IS the completed value.  Frameworks override this to
// implement prefix-aware completion (e.g. only appending the
// suffix that extends beyond the user's current input).

template <typename _Input     = std::string,
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

    // ── adapter connection ───────────────────────────────────────────
    suggest_fn  source;

    // ── matcher ──────────────────────────────────────────────────────
    //   Transforms (input, suggestion) → completed input.
    // If unset, the suggestion is used directly as the completed
    // value.
    matcher_fn  matcher;

    // ── results ──────────────────────────────────────────────────────
    _Container  candidates {};
    size_type   selected    = 0;

    // ── completion state ─────────────────────────────────────────────
    //   completed_value holds the result of applying the matcher
    // to the current input and selected candidate.  The framework
    // reads this to know what to display / insert.
    _Input      completed_value {};
    bool        has_completion   = false;

    // ── display ──────────────────────────────────────────────────────
    completion_mode mode    = completion_mode::automatic;
    bool            active  = true;
    bool            visible = false;

    // ── construction ─────────────────────────────────────────────────
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

    // ── queries ──────────────────────────────────────────────────────
    [[nodiscard]] bool
    has_source() const noexcept
    {
        return static_cast<bool>(source);
    }

    [[nodiscard]] bool
    has_candidates() const noexcept
    {
        return !candidates.empty();
    }

    [[nodiscard]] bool
    has_matcher() const noexcept
    {
        return static_cast<bool>(matcher);
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// ac_set_source
template <typename _I, typename _S,
          typename _C, typename _P>
void ac_set_source(
    autocomplete<_I, _S, _C, _P>&                       ac,
    typename autocomplete<_I, _S, _C, _P>::suggest_fn    fn)
{
    ac.source = std::move(fn);

    return;
}

// ac_set_matcher
template <typename _I, typename _S,
          typename _C, typename _P>
void ac_set_matcher(
    autocomplete<_I, _S, _C, _P>&                       ac,
    typename autocomplete<_I, _S, _C, _P>::matcher_fn    fn)
{
    ac.matcher = std::move(fn);

    return;
}

// ac_update
//   regenerates the candidate list from the given input and
// computes the completion for the first/selected candidate.
template <typename _I, typename _S,
          typename _C, typename _P>
void ac_update(autocomplete<_I, _S, _C, _P>& ac,
               const _I&                     input)
{
    if (!ac.active || !ac.source)
    {
        ac.candidates      = _C{};
        ac.has_completion   = false;
        ac.visible          = false;

        return;
    }

    ac.candidates = ac.source(input);
    ac.selected   = 0;

    if (ac.candidates.empty())
    {
        ac.has_completion = false;
        ac.visible        = false;

        return;
    }

    // apply matcher to produce the completed value.
    if (ac.matcher)
    {
        ac.completed_value = ac.matcher(
            input,
            ac.candidates[ac.selected]);
    }
    else
    {
        // default: the suggestion IS the completed value.
        // This requires _Suggest to be convertible to _Input.
        ac.completed_value = static_cast<_I>(
            ac.candidates[ac.selected]);
    }

    ac.has_completion = true;
    ac.visible        = true;

    return;
}

// ac_cycle_next
//   advances to the next candidate and recomputes the
// completion.
template <typename _I, typename _S,
          typename _C, typename _P>
bool ac_cycle_next(autocomplete<_I, _S, _C, _P>& ac,
                   const _I&                     input)
{
    if (ac.candidates.empty())
    {
        return false;
    }

    ac.selected = (ac.selected + 1) % ac.candidates.size();

    // recompute completion for new selection.
    if (ac.matcher)
    {
        ac.completed_value = ac.matcher(
            input,
            ac.candidates[ac.selected]);
    }
    else
    {
        ac.completed_value = static_cast<_I>(
            ac.candidates[ac.selected]);
    }

    ac.has_completion = true;

    return true;
}

// ac_cycle_prev
template <typename _I, typename _S,
          typename _C, typename _P>
bool ac_cycle_prev(autocomplete<_I, _S, _C, _P>& ac,
                   const _I&                     input)
{
    if (ac.candidates.empty())
    {
        return false;
    }

    ac.selected = (ac.selected + ac.candidates.size() - 1)
                  % ac.candidates.size();

    if (ac.matcher)
    {
        ac.completed_value = ac.matcher(
            input,
            ac.candidates[ac.selected]);
    }
    else
    {
        ac.completed_value = static_cast<_I>(
            ac.candidates[ac.selected]);
    }

    ac.has_completion = true;

    return true;
}

// ac_accept
//   returns the completed value for the current selection.
// The caller applies it to the input control.  Returns
// nullptr if no completion is available.
template <typename _I, typename _S,
          typename _C, typename _P>
const _I* ac_accept(
    const autocomplete<_I, _S, _C, _P>& ac)
{
    if (!ac.has_completion)
    {
        return nullptr;
    }

    return &(ac.completed_value);
}

// ac_dismiss
template <typename _I, typename _S,
          typename _C, typename _P>
void ac_dismiss(autocomplete<_I, _S, _C, _P>& ac)
{
    ac.visible        = false;
    ac.has_completion = false;

    return;
}

// ac_clear
template <typename _I, typename _S,
          typename _C, typename _P>
void ac_clear(autocomplete<_I, _S, _C, _P>& ac)
{
    ac.candidates      = _C{};
    ac.selected        = 0;
    ac.completed_value = _I{};
    ac.has_completion  = false;
    ac.visible         = false;

    return;
}

// ac_activate / ac_deactivate
template <typename _I, typename _S,
          typename _C, typename _P>
void ac_activate(autocomplete<_I, _S, _C, _P>& ac)
{
    ac.active = true;

    return;
}

template <typename _I, typename _S,
          typename _C, typename _P>
void ac_deactivate(autocomplete<_I, _S, _C, _P>& ac)
{
    ac.active         = false;
    ac.visible        = false;
    ac.has_completion = false;

    return;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace autocomplete_traits {
namespace detail {

    template <typename, typename = void>
    struct has_source_member : std::false_type {};
    template <typename _T>
    struct has_source_member<_T, std::void_t<
        decltype(std::declval<_T>().source)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_candidates_member : std::false_type {};
    template <typename _T>
    struct has_candidates_member<_T, std::void_t<
        decltype(std::declval<_T>().candidates)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_completed_value_member : std::false_type {};
    template <typename _T>
    struct has_completed_value_member<_T, std::void_t<
        decltype(std::declval<_T>().completed_value)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_has_completion_member : std::false_type {};
    template <typename _T>
    struct has_has_completion_member<_T, std::void_t<
        decltype(std::declval<_T>().has_completion)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_matcher_member : std::false_type {};
    template <typename _T>
    struct has_matcher_member<_T, std::void_t<
        decltype(std::declval<_T>().matcher)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_mode_member : std::false_type {};
    template <typename _T>
    struct has_mode_member<_T, std::void_t<
        decltype(std::declval<_T>().mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_active_member : std::false_type {};
    template <typename _T>
    struct has_active_member<_T, std::void_t<
        decltype(std::declval<_T>().active)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_visible_member : std::false_type {};
    template <typename _T>
    struct has_visible_member<_T, std::void_t<
        decltype(std::declval<_T>().visible)
    >> : std::true_type {};

}   // namespace detail

template <typename _T>
inline constexpr bool has_source_v =
    detail::has_source_member<_T>::value;
template <typename _T>
inline constexpr bool has_candidates_v =
    detail::has_candidates_member<_T>::value;
template <typename _T>
inline constexpr bool has_completed_value_v =
    detail::has_completed_value_member<_T>::value;
template <typename _T>
inline constexpr bool has_matcher_v =
    detail::has_matcher_member<_T>::value;
template <typename _T>
inline constexpr bool has_mode_v =
    detail::has_mode_member<_T>::value;

// is_autocomplete
//   type trait: has source + candidates + completed_value +
// has_completion + active + visible.
template <typename _Type>
struct is_autocomplete : std::conjunction<
    detail::has_source_member<_Type>,
    detail::has_candidates_member<_Type>,
    detail::has_completed_value_member<_Type>,
    detail::has_has_completion_member<_Type>,
    detail::has_active_member<_Type>,
    detail::has_visible_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_autocomplete_v =
    is_autocomplete<_T>::value;

// is_matcher_autocomplete
//   type trait: autocomplete with a custom matcher.
template <typename _Type>
struct is_matcher_autocomplete : std::conjunction<
    is_autocomplete<_Type>,
    detail::has_matcher_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_matcher_autocomplete_v =
    is_matcher_autocomplete<_T>::value;

}   // namespace autocomplete_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_AUTOCOMPLETE_
