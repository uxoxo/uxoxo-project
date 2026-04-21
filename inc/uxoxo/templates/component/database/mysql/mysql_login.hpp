/*******************************************************************************
* uxoxo [component]                                               mysql_login.hpp
*
* Oracle MySQL login form:
*   Vendor-specialized extension of database_login<database_type::mysql, _F>.
* Adds slots for the MySQL-specific connection surface (SSL mode enum,
* auth plugin, X Protocol endpoint, session tracking, cleartext plugin,
* statement-cache size, connect attributes) plus the MySQL-family common
* extras (unix socket, init command, compression, multi-statement,
* local-infile).
*
*   The base identity + network + generic-target slots are inherited
* intact from dbl_form_base<_Feat> via the form_append_slots_t utility
* in database_login.hpp — so the existing dbl_tag::* accessors, dbl_*
* free functions, and fm_* / tc_* form-builder vocabulary all work on
* mysql_login unchanged.
*
*   ml_to_config() extracts a djinterp::database::mysql_connect_config
* (Oracle MySQL) from the form's current field values, layering
* MySQL-common and MySQL-specific overlays on top of the generic
* connection_config produced by dbl_to_config-equivalent logic.
*
*   Zero overhead:  every vendor slot is EBO-collapsed when its
* feature bit is clear; every extraction and validation branch
* resolves at compile time.
*
* Contents:
*   1  vendor value-type aggregates
*   2  feature flags (mysql_login_feat)
*   3  field tags (mylf_tag)
*   4  slot-list extension
*   5  mysql_login struct
*   6  free functions (setters, validate, to_config)
*   7  traits (is_mysql_login)
*
*
* path:      /inc/uxoxo/component/mysql_login.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_MYSQL_LOGIN_
#define  UXOXO_COMPONENT_MYSQL_LOGIN_ 1

// std
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/db/mysql.hpp>
#include <djinterp/core/db/mysql_common.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./database_login.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  VENDOR VALUE-TYPE AGGREGATES
// ===============================================================================
//   Bundled value types for composite MySQL slots whose logical unit is
// larger than a single primitive.  Grouping related fields into one
// struct keeps the slot list compact and, when the slot is disabled,
// reclaims the entire group's storage via EBO.

// mysql_ssl_settings
//   struct: MySQL-aware SSL/TLS configuration bundle.  Supersedes the
// generic ssl_settings when paired with mylf_ssl_mode; carries the
// explicit ssl_mode enum, CA/cert/key paths, and the TLS
// version / ciphersuite strings consumed by mysql_connect_config.
struct mysql_ssl_settings
{
    djinterp::database::mysql_ssl_mode  mode
        = djinterp::database::mysql_ssl_mode::preferred;
    std::string                   ca;
    std::string                   cert;
    std::string                   key;
    std::string                   tls_version;       // e.g. "TLSv1.2,TLSv1.3"
    std::string                   tls_ciphersuites;
};

// x_protocol_settings
//   struct: MySQL X Protocol endpoint configuration.  Paired with the
// classic host from the base dlf_host slot; only the port and the
// enable toggle differ from the SQL-protocol endpoint.
struct x_protocol_settings
{
    bool           enabled = false;
    std::uint16_t  port    = 33060;
};




// ===============================================================================
//  2  MYSQL LOGIN FEATURE FLAGS
// ===============================================================================
//   Non-overlapping with database_login_feat (bits 0-14).  The
// MySQL-common extras occupy bits 16-21; MySQL-specific extensions
// occupy bits 24-29.  Callers combine dlf_* and mylf_* bits with
// operator| freely.

enum mysql_login_feat : unsigned
{
    mylf_none                = 0,

    // -- mysql-family common extras ----------------------------------
    mylf_unix_socket         = 1u << 16,
    mylf_init_command        = 1u << 17,
    mylf_compression         = 1u << 18,
    mylf_multi_stmt          = 1u << 19,
    mylf_local_infile        = 1u << 20,
    mylf_connect_attrs       = 1u << 21,

    // -- mysql-specific ----------------------------------------------
    mylf_ssl_mode            = 1u << 24,
    mylf_auth_plugin         = 1u << 25,
    mylf_x_protocol          = 1u << 26,
    mylf_session_tracking    = 1u << 27,
    mylf_cleartext_plugin    = 1u << 28,
    mylf_stmt_cache          = 1u << 29,

    // -- presets -----------------------------------------------------
    mylf_common_extras       = mylf_init_command | mylf_compression,
    mylf_typical             = mylf_ssl_mode     | mylf_common_extras,
    mylf_enterprise          = mylf_typical      | mylf_auth_plugin
                             | mylf_session_tracking
                             | mylf_connect_attrs,

    mylf_all                 = mylf_unix_socket    | mylf_init_command
                             | mylf_compression    | mylf_multi_stmt
                             | mylf_local_infile   | mylf_connect_attrs
                             | mylf_ssl_mode       | mylf_auth_plugin
                             | mylf_x_protocol     | mylf_session_tracking
                             | mylf_cleartext_plugin
                             | mylf_stmt_cache
};

constexpr unsigned
operator|(
    mysql_login_feat _a,
    mysql_login_feat _b
) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr unsigned
operator|(
    database_login_feat _a,
    mysql_login_feat    _b
) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr unsigned
operator|(
    mysql_login_feat    _a,
    database_login_feat _b
) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

// has_mylf
//   function: compile-time test of a mysql_login feature bit against a
// raw feature-mask.
constexpr bool
has_mylf(
    unsigned         _f,
    mysql_login_feat _bit
) noexcept
{
    return ( (_f & static_cast<unsigned>(_bit)) != 0u );
}




// ===============================================================================
//  3  FIELD TAGS
// ===============================================================================
//   Tags for the mysql-specific slots.  Tags for the inherited base
// slots (username, password, host, port, schema, ssl, uri, charset)
// remain in dbl_tag::.

namespace mylf_tag
{
    struct unix_socket_tag       {};
    struct init_command_tag      {};
    struct compression_tag       {};
    struct multi_stmt_tag        {};
    struct local_infile_tag      {};
    struct connect_attrs_tag     {};
    struct ssl_mode_tag          {};
    struct auth_plugin_tag       {};
    struct x_protocol_tag        {};
    struct session_tracking_tag  {};
    struct cleartext_plugin_tag  {};
    struct stmt_cache_tag        {};

}   // namespace mylf_tag




// ===============================================================================
//  4  SLOT-LIST EXTENSION
// ===============================================================================
//   The MySQL vendor slots are appended to dbl_form_base<_Feat> via the
// form_append_slots_t utility.  The resulting form<> carries the full
// union of base + vendor slots under a single _Feat — dlf_* bits gate
// the base slots, mylf_* bits gate the vendor slots.

NS_INTERNAL

    // mysql_login_form_base
    //   alias: the form<_Feat, ...combined_slots...> base type of
    // mysql_login.  Composed by appending MySQL-common + MySQL-specific
    // slots to dbl_form_base<_Feat>.
    template <unsigned _Feat>
    using mysql_login_form_base = form_append_slots_t<
        dbl_form_base<_Feat>,

        // -- mysql-family common extras ------------------------------
        slot<mylf_unix_socket,
             field<mylf_tag::unix_socket_tag,      text_input<>>>,
        slot<mylf_init_command,
             field<mylf_tag::init_command_tag,     text_input<>>>,
        slot<mylf_compression,
             field<mylf_tag::compression_tag,      bool>>,
        slot<mylf_multi_stmt,
             field<mylf_tag::multi_stmt_tag,       bool>>,
        slot<mylf_local_infile,
             field<mylf_tag::local_infile_tag,     bool>>,
        slot<mylf_connect_attrs,
             field<mylf_tag::connect_attrs_tag,
                   std::map<std::string, std::string>>>,

        // -- mysql-specific ------------------------------------------
        slot<mylf_ssl_mode,
             field<mylf_tag::ssl_mode_tag,         mysql_ssl_settings>>,
        slot<mylf_auth_plugin,
             field<mylf_tag::auth_plugin_tag,      text_input<>>>,
        slot<mylf_x_protocol,
             field<mylf_tag::x_protocol_tag,       x_protocol_settings>>,
        slot<mylf_session_tracking,
             field<mylf_tag::session_tracking_tag, bool>>,
        slot<mylf_cleartext_plugin,
             field<mylf_tag::cleartext_plugin_tag, bool>>,
        slot<mylf_stmt_cache,
             field<mylf_tag::stmt_cache_tag,       std::size_t>>
    >;

NS_END  // internal




// ===============================================================================
//  5  MYSQL LOGIN
// ===============================================================================
//   _Feat  bitwise-OR of database_login_feat (dlf_*) and mysql_login_feat
//          (mylf_*) bits.  dlf_* bits gate base identity / network /
//          generic-target slots; mylf_* bits gate MySQL-common and
//          MySQL-specific slots.  At least one of dlf_username,
//          dlf_password, or dlf_database_name must be set (inherited
//          contract from database_login).

// mysql_login
//   struct: Oracle MySQL login form.  A sibling of
// database_login<database_type::mysql, _Feat> that additionally carries
// the MySQL vendor surface as EBO-collapsing slots.  Satisfies
// is_database_login_v<> structurally via db_type + submittable-form.
template <unsigned _Feat = (dlf_default | dlf_charset | mylf_typical)>
struct mysql_login : internal::mysql_login_form_base<_Feat>
{
    using base_type = internal::mysql_login_form_base<_Feat>;
    using self_type = mysql_login<_Feat>;
    using vendor    = djinterp::database::vendor_traits<
                          djinterp::database::database_type::mysql>;

    static constexpr djinterp::database::database_type  db_type       = djinterp::database::database_type::mysql;
    static constexpr unsigned                     login_features = _Feat;

    // -- base-slot presence (inherited contract) ----------------------
    static constexpr bool has_username        = has_dlf(_Feat, dlf_username);
    static constexpr bool has_password        = has_dlf(_Feat, dlf_password);
    static constexpr bool has_host            = has_dlf(_Feat, dlf_host);
    static constexpr bool has_port            = has_dlf(_Feat, dlf_port);
    static constexpr bool has_database_name   = has_dlf(_Feat, dlf_database_name);
    static constexpr bool has_schema          = has_dlf(_Feat, dlf_schema);
    static constexpr bool has_ssl             = has_dlf(_Feat, dlf_ssl);
    static constexpr bool has_uri             = has_dlf(_Feat, dlf_uri);
    static constexpr bool has_charset         = has_dlf(_Feat, dlf_charset);

    // -- vendor-slot presence -----------------------------------------
    static constexpr bool has_unix_socket       = has_mylf(_Feat, mylf_unix_socket);
    static constexpr bool has_init_command      = has_mylf(_Feat, mylf_init_command);
    static constexpr bool has_compression       = has_mylf(_Feat, mylf_compression);
    static constexpr bool has_multi_stmt        = has_mylf(_Feat, mylf_multi_stmt);
    static constexpr bool has_local_infile      = has_mylf(_Feat, mylf_local_infile);
    static constexpr bool has_connect_attrs     = has_mylf(_Feat, mylf_connect_attrs);
    static constexpr bool has_ssl_mode          = has_mylf(_Feat, mylf_ssl_mode);
    static constexpr bool has_auth_plugin       = has_mylf(_Feat, mylf_auth_plugin);
    static constexpr bool has_x_protocol        = has_mylf(_Feat, mylf_x_protocol);
    static constexpr bool has_session_tracking  = has_mylf(_Feat, mylf_session_tracking);
    static constexpr bool has_cleartext_plugin  = has_mylf(_Feat, mylf_cleartext_plugin);
    static constexpr bool has_stmt_cache        = has_mylf(_Feat, mylf_stmt_cache);

    static_assert( (has_username || has_password || has_database_name),
                   "mysql_login requires at least one of dlf_username, "
                   "dlf_password, or dlf_database_name." );

    // -- construction -------------------------------------------------
    mysql_login()
    {
        apply_vendor_defaults_();

        return;
    }

private:
    /*
    apply_vendor_defaults_
      Seeds enabled slots from the MySQL vendor contract — classic SQL
    port 3306, X Protocol port 33060, ssl_mode::preferred, charset
    "utf8mb4" — plus the base host / port defaults from
    vendor_traits<database_type::mysql>.  Only slots enabled by _Feat
    are touched; every other branch elides at compile time.

    Parameter(s):
      none.
    Return:
      none.
    */
    void
    apply_vendor_defaults_()
    {
        // -- base-slot defaults ---------------------------------------
        if constexpr (has_host)
        {
            auto cfg = vendor::make_default_config();

            ti_set_value(tc_get<dbl_tag::host_tag>(*this),
                         std::move(cfg.host));
        }

        if constexpr (has_port)
        {
            tc_get<dbl_tag::port_tag>(*this) = vendor::default_port;
        }

        if constexpr (has_charset)
        {
            // MySQL family standardizes on utf8mb4 as the modern
            // default; vendor_traits' generic charset may be empty.
            tc_get<dbl_tag::charset_tag>(*this) = "utf8mb4";
        }

        // -- vendor-slot defaults -------------------------------------
        if constexpr (has_ssl_mode)
        {
            tc_get<mylf_tag::ssl_mode_tag>(*this).mode =
                djinterp::database::mysql_ssl_mode::preferred;
        }

        if constexpr (has_x_protocol)
        {
            tc_get<mylf_tag::x_protocol_tag>(*this).port = 33060;
        }

        return;
    }
};




