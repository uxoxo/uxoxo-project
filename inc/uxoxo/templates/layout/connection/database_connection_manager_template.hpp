/*******************************************************************************
* uxoxo [layout]                       database_connection_manager_template.hpp
*
* Database-specific session manager layout:
*   A specialization of connection_manager_template that carries the extra
* concerns unique to database connections: the vendor (database_type)
* dimension, SSH-tunnel wrapping, SSL/TLS configuration, per-vendor default
* ports and field visibility, and the Test-Connection affordance shared by
* every major database client (HeidiSQL, DBeaver, DataGrip, MySQL Workbench,
* pgAdmin, SSMS).
*
*   The template extends connection_manager_template<_helper, _Session> with
* _Session defaulted to `database_session` - a bundle that composes the
* djinterp `connection_config` with SSH-tunnel state, SSL state, and
* stat-tracking metadata.  Callers who need richer per-session data can
* pass their own _Session type so long as it exposes the expected member
* names (name, comment, vendor, config, ssh, ssl) - detection is purely
* structural.
*
*   VENDOR DIMENSION:
*   Each session carries a database_type enumerator identifying its vendor
* (MySQL, PostgreSQL, SQLite, Oracle, MSSQL, ...).  The template offers:
*     - a runtime lookup from database_type to vendor_info (vendor_info_for)
*     - default-port / default-schema / capability queries per vendor
*     - per-vendor field-visibility predicates (uses_host_port, uses_file,
*       uses_dsn) so the detail form can show or hide controls as the
*       user switches vendors in the Network-Type dropdown
*     - vendor-aware session initialization that resets ports, paths, and
*       schema defaults when the user picks a new vendor
*
*   TUNNELING AND TLS:
*   ssh_tunnel_config and ssl_config are defined here rather than in
* database_common.hpp because SSH tunneling in particular is a UI-layer
* concern - the djinterp connection layer consumes a TCP endpoint, not a
* tunnel descriptor.  The layout resolves the tunnel into a local
* endpoint before handing a connection_config to djinterp.
*
*   LAYER DIAGRAM:
*     database_connection_manager_template<_helper, _Session>
*       -> connection_manager_template<_helper, _Session>
*         -> layout_template<_helper, DLayoutKind::session_manager>
*
*   CRTP HOOKS required on _helper (in addition to those on the base):
*     bool test_connection_helper(const _Session&)
*         -> attempt a lightweight connect + ping + disconnect
*     void open_connection_helper(const _Session&)
*         -> establish the real connection and hand it off
*     void vendor_changed_helper(database_type, database_type)
*         -> called after set_session_vendor mutates the selection
*
* Contents:
*   1  ssh_tunnel_config                      - optional SSH wrapper state
*   2  ssl_config                             - TLS parameters
*   3  database_session                       - default session type
*   4  vendor_info_for                        - runtime db_type -> vendor_info
*   5  field-visibility helpers               - per-vendor predicates
*   6  database_connection_manager_template   - the template class
*
*
* path:      /inc/uxoxo/layout/database_connection_manager_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.24
*******************************************************************************/

#ifndef  UXOXO_LAYOUT_DATABASE_CONNECTION_MANAGER_TEMPLATE_
#define  UXOXO_LAYOUT_DATABASE_CONNECTION_MANAGER_TEMPLATE_ 1

// std
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/db/database_common.hpp>
#include <djinterp/core/db/database_connection_template.hpp>
// uxoxo
#include "../uxoxo.hpp"
#include "./connection_manager_template.hpp"


#ifndef NS_LAYOUT
    #define NS_LAYOUT   D_NAMESPACE(layout)
#endif


NS_UXOXO
NS_LAYOUT


// type aliases - bring the djinterp vocabulary into uxoxo::layout so
// consumers of this header do not need to fully qualify at every
// usage site.
using djinterp::database::database_type;
using djinterp::database::connection_config;
using djinterp::database::vendor_info;


// ===============================================================================
//  1  SSH TUNNEL CONFIG
// ===============================================================================

