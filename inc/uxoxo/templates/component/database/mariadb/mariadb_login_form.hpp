/*******************************************************************************
* uxoxo [component]                                   mariadb_login_form.hpp
*
* MariaDB database login form:
*   Vendor-specific login form for MariaDB connections.  Derives from
* database_login<database_type::mariadb, ...> and adds MariaDB-specific
* fields: Galera cluster options, unix socket path, storage engine
* preference, compression toggle, and multi-statement mode.
*
*   Fields inherited from database_login (all enabled by default):
*     host, port, database_name, schema, SSL, charset, URI,
*     username, password, remember-me, show-password
*
*   MariaDB-specific fields (gated by mariadb_login_feat):
*     mlf_galera          Galera wsrep_sync_wait + wsrep_causal_reads
*     mlf_unix_socket     unix socket path (alternative to host:port)
*     mlf_engine          default storage engine
*     mlf_compression     wire compression toggle
*     mlf_multi_stmt      multi-statement mode toggle
*     mlf_init_command    initial SQL command executed on connect
*
*   Feature composition follows the same EBO-mixin bitfield pattern used
* throughout the uxoxo component layer.
*
*   The free function mdbl_to_config() extracts a full
* mariadb_connect_config from the form for hand-off to the
* connection layer.
*
* Contents:
*   §1  Feature flags (mariadb_login_feat)
*   §2  EBO mixins
*   §3  mariadb_login_form struct
*   §4  Free functions
*   §5  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/mariadb_login_form.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_MARIADB_LOGIN_FORM_
#define  UXOXO_COMPONENT_MARIADB_LOGIN_FORM_ 1

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
#include <database/mysql/mariadb.hpp>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  MARIADB LOGIN FEATURE FLAGS
// ═══════════════════════════════════════════════════════════════════════════════
//   Separate feature parameter from the database_login flags.
// Bits start at 0.

enum mariadb_login_feat : unsigned
{
    mlf_none          = 0,
    mlf_galera        = 1u << 0,     // Galera cluster toggles
    mlf_unix_socket   = 1u << 1,     // unix socket path field
    mlf_engine        = 1u << 2,     // default storage engine
    mlf_compression   = 1u << 3,     // wire compression toggle
    mlf_multi_stmt    = 1u << 4,     // multi-statement mode
    mlf_init_command  = 1u << 5,     // initial SQL on connect

    mlf_all           = mlf_galera   | mlf_unix_socket | mlf_engine
                      | mlf_compression | mlf_multi_stmt | mlf_init_command
};

constexpr unsigned operator|(mariadb_login_feat a,
                             mariadb_login_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_mlf(unsigned            f,
                       mariadb_login_feat  bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  EBO MIXINS
// ═══════════════════════════════════════════════════════════════════════════════

namespace mariadb_login_mixin {

    // ── galera cluster ───────────────────────────────────────────────
    template <bool _Enable>
    struct galera_data
    {};

    template <>
    struct galera_data<true>
    {
        bool wsrep_sync_wait    = false;
        bool wsrep_causal_reads = false;
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

    // ── default storage engine ───────────────────────────────────────
    template <bool _Enable>
    struct engine_data
    {};

    template <>
    struct engine_data<true>
    {
        std::string default_storage_engine;
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

}   // namespace mariadb_login_mixin


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  MARIADB LOGIN FORM
// ═══════════════════════════════════════════════════════════════════════════════
//   _LoginFeat   login_form_feat flags (username/password/remember/show)
//   _DbFeat      database_login_feat flags (host/port/db/schema/ssl/…)
//   _MdbFeat     mariadb_login_feat flags (galera/socket/engine/…)
//
//   Default template arguments give a "full MariaDB login" out of the
// box: username + password + show-password, network + schema + SSL +
// charset, no MariaDB extras.

template <unsigned _LoginFeat = lf_user_pass | lf_show_password,
          unsigned _DbFeat    = dlf_network_full,
          unsigned _MdbFeat   = mlf_none>
struct mariadb_login_form
    : database_login<djinterp::db::database_type::mariadb,
                     _LoginFeat,
                     _DbFeat>
    , mariadb_login_mixin::galera_data       <has_mlf(_MdbFeat, mlf_galera)>
    , mariadb_login_mixin::unix_socket_data  <has_mlf(_MdbFeat, mlf_unix_socket)>
    , mariadb_login_mixin::engine_data       <has_mlf(_MdbFeat, mlf_engine)>
    , mariadb_login_mixin::compression_data  <has_mlf(_MdbFeat, mlf_compression)>
    , mariadb_login_mixin::multi_stmt_data   <has_mlf(_MdbFeat, mlf_multi_stmt)>
    , mariadb_login_mixin::init_command_data <has_mlf(_MdbFeat, mlf_init_command)>
{
    using base_db_login = database_login<
        djinterp::db::database_type::mariadb,
        _LoginFeat,
        _DbFeat>;

    static constexpr unsigned mdb_features      = _MdbFeat;

    static constexpr bool has_galera            = has_mlf(_MdbFeat, mlf_galera);
    static constexpr bool has_unix_socket       = has_mlf(_MdbFeat, mlf_unix_socket);
    static constexpr bool has_engine            = has_mlf(_MdbFeat, mlf_engine);
    static constexpr bool has_compression       = has_mlf(_MdbFeat, mlf_compression);
    static constexpr bool has_multi_stmt        = has_mlf(_MdbFeat, mlf_multi_stmt);
    static constexpr bool has_init_command      = has_mlf(_MdbFeat, mlf_init_command);

    // ── construction ─────────────────────────────────────────────────
    mariadb_login_form() = default;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// mdbl_set_unix_socket
//   sets the unix socket path and switches to socket mode
// (requires mlf_unix_socket).
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mdbl_set_unix_socket(mariadb_login_form<_LF, _DF, _MF>& mdb,
                          std::string                         path)
{
    static_assert(has_mlf(_MF, mlf_unix_socket),
                  "requires mlf_unix_socket");

    ti_set_value(mdb.unix_socket, std::move(path));
    mdb.use_unix_socket = true;

    return;
}

// mdbl_set_engine
//   sets the default storage engine (requires mlf_engine).
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mdbl_set_engine(mariadb_login_form<_LF, _DF, _MF>& mdb,
                     std::string                         engine)
{
    static_assert(has_mlf(_MF, mlf_engine), "requires mlf_engine");

    mdb.default_storage_engine = std::move(engine);

    return;
}

// mdbl_toggle_galera_sync_wait
//   toggles wsrep_sync_wait (requires mlf_galera).
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mdbl_toggle_galera_sync_wait(
    mariadb_login_form<_LF, _DF, _MF>& mdb)
{
    static_assert(has_mlf(_MF, mlf_galera), "requires mlf_galera");

    mdb.wsrep_sync_wait = !mdb.wsrep_sync_wait;

    return;
}

// mdbl_toggle_galera_causal_reads
//   toggles wsrep_causal_reads (requires mlf_galera).
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mdbl_toggle_galera_causal_reads(
    mariadb_login_form<_LF, _DF, _MF>& mdb)
{
    static_assert(has_mlf(_MF, mlf_galera), "requires mlf_galera");

    mdb.wsrep_causal_reads = !mdb.wsrep_causal_reads;

    return;
}

// mdbl_toggle_compression
//   toggles wire compression (requires mlf_compression).
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mdbl_toggle_compression(
    mariadb_login_form<_LF, _DF, _MF>& mdb)
{
    static_assert(has_mlf(_MF, mlf_compression),
                  "requires mlf_compression");

    mdb.use_compression = !mdb.use_compression;

    return;
}

// mdbl_toggle_multi_statements
//   toggles multi-statement mode (requires mlf_multi_stmt).
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mdbl_toggle_multi_statements(
    mariadb_login_form<_LF, _DF, _MF>& mdb)
{
    static_assert(has_mlf(_MF, mlf_multi_stmt),
                  "requires mlf_multi_stmt");

    mdb.multi_statements = !mdb.multi_statements;

    return;
}

// mdbl_set_init_command
//   sets the initial SQL command (requires mlf_init_command).
template <unsigned _LF, unsigned _DF, unsigned _MF>
void mdbl_set_init_command(
    mariadb_login_form<_LF, _DF, _MF>& mdb,
    std::string                         cmd)
{
    static_assert(has_mlf(_MF, mlf_init_command),
                  "requires mlf_init_command");

    ti_set_value(mdb.init_command, std::move(cmd));

    return;
}

// mdbl_validate
//   validates MariaDB-specific fields, then delegates to
// database_login validation.
template <unsigned _LF, unsigned _DF, unsigned _MF>
bool mdbl_validate(mariadb_login_form<_LF, _DF, _MF>& mdb)
{
    bool valid = true;

    // if using unix socket, validate the path
    if constexpr (has_mlf(_MF, mlf_unix_socket))
    {
        if (mdb.use_unix_socket)
        {
            auto r = ti_validate(mdb.unix_socket);

            if (r.result != validation_result::valid)
            {
                valid = false;
            }
        }
    }

    // delegate to database_login validation
    if (!dbl_validate(mdb))
    {
        valid = false;
    }

    return valid;
}

// mdbl_to_config
//   extracts a mariadb_connect_config from the form's current
// field values.
template <unsigned _LF, unsigned _DF, unsigned _MF>
djinterp::db::mariadb_connect_config
mdbl_to_config(const mariadb_login_form<_LF, _DF, _MF>& mdb)
{
    djinterp::db::mariadb_connect_config cfg;

    // populate the base connection_config via database_login
    cfg.mysql_config.base = dbl_to_config(mdb);

    // charset from base → mysql_common default_charset
    if constexpr (has_dlf(_DF, dlf_charset))
    {
        cfg.mysql_config.default_charset = mdb.charset;
    }

    // unix socket
    if constexpr (has_mlf(_MF, mlf_unix_socket))
    {
        if (mdb.use_unix_socket)
        {
            cfg.mysql_config.unix_socket = mdb.unix_socket.value;
        }
    }

    // compression
    if constexpr (has_mlf(_MF, mlf_compression))
    {
        cfg.mysql_config.use_compression = mdb.use_compression;
    }

    // multi-statement
    if constexpr (has_mlf(_MF, mlf_multi_stmt))
    {
        cfg.mysql_config.multi_statements = mdb.multi_statements;
    }

    // init command
    if constexpr (has_mlf(_MF, mlf_init_command))
    {
        cfg.mysql_config.init_command = mdb.init_command.value;
    }

    // galera
    if constexpr (has_mlf(_MF, mlf_galera))
    {
        cfg.galera_wsrep_sync_wait    = mdb.wsrep_sync_wait;
        cfg.galera_wsrep_causal_reads = mdb.wsrep_causal_reads;
    }

    // default storage engine
    if constexpr (has_mlf(_MF, mlf_engine))
    {
        cfg.default_storage_engine = mdb.default_storage_engine;
    }

    return cfg;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace mariadb_login_traits {
namespace detail {

    template <typename, typename = void>
    struct has_wsrep_sync_wait_member : std::false_type {};
    template <typename _T>
    struct has_wsrep_sync_wait_member<_T, std::void_t<
        decltype(std::declval<_T>().wsrep_sync_wait)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_unix_socket_member : std::false_type {};
    template <typename _T>
    struct has_unix_socket_member<_T, std::void_t<
        decltype(std::declval<_T>().unix_socket)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_default_storage_engine_member : std::false_type {};
    template <typename _T>
    struct has_default_storage_engine_member<_T, std::void_t<
        decltype(std::declval<_T>().default_storage_engine)
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

    template <typename, typename = void>
    struct has_init_command_member : std::false_type {};
    template <typename _T>
    struct has_init_command_member<_T, std::void_t<
        decltype(std::declval<_T>().init_command)
    >> : std::true_type {};

}   // namespace detail

template <typename _T>
inline constexpr bool has_wsrep_sync_wait_v =
    detail::has_wsrep_sync_wait_member<_T>::value;
template <typename _T>
inline constexpr bool has_unix_socket_v =
    detail::has_unix_socket_member<_T>::value;
template <typename _T>
inline constexpr bool has_default_storage_engine_v =
    detail::has_default_storage_engine_member<_T>::value;
template <typename _T>
inline constexpr bool has_use_compression_v =
    detail::has_use_compression_member<_T>::value;
template <typename _T>
inline constexpr bool has_multi_statements_v =
    detail::has_multi_statements_member<_T>::value;
template <typename _T>
inline constexpr bool has_init_command_v =
    detail::has_init_command_member<_T>::value;

// is_mariadb_login_form
//   type trait: is a database_login with db_type == mariadb.
// Detected structurally: has db_type + submitting + at least
// one of the MariaDB-specific members OR satisfies
// is_database_login with the mariadb vendor constant.
template <typename _Type>
struct is_mariadb_login_form : std::conjunction<
    database_login_traits::is_database_login<_Type>,
    std::bool_constant<_Type::db_type ==
        djinterp::db::database_type::mariadb>
>
{};

template <typename _T>
inline constexpr bool is_mariadb_login_form_v =
    is_mariadb_login_form<_T>::value;

// is_galera_login
template <typename _Type>
struct is_galera_login : std::conjunction<
    is_mariadb_login_form<_Type>,
    detail::has_wsrep_sync_wait_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_galera_login_v =
    is_galera_login<_T>::value;

// is_socket_login
template <typename _Type>
struct is_socket_login : std::conjunction<
    is_mariadb_login_form<_Type>,
    detail::has_unix_socket_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_socket_login_v =
    is_socket_login<_T>::value;

}   // namespace mariadb_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MARIADB_LOGIN_FORM_