// ===============================================================================
//  6  FREE FUNCTIONS
// ===============================================================================
//   Setters for the MySQL-specific slots; validate and config-extract
// entry points.  The dbl_* setters from database_login.hpp continue to
// work on mysql_login for the inherited base slots — these free
// functions only cover slots not present in the base.

// ml_set_ssl_mode
//   function: set the MySQL ssl_mode enum on the ssl_mode slot
// (requires mylf_ssl_mode).
template <unsigned _F>
void
ml_set_ssl_mode(
    mysql_login<_F>&              _ml,
    djinterp::database::mysql_ssl_mode  _mode
)
{
    static_assert(has_mylf(_F, mylf_ssl_mode), "requires mylf_ssl_mode");

    tc_get<mylf_tag::ssl_mode_tag>(_ml).mode = _mode;

    return;
}

// ml_set_ssl_certs
//   function: set MySQL CA, client-cert, and private-key paths on the
// ssl_mode slot (requires mylf_ssl_mode).
template <unsigned _F>
void
ml_set_ssl_certs(
    mysql_login<_F>& _ml,
    std::string      _ca,
    std::string      _cert,
    std::string      _key
)
{
    static_assert(has_mylf(_F, mylf_ssl_mode), "requires mylf_ssl_mode");

    auto& s = tc_get<mylf_tag::ssl_mode_tag>(_ml);

    s.ca   = std::move(_ca);
    s.cert = std::move(_cert);
    s.key  = std::move(_key);

    return;
}

