/*******************************************************************************
* uxoxo [component]                                              login_prompt.hpp
*
* Generic login form:
*   A framework-agnostic, pure-data login form template.  Composes
* text_input fields for username and password behind compile-time
* feature flags.  No rendering, no networking — just the data shape
* and mutation functions a renderer or auth backend would operate on.
*
*   Feature composition via the same EBO-mixin bitfield pattern used
* by input_control and text_input:
*     lf_username       include a username / identifier field
*     lf_password        include a password field (masked text_input)
*     lf_remember_me     "remember me" checkbox flag
*     lf_show_password   toggle to unmask the password field
*
*   At least one of lf_username or lf_password must be set.
*
* Contents:
*   §1  Feature flags (login_prompt_feat)
*   §2  EBO mixins
*   §3  login_prompt struct
*   §4  Free functions
*   §5  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/prompt/login_prompt.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_LOGIN_PROMPT_
#define  UXOXO_COMPONENT_LOGIN_PROMPT_ 1

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
#include "../../../../uxoxo.hpp"
#include "../../view_common.hpp>
#include "../../input/text_input.hpp>
#include "../../input/input_control.hpp>


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  LOGIN FORM FEATURE FLAGS
// ===============================================================================
//   Start at bit 24 to avoid colliding with view_feat (0–7),
// text_input_feat (8–15), and input_control_feat (16–23).

enum login_prompt_feat : unsigned
{
    lf_none          = 0,
    lf_username      = 1u << 24,
    lf_password      = 1u << 25,
    lf_remember_me   = 1u << 26,
    lf_show_password = 1u << 27,

    lf_user_pass     = lf_username | lf_password,
    lf_all           = lf_username | lf_password
                     | lf_remember_me | lf_show_password
};

constexpr unsigned operator|(login_prompt_feat a,
                             login_prompt_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_lf(unsigned         f,
                      login_prompt_feat  bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}

// ===============================================================================
//  2.  EBO MIXINS
// ===============================================================================

namespace login_mixin {

    // -- username field -----------------------------------------------
    template <bool _Enable>
    struct username_data
    {};

    template <>
    struct username_data<true>
    {
        text_input<tif_validation> username;
    };

    // -- password field -----------------------------------------------
    template <bool _Enable>
    struct password_data
    {};

    template <>
    struct password_data<true>
    {
        text_input<tif_masked | tif_validation> password;
    };

    // -- remember-me flag ---------------------------------------------
    template <bool _Enable>
    struct remember_me_data
    {};

    template <>
    struct remember_me_data<true>
    {
        bool remember_me = false;
    };

    // -- show-password toggle -----------------------------------------
    template <bool _Enable>
    struct show_password_data
    {};

    template <>
    struct show_password_data<true>
    {
        bool show_password = false;
    };

}   // namespace login_mixin

// ===============================================================================
//  3.  LOGIN FORM
// ===============================================================================
//   _Feat      bitwise OR of login_prompt_feat flags.  At least one of
//              lf_username or lf_password must be present.
//
//   The submit callback is std::function<void(const login_prompt&)>.
// It is invoked by lf_submit() when the user confirms the form.

template <unsigned _Feat = lf_user_pass>
struct login_prompt
    : login_mixin::username_data      <has_lf(_Feat, lf_username)>,
      login_mixin::password_data      <has_lf(_Feat, lf_password)>,
      login_mixin::remember_me_data   <has_lf(_Feat, lf_remember_me)>,
      login_mixin::show_password_data <has_lf(_Feat, lf_show_password)>
{
    static_assert( (has_lf(_Feat, lf_username) ||
                    has_lf(_Feat, lf_password)),
                  "login_prompt requires at least lf_username or "
                  "lf_password.");

    using self_type = login_prompt<_Feat>;
    using submit_fn = std::function<void(const self_type&)>;

    static constexpr unsigned features      = _Feat;
    static constexpr bool has_username      = has_lf(_Feat, lf_username);
    static constexpr bool has_password      = has_lf(_Feat, lf_password);
    static constexpr bool has_remember_me   = has_lf(_Feat, lf_remember_me);
    static constexpr bool has_show_password = has_lf(_Feat, lf_show_password);
    static constexpr bool focusable         = true;

    // -- core state ---------------------------------------------------
    bool       enabled     = true;
    bool       submitting  = false;     // busy flag for async auth
    std::string error_message;          // last auth error, if any

    // -- submit callback ----------------------------------------------
    submit_fn  on_submit;

    // -- construction -------------------------------------------------
    login_prompt() = default;

    // -- queries ------------------------------------------------------

    // is_enabled
    //   form is enabled only when not mid-submission.
    [[nodiscard]] bool
    is_enabled() const noexcept
    {
        return enabled && !submitting;
    }

    // has_error
    [[nodiscard]] bool
    has_error() const noexcept
    {
        return !error_message.empty();
    }
};

// ===============================================================================
//  4.  FREE FUNCTIONS
// ===============================================================================

// lf_enable
template <unsigned _F>
void lf_enable(login_prompt<_F>& lf)
{
    lf.enabled = true;

    return;
}

// lf_disable
template <unsigned _F>
void lf_disable(login_prompt<_F>& lf)
{
    lf.enabled = false;

    return;
}

// lf_set_error
//   sets an error message on the form (e.g. "Invalid credentials").
template <unsigned _F>
void lf_set_error(login_prompt<_F>& lf,
                  std::string     msg)
{
    lf.error_message = std::move(msg);

    return;
}

// lf_clear_error
template <unsigned _F>
void lf_clear_error(login_prompt<_F>& lf)
{
    lf.error_message.clear();

    return;
}

// lf_set_submitting
//   marks the form as busy (disables interaction during auth).
template <unsigned _F>
void lf_set_submitting(login_prompt<_F>& lf,
                       bool            busy)
{
    lf.submitting = busy;

    return;
}

// lf_toggle_show_password
//   toggles password visibility (requires lf_show_password).
// Also toggles the underlying text_input masked state when
// lf_password is present.
template <unsigned _F>
void lf_toggle_show_password(login_prompt<_F>& lf)
{
    static_assert(has_lf(_F, lf_show_password),
                  "requires lf_show_password");

    lf.show_password = !lf.show_password;

    if constexpr (has_lf(_F, lf_password))
    {
        lf.password.masked = !lf.show_password;
    }

    return;
}

// lf_toggle_remember_me
//   toggles the remember-me flag (requires lf_remember_me).
template <unsigned _F>
void lf_toggle_remember_me(login_prompt<_F>& lf)
{
    static_assert(has_lf(_F, lf_remember_me),
                  "requires lf_remember_me");

    lf.remember_me = !lf.remember_me;

    return;
}

// lf_validate
//   runs validation on all fields that have validators.
// Returns true if every field is valid.
template <unsigned _F>
bool lf_validate(login_prompt<_F>& lf)
{
    bool valid = true;

    if constexpr (has_lf(_F, lf_username))
    {
        auto r = ti_validate(lf.username);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    if constexpr (has_lf(_F, lf_password))
    {
        auto r = ti_validate(lf.password);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    return valid;
}

// lf_submit
//   validates, then invokes the submit callback.
// Returns true if submission was initiated.
template <unsigned _F>
bool lf_submit(login_prompt<_F>& lf)
{
    if (!lf.is_enabled())
    {
        return false;
    }

    // clear any previous error
    lf.error_message.clear();

    // validate fields
    if (!lf_validate(lf))
    {
        return false;
    }

    if (lf.on_submit)
    {
        lf.submitting = true;
        lf.on_submit(lf);
    }

    return true;
}

// lf_clear
//   resets all fields to empty and clears error state.
template <unsigned _F>
void lf_clear(login_prompt<_F>& lf)
{
    if constexpr (has_lf(_F, lf_username))
    {
        ti_clear(lf.username);
    }

    if constexpr (has_lf(_F, lf_password))
    {
        ti_clear(lf.password);
    }

    lf.error_message.clear();
    lf.submitting = false;

    return;
}

// ===============================================================================
//  5.  TRAITS
// ===============================================================================

NS_INTERNAL

    template <typename, typename = void>
    struct has_username_member : std::false_type {};

    template <typename _T>
    struct has_username_member<_T, std::void_t<
        decltype(std::declval<_T>().username)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_password_member : std::false_type {};

    template <typename _T>
    struct has_password_member<_T, std::void_t<
        decltype(std::declval<_T>().password)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_remember_me_member : std::false_type {};

    template <typename _T>
    struct has_remember_me_member<_T, std::void_t<
        decltype(std::declval<_T>().remember_me)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_show_password_member : std::false_type {};

    template <typename _T>
    struct has_show_password_member<_T, std::void_t<
        decltype(std::declval<_T>().show_password)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_submitting_member : std::false_type {};

    template <typename _T>
    struct has_submitting_member<_T, std::void_t<
        decltype(std::declval<_T>().submitting)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_error_message_member : std::false_type {};

    template <typename _T>
    struct has_error_message_member<_T, std::void_t<
        decltype(std::declval<_T>().error_message)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_on_submit_member : std::false_type {};

    template <typename _T>
    struct has_on_submit_member<_T, std::void_t<
        decltype(std::declval<_T>().on_submit)
    >> : std::true_type {};

NS_END  // internal

template <typename _T>
inline constexpr bool has_username_v =
    detail::has_username_member<_T>::value;
template <typename _T>
inline constexpr bool has_password_v =
    detail::has_password_member<_T>::value;
template <typename _T>
inline constexpr bool has_remember_me_v =
    detail::has_remember_me_member<_T>::value;
template <typename _T>
inline constexpr bool has_show_password_v =
    detail::has_show_password_member<_T>::value;
template <typename _T>
inline constexpr bool has_submitting_v =
    detail::has_submitting_member<_T>::value;
template <typename _T>
inline constexpr bool has_error_message_v =
    detail::has_error_message_member<_T>::value;
template <typename _T>
inline constexpr bool has_on_submit_v =
    detail::has_on_submit_member<_T>::value;

// is_login_prompt
//   type trait: has enabled + submitting + error_message +
// on_submit, and at least one of username or password.
template <typename _Type>
struct is_login_prompt : std::conjunction<
    detail::has_submitting_member<_Type>,
    detail::has_error_message_member<_Type>,
    detail::has_on_submit_member<_Type>,
    std::disjunction<
        detail::has_username_member<_Type>,
        detail::has_password_member<_Type>
    >
>
{};

template <typename _T>
inline constexpr bool is_login_prompt_v =
    is_login_prompt<_T>::value;

// is_rememberable_login
template <typename _Type>
struct is_rememberable_login : std::conjunction<
    is_login_prompt<_Type>,
    detail::has_remember_me_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_rememberable_login_v =
    is_rememberable_login<_T>::value;


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_LOGIN_PROMPT_
