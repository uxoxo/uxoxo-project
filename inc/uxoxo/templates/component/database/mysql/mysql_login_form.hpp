/*******************************************************************************
* uxoxo [component]                                      mysql_login_form.hpp
*
* Oracle MySQL database login form:
*   Vendor-specific login form for Oracle MySQL connections.  Derives from
* database_login<database_type::mysql, ...> and adds MySQL-specific
* fields: SSL mode enum, authentication plugin, X Protocol port, session
* tracking, query attributes, TLS version, compression, multi-statement
* mode, unix socket, and init command.
*
*   Fields inherited from database_login (all enabled by default):
*     host, port, database_name, schema, SSL, charset, URI,
*     username, password, remember-me, show-password
*
*   MySQL-specific fields (gated by mysql_login_feat):
*     myf_ssl_mode         SSL mode enum (disabled/preferred/required/…)
*     myf_auth_plugin      authentication plugin selector
*     myf_x_protocol       X Protocol port + toggle
*     myf_session_tracking  session tracking toggle
*     myf_query_attrs      query attributes toggle
*     myf_tls_version      TLS version string
*     myf_unix_socket      unix socket path
*     myf_compression      wire compression toggle
*     myf_multi_stmt       multi-statement mode
*     myf_init_command     initial SQL command on connect
*
*   The free function mydbl_to_config() extracts a full
* mysql_connect_config from the form.
*
* Contents:
*   §1  Feature flags (mysql_login_feat)
*   §2  EBO mixins
*   §3  mysql_login_form struct
*   §4  Free functions
*   §5  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/mysql_login_form.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_MYSQL_LOGIN_FORM_
#define  UXOXO_COMPONENT_MYSQL_LOGIN_FORM_ 1

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
#include <database/mysql/mysql.hpp>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  MYSQL LOGIN FEATURE FLAGS
// ═══════════════════════════════════════════════════════════════════════════════

enum mysql_login_feat : unsigned
{
    myf_none             = 0,
    myf_ssl_mode         = 1u << 0,     // SSL mode enum selector
    myf_auth_plugin      = 1u << 1,     // authentication plugin
    myf_x_protocol       = 1u << 2,     // X Protocol port + toggle
    myf_session_tracking = 1u << 3,     // session tracking toggle
    myf_query_attrs      = 1u << 4,     // query attributes toggle
    myf_tls_version      = 1u << 5,     // TLS version constraint
    myf_unix_socket      = 1u << 6,     // unix socket path
    myf_compression      = 1u << 7,     // wire compression
    myf_multi_stmt       = 1u << 8,     // multi-statement mode
    myf_init_command     = 1u << 9,     // initial SQL on connect

    myf_all              = myf_ssl_mode    | myf_auth_plugin
                         | myf_x_protocol  | myf_session_tracking
                         | myf_query_attrs | myf_tls_version
                         | myf_unix_socket | myf_compression
                         | myf_multi_stmt  | myf_init_command
};

constexpr unsigned operator|(mysql_login_feat a,
                             mysql_login_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_myf(unsigned          f,
                       mysql_login_feat  bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  EBO MIXINS
// ═══════════════════════════════════════════════════════════════════════════════

namespace mysql_login_mixin {

    // ── SSL mode ─────────────────────────────────────────────────────
    template <bool _Enable>
    struct ssl_mode_data
    {};

    template <>
    struct ssl_mode_data<true>
    {
        djinterp::db::mysql_ssl_mode ssl_mode =
            djinterp::db::mysql_ssl_mode::preferred;
    };

    // ── auth plugin ──────────────────────────────────────────────────
    template <bool _Enable>
    struct auth_plugin_data
    {};

    template <>
    struct auth_plugin_data<true>
    {
        std::string auth_plugin;
    };

    // ── X Protocol ───────────────────────────────────────────────────
    template <bool _Enable>
    struct x_protocol_data
    {};

    template <>
    struct x_protocol_data<true>
    {
        std::uint16_t x_protocol_port = 33060;
        bool          use_x_protocol  = false;
    };

    // ── session tracking ─────────────────────────────────────────────
    template <bool _Enable>
    struct session_tracking_data
    {};

    template <>
    struct session_tracking_data<true>
    {
        bool enable_session_tracking = false;
    };

    // ── query attributes ─────────────────────────────────────────────
    template <bool _Enable>
    struct query_attrs_data
    {};

    template <>
    struct query_attrs_data<true>
    {
        bool enable_query_attributes = false;
    };

    // ── TLS version ──────────────────────────────────────────────────
    template <bool _Enable>
    struct tls_version_data
    {};

    template <>
    struct tls_version_data<true>
    {
        std::string tls_version;
        std::string tls_ciphersuites;
    };

    // ── unix socket ──────────────────────────────────────────────────
    template <bool _Enable>
    struct unix_socket_data
    {};

    template <>
    struct unix_socket_data<true>
    {
        text_input<tif_validation> unix_socket;
        bool                       use_unix_socket = false;
    };

    // ── compression ──────────────────────────────────────────────────
    template <bool _Enable>
    struct compression_data
    {};

    template <>
    struct compression_data<true>
    {
        bool use_compression = false;
    };

    // ── multi-statement ──────────────────────────────────────────────
    template <bool _Enable>
    struct multi_stmt_data
    {};

    template <>
    struct multi_stmt_data<true>
    {
        bool multi_statements = false;
    };

    // ── init command ─────────────────────────────────────────────────
    template <bool _Enable>
    struct init_command_data
    {};

    template <>
    struct init_command_data<true>
    {
        text_input<> init_command;
    };

}   // namespace mysql_login_mixin


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  MYSQL LOGIN FORM
// ═══════════════════════════════════════════════════════════════════════════════

template <unsigned _LoginFeat = lf_user_pass | lf_show_password,
          unsigned _DbFeat    = dlf_network_full,
          unsigned _MyFeat    = myf_none>
struct mysql_login_form
    : database_login<djinterp::db::database_type::mysql,
                     _LoginFeat,
                     _DbFeat>
    , mysql_login_mixin::ssl_mode_data         <has_myf(_MyFeat, myf_ssl_mode)>
    , mysql_login_mixin::auth_plugin_data      <has_myf(_MyFeat, myf_auth_plugin)>
    , mysql_login_mixin::x_protocol_data       <has_myf(_MyFeat, myf_x_protocol)>
    , mysql_login_mixin::session_tracking_data <has_myf(_MyFeat, myf_session_tracking)>
    , mysql_login_mixin::query_attrs_data      <has_myf(_MyFeat, myf_query_attrs)>
    , mysql_login_mixin::tls_version_data      <has_myf(_MyFeat, myf_tls_version)>
    , mysql_login_mixin::unix_socket_data      <has_myf(_MyFeat, myf_unix_socket)>
    , mysql_login_mixin::compression_data      <has_myf(_MyFeat, myf_compression)>
    , mysql_login_mixin::multi_stmt_data       <has_myf(_MyFeat, myf_multi_stmt)>
    , mysql_login_mixin::init_command_data     <has_myf(_MyFeat, myf_init_command)>
{
    using base_db_login = database_login<
        djinterp::db::database_type::mysql,
        _LoginFeat,
        _DbFeat>;

    static constexpr unsigned my_features         = _MyFeat;

    static constexpr bool has_ssl_mode            = has_myf(_MyFeat, myf_ssl_mode);
    static constexpr bool has_auth_plugin         = has_myf(_MyFeat, myf_auth_plugin);
    static constexpr bool has_x_protocol          = has_myf(_MyFeat, myf_x_protocol);
    static constexpr bool has_session_tracking    = has_myf(_MyFeat, myf_session_tracking);
    static constexpr bool has_query_attrs         = has_myf(_MyFeat, myf_query_attrs);
    static constexpr bool has_tls_version         = has_myf(_MyFeat, myf_tls_version);
    static constexpr bool has_unix_socket         = has_myf(_MyFeat, myf_unix_socket);
    static constexpr bool has_compression         = has_myf(_MyFeat, myf_compression);
    static constexpr bool has_multi_stmt          = has_myf(_MyFeat, myf_multi_stmt);
    static constexpr bool has_init_command        = has_myf(_MyFeat, myf_init_command);

    // ── construction ─────────────────────────────────────────────────
    mysql_login_form() = default;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// mydbl_set_ssl_mode
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_set_ssl_mode(
    mysql_login_form<_LF, _DF, _MF>&  mydb,
    djinterp::db::mysql_ssl_mode       mode)
{
    static_assert(has_myf(_MF, myf_ssl_mode),
                  "requires myf_ssl_mode");

    mydb.ssl_mode = mode;

    return;
}

// mydbl_set_auth_plugin
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_set_auth_plugin(
    mysql_login_form<_LF, _DF, _MF>& mydb,
    std::string                       plugin)
{
    static_assert(has_myf(_MF, myf_auth_plugin),
                  "requires myf_auth_plugin");

    mydb.auth_plugin = std::move(plugin);

    return;
}

// mydbl_set_x_protocol_port
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_set_x_protocol_port(
    mysql_login_form<_LF, _DF, _MF>& mydb,
    std::uint16_t                     port)
{
    static_assert(has_myf(_MF, myf_x_protocol),
                  "requires myf_x_protocol");

    mydb.x_protocol_port = port;

    return;
}

// mydbl_toggle_x_protocol
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_toggle_x_protocol(
    mysql_login_form<_LF, _DF, _MF>& mydb)
{
    static_assert(has_myf(_MF, myf_x_protocol),
                  "requires myf_x_protocol");

    mydb.use_x_protocol = !mydb.use_x_protocol;

    return;
}

// mydbl_toggle_session_tracking
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_toggle_session_tracking(
    mysql_login_form<_LF, _DF, _MF>& mydb)
{
    static_assert(has_myf(_MF, myf_session_tracking),
                  "requires myf_session_tracking");

    mydb.enable_session_tracking = !mydb.enable_session_tracking;

    return;
}

// mydbl_set_unix_socket
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_set_unix_socket(
    mysql_login_form<_LF, _DF, _MF>& mydb,
    std::string                       path)
{
    static_assert(has_myf(_MF, myf_unix_socket),
                  "requires myf_unix_socket");

    ti_set_value(mydb.unix_socket, std::move(path));
    mydb.use_unix_socket = true;

    return;
}

// mydbl_toggle_compression
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_toggle_compression(
    mysql_login_form<_LF, _DF, _MF>& mydb)
{
    static_assert(has_myf(_MF, myf_compression),
                  "requires myf_compression");

    mydb.use_compression = !mydb.use_compression;

    return;
}

// mydbl_toggle_multi_statements
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_toggle_multi_statements(
    mysql_login_form<_LF, _DF, _MF>& mydb)
{
    static_assert(has_myf(_MF, myf_multi_stmt),
                  "requires myf_multi_stmt");

    mydb.multi_statements = !mydb.multi_statements;

    return;
}

// mydbl_set_init_command
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mydbl_set_init_command(
    mysql_login_form<_LF, _DF, _MF>& mydb,
    std::string                       cmd)
{
    static_assert(has_myf(_MF, myf_init_command),
                  "requires myf_init_command");

    ti_set_value(mydb.init_command, std::move(cmd));

    return;
}

// mydbl_validate
//   validates MySQL-specific fields, then delegates to
// database_login validation.
template <unsigned _LF, unsigned _DF, unsigned _MF>
bool mydbl_validate(mysql_login_form<_LF, _DF, _MF>& mydb)
{
    bool valid = true;

    if constexpr (has_myf(_MF, myf_unix_socket))
    {
        if (mydb.use_unix_socket)
        {
            auto r = ti_validate(mydb.unix_socket);

            if (r.result != validation_result::valid)
            {
                valid = false;
            }
        }
    }

    if (!dbl_validate(mydb))
    {
        valid = false;
    }

    return valid;
}

// mydbl_to_config
//   extracts a djinterp::db::mysql_connect_config from the form.
template <unsigned _LF, unsigned _DF, unsigned _MF>
djinterp::db::mysql_connect_config
mydbl_to_config(const mysql_login_form<_LF, _DF, _MF>& mydb)
{
    djinterp::db::mysql_connect_config cfg;

    // populate the base via database_login
    cfg.common.base = dbl_to_config(mydb);

    // charset → mysql_common default_charset
    if constexpr (has_dlf(_DF, dlf_charset))
    {
        cfg.common.default_charset = mydb.charset;
    }

    // unix socket
    if constexpr (has_myf(_MF, myf_unix_socket))
    {
        if (mydb.use_unix_socket)
        {
            cfg.common.unix_socket = mydb.unix_socket.value;
        }
    }

    // compression
    if constexpr (has_myf(_MF, myf_compression))
    {
        cfg.common.use_compression = mydb.use_compression;
    }

    // multi-statement
    if constexpr (has_myf(_MF, myf_multi_stmt))
    {
        cfg.common.multi_statements = mydb.multi_statements;
    }

    // init command
    if constexpr (has_myf(_MF, myf_init_command))
    {
        cfg.common.init_command = mydb.init_command.value;
    }

    // SSL mode
    if constexpr (has_myf(_MF, myf_ssl_mode))
    {
        cfg.ssl_mode = mydb.ssl_mode;
    }

    // auth plugin
    if constexpr (has_myf(_MF, myf_auth_plugin))
    {
        cfg.auth_plugin = mydb.auth_plugin;
    }

    // X Protocol
    if constexpr (has_myf(_MF, myf_x_protocol))
    {
        cfg.x_protocol_port = mydb.x_protocol_port;
        cfg.use_x_protocol  = mydb.use_x_protocol;
    }

    // session tracking
    if constexpr (has_myf(_MF, myf_session_tracking))
    {
        cfg.enable_session_tracking = mydb.enable_session_tracking;
    }

    // TLS version
    if constexpr (has_myf(_MF, myf_tls_version))
    {
        cfg.tls_version      = mydb.tls_version;
        cfg.tls_ciphersuites = mydb.tls_ciphersuites;
    }

    return cfg;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace mysql_login_traits {
namespace detail {

    template <typename, typename = void>
    struct has_ssl_mode_member : std::false_type {};
    template <typename _T>
    struct has_ssl_mode_member<_T, std::void_t<
        decltype(std::declval<_T>().ssl_mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_auth_plugin_member : std::false_type {};
    template <typename _T>
    struct has_auth_plugin_member<_T, std::void_t<
        decltype(std::declval<_T>().auth_plugin)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_x_protocol_port_member : std::false_type {};
    template <typename _T>
    struct has_x_protocol_port_member<_T, std::void_t<
        decltype(std::declval<_T>().x_protocol_port)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_session_tracking_member : std::false_type {};
    template <typename _T>
    struct has_session_tracking_member<_T, std::void_t<
        decltype(std::declval<_T>().enable_session_tracking)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_tls_version_member : std::false_type {};
    template <typename _T>
    struct has_tls_version_member<_T, std::void_t<
        decltype(std::declval<_T>().tls_version)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_unix_socket_member : std::false_type {};
    template <typename _T>
    struct has_unix_socket_member<_T, std::void_t<
        decltype(std::declval<_T>().unix_socket)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_use_compression_member : std::false_type {};
    template <typename _T>
    struct has_use_compression_member<_T, std::void_t<
        decltype(std::declval<_T>().use_compression)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_multi_statements_member : std::false_type {};
    template <typename _T>
    struct has_multi_statements_member<_T, std::void_t<
        decltype(std::declval<_T>().multi_statements)
    >> : std::true_type {};

}   // namespace detail

template <typename _T>
inline constexpr bool has_ssl_mode_v =
    detail::has_ssl_mode_member<_T>::value;
template <typename _T>
inline constexpr bool has_auth_plugin_v =
    detail::has_auth_plugin_member<_T>::value;
template <typename _T>
inline constexpr bool has_x_protocol_port_v =
    detail::has_x_protocol_port_member<_T>::value;
template <typename _T>
inline constexpr bool has_session_tracking_v =
    detail::has_session_tracking_member<_T>::value;
template <typename _T>
inline constexpr bool has_tls_version_v =
    detail::has_tls_version_member<_T>::value;
template <typename _T>
inline constexpr bool has_unix_socket_v =
    detail::has_unix_socket_member<_T>::value;
template <typename _T>
inline constexpr bool has_use_compression_v =
    detail::has_use_compression_member<_T>::value;
template <typename _T>
inline constexpr bool has_multi_statements_v =
    detail::has_multi_statements_member<_T>::value;

// is_mysql_login_form
//   type trait: is a database_login with db_type == mysql.
template <typename _Type>
struct is_mysql_login_form : std::conjunction<
    database_login_traits::is_database_login<_Type>,
    std::bool_constant<_Type::db_type ==
        djinterp::db::database_type::mysql>
>
{};

template <typename _T>
inline constexpr bool is_mysql_login_form_v =
    is_mysql_login_form<_T>::value;

// is_x_protocol_login
template <typename _Type>
struct is_x_protocol_login : std::conjunction<
    is_mysql_login_form<_Type>,
    detail::has_x_protocol_port_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_x_protocol_login_v =
    is_x_protocol_login<_T>::value;

}   // namespace mysql_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MYSQL_LOGIN_FORM_