// ml_set_auth_plugin
//   function: set the authentication plugin name (requires
// mylf_auth_plugin).  Typical values: "caching_sha2_password",
// "mysql_native_password", "authentication_ldap_sasl_client".
template <unsigned _F>
void
ml_set_auth_plugin(
    mysql_login<_F>& _ml,
    std::string      _plugin
)
{
    static_assert(has_mylf(_F, mylf_auth_plugin),
                  "requires mylf_auth_plugin");

    ti_set_value(tc_get<mylf_tag::auth_plugin_tag>(_ml),
                 std::move(_plugin));

    return;
}

// ml_set_x_protocol
//   function: enable/disable X Protocol and optionally override its
// port (requires mylf_x_protocol).
template <unsigned _F>
void
ml_set_x_protocol(
    mysql_login<_F>& _ml,
    bool             _enabled,
    std::uint16_t    _port = 33060
)
{
    static_assert(has_mylf(_F, mylf_x_protocol),
                  "requires mylf_x_protocol");

    auto& x   = tc_get<mylf_tag::x_protocol_tag>(_ml);

    x.enabled = _enabled;
    x.port    = _port;

    return;
}

// ml_set_init_command
//   function: set the init SQL fired on every new connection (requires
// mylf_init_command).
template <unsigned _F>
void
ml_set_init_command(
    mysql_login<_F>& _ml,
    std::string      _sql
)
{
    static_assert(has_mylf(_F, mylf_init_command),
                  "requires mylf_init_command");

    ti_set_value(tc_get<mylf_tag::init_command_tag>(_ml),
                 std::move(_sql));

    return;
}

