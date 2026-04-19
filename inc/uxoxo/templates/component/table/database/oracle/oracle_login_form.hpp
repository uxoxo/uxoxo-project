/*******************************************************************************
* uxoxo [component]                                    oracle_login_form.hpp
*
* Oracle Database login form:
*   Vendor-specific login form for Oracle connections.  Derives from
* database_login<database_type::oracle, ...> and adds Oracle-specific
* fields: service name, SID, TNS descriptor/alias, wallet location,
* session mode (SYSDBA/SYSOPER/…), edition-based redefinition, and
* NLS_LANG.
*
*   Oracle supports multiple connection addressing formats:
*     - Easy Connect:    //host[:port]/service_name
*     - TNS alias:       resolved via tnsnames.ora
*     - TNS descriptor:  inline (DESCRIPTION=(ADDRESS=…)(CONNECT_DATA=…))
*     - Wallet:          auto-login via Oracle Wallet
*   The login form captures all four as feature-gated fields; the
* renderer can show the appropriate UI based on which are enabled.
*
*   The free function odbl_to_config() extracts an ora_connect_config.
*
* Contents:
*   §1  Feature flags (oracle_login_feat)
*   §2  EBO mixins
*   §3  oracle_login_form struct
*   §4  Free functions
*   §5  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/oracle_login_form.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_ORACLE_LOGIN_FORM_
#define  UXOXO_COMPONENT_ORACLE_LOGIN_FORM_ 1

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include <uxoxo>
#include <component/view_common.hpp>
#include <component/text_input.hpp>
#include <component/login_form.hpp>
#include <component/database_login.hpp>
#include <database/oracle/oracle.hpp>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  ORACLE LOGIN FEATURE FLAGS
// ═══════════════════════════════════════════════════════════════════════════════

enum oracle_login_feat : unsigned
{
    olf_none             = 0,
    olf_service_name     = 1u << 0,     // service name field
    olf_sid              = 1u << 1,     // SID field (legacy)
    olf_tns_alias        = 1u << 2,     // TNS alias lookup
    olf_tns_descriptor   = 1u << 3,     // inline TNS descriptor
    olf_wallet           = 1u << 4,     // wallet location
    olf_session_mode     = 1u << 5,     // SYSDBA/SYSOPER/… selector
    olf_edition          = 1u << 6,     // edition-based redefinition
    olf_nls_lang         = 1u << 7,     // NLS_LANG / globalization
    olf_stmt_cache       = 1u << 8,     // statement cache size

    // ── presets ──────────────────────────────────────────────────────
    //   easy connect: host + port + service_name (+ optional SID)
    olf_easy_connect     = olf_service_name | olf_session_mode,
    //   TNS: tns_alias + session_mode
    olf_tns              = olf_tns_alias | olf_session_mode,
    //   full
    olf_all              = olf_service_name | olf_sid
                         | olf_tns_alias    | olf_tns_descriptor
                         | olf_wallet       | olf_session_mode
                         | olf_edition      | olf_nls_lang
                         | olf_stmt_cache
};

constexpr unsigned operator|(oracle_login_feat a,
                             oracle_login_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_olf(unsigned            f,
                       oracle_login_feat   bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  EBO MIXINS
// ═══════════════════════════════════════════════════════════════════════════════

namespace oracle_login_mixin {

    // ── service name ─────────────────────────────────────────────────
    template <bool _Enable>
    struct service_name_data
    {};

    template <>
    struct service_name_data<true>
    {
        text_input<tif_validation> service_name;
    };

    // ── SID ──────────────────────────────────────────────────────────
    template <bool _Enable>
    struct sid_data
    {};

    template <>
    struct sid_data<true>
    {
        text_input<tif_validation> sid;
    };

    // ── TNS alias ────────────────────────────────────────────────────
    template <bool _Enable>
    struct tns_alias_data
    {};

    template <>
    struct tns_alias_data<true>
    {
        text_input<tif_validation> tns_alias;
    };

    // ── TNS descriptor ───────────────────────────────────────────────
    template <bool _Enable>
    struct tns_descriptor_data
    {};

    template <>
    struct tns_descriptor_data<true>
    {
        text_input<tif_multiline | tif_validation> tns_descriptor;
    };

    // ── wallet ───────────────────────────────────────────────────────
    template <bool _Enable>
    struct wallet_data
    {};

    template <>
    struct wallet_data<true>
    {
        text_input<tif_validation> wallet_location;
    };

    // ── session mode ─────────────────────────────────────────────────
    template <bool _Enable>
    struct session_mode_data
    {};

    template <>
    struct session_mode_data<true>
    {
        djinterp::db::ora::oci_session_mode session_mode =
            djinterp::db::ora::oci_session_mode::default_mode;
    };

    // ── edition ──────────────────────────────────────────────────────
    template <bool _Enable>
    struct edition_data
    {};

    template <>
    struct edition_data<true>
    {
        std::string edition;
    };

    // ── NLS_LANG ─────────────────────────────────────────────────────
    template <bool _Enable>
    struct nls_lang_data
    {};

    template <>
    struct nls_lang_data<true>
    {
        std::string nls_lang = "AMERICAN_AMERICA.AL32UTF8";
    };

    // ── statement cache ──────────────────────────────────────────────
    template <bool _Enable>
    struct stmt_cache_data
    {};

    template <>
    struct stmt_cache_data<true>
    {
        std::size_t statement_cache_size = 20;
        bool        use_statement_cache  = true;
    };

}   // namespace oracle_login_mixin


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  ORACLE LOGIN FORM
// ═══════════════════════════════════════════════════════════════════════════════

template <unsigned _LoginFeat = lf_user_pass | lf_show_password,
          unsigned _DbFeat    = dlf_network | dlf_ssl,
          unsigned _OraFeat   = olf_easy_connect>
struct oracle_login_form
    : database_login<djinterp::db::database_type::oracle,
                     _LoginFeat,
                     _DbFeat>
    , oracle_login_mixin::service_name_data   <has_olf(_OraFeat, olf_service_name)>
    , oracle_login_mixin::sid_data            <has_olf(_OraFeat, olf_sid)>
    , oracle_login_mixin::tns_alias_data      <has_olf(_OraFeat, olf_tns_alias)>
    , oracle_login_mixin::tns_descriptor_data <has_olf(_OraFeat, olf_tns_descriptor)>
    , oracle_login_mixin::wallet_data         <has_olf(_OraFeat, olf_wallet)>
    , oracle_login_mixin::session_mode_data   <has_olf(_OraFeat, olf_session_mode)>
    , oracle_login_mixin::edition_data        <has_olf(_OraFeat, olf_edition)>
    , oracle_login_mixin::nls_lang_data       <has_olf(_OraFeat, olf_nls_lang)>
    , oracle_login_mixin::stmt_cache_data     <has_olf(_OraFeat, olf_stmt_cache)>
{
    using base_db_login = database_login<
        djinterp::db::database_type::oracle,
        _LoginFeat,
        _DbFeat>;

    static constexpr unsigned ora_features       = _OraFeat;

    static constexpr bool has_service_name       = has_olf(_OraFeat, olf_service_name);
    static constexpr bool has_sid                = has_olf(_OraFeat, olf_sid);
    static constexpr bool has_tns_alias          = has_olf(_OraFeat, olf_tns_alias);
    static constexpr bool has_tns_descriptor     = has_olf(_OraFeat, olf_tns_descriptor);
    static constexpr bool has_wallet             = has_olf(_OraFeat, olf_wallet);
    static constexpr bool has_session_mode       = has_olf(_OraFeat, olf_session_mode);
    static constexpr bool has_edition            = has_olf(_OraFeat, olf_edition);
    static constexpr bool has_nls_lang           = has_olf(_OraFeat, olf_nls_lang);
    static constexpr bool has_stmt_cache         = has_olf(_OraFeat, olf_stmt_cache);

    // ── construction ─────────────────────────────────────────────────
    oracle_login_form() = default;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// odbl_set_service_name
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_service_name(
    oracle_login_form<_LF, _DF, _OF>& ora,
    std::string                        val)
{
    static_assert(has_olf(_OF, olf_service_name),
                  "requires olf_service_name");

    ti_set_value(ora.service_name, std::move(val));

    return;
}

// odbl_set_sid
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_sid(
    oracle_login_form<_LF, _DF, _OF>& ora,
    std::string                        val)
{
    static_assert(has_olf(_OF, olf_sid), "requires olf_sid");

    ti_set_value(ora.sid, std::move(val));

    return;
}

// odbl_set_tns_alias
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_tns_alias(
    oracle_login_form<_LF, _DF, _OF>& ora,
    std::string                        alias)
{
    static_assert(has_olf(_OF, olf_tns_alias),
                  "requires olf_tns_alias");

    ti_set_value(ora.tns_alias, std::move(alias));

    return;
}

// odbl_set_tns_descriptor
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_tns_descriptor(
    oracle_login_form<_LF, _DF, _OF>& ora,
    std::string                        desc)
{
    static_assert(has_olf(_OF, olf_tns_descriptor),
                  "requires olf_tns_descriptor");

    ti_set_value(ora.tns_descriptor, std::move(desc));

    return;
}

// odbl_set_wallet_location
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_wallet_location(
    oracle_login_form<_LF, _DF, _OF>& ora,
    std::string                        path)
{
    static_assert(has_olf(_OF, olf_wallet), "requires olf_wallet");

    ti_set_value(ora.wallet_location, std::move(path));

    return;
}

// odbl_set_session_mode
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_session_mode(
    oracle_login_form<_LF, _DF, _OF>&      ora,
    djinterp::db::ora::oci_session_mode     mode)
{
    static_assert(has_olf(_OF, olf_session_mode),
                  "requires olf_session_mode");

    ora.session_mode = mode;

    return;
}

// odbl_set_edition
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_edition(
    oracle_login_form<_LF, _DF, _OF>& ora,
    std::string                        ed)
{
    static_assert(has_olf(_OF, olf_edition), "requires olf_edition");

    ora.edition = std::move(ed);

    return;
}

// odbl_set_nls_lang
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_nls_lang(
    oracle_login_form<_LF, _DF, _OF>& ora,
    std::string                        nls)
{
    static_assert(has_olf(_OF, olf_nls_lang),
                  "requires olf_nls_lang");

    ora.nls_lang = std::move(nls);

    return;
}

// odbl_set_stmt_cache_size
template <unsigned _LF, unsigned _DF, unsigned _OF>
void odbl_set_stmt_cache_size(
    oracle_login_form<_LF, _DF, _OF>& ora,
    std::size_t                        size)
{
    static_assert(has_olf(_OF, olf_stmt_cache),
                  "requires olf_stmt_cache");

    ora.statement_cache_size = size;
    ora.use_statement_cache  = (size > 0);

    return;
}

// odbl_validate
//   validates Oracle-specific fields, then delegates to
// database_login validation.
template <unsigned _LF, unsigned _DF, unsigned _OF>
bool odbl_validate(oracle_login_form<_LF, _DF, _OF>& ora)
{
    bool valid = true;

    if constexpr (has_olf(_OF, olf_service_name))
    {
        auto r = ti_validate(ora.service_name);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    if constexpr (has_olf(_OF, olf_sid))
    {
        auto r = ti_validate(ora.sid);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    if constexpr (has_olf(_OF, olf_tns_alias))
    {
        auto r = ti_validate(ora.tns_alias);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    if constexpr (has_olf(_OF, olf_tns_descriptor))
    {
        auto r = ti_validate(ora.tns_descriptor);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    if constexpr (has_olf(_OF, olf_wallet))
    {
        auto r = ti_validate(ora.wallet_location);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    if (!dbl_validate(ora))
    {
        valid = false;
    }

    return valid;
}

// odbl_to_config
//   extracts a djinterp::db::ora::ora_connect_config from the form.
template <unsigned _LF, unsigned _DF, unsigned _OF>
djinterp::db::ora::ora_connect_config
odbl_to_config(const oracle_login_form<_LF, _DF, _OF>& ora)
{
    djinterp::db::ora::ora_connect_config cfg;

    // populate the base via database_login
    cfg.base = dbl_to_config(ora);

    if constexpr (has_olf(_OF, olf_service_name))
    {
        cfg.service_name = ora.service_name.value;
    }

    if constexpr (has_olf(_OF, olf_sid))
    {
        cfg.sid = ora.sid.value;
    }

    if constexpr (has_olf(_OF, olf_tns_alias))
    {
        cfg.tns_alias = ora.tns_alias.value;
    }

    if constexpr (has_olf(_OF, olf_tns_descriptor))
    {
        cfg.tns_descriptor = ora.tns_descriptor.value;
    }

    if constexpr (has_olf(_OF, olf_wallet))
    {
        cfg.wallet_location = ora.wallet_location.value;
    }

    if constexpr (has_olf(_OF, olf_session_mode))
    {
        cfg.session_mode = ora.session_mode;
    }

    if constexpr (has_olf(_OF, olf_edition))
    {
        cfg.edition = ora.edition;
    }

    if constexpr (has_olf(_OF, olf_nls_lang))
    {
        cfg.nls_lang = ora.nls_lang;
    }

    if constexpr (has_olf(_OF, olf_stmt_cache))
    {
        cfg.statement_cache_size = ora.statement_cache_size;
        cfg.use_statement_cache  = ora.use_statement_cache;
    }

    return cfg;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace oracle_login_traits {
namespace detail {

    template <typename, typename = void>
    struct has_service_name_member : std::false_type {};
    template <typename _T>
    struct has_service_name_member<_T, std::void_t<
        decltype(std::declval<_T>().service_name)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_sid_member : std::false_type {};
    template <typename _T>
    struct has_sid_member<_T, std::void_t<
        decltype(std::declval<_T>().sid)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_tns_alias_member : std::false_type {};
    template <typename _T>
    struct has_tns_alias_member<_T, std::void_t<
        decltype(std::declval<_T>().tns_alias)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_tns_descriptor_member : std::false_type {};
    template <typename _T>
    struct has_tns_descriptor_member<_T, std::void_t<
        decltype(std::declval<_T>().tns_descriptor)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_wallet_location_member : std::false_type {};
    template <typename _T>
    struct has_wallet_location_member<_T, std::void_t<
        decltype(std::declval<_T>().wallet_location)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_session_mode_member : std::false_type {};
    template <typename _T>
    struct has_session_mode_member<_T, std::void_t<
        decltype(std::declval<_T>().session_mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_edition_member : std::false_type {};
    template <typename _T>
    struct has_edition_member<_T, std::void_t<
        decltype(std::declval<_T>().edition)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_nls_lang_member : std::false_type {};
    template <typename _T>
    struct has_nls_lang_member<_T, std::void_t<
        decltype(std::declval<_T>().nls_lang)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_stmt_cache_size_member : std::false_type {};
    template <typename _T>
    struct has_stmt_cache_size_member<_T, std::void_t<
        decltype(std::declval<_T>().statement_cache_size)
    >> : std::true_type {};

}   // namespace detail

template <typename _T>
inline constexpr bool has_service_name_v =
    detail::has_service_name_member<_T>::value;
template <typename _T>
inline constexpr bool has_sid_v =
    detail::has_sid_member<_T>::value;
template <typename _T>
inline constexpr bool has_tns_alias_v =
    detail::has_tns_alias_member<_T>::value;
template <typename _T>
inline constexpr bool has_tns_descriptor_v =
    detail::has_tns_descriptor_member<_T>::value;
template <typename _T>
inline constexpr bool has_wallet_location_v =
    detail::has_wallet_location_member<_T>::value;
template <typename _T>
inline constexpr bool has_session_mode_v =
    detail::has_session_mode_member<_T>::value;
template <typename _T>
inline constexpr bool has_edition_v =
    detail::has_edition_member<_T>::value;
template <typename _T>
inline constexpr bool has_nls_lang_v =
    detail::has_nls_lang_member<_T>::value;
template <typename _T>
inline constexpr bool has_stmt_cache_size_v =
    detail::has_stmt_cache_size_member<_T>::value;

// is_oracle_login_form
template <typename _Type>
struct is_oracle_login_form : std::conjunction<
    database_login_traits::is_database_login<_Type>,
    std::bool_constant<_Type::db_type ==
        djinterp::db::database_type::oracle>
>
{};

template <typename _T>
inline constexpr bool is_oracle_login_form_v =
    is_oracle_login_form<_T>::value;

// is_tns_oracle_login
template <typename _Type>
struct is_tns_oracle_login : std::conjunction<
    is_oracle_login_form<_Type>,
    std::disjunction<
        detail::has_tns_alias_member<_Type>,
        detail::has_tns_descriptor_member<_Type>
    >
>
{};

template <typename _T>
inline constexpr bool is_tns_oracle_login_v =
    is_tns_oracle_login<_T>::value;

// is_wallet_oracle_login
template <typename _Type>
struct is_wallet_oracle_login : std::conjunction<
    is_oracle_login_form<_Type>,
    detail::has_wallet_location_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_wallet_oracle_login_v =
    is_wallet_oracle_login<_T>::value;

}   // namespace oracle_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_ORACLE_LOGIN_FORM_
