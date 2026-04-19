/*******************************************************************************
* uxoxo [component]                                        database_login.hpp
*
* Generic database login form:
*   A framework-agnostic, pure-data database login template.  Derives from
* login_form (username/password) and adds the connection fields common to
* ALL database vendors — host, port, database name, SSL, schema, and
* connection-string URI.  Each of these is gated by a compile-time feature
* flag so that vendor-specific derived forms can enable only what applies.
*
*   The template is parameterized on database_type from database_common.hpp
* so that vendor_traits can supply compile-time defaults (default port,
* SSL support, embedded flag, etc.).  A free function db_to_config()
* extracts a connection_config from a database_login for hand-off to the
* connection layer.
*
*   Vendor-specific forms (sqlite_database_login, mariadb_database_login,
* etc.) will derive from or alias this template with the appropriate
* feature-flag combination.
*
*   Feature composition follows the same EBO-mixin bitfield pattern used
* by input_control, text_input, and login_form.
*
* Contents:
*   §1  Feature flags (database_login_feat)
*   §2  EBO mixins
*   §3  database_login struct
*   §4  Free functions
*   §5  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/database_login.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_DATABASE_LOGIN_
#define  UXOXO_COMPONENT_DATABASE_LOGIN_ 1

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include <uxoxo>
#include <component/view_common.hpp>
#include <component/text_input.hpp>
#include <component/input_control.hpp>
#include <component/login_form.hpp>
#include <database/database_common.hpp>
#include <database/database_connection_template.hpp>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  DATABASE LOGIN FEATURE FLAGS
// ═══════════════════════════════════════════════════════════════════════════════
//   These flags are in their own unsigned parameter (_DbFeat), separate
// from the login_form flags (_LoginFeat).  They start at bit 0.

enum database_login_feat : unsigned
{
    dlf_none          = 0,
    dlf_host          = 1u << 0,     // host text field
    dlf_port          = 1u << 1,     // port spinner / numeric input
    dlf_database_name = 1u << 2,     // database / catalog name
    dlf_schema        = 1u << 3,     // schema / namespace
    dlf_ssl           = 1u << 4,     // SSL/TLS toggle + cert paths
    dlf_uri           = 1u << 5,     // connection-string URI (alternative)
    dlf_charset       = 1u << 6,     // character set selector

    // ── common presets ───────────────────────────────────────────────
    //   network database (host + port + db name)
    dlf_network       = dlf_host | dlf_port | dlf_database_name,
    //   full network (host + port + db + schema + ssl + charset)
    dlf_network_full  = dlf_network | dlf_schema | dlf_ssl | dlf_charset,
    //   embedded database (db name only, path-as-database)
    dlf_embedded      = dlf_database_name,

    dlf_all           = dlf_host    | dlf_port    | dlf_database_name
                      | dlf_schema  | dlf_ssl     | dlf_uri
                      | dlf_charset
};

constexpr unsigned operator|(database_login_feat a,
                             database_login_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_dlf(unsigned             f,
                       database_login_feat  bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  EBO MIXINS
// ═══════════════════════════════════════════════════════════════════════════════

namespace db_login_mixin {

    // ── host field ───────────────────────────────────────────────────
    template <bool _Enable>
    struct host_data
    {};

    template <>
    struct host_data<true>
    {
        text_input<tif_validation> host;
    };

    // ── port field ───────────────────────────────────────────────────
    template <bool _Enable>
    struct port_data
    {};

    template <>
    struct port_data<true>
    {
        std::uint16_t port = 0;
    };

    // ── database name field ──────────────────────────────────────────
    template <bool _Enable>
    struct database_name_data
    {};

    template <>
    struct database_name_data<true>
    {
        text_input<tif_validation> database_name;
    };

    // ── schema field ─────────────────────────────────────────────────
    template <bool _Enable>
    struct schema_data
    {};

    template <>
    struct schema_data<true>
    {
        text_input<> schema;
    };

    // ── SSL/TLS ──────────────────────────────────────────────────────
    template <bool _Enable>
    struct ssl_data
    {};

    template <>
    struct ssl_data<true>
    {
        bool        enable_ssl  = false;
        bool        verify_ssl  = false;
        std::string ssl_ca;
        std::string ssl_cert;
        std::string ssl_key;
    };

    // ── connection-string URI ────────────────────────────────────────
    template <bool _Enable>
    struct uri_data
    {};

    template <>
    struct uri_data<true>
    {
        text_input<tif_validation> connection_uri;
        bool                       use_uri = false;  // prefer URI over fields
    };

    // ── charset ──────────────────────────────────────────────────────
    template <bool _Enable>
    struct charset_data
    {};

    template <>
    struct charset_data<true>
    {
        std::string charset;
    };

}   // namespace db_login_mixin


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  DATABASE LOGIN
// ═══════════════════════════════════════════════════════════════════════════════
//   _Vendor      djinterp::db::database_type enum value.  Used to pull
//                compile-time defaults from vendor_traits.
//   _LoginFeat   bitwise OR of login_form_feat flags (username/password/
//                remember-me/show-password).
//   _DbFeat      bitwise OR of database_login_feat flags (host/port/
//                database name/schema/ssl/uri/charset).
//
//   The form derives from login_form<_LoginFeat> and adds the
// database-specific mixins.

template <djinterp::db::database_type _Vendor,
          unsigned                    _LoginFeat = lf_user_pass,
          unsigned                    _DbFeat    = dlf_network>
struct database_login
    : login_form<_LoginFeat>
    , db_login_mixin::host_data          <has_dlf(_DbFeat, dlf_host)>
    , db_login_mixin::port_data          <has_dlf(_DbFeat, dlf_port)>
    , db_login_mixin::database_name_data <has_dlf(_DbFeat, dlf_database_name)>
    , db_login_mixin::schema_data        <has_dlf(_DbFeat, dlf_schema)>
    , db_login_mixin::ssl_data           <has_dlf(_DbFeat, dlf_ssl)>
    , db_login_mixin::uri_data           <has_dlf(_DbFeat, dlf_uri)>
    , db_login_mixin::charset_data       <has_dlf(_DbFeat, dlf_charset)>
{
    using base_login   = login_form<_LoginFeat>;
    using vendor       = djinterp::db::vendor_traits<_Vendor>;

    static constexpr djinterp::db::database_type db_type = _Vendor;
    static constexpr unsigned db_features     = _DbFeat;

    static constexpr bool has_host            = has_dlf(_DbFeat, dlf_host);
    static constexpr bool has_port            = has_dlf(_DbFeat, dlf_port);
    static constexpr bool has_database_name   = has_dlf(_DbFeat, dlf_database_name);
    static constexpr bool has_schema          = has_dlf(_DbFeat, dlf_schema);
    static constexpr bool has_ssl             = has_dlf(_DbFeat, dlf_ssl);
    static constexpr bool has_uri             = has_dlf(_DbFeat, dlf_uri);
    static constexpr bool has_charset         = has_dlf(_DbFeat, dlf_charset);

    // ── construction ─────────────────────────────────────────────────
    //   default constructor applies vendor defaults where appropriate.
    database_login()
    {
        apply_vendor_defaults_();
    }

private:
    void apply_vendor_defaults_()
    {
        if constexpr (has_host)
        {
            auto cfg = vendor::make_default_config();
            ti_set_value(this->host, std::move(cfg.host));
        }

        if constexpr (has_port)
        {
            this->port = vendor::default_port;
        }

        if constexpr (has_ssl)
        {
            this->enable_ssl = vendor::supports_ssl;
        }

        if constexpr (has_charset)
        {
            auto cfg = vendor::make_default_config();

            if (cfg.charset.has_value())
            {
                this->charset = std::move(cfg.charset.value());
            }
        }

        if constexpr (has_schema)
        {
            auto cfg = vendor::make_default_config();

            if (cfg.schema.has_value())
            {
                ti_set_value(this->schema,
                             std::move(cfg.schema.value()));
            }
        }
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// dbl_set_host
//   sets the host field value (requires dlf_host).
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_set_host(database_login<_V, _LF, _DF>& dbl,
                  std::string                    val)
{
    static_assert(has_dlf(_DF, dlf_host), "requires dlf_host");

    ti_set_value(dbl.host, std::move(val));

    return;
}

// dbl_set_port
//   sets the port value (requires dlf_port).
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_set_port(database_login<_V, _LF, _DF>& dbl,
                  std::uint16_t                  val)
{
    static_assert(has_dlf(_DF, dlf_port), "requires dlf_port");

    dbl.port = val;

    return;
}

// dbl_set_database_name
//   sets the database name (requires dlf_database_name).
// For embedded databases like SQLite this is the file path.
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_set_database_name(database_login<_V, _LF, _DF>& dbl,
                           std::string                    val)
{
    static_assert(has_dlf(_DF, dlf_database_name),
                  "requires dlf_database_name");

    ti_set_value(dbl.database_name, std::move(val));

    return;
}

// dbl_set_schema
//   sets the schema / namespace (requires dlf_schema).
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_set_schema(database_login<_V, _LF, _DF>& dbl,
                    std::string                    val)
{
    static_assert(has_dlf(_DF, dlf_schema), "requires dlf_schema");

    ti_set_value(dbl.schema, std::move(val));

    return;
}

// dbl_set_ssl
//   enables or disables SSL (requires dlf_ssl).
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_set_ssl(database_login<_V, _LF, _DF>& dbl,
                 bool                           on)
{
    static_assert(has_dlf(_DF, dlf_ssl), "requires dlf_ssl");

    dbl.enable_ssl = on;

    return;
}

// dbl_set_ssl_certs
//   sets SSL certificate paths (requires dlf_ssl).
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_set_ssl_certs(database_login<_V, _LF, _DF>& dbl,
                       std::string                    ca,
                       std::string                    cert,
                       std::string                    key)
{
    static_assert(has_dlf(_DF, dlf_ssl), "requires dlf_ssl");

    dbl.ssl_ca   = std::move(ca);
    dbl.ssl_cert = std::move(cert);
    dbl.ssl_key  = std::move(key);

    return;
}

// dbl_set_connection_uri
//   sets the connection URI and switches to URI mode
// (requires dlf_uri).
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_set_connection_uri(database_login<_V, _LF, _DF>& dbl,
                            std::string                    uri)
{
    static_assert(has_dlf(_DF, dlf_uri), "requires dlf_uri");

    ti_set_value(dbl.connection_uri, std::move(uri));
    dbl.use_uri = true;

    return;
}

// dbl_set_charset
//   sets the character set (requires dlf_charset).
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_set_charset(database_login<_V, _LF, _DF>& dbl,
                     std::string                    val)
{
    static_assert(has_dlf(_DF, dlf_charset), "requires dlf_charset");

    dbl.charset = std::move(val);

    return;
}

// dbl_validate
//   validates all database fields, then delegates to login_form
// validation.  Returns true if everything is valid.
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
bool dbl_validate(database_login<_V, _LF, _DF>& dbl)
{
    bool valid = true;

    // if URI mode is active and present, skip discrete field validation
    if constexpr (has_dlf(_DF, dlf_uri))
    {
        if (dbl.use_uri)
        {
            auto r = ti_validate(dbl.connection_uri);

            return r.result == validation_result::valid;
        }
    }

    if constexpr (has_dlf(_DF, dlf_host))
    {
        auto r = ti_validate(dbl.host);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    if constexpr (has_dlf(_DF, dlf_database_name))
    {
        auto r = ti_validate(dbl.database_name);

        if (r.result != validation_result::valid)
        {
            valid = false;
        }
    }

    // delegate username/password to login_form
    if (!lf_validate(dbl))
    {
        valid = false;
    }

    return valid;
}

// dbl_to_config
//   extracts a djinterp::db::connection_config from the form's
// current field values.
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
djinterp::db::connection_config
dbl_to_config(const database_login<_V, _LF, _DF>& dbl)
{
    djinterp::db::connection_config cfg;

    // start from vendor defaults
    cfg = database_login<_V, _LF, _DF>::vendor::make_default_config();

    if constexpr (has_dlf(_DF, dlf_host))
    {
        cfg.host = dbl.host.value;
    }

    if constexpr (has_dlf(_DF, dlf_port))
    {
        cfg.port = dbl.port;
    }

    if constexpr (has_dlf(_DF, dlf_database_name))
    {
        cfg.database = dbl.database_name.value;
    }

    if constexpr (has_dlf(_DF, dlf_schema))
    {
        cfg.schema = dbl.schema.value;
    }

    if constexpr (has_lf(_LF, lf_username))
    {
        cfg.username = dbl.username.value;
    }

    if constexpr (has_lf(_LF, lf_password))
    {
        cfg.password = dbl.password.value;
    }

    if constexpr (has_dlf(_DF, dlf_ssl))
    {
        cfg.enable_ssl = dbl.enable_ssl;
        cfg.verify_ssl = dbl.verify_ssl;
        cfg.ssl_ca     = dbl.ssl_ca;
        cfg.ssl_cert   = dbl.ssl_cert;
        cfg.ssl_key    = dbl.ssl_key;
    }

    if constexpr (has_dlf(_DF, dlf_charset))
    {
        cfg.charset = dbl.charset;
    }

    return cfg;
}

// dbl_reset_to_defaults
//   resets all fields to vendor defaults and clears errors.
template <djinterp::db::database_type _V, unsigned _LF, unsigned _DF>
void dbl_reset_to_defaults(database_login<_V, _LF, _DF>& dbl)
{
    dbl = database_login<_V, _LF, _DF>{};

    return;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace database_login_traits {
namespace detail {

    template <typename, typename = void>
    struct has_host_member : std::false_type {};
    template <typename _T>
    struct has_host_member<_T, std::void_t<
        decltype(std::declval<_T>().host)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_port_member : std::false_type {};
    template <typename _T>
    struct has_port_member<_T, std::void_t<
        decltype(std::declval<_T>().port)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_database_name_member : std::false_type {};
    template <typename _T>
    struct has_database_name_member<_T, std::void_t<
        decltype(std::declval<_T>().database_name)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_schema_member : std::false_type {};
    template <typename _T>
    struct has_schema_member<_T, std::void_t<
        decltype(std::declval<_T>().schema)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_enable_ssl_member : std::false_type {};
    template <typename _T>
    struct has_enable_ssl_member<_T, std::void_t<
        decltype(std::declval<_T>().enable_ssl)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_connection_uri_member : std::false_type {};
    template <typename _T>
    struct has_connection_uri_member<_T, std::void_t<
        decltype(std::declval<_T>().connection_uri)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_charset_member : std::false_type {};
    template <typename _T>
    struct has_charset_member<_T, std::void_t<
        decltype(std::declval<_T>().charset)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_db_type_constant : std::false_type {};
    template <typename _T>
    struct has_db_type_constant<_T, std::void_t<
        decltype(_T::db_type)
    >> : std::true_type {};

}   // namespace detail

template <typename _T>
inline constexpr bool has_host_v =
    detail::has_host_member<_T>::value;
template <typename _T>
inline constexpr bool has_port_v =
    detail::has_port_member<_T>::value;
template <typename _T>
inline constexpr bool has_database_name_v =
    detail::has_database_name_member<_T>::value;
template <typename _T>
inline constexpr bool has_schema_v =
    detail::has_schema_member<_T>::value;
template <typename _T>
inline constexpr bool has_enable_ssl_v =
    detail::has_enable_ssl_member<_T>::value;
template <typename _T>
inline constexpr bool has_connection_uri_v =
    detail::has_connection_uri_member<_T>::value;
template <typename _T>
inline constexpr bool has_charset_v =
    detail::has_charset_member<_T>::value;
template <typename _T>
inline constexpr bool has_db_type_v =
    detail::has_db_type_constant<_T>::value;

// is_database_login
//   type trait: has db_type + submitting + error_message + on_submit,
// and at least one of host or database_name.
template <typename _Type>
struct is_database_login : std::conjunction<
    detail::has_db_type_constant<_Type>,
    login_form_traits::detail::has_submitting_member<_Type>,
    login_form_traits::detail::has_error_message_member<_Type>,
    login_form_traits::detail::has_on_submit_member<_Type>,
    std::disjunction<
        detail::has_host_member<_Type>,
        detail::has_database_name_member<_Type>
    >
>
{};

template <typename _T>
inline constexpr bool is_database_login_v =
    is_database_login<_T>::value;

// is_network_database_login
//   type trait: database_login with host + port.
template <typename _Type>
struct is_network_database_login : std::conjunction<
    is_database_login<_Type>,
    detail::has_host_member<_Type>,
    detail::has_port_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_network_database_login_v =
    is_network_database_login<_T>::value;

// is_embedded_database_login
//   type trait: database_login with database_name but no host.
template <typename _Type>
struct is_embedded_database_login : std::conjunction<
    is_database_login<_Type>,
    detail::has_database_name_member<_Type>,
    std::negation<detail::has_host_member<_Type>>
>
{};

template <typename _T>
inline constexpr bool is_embedded_database_login_v =
    is_embedded_database_login<_T>::value;

// is_ssl_database_login
template <typename _Type>
struct is_ssl_database_login : std::conjunction<
    is_database_login<_Type>,
    detail::has_enable_ssl_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_ssl_database_login_v =
    is_ssl_database_login<_T>::value;

// is_uri_database_login
template <typename _Type>
struct is_uri_database_login : std::conjunction<
    is_database_login<_Type>,
    detail::has_connection_uri_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_uri_database_login_v =
    is_uri_database_login<_T>::value;

}   // namespace database_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DATABASE_LOGIN_