// ssh_tunnel_config
//   type: optional SSH wrapping for a database connection.  The djinterp
// connection layer consumes a TCP endpoint (host + port); a session
// manager that supports SSH tunneling resolves this struct into a local
// forwarded endpoint before handing the resulting connection_config to
// djinterp.  Leaving `enabled` false makes the rest of the struct
// irrelevant.
struct ssh_tunnel_config
{
    bool           enabled       = false;
    std::string    host;                  // SSH server host
    std::uint16_t  port          = 22;    // SSH server port

    std::string    user;                  // SSH username
    std::string    key_path;              // private key file path
    std::string    key_passphrase;        // passphrase for encrypted keys
    std::string    password;              // password auth (if no key)

    std::uint16_t  local_bind_port = 0;   // 0 = auto-assign
    std::string    local_bind_host = "127.0.0.1";

    std::uint32_t  keepalive_seconds = 30;
    bool           strict_host_key_check = true;
    std::string    known_hosts_path;      // OpenSSH-style known_hosts file
};




// ===============================================================================
//  2  SSL CONFIG
// ===============================================================================

// ssl_config
//   type: TLS parameters for a database connection.  Kept separate from
// connection_config so that UI state (e.g. the user checked the SSL box
// but has not yet filled in paths) stays out of the djinterp layer.
// The _helper is responsible for merging this into the connection_config
// in a vendor-appropriate way just before opening the connection.
struct ssl_config
{
    bool         enabled        = false;

    std::string  ca_path;                 // certificate authority bundle
    std::string  cert_path;               // client certificate
    std::string  key_path;                // client private key
    std::string  cipher_list;             // OpenSSL cipher spec (optional)

    bool         verify_peer    = true;   // validate server cert
    bool         verify_host    = true;   // validate cert matches host

    // protocol floor.  Empty string means "vendor default"; common values
    // are "TLSv1.2" and "TLSv1.3".
    std::string  min_protocol;
};




// ===============================================================================
//  3  DATABASE SESSION
// ===============================================================================

// database_session
//   type: default _Session for database_connection_manager_template.
// Composes the djinterp connection_config (core networking / auth) with
// UI-layer concerns (name, comment, stats) and the two optional wrappers
// (SSH tunnel, SSL).  Callers who need more fields may either extend
// this struct or provide their own _Session with the same member names.
struct database_session
{
    // ---- identity ------------------------------------------------------

    std::string       name;               // user-visible session name
    std::string       comment;            // free-form note

    // ---- vendor and core connection ------------------------------------

    database_type     vendor  = database_type::unknown;
    connection_config config{};

    // ---- optional wrappers ---------------------------------------------

    ssh_tunnel_config ssh{};
    ssl_config        ssl{};

    // ---- advanced ------------------------------------------------------

    std::uint32_t  timeout_seconds = 30;
    std::string    startup_sql;           // executed on successful open
    bool           compressed_protocol = false;

    // ---- statistics (helper-maintained, read-only to the UI) -----------

    std::chrono::system_clock::time_point  last_connected{};
    std::uint64_t  connection_count = 0;
};




// ===============================================================================
//  4  VENDOR INFO LOOKUP
// ===============================================================================

// vendor_info_for
//   function: runtime adapter from database_type to vendor_info.  Bridges
// the compile-time vendor_traits<> specializations in djinterp to the
// runtime enum-based dispatch that a multi-vendor session manager needs.
// Unknown vendors return a zero-initialized vendor_info with the type
// field preserved.
inline vendor_info vendor_info_for(database_type _dt)
{
    // dispatch via vendor_traits specializations - each branch emits a
    // single call; the compiler collapses these to a small jump table
    // at the point of instantiation.
    switch (_dt)
    {
        case database_type::mysql:
            return djinterp::database::
                vendor_traits<database_type::mysql>::get_info();

        case database_type::mariadb:
            return djinterp::database::
                vendor_traits<database_type::mariadb>::get_info();

        case database_type::postgresql:
            return djinterp::database::
                vendor_traits<database_type::postgresql>::get_info();

        case database_type::sqlite:
            return djinterp::database::
                vendor_traits<database_type::sqlite>::get_info();

        case database_type::mssql:
            return djinterp::database::
                vendor_traits<database_type::mssql>::get_info();

        case database_type::oracle:
            return djinterp::database::
                vendor_traits<database_type::oracle>::get_info();

        case database_type::redis:
            return djinterp::database::
                vendor_traits<database_type::redis>::get_info();

        case database_type::mongodb:
            return djinterp::database::
                vendor_traits<database_type::mongodb>::get_info();

        case database_type::unknown:
        default:
            // return a zero-initialized record, preserving the tag so
            // callers can still branch on it
            {
                vendor_info info{};
                info.type = _dt;
                return info;
            }
    }
}