// ml_add_connect_attr
//   function: add / overwrite a single connect attribute pair on the
// connect_attrs slot (requires mylf_connect_attrs).
template <unsigned _F>
void
ml_add_connect_attr(
    mysql_login<_F>& _ml,
    std::string      _key,
    std::string      _value
)
{
    static_assert(has_mylf(_F, mylf_connect_attrs),
                  "requires mylf_connect_attrs");

    tc_get<mylf_tag::connect_attrs_tag>(_ml)[std::move(_key)]
        = std::move(_value);

    return;
}

// ml_validate
//   function: validate every enabled input field and return true iff
// all pass.  Delegates to dbl_validate for the base slots, applied to
// the upcast form — the mysql_login's extra slots (text_inputs without
// tif_validation, bools, aggregates) are skipped as trivially valid.
template <unsigned _F>
bool
ml_validate(
    mysql_login<_F>& _ml
)
{
    // URI-mode short-circuit (same contract as dbl_validate).
    if constexpr (has_dlf(_F, dlf_uri))
    {
        auto& u = tc_get<dbl_tag::uri_tag>(_ml);

        if (u.active)
        {
            auto r = ti_validate(u.uri);

            return ( r.result == validation_result::valid );
        }
    }

    return fm_validate_with(_ml,
        [](auto& _fld)
        {
            using field_t = std::remove_reference_t<decltype(_fld)>;

            if constexpr (internal::is_text_input<field_t>::value)
            {
                if constexpr (field_t::has_validation)
                {
                    auto r = ti_validate(_fld);

                    return ( r.result == validation_result::valid );
                }
            }

            return true;
        });
}

