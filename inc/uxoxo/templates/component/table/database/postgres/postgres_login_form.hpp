/*******************************************************************************
* uxoxo [component]                                   postgres_login_form.hpp
*
* PostgreSQL database login form:
*   Vendor-specific login form for PostgreSQL connections.  Derives from
* database_login<database_type::postgresql, ...> and adds PG-specific
* fields: SSL mode enum, application name, search path, client
* encoding, statement timeout, and binary result mode.
*
*   Fields inherited from database_login (all enabled by default):
*     host, port, database_name, schema, SSL, charset, URI,
*     username, password, remember-me, show-password
*
*   PostgreSQL-specific fields (gated by pg_login_feat):
*     pgf_ssl_mode          pg_ssl_mode enum selector
*     pgf_app_name          application_name for pg_stat_activity
*     pgf_search_path       search_path (overrides schema)
*     pgf_client_encoding   client_encoding (overrides charset)
*     pgf_stmt_timeout      statement_timeout_ms
*     pgf_binary_results    use binary result format
*     pgf_options           extra libpq options string
*
*   The free function pgdbl_to_config() extracts a pg_connect_config.
*
* Contents:
*   §1  Feature flags (pg_login_feat)
*   §2  EBO mixins
*   §3  postgres_login_form struct
*   §4  Free functions
*   §5  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/postgres_login_form.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_POSTGRES_LOGIN_FORM_
#define  UXOXO_COMPONENT_POSTGRES_LOGIN_FORM_ 1

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
#include <database/postgresql/postgres.hpp>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  POSTGRES LOGIN FEATURE FLAGS
// ═══════════════════════════════════════════════════════════════════════════════

enum pg_login_feat : unsigned
{
    pgf_none             = 0,
    pgf_ssl_mode         = 1u << 0,     // pg_ssl_mode enum
    pgf_app_name         = 1u << 1,     // application_name
    pgf_search_path      = 1u << 2,     // search_path
    pgf_client_encoding  = 1u << 3,     // client_encoding
    pgf_stmt_timeout     = 1u << 4,     // statement_timeout_ms
    pgf_binary_results   = 1u << 5,     // binary result mode
    pgf_options          = 1u << 6,     // extra libpq options

    // ── presets ──────────────────────────────────────────────────────
    pgf_basic            = pgf_ssl_mode | pgf_app_name,
    pgf_all              = pgf_ssl_mode    | pgf_app_name
                         | pgf_search_path | pgf_client_encoding
                         | pgf_stmt_timeout | pgf_binary_results
                         | pgf_options
};

constexpr unsigned operator|(pg_login_feat a,
                             pg_login_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_pgf(unsigned        f,
                       pg_login_feat   bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  EBO MIXINS
// ═══════════════════════════════════════════════════════════════════════════════

namespace pg_login_mixin {

    // ── SSL mode ─────────────────────────────────────────────────────
    template <bool _Enable>
    struct ssl_mode_data
    {};

    template <>
    struct ssl_mode_data<true>
    {
        djinterp::db::pg_ssl_mode ssl_mode =
            djinterp::db::pg_ssl_mode::prefer;
    };

    // ── application name ─────────────────────────────────────────────
    template <bool _Enable>
    struct app_name_data
    {};

    template <>
    struct app_name_data<true>
    {
        text_input<> application_name;
    };

    // ── search path ──────────────────────────────────────────────────
    template <bool _Enable>
    struct search_path_data
    {};

    template <>
    struct search_path_data<true>
    {
        text_input<> search_path;
    };

    // ── client encoding ──────────────────────────────────────────────
    template <bool _Enable>
    struct client_encoding_data
    {};

    template <>
    struct client_encoding_data<true>
    {
        std::string client_encoding = "UTF8";
    };

    // ── statement timeout ────────────────────────────────────────────
    template <bool _Enable>
    struct stmt_timeout_data
    {};

    template <>
    struct stmt_timeout_data<true>
    {
        int statement_timeout_ms = 0;       // 0 = no limit
    };

    // ── binary results ───────────────────────────────────────────────
    template <bool _Enable>
    struct binary_results_data
    {};

    template <>
    struct binary_results_data<true>
    {
        bool use_binary_results = false;
    };

    // ── extra options ────────────────────────────────────────────────
    template <bool _Enable>
    struct options_data
    {};

    template <>
    struct options_data<true>
    {
        text_input<> options;
    };

}   // namespace pg_login_mixin


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  POSTGRES LOGIN FORM
// ═══════════════════════════════════════════════════════════════════════════════

template <unsigned _LoginFeat = lf_user_pass | lf_show_password,
          unsigned _DbFeat    = dlf_network_full | dlf_uri,
          unsigned _PgFeat    = pgf_basic>
struct postgres_login_form
    : database_login<djinterp::db::database_type::postgresql,
                     _LoginFeat,
                     _DbFeat>
    , pg_login_mixin::ssl_mode_data        <has_pgf(_PgFeat, pgf_ssl_mode)>
    , pg_login_mixin::app_name_data        <has_pgf(_PgFeat, pgf_app_name)>
    , pg_login_mixin::search_path_data     <has_pgf(_PgFeat, pgf_search_path)>
    , pg_login_mixin::client_encoding_data <has_pgf(_PgFeat, pgf_client_encoding)>
    , pg_login_mixin::stmt_timeout_data    <has_pgf(_PgFeat, pgf_stmt_timeout)>
    , pg_login_mixin::binary_results_data  <has_pgf(_PgFeat, pgf_binary_results)>
    , pg_login_mixin::options_data         <has_pgf(_PgFeat, pgf_options)>
{
    using base_db_login = database_login<
        djinterp::db::database_type::postgresql,
        _LoginFeat,
        _DbFeat>;

    static constexpr unsigned pg_features         = _PgFeat;

    static constexpr bool has_ssl_mode            = has_pgf(_PgFeat, pgf_ssl_mode);
    static constexpr bool has_app_name            = has_pgf(_PgFeat, pgf_app_name);
    static constexpr bool has_search_path         = has_pgf(_PgFeat, pgf_search_path);
    static constexpr bool has_client_encoding     = has_pgf(_PgFeat, pgf_client_encoding);
    static constexpr bool has_stmt_timeout        = has_pgf(_PgFeat, pgf_stmt_timeout);
    static constexpr bool has_binary_results      = has_pgf(_PgFeat, pgf_binary_results);
    static constexpr bool has_options             = has_pgf(_PgFeat, pgf_options);

    // ── construction ─────────────────────────────────────────────────
    postgres_login_form() = default;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// pgdbl_set_ssl_mode
template <unsigned _LF, unsigned _DF, unsigned _PF>
void pgdbl_set_ssl_mode(
    postgres_login_form<_LF, _DF, _PF>& pg,
    djinterp::db::pg_ssl_mode           mode)
{
    static_assert(has_pgf(_PF, pgf_ssl_mode),
                  "requires pgf_ssl_mode");

    pg.ssl_mode = mode;

    return;
}

// pgdbl_set_app_name
template <unsigned _LF, unsigned _DF, unsigned _PF>
void pgdbl_set_app_name(
    postgres_login_form<_LF, _DF, _PF>& pg,
    std::string                          name)
{
    static_assert(has_pgf(_PF, pgf_app_name),
                  "requires pgf_app_name");

    ti_set_value(pg.application_name, std::move(name));

    return;
}

// pgdbl_set_search_path
template <unsigned _LF, unsigned _DF, unsigned _PF>
void pgdbl_set_search_path(
    postgres_login_form<_LF, _DF, _PF>& pg,
    std::string                          path)
{
    static_assert(has_pgf(_PF, pgf_search_path),
                  "requires pgf_search_path");

    ti_set_value(pg.search_path, std::move(path));

    return;
}

// pgdbl_set_client_encoding
template <unsigned _LF, unsigned _DF, unsigned _PF>
void pgdbl_set_client_encoding(
    postgres_login_form<_LF, _DF, _PF>& pg,
    std::string                          enc)
{
    static_assert(has_pgf(_PF, pgf_client_encoding),
                  "requires pgf_client_encoding");

    pg.client_encoding = std::move(enc);

    return;
}

// pgdbl_set_stmt_timeout
template <unsigned _LF, unsigned _DF, unsigned _PF>
void pgdbl_set_stmt_timeout(
    postgres_login_form<_LF, _DF, _PF>& pg,
    int                                  ms)
{
    static_assert(has_pgf(_PF, pgf_stmt_timeout),
                  "requires pgf_stmt_timeout");

    pg.statement_timeout_ms = ms;

    return;
}

// pgdbl_toggle_binary_results
template <unsigned _LF, unsigned _DF, unsigned _PF>
void pgdbl_toggle_binary_results(
    postgres_login_form<_LF, _DF, _PF>& pg)
{
    static_assert(has_pgf(_PF, pgf_binary_results),
                  "requires pgf_binary_results");

    pg.use_binary_results = !pg.use_binary_results;

    return;
}

// pgdbl_set_options
template <unsigned _LF, unsigned _DF, unsigned _PF>
void pgdbl_set_options(
    postgres_login_form<_LF, _DF, _PF>& pg,
    std::string                          opts)
{
    static_assert(has_pgf(_PF, pgf_options),
                  "requires pgf_options");

    ti_set_value(pg.options, std::move(opts));

    return;
}

// pgdbl_validate
//   validates PG-specific fields, then delegates to database_login.
template <unsigned _LF, unsigned _DF, unsigned _PF>
bool pgdbl_validate(postgres_login_form<_LF, _DF, _PF>& pg)
{
    // no PG-specific text fields need validation beyond what
    // database_login already covers; delegate directly.

    return dbl_validate(pg);
}

// pgdbl_to_config
//   extracts a djinterp::db::pg_connect_config from the form.
template <unsigned _LF, unsigned _DF, unsigned _PF>
djinterp::db::pg_connect_config
pgdbl_to_config(const postgres_login_form<_LF, _DF, _PF>& pg)
{
    djinterp::db::pg_connect_config cfg;

    // populate the base via database_login
    cfg.base = dbl_to_config(pg);

    // URI → connection_string
    if constexpr (has_dlf(_DF, dlf_uri))
    {
        if (pg.use_uri)
        {
            cfg.connection_string = pg.connection_uri.value;
        }
    }

    // schema → search_path (base level)
    if constexpr (has_dlf(_DF, dlf_schema))
    {
        cfg.search_path = pg.schema.value;
    }

    // PG-specific search_path overrides base schema
    if constexpr (has_pgf(_PF, pgf_search_path))
    {
        if (!pg.search_path.value.empty())
        {
            cfg.search_path = pg.search_path.value;
        }
    }

    if constexpr (has_pgf(_PF, pgf_ssl_mode))
    {
        cfg.ssl_mode = pg.ssl_mode;
    }

    if constexpr (has_pgf(_PF, pgf_app_name))
    {
        cfg.application_name = pg.application_name.value;
    }

    if constexpr (has_pgf(_PF, pgf_client_encoding))
    {
        cfg.client_encoding = pg.client_encoding;
    }
    else if constexpr (has_dlf(_DF, dlf_charset))
    {
        // fall back to charset from database_login
        cfg.client_encoding = pg.charset;
    }

    if constexpr (has_pgf(_PF, pgf_stmt_timeout))
    {
        cfg.statement_timeout_ms = pg.statement_timeout_ms;
    }

    if constexpr (has_pgf(_PF, pgf_binary_results))
    {
        cfg.use_binary_results = pg.use_binary_results;
    }

    if constexpr (has_pgf(_PF, pgf_options))
    {
        cfg.options = pg.options.value;
    }

    return cfg;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace pg_login_traits {
namespace detail {

    template <typename, typename = void>
    struct has_ssl_mode_member : std::false_type {};
    template <typename _T>
    struct has_ssl_mode_member<_T, std::void_t<
        decltype(std::declval<_T>().ssl_mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_application_name_member : std::false_type {};
    template <typename _T>
    struct has_application_name_member<_T, std::void_t<
        decltype(std::declval<_T>().application_name)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_search_path_member : std::false_type {};
    template <typename _T>
    struct has_search_path_member<_T, std::void_t<
        decltype(std::declval<_T>().search_path)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_client_encoding_member : std::false_type {};
    template <typename _T>
    struct has_client_encoding_member<_T, std::void_t<
        decltype(std::declval<_T>().client_encoding)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_stmt_timeout_member : std::false_type {};
    template <typename _T>
    struct has_stmt_timeout_member<_T, std::void_t<
        decltype(std::declval<_T>().statement_timeout_ms)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_binary_results_member : std::false_type {};
    template <typename _T>
    struct has_binary_results_member<_T, std::void_t<
        decltype(std::declval<_T>().use_binary_results)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_options_member : std::false_type {};
    template <typename _T>
    struct has_options_member<_T, std::void_t<
        decltype(std::declval<_T>().options)
    >> : std::true_type {};

}   // namespace detail

template <typename _T>
inline constexpr bool has_ssl_mode_v =
    detail::has_ssl_mode_member<_T>::value;
template <typename _T>
inline constexpr bool has_application_name_v =
    detail::has_application_name_member<_T>::value;
template <typename _T>
inline constexpr bool has_search_path_v =
    detail::has_search_path_member<_T>::value;
template <typename _T>
inline constexpr bool has_client_encoding_v =
    detail::has_client_encoding_member<_T>::value;
template <typename _T>
inline constexpr bool has_stmt_timeout_v =
    detail::has_stmt_timeout_member<_T>::value;
template <typename _T>
inline constexpr bool has_binary_results_v =
    detail::has_binary_results_member<_T>::value;
template <typename _T>
inline constexpr bool has_options_v =
    detail::has_options_member<_T>::value;

// is_postgres_login_form
template <typename _Type>
struct is_postgres_login_form : std::conjunction<
    database_login_traits::is_database_login<_Type>,
    std::bool_constant<_Type::db_type ==
        djinterp::db::database_type::postgresql>
>
{};

template <typename _T>
inline constexpr bool is_postgres_login_form_v =
    is_postgres_login_form<_T>::value;

}   // namespace pg_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_POSTGRES_LOGIN_FORM_