// ===============================================================================
//  5  FIELD-VISIBILITY HELPERS
// ===============================================================================
//   Per-vendor predicates that drive which fields the detail form shows.
// Kept as free functions so they are usable outside the template (e.g.
// in serialization paths that also need to know whether to read a
// `host` field for a given vendor).

// uses_host_port
//   function: true if the vendor is TCP-based and needs host / port
// fields (every network RDBMS and KV store).
inline bool uses_host_port(database_type _dt) noexcept
{
    const auto info = vendor_info_for(_dt);

    // embedded databases do not use host/port
    return !info.is_embedded;
}

// uses_file_path
//   function: true if the vendor stores its data in a file that the
// user must locate (SQLite, embedded Firebird, DuckDB).
inline bool uses_file_path(database_type _dt) noexcept
{
    return vendor_info_for(_dt).is_embedded;
}

// uses_named_database
//   function: true if the vendor distinguishes between schemas /
// databases at connection time (most RDBMSes; not Redis, not SQLite).
inline bool uses_named_database(database_type _dt) noexcept
{
    switch (_dt)
    {
        case database_type::sqlite:
        case database_type::redis:
        case database_type::unknown:
            return false;

        default:
            return true;
    }
}

// uses_ssh_tunnel
//   function: true if it makes sense to wrap this vendor in an SSH
// tunnel.  Embedded databases do not benefit - they are local-file.
inline bool uses_ssh_tunnel(database_type _dt) noexcept
{
    return !vendor_info_for(_dt).is_embedded;
}

// uses_ssl
//   function: true if the vendor itself supports SSL/TLS.  Consulted
// when deciding whether to show the SSL tab at all.
inline bool uses_ssl(database_type _dt) noexcept
{
    return vendor_info_for(_dt).supports_ssl;
}




// ===============================================================================
//  6  DATABASE CONNECTION MANAGER TEMPLATE
// ===============================================================================

// database_connection_manager_template
//   class template: session manager specialized for database connections.
// Adds vendor awareness, SSH/SSL state plumbing, per-vendor field
// visibility, default-port assignment, and a Test-Connection affordance
// on top of the generic connection_manager_template.
//
// Template parameters:
//   _helper:   the concrete CRTP implementation type
//   _Session:  session record type; defaults to `database_session`.
//              Must expose .name, .comment, .vendor, .config, .ssh, .ssl
//              for the template's helpers to work (detection is
//              structural - non-default _Session types with matching
//              members work without modification).
template <typename _helper,
          typename _Session = database_session>