/*
ml_to_config
  Extract a djinterp::database::mysql_connect_config (Oracle MySQL) from the
mysql_login's current field values.  Layers three overlays in order:
  1. vendor_traits<mysql>::make_default_config() for the base
     connection_config surface (host, port, credentials, ssl paths,
     charset, schema).
  2. mysql_common overlays from mylf_* common-extras slots
     (unix_socket, init_command, compression, multi_stmt,
     local_infile, connect_attrs).
  3. MySQL-specific overlays from mylf_* vendor slots (ssl_mode,
     auth_plugin, x_protocol, session_tracking, cleartext_plugin,
     stmt_cache).

  Disabled slots elide at compile time.  The returned config is ready
for hand-off to djinterp::database::mysql_connection.

Parameter(s):
  _ml: the login form to read.
Return:
  A fully populated mysql_connect_config.
*/
template <unsigned _F>
djinterp::database::mysql_connect_config
ml_to_config(
    const mysql_login<_F>& _ml
)
{
    djinterp::database::mysql_connect_config cfg;

    // -- base connection_config (cfg.common.base) --------------------
    cfg.common.base =
        djinterp::database::vendor_traits<
            djinterp::database::database_type::mysql>::make_default_config();

    if constexpr (has_dlf(_F, dlf_host))
    {
        cfg.common.base.host = tc_get<dbl_tag::host_tag>(_ml).value;
    }

    if constexpr (has_dlf(_F, dlf_port))
    {
        cfg.common.base.port = tc_get<dbl_tag::port_tag>(_ml);
    }

    if constexpr (has_dlf(_F, dlf_database_name))
    {
        cfg.common.base.database =
            tc_get<dbl_tag::database_name_tag>(_ml).value;
    }

    if constexpr (has_dlf(_F, dlf_schema))
    {
        cfg.common.base.schema = tc_get<dbl_tag::schema_tag>(_ml).value;
    }

    if constexpr (has_dlf(_F, dlf_username))
    {
        cfg.common.base.username = tc_get<dbl_tag::username_tag>(_ml).value;
    }

    if constexpr (has_dlf(_F, dlf_password))
    {
        cfg.common.base.password = tc_get<dbl_tag::password_tag>(_ml).value;
    }

    if constexpr (has_dlf(_F, dlf_ssl))
    {
        const auto& s = tc_get<dbl_tag::ssl_tag>(_ml);

        cfg.common.base.enable_ssl = s.enable;
        cfg.common.base.verify_ssl = s.verify;
        cfg.common.base.ssl_ca     = s.ca;
        cfg.common.base.ssl_cert   = s.cert;
        cfg.common.base.ssl_key    = s.key;
    }

    if constexpr (has_dlf(_F, dlf_charset))
    {
        cfg.common.base.charset = tc_get<dbl_tag::charset_tag>(_ml);
    }

    // -- mysql-common overlays (cfg.common.*) ------------------------
    if constexpr (has_mylf(_F, mylf_unix_socket))
    {
        cfg.common.unix_socket =
            tc_get<mylf_tag::unix_socket_tag>(_ml).value;
    }

    if constexpr (has_mylf(_F, mylf_init_command))
    {
        cfg.common.init_command =
            tc_get<mylf_tag::init_command_tag>(_ml).value;
    }

    if constexpr (has_mylf(_F, mylf_compression))
    {
        cfg.common.use_compression = tc_get<mylf_tag::compression_tag>(_ml);
    }

    if constexpr (has_mylf(_F, mylf_multi_stmt))
    {
        cfg.common.multi_statements = tc_get<mylf_tag::multi_stmt_tag>(_ml);
    }

    if constexpr (has_mylf(_F, mylf_local_infile))
    {
        cfg.common.use_local_infile = tc_get<mylf_tag::local_infile_tag>(_ml);
    }

    if constexpr (has_mylf(_F, mylf_connect_attrs))
    {
        cfg.common.connect_attributes =
            tc_get<mylf_tag::connect_attrs_tag>(_ml);
    }

    // -- mysql-specific overlays (cfg.*) -----------------------------
    if constexpr (has_mylf(_F, mylf_ssl_mode))
    {
        const auto& s = tc_get<mylf_tag::ssl_mode_tag>(_ml);

        cfg.ssl_mode             = s.mode;
        cfg.tls_version          = s.tls_version;
        cfg.tls_ciphersuites     = s.tls_ciphersuites;
        cfg.common.base.ssl_ca   = s.ca;
        cfg.common.base.ssl_cert = s.cert;
        cfg.common.base.ssl_key  = s.key;
        cfg.common.base.enable_ssl =
            ( s.mode != djinterp::database::mysql_ssl_mode::disabled );
    }

    if constexpr (has_mylf(_F, mylf_auth_plugin))
    {
        cfg.auth_plugin = tc_get<mylf_tag::auth_plugin_tag>(_ml).value;
    }

    if constexpr (has_mylf(_F, mylf_x_protocol))
    {
        const auto& x = tc_get<mylf_tag::x_protocol_tag>(_ml);

        cfg.use_x_protocol  = x.enabled;
        cfg.x_protocol_port = x.port;
    }

    if constexpr (has_mylf(_F, mylf_session_tracking))
    {
        cfg.enable_session_tracking =
            tc_get<mylf_tag::session_tracking_tag>(_ml);
    }

    if constexpr (has_mylf(_F, mylf_cleartext_plugin))
    {
        cfg.enable_cleartext_plugin =
            tc_get<mylf_tag::cleartext_plugin_tag>(_ml);
    }

    if constexpr (has_mylf(_F, mylf_stmt_cache))
    {
        cfg.statement_cache_size = tc_get<mylf_tag::stmt_cache_tag>(_ml);
    }

    return cfg;
}

// ml_reset_to_defaults
//   function: reset every field to vendor defaults and clear form
// error / submitting state.
template <unsigned _F>
void
ml_reset_to_defaults(
    mysql_login<_F>& _ml
)
{
    _ml = mysql_login<_F>{};

    return;
}




// ===============================================================================
//  7  TRAITS
// ===============================================================================

namespace mysql_login_traits
{
NS_INTERNAL

    // is_mysql_login_impl
    //   trait: structural detector for instantiations of mysql_login.
    template <typename>
    struct is_mysql_login_impl : std::false_type
    {};

    template <unsigned _F>
    struct is_mysql_login_impl<mysql_login<_F>> : std::true_type
    {};

NS_END  // internal

// is_mysql_login
//   trait: structural predicate for mysql_login instantiations.
// Convenience wrapper; user-defined aggregates that want to pose as a
// MySQL login form are better tested via is_database_login_v plus a
// db_type == database_type::mysql check.
template <typename _Type>
struct is_mysql_login : internal::is_mysql_login_impl<_Type>
{};

template <typename _T>
inline constexpr bool is_mysql_login_v =
    is_mysql_login<_T>::value;


}   // namespace mysql_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MYSQL_LOGIN_