class database_connection_manager_template
    : public connection_manager_template<_helper, _Session>
{
public:

    // ---------------------------------------------------------------------
    //  type aliases
    // ---------------------------------------------------------------------

    using base_type    = connection_manager_template<_helper, _Session>;
    using session_type = _Session;

    using typename base_type::session_storage_type;
    using typename base_type::index_type;
    using typename base_type::selection_type;


    // ---------------------------------------------------------------------
    //  construction
    // ---------------------------------------------------------------------

    database_connection_manager_template()
        : base_type()
        , supported_vendors{}
        , supports_ssh_tunnel_feature(true)
        , supports_ssl_feature(true)
        , supports_compression(true)
        , supports_startup_sql(true)
    {
        // enable the vendor-level features that make sense by default
        // for a database session manager
        this->can_test      = true;
        this->can_duplicate = true;
        this->can_rename    = true;
        this->can_import    = true;
        this->can_export    = true;

        // populate a reasonable default vendor list - the concrete
        // _helper is expected to prune this based on which vendors it
        // has actually linked in
        supported_vendors = {
            database_type::mysql,
            database_type::mariadb,
            database_type::postgresql,
            database_type::sqlite,
            database_type::mssql,
            database_type::oracle
        };
    }

    explicit database_connection_manager_template(layout_metadata _metadata)
        : base_type(std::move(_metadata))
        , supported_vendors{}
        , supports_ssh_tunnel_feature(true)
        , supports_ssl_feature(true)
        , supports_compression(true)
        , supports_startup_sql(true)
    {
        this->can_test      = true;
        this->can_duplicate = true;
        this->can_rename    = true;
        this->can_import    = true;
        this->can_export    = true;

        supported_vendors = {
            database_type::mysql,
            database_type::mariadb,
            database_type::postgresql,
            database_type::sqlite,
            database_type::mssql,
            database_type::oracle
        };
    }

    ~database_connection_manager_template() = default;


    // ---------------------------------------------------------------------
    //  vendor queries
    // ---------------------------------------------------------------------

    // vendor_info_of
    //   function: vendor metadata (display name, default port, flags)
    // for the given database_type.  Thin wrapper around vendor_info_for
    // for in-class call sites.
    [[nodiscard]] static vendor_info
    vendor_info_of(database_type _dt) noexcept
    {
        return vendor_info_for(_dt);
    }

    // default_port_for
    //   function: vendor-preferred TCP port.  0 for embedded databases.
    [[nodiscard]] static std::uint16_t
    default_port_for(database_type _dt) noexcept
    {
        return vendor_info_for(_dt).default_port;
    }

    // display_name_for
    //   function: user-facing vendor name (e.g. "PostgreSQL").
    [[nodiscard]] static std::string
    display_name_for(database_type _dt)
    {
        const auto info = vendor_info_for(_dt);

        // vendor_info.display_name is a C string owned by the trait;
        // copy into a std::string for safe return-by-value
        return (info.display_name != nullptr)
             ? std::string(info.display_name)
             : std::string();
    }


    // ---------------------------------------------------------------------
    //  vendor mutation
    // ---------------------------------------------------------------------

    // set_session_vendor
    //   function: change the vendor tag of the session at _idx.  If
    // _apply_defaults is true, the session's default port / schema is
    // reset to the new vendor's defaults.  Fires the helper's
    // vendor_changed_helper and the on_vendor_changed callback.
    void set_session_vendor(index_type    _idx,
                            database_type _vendor,
                            bool          _apply_defaults = true)
    {
        // guard against out-of-range indices
        if (_idx >= this->sessions.size())
        {
            return;
        }

        _Session&           s       = this->sessions[_idx];
        const database_type old_dt  = s.vendor;

        // short-circuit if nothing actually changes
        if (old_dt == _vendor)
        {
            return;
        }

        s.vendor = _vendor;

        if (_apply_defaults)
        {
            apply_vendor_defaults(s);
        }

        this->dirty = true;

        this->impl().vendor_changed_helper(old_dt, _vendor);

        if (on_vendor_changed)
        {
            on_vendor_changed(old_dt, _vendor);
        }

        return;
    }

    // apply_vendor_defaults
    //   function: reset a session's port, schema defaults, and
    // SSL-availability flag based on its current vendor.  Does not
    // touch host, user, password, or database name - those are treated
    // as user intent that should survive a vendor switch.
    void apply_vendor_defaults(_Session& _s) const
    {
        const auto info = vendor_info_for(_s.vendor);

        // port: always reset to vendor default on vendor switch
        _s.config.port = info.default_port;

        // SSL availability: clear ssl.enabled on vendors that cannot
        // do SSL at all, but leave the fields alone so the user does
        // not lose typed paths if they switch back
        if (!info.supports_ssl)
        {
            _s.ssl.enabled = false;
        }

        // SSH tunnel: pointless for embedded databases
        if (info.is_embedded)
        {
            _s.ssh.enabled = false;
        }

        return;
    }


    // ---------------------------------------------------------------------
    //  per-session field visibility
    //   Convenience wrappers around the free helpers that look up the
    // vendor from a specific session or from the current selection.
    // ---------------------------------------------------------------------

    // selection_uses_host_port
    //   function: true if the currently selected session needs host/port.
    [[nodiscard]] bool selection_uses_host_port() const noexcept
    {
        const auto* s = this->selected_session();

        return (s != nullptr) && uses_host_port(s->vendor);
    }

    // selection_uses_file_path
    //   function: true if the currently selected session needs a file
    // path (SQLite and other embedded vendors).
    [[nodiscard]] bool selection_uses_file_path() const noexcept
    {
        const auto* s = this->selected_session();

        return (s != nullptr) && uses_file_path(s->vendor);
    }

    // selection_uses_ssh_tunnel
    //   function: true if the current selection could be tunneled over
    // SSH AND the manager has the feature enabled.
    [[nodiscard]] bool selection_uses_ssh_tunnel() const noexcept
    {
        const auto* s = this->selected_session();

        return (s != nullptr)
            && supports_ssh_tunnel_feature
            && uses_ssh_tunnel(s->vendor);
    }

    // selection_uses_ssl
    //   function: true if the current selection supports SSL AND the
    // manager has the feature enabled.
    [[nodiscard]] bool selection_uses_ssl() const noexcept
    {
        const auto* s = this->selected_session();

        return (s != nullptr)
            && supports_ssl_feature
            && uses_ssl(s->vendor);
    }


    // ---------------------------------------------------------------------
    //  CRTP-forwarded database-specific actions
    // ---------------------------------------------------------------------

    // test_selected_connection
    //   function: ask the _helper to attempt a lightweight connect +
    // ping + disconnect against the currently selected session.
    // Returns the helper's verdict; the on_test_completed callback
    // fires regardless of outcome.
    bool test_selected_connection()
    {
        const auto* s = this->selected_session();

        if (s == nullptr)
        {
            return false;
        }

        const bool ok = this->impl().test_connection_helper(*s);

        if (on_test_completed)
        {
            on_test_completed(*s, ok);
        }

        return ok;
    }

    // open_selected_connection
    //   function: ask the _helper to open a real connection to the
    // currently selected session.  On return, last_connected and
    // connection_count on the selected session are updated.
    void open_selected_connection()
    {
        _Session* s = this->selected_session();

        if (s == nullptr)
        {
            return;
        }

        this->impl().open_connection_helper(*s);

        // stat tracking - the helper may have populated these itself
        // (preferred, since it knows whether the open truly succeeded),
        // but update as a fallback
        s->last_connected = std::chrono::system_clock::now();
        s->connection_count += 1;

        if (on_connection_opened)
        {
            on_connection_opened(*s);
        }

        return;
    }


    // ---------------------------------------------------------------------
    //  default CRTP hooks
    //   These provide sensible no-ops so the template is instantiable on
    // its own.  Concrete _helpers shadow these with their real
    // implementations via name hiding.
    // ---------------------------------------------------------------------

    // test_connection_helper
    //   function: default - returns false (nothing to test).
    [[nodiscard]] bool test_connection_helper(const _Session&) const
    {
        return false;
    }

    // open_connection_helper
    //   function: default - no-op.
    void open_connection_helper(const _Session&)
    {
        return;
    }

    // vendor_changed_helper
    //   function: default - no-op.
    void vendor_changed_helper(database_type  /*old_dt*/,
                               database_type  /*new_dt*/)
    {
        return;
    }


    // =====================================================================
    //  public members
    // =====================================================================

    // ---- vendor enumeration --------------------------------------------

    // supported_vendors
    //   member: the vendors that appear in the Network-Type dropdown.
    // Populated with a broad default set in the constructor; _helpers
    // that link only a subset of vendor back-ends are expected to
    // prune this list at construction.
    std::vector<database_type>  supported_vendors;


    // ---- manager-wide feature toggles ----------------------------------

    bool  supports_ssh_tunnel_feature;  // show SSH tab at all
    bool  supports_ssl_feature;         // show SSL tab at all
    bool  supports_compression;         // expose compressed_protocol
    bool  supports_startup_sql;         // expose startup_sql field


    // ---- callbacks -----------------------------------------------------

    // on_vendor_changed
    //   callable: fired after set_session_vendor successfully changes
    // the vendor tag.  Arguments are (old_type, new_type).
    std::function<void(database_type,
                       database_type)>   on_vendor_changed;

    // on_test_completed
    //   callable: fired after test_selected_connection returns.
    // Arguments are (session, ok).
    std::function<void(const _Session&,
                       bool)>            on_test_completed;

    // on_connection_opened
    //   callable: fired after open_selected_connection returns.
    std::function<void(const _Session&)> on_connection_opened;
};


NS_END  // layout
NS_END  // uxoxo


#endif  // UXOXO_LAYOUT_DATABASE_CONNECTION_MANAGER_TEMPLATE_
