/*******************************************************************************
* uxoxo [component]                                             mariadb_login.hpp
*
* MariaDB login form:
*   Vendor-specialized extension of database_login<database_type::mariadb,
* _F>.  Adds slots for the MariaDB-specific connection surface (Galera-
* aware wsrep options, default storage engine) plus the MySQL-family
* common extras shared with Oracle MySQL (unix socket, init command,
* compression, multi-statement, local-infile, connect attributes).
*
*   MariaDB uses the generic dlf_ssl slot rather than a vendor-specific
* SSL-mode enum; the MariaDB C client exposes SSL options through the
* same CA / cert / key / cipher surface that dlf_ssl already carries,
* so a dedicated mrlf_ssl_mode slot would be redundant.
*
*   The base identity + network + generic-target slots are inherited
* intact from dbl_form_base<_Feat> via the form_append_slots_t utility
* in database_login.hpp — so the existing dbl_tag::* accessors and
* dbl_* / fm_* / tc_* free functions all work on mariadb_login
* unchanged.
*
*   mdl_to_config() extracts a djinterp::database::mariadb_connect_config
* from the form's current field values, layering MySQL-common and
* MariaDB-specific overlays on top of the generic connection_config.
*
*   Zero overhead:  every vendor slot is EBO-collapsed when its
* feature bit is clear; every extraction and validation branch
* resolves at compile time.
*
* Contents:
*   1  vendor value-type aggregates
*   2  feature flags (mariadb_login_feat)
*   3  field tags (mdlf_tag)
*   4  slot-list extension
*   5  mariadb_login struct
*   6  free functions (setters, validate, to_config)
*   7  traits (is_mariadb_login)
*
*
* path:      /inc/uxoxo/component/mariadb_login.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_MARIADB_LOGIN_
#define  UXOXO_COMPONENT_MARIADB_LOGIN_ 1

// std
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/db/mariadb.hpp>
#include <djinterp/core/db/mysql_common.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./database_login.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  VENDOR VALUE-TYPE AGGREGATES
// ===============================================================================

// galera_settings
//   struct: Galera-cluster awareness flags bundled as one slot value.
// Both are "false" under single-node MariaDB and only meaningful on a
// wsrep-enabled cluster; bundling them keeps the slot list compact
// and reclaims the full group's storage via EBO when disabled.
struct galera_settings
{
    bool  wsrep_sync_wait    = false;
    bool  wsrep_causal_reads = false;
};




// ===============================================================================
//  2  MARIADB LOGIN FEATURE FLAGS
// ===============================================================================
//   Non-overlapping with database_login_feat (bits 0-14).  The
// MySQL-common extras occupy bits 16-21 (identical assignments to
// mysql_login for wire-level consistency); MariaDB-specific bits
// occupy 24-25.

enum mariadb_login_feat : unsigned
{
    mdlf_none            = 0,

    // -- mysql-family common extras ----------------------------------
    mdlf_unix_socket     = 1u << 16,
    mdlf_init_command    = 1u << 17,
    mdlf_compression     = 1u << 18,
    mdlf_multi_stmt      = 1u << 19,
    mdlf_local_infile    = 1u << 20,
    mdlf_connect_attrs   = 1u << 21,

    // -- mariadb-specific --------------------------------------------
    mdlf_storage_engine  = 1u << 24,
    mdlf_galera          = 1u << 25,

    // -- presets -----------------------------------------------------
    mdlf_common_extras   = mdlf_init_command | mdlf_compression,
    mdlf_typical         = mdlf_common_extras,
    mdlf_clustered       = mdlf_typical | mdlf_galera,

    mdlf_all             = mdlf_unix_socket    | mdlf_init_command
                         | mdlf_compression    | mdlf_multi_stmt
                         | mdlf_local_infile   | mdlf_connect_attrs
                         | mdlf_storage_engine | mdlf_galera
};

constexpr unsigned
operator|(
    mariadb_login_feat _a,
    mariadb_login_feat _b
) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr unsigned
operator|(
    database_login_feat _a,
    mariadb_login_feat  _b
) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr unsigned
operator|(
    mariadb_login_feat  _a,
    database_login_feat _b
) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

// has_mdlf
//   function: compile-time test of a mariadb_login feature bit against
// a raw feature-mask.
constexpr bool
has_mdlf(
    unsigned           _f,
    mariadb_login_feat _bit
) noexcept
{
    return ( (_f & static_cast<unsigned>(_bit)) != 0u );
}




// ===============================================================================
//  3  FIELD TAGS
// ===============================================================================
//   Tags for the mariadb-specific slots.  Tags for the inherited base
// slots (username, password, host, port, schema, ssl, uri, charset)
// remain in dbl_tag::.

namespace mdlf_tag
{
    struct unix_socket_tag      {};
    struct init_command_tag     {};
    struct compression_tag      {};
    struct multi_stmt_tag       {};
    struct local_infile_tag     {};
    struct connect_attrs_tag    {};
    struct storage_engine_tag   {};
    struct galera_tag           {};

}   // namespace mdlf_tag




// ===============================================================================
//  4  SLOT-LIST EXTENSION
// ===============================================================================
//   The MariaDB vendor slots are appended to dbl_form_base<_Feat> via
// the shared form_append_slots_t utility.  The resulting form<>
// carries the union of base + vendor slots under a single _Feat —
// dlf_* bits gate the base slots, mdlf_* bits gate the vendor slots.

NS_INTERNAL

    // mariadb_login_form_base
    //   alias: the form<_Feat, ...combined_slots...> base type of
    // mariadb_login.  Composed by appending MySQL-common + MariaDB-
    // specific slots to dbl_form_base<_Feat>.
    template <unsigned _Feat>
    using mariadb_login_form_base = form_append_slots_t<
        dbl_form_base<_Feat>,

        // -- mysql-family common extras ------------------------------
        slot<mdlf_unix_socket,
             field<mdlf_tag::unix_socket_tag,     text_input<>>>,
        slot<mdlf_init_command,
             field<mdlf_tag::init_command_tag,    text_input<>>>,
        slot<mdlf_compression,
             field<mdlf_tag::compression_tag,     bool>>,
        slot<mdlf_multi_stmt,
             field<mdlf_tag::multi_stmt_tag,      bool>>,
        slot<mdlf_local_infile,
             field<mdlf_tag::local_infile_tag,    bool>>,
        slot<mdlf_connect_attrs,
             field<mdlf_tag::connect_attrs_tag,
                   std::map<std::string, std::string>>>,

        // -- mariadb-specific ----------------------------------------
        slot<mdlf_storage_engine,
             field<mdlf_tag::storage_engine_tag,  text_input<>>>,
        slot<mdlf_galera,
             field<mdlf_tag::galera_tag,          galera_settings>>
    >;

NS_END  // internal




// ===============================================================================
//  5  MARIADB LOGIN
// ===============================================================================
//   _Feat  bitwise-OR of database_login_feat (dlf_*) and mariadb_login_feat
//          (mdlf_*) bits.  dlf_* bits gate base identity / network /
//          generic-target slots; mdlf_* bits gate MySQL-common and
//          MariaDB-specific slots.  At least one of dlf_username,
//          dlf_password, or dlf_database_name must be set (inherited
//          contract from database_login).

// mariadb_login
//   struct: MariaDB login form.  A sibling of
// database_login<database_type::mariadb, _Feat> that additionally
// carries the MariaDB vendor surface as EBO-collapsing slots.
// Satisfies is_database_login_v<> structurally via db_type +
// submittable-form.
template <unsigned _Feat = (dlf_default | dlf_charset | mdlf_typical)>
struct mariadb_login : internal::mariadb_login_form_base<_Feat>
{
    using base_type = internal::mariadb_login_form_base<_Feat>;
    using self_type = mariadb_login<_Feat>;
    using vendor    = djinterp::database::vendor_traits<
                          djinterp::database::database_type::mariadb>;

    static constexpr djinterp::database::database_type  db_type        = djinterp::database::database_type::mariadb;
    static constexpr unsigned                     login_features = _Feat;

    // -- base-slot presence (inherited contract) ----------------------
    static constexpr bool has_username       = has_dlf(_Feat, dlf_username);
    static constexpr bool has_password       = has_dlf(_Feat, dlf_password);
    static constexpr bool has_host           = has_dlf(_Feat, dlf_host);
    static constexpr bool has_port           = has_dlf(_Feat, dlf_port);
    static constexpr bool has_database_name  = has_dlf(_Feat, dlf_database_name);
    static constexpr bool has_schema         = has_dlf(_Feat, dlf_schema);
    static constexpr bool has_ssl            = has_dlf(_Feat, dlf_ssl);
    static constexpr bool has_uri            = has_dlf(_Feat, dlf_uri);
    static constexpr bool has_charset        = has_dlf(_Feat, dlf_charset);

    // -- vendor-slot presence -----------------------------------------
    static constexpr bool has_unix_socket    = has_mdlf(_Feat, mdlf_unix_socket);
    static constexpr bool has_init_command   = has_mdlf(_Feat, mdlf_init_command);
    static constexpr bool has_compression    = has_mdlf(_Feat, mdlf_compression);
    static constexpr bool has_multi_stmt     = has_mdlf(_Feat, mdlf_multi_stmt);
    static constexpr bool has_local_infile   = has_mdlf(_Feat, mdlf_local_infile);
    static constexpr bool has_connect_attrs  = has_mdlf(_Feat, mdlf_connect_attrs);
    static constexpr bool has_storage_engine = has_mdlf(_Feat, mdlf_storage_engine);
    static constexpr bool has_galera         = has_mdlf(_Feat, mdlf_galera);

    static_assert( (has_username || has_password || has_database_name),
                   "mariadb_login requires at least one of dlf_username, "
                   "dlf_password, or dlf_database_name." );

    // -- construction -------------------------------------------------
    mariadb_login()
    {
        apply_vendor_defaults_();

        return;
    }

private:
    /*
    apply_vendor_defaults_
      Seeds enabled slots from the MariaDB vendor contract — port 3306,
    charset "utf8mb4", default storage engine "InnoDB" — plus the base
    host / port defaults from vendor_traits<database_type::mariadb>.
    Only slots enabled by _Feat are touched; every other branch elides
    at compile time.

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
            // MariaDB 10.6+ defaults to utf8mb4; keep in sync with
            // mysql_common::mysql_connect_config's default_charset.
            tc_get<dbl_tag::charset_tag>(*this) = "utf8mb4";
        }

        // -- vendor-slot defaults -------------------------------------
        if constexpr (has_storage_engine)
        {
            ti_set_value(tc_get<mdlf_tag::storage_engine_tag>(*this),
                         std::string{"InnoDB"});
        }

        return;
    }
};




// ===============================================================================
//  6  FREE FUNCTIONS
// ===============================================================================

// mdl_set_storage_engine
//   function: set the default storage engine (requires
// mdlf_storage_engine).  Typical values: "InnoDB", "Aria", "MyISAM",
// "Memory", "Spider".
template <unsigned _F>
void
mdl_set_storage_engine(
    mariadb_login<_F>& _mdl,
    std::string        _engine
)
{
    static_assert(has_mdlf(_F, mdlf_storage_engine),
                  "requires mdlf_storage_engine");

    ti_set_value(tc_get<mdlf_tag::storage_engine_tag>(_mdl),
                 std::move(_engine));

    return;
}

// mdl_set_galera
//   function: set Galera wsrep_sync_wait and wsrep_causal_reads
// (requires mdlf_galera).
template <unsigned _F>
void
mdl_set_galera(
    mariadb_login<_F>& _mdl,
    bool               _sync_wait,
    bool               _causal_reads
)
{
    static_assert(has_mdlf(_F, mdlf_galera), "requires mdlf_galera");

    auto& g = tc_get<mdlf_tag::galera_tag>(_mdl);

    g.wsrep_sync_wait    = _sync_wait;
    g.wsrep_causal_reads = _causal_reads;

    return;
}

// mdl_set_init_command
//   function: set the init SQL fired on every new connection (requires
// mdlf_init_command).
template <unsigned _F>
void
mdl_set_init_command(
    mariadb_login<_F>& _mdl,
    std::string        _sql
)
{
    static_assert(has_mdlf(_F, mdlf_init_command),
                  "requires mdlf_init_command");

    ti_set_value(tc_get<mdlf_tag::init_command_tag>(_mdl),
                 std::move(_sql));

    return;
}

// mdl_add_connect_attr
//   function: add / overwrite a single connect attribute pair on the
// connect_attrs slot (requires mdlf_connect_attrs).
template <unsigned _F>
void
mdl_add_connect_attr(
    mariadb_login<_F>& _mdl,
    std::string        _key,
    std::string        _value
)
{
    static_assert(has_mdlf(_F, mdlf_connect_attrs),
                  "requires mdlf_connect_attrs");

    tc_get<mdlf_tag::connect_attrs_tag>(_mdl)[std::move(_key)]
        = std::move(_value);

    return;
}

// mdl_validate
//   function: validate every enabled input field and return true iff
// all pass.  Same dispatch pattern as dbl_validate / ml_validate —
// URI-mode short-circuit plus fm_validate_with for the remaining
// fields.
template <unsigned _F>
bool
mdl_validate(
    mariadb_login<_F>& _mdl
)
{
    // URI-mode short-circuit (same contract as dbl_validate).
    if constexpr (has_dlf(_F, dlf_uri))
    {
        auto& u = tc_get<dbl_tag::uri_tag>(_mdl);

        if (u.active)
        {
            auto r = ti_validate(u.uri);

            return ( r.result == validation_result::valid );
        }
    }

    return fm_validate_with(_mdl,
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
mdl_to_config
  Extract a djinterp::database::mariadb_connect_config from the
mariadb_login's current field values.  Layers three overlays in
order:
  1. vendor_traits<mariadb>::make_default_config() for the base
     connection_config surface (host, port, credentials, ssl paths,
     charset, schema).
  2. mysql_common overlays from mdlf_* common-extras slots
     (unix_socket, init_command, compression, multi_stmt,
     local_infile, connect_attrs).
  3. MariaDB-specific overlays (default_storage_engine, galera
     options).

  Disabled slots elide at compile time.  The returned config is ready
for hand-off to djinterp::database::mariadb_connection.

Parameter(s):
  _mdl: the login form to read.
Return:
  A fully populated mariadb_connect_config.
*/
template <unsigned _F>
djinterp::database::mariadb_connect_config
mdl_to_config(
    const mariadb_login<_F>& _mdl
)
{
    djinterp::database::mariadb_connect_config cfg;

    // -- base connection_config (cfg.mysql_config.base) --------------
    cfg.mysql_config.base =
        djinterp::database::vendor_traits<
            djinterp::database::database_type::mariadb>::make_default_config();

    if constexpr (has_dlf(_F, dlf_host))
    {
        cfg.mysql_config.base.host = tc_get<dbl_tag::host_tag>(_mdl).value;
    }

    if constexpr (has_dlf(_F, dlf_port))
    {
        cfg.mysql_config.base.port = tc_get<dbl_tag::port_tag>(_mdl);
    }

    if constexpr (has_dlf(_F, dlf_database_name))
    {
        cfg.mysql_config.base.database =
            tc_get<dbl_tag::database_name_tag>(_mdl).value;
    }

    if constexpr (has_dlf(_F, dlf_schema))
    {
        cfg.mysql_config.base.schema =
            tc_get<dbl_tag::schema_tag>(_mdl).value;
    }

    if constexpr (has_dlf(_F, dlf_username))
    {
        cfg.mysql_config.base.username =
            tc_get<dbl_tag::username_tag>(_mdl).value;
    }

    if constexpr (has_dlf(_F, dlf_password))
    {
        cfg.mysql_config.base.password =
            tc_get<dbl_tag::password_tag>(_mdl).value;
    }

    if constexpr (has_dlf(_F, dlf_ssl))
    {
        const auto& s = tc_get<dbl_tag::ssl_tag>(_mdl);

        cfg.mysql_config.base.enable_ssl = s.enable;
        cfg.mysql_config.base.verify_ssl = s.verify;
        cfg.mysql_config.base.ssl_ca     = s.ca;
        cfg.mysql_config.base.ssl_cert   = s.cert;
        cfg.mysql_config.base.ssl_key    = s.key;
    }

    if constexpr (has_dlf(_F, dlf_charset))
    {
        cfg.mysql_config.base.charset =
            tc_get<dbl_tag::charset_tag>(_mdl);
    }

    // -- mysql-common overlays (cfg.mysql_config.*) ------------------
    if constexpr (has_mdlf(_F, mdlf_unix_socket))
    {
        cfg.mysql_config.unix_socket =
            tc_get<mdlf_tag::unix_socket_tag>(_mdl).value;
    }

    if constexpr (has_mdlf(_F, mdlf_init_command))
    {
        cfg.mysql_config.init_command =
            tc_get<mdlf_tag::init_command_tag>(_mdl).value;
    }

    if constexpr (has_mdlf(_F, mdlf_compression))
    {
        cfg.mysql_config.use_compression =
            tc_get<mdlf_tag::compression_tag>(_mdl);
    }

    if constexpr (has_mdlf(_F, mdlf_multi_stmt))
    {
        cfg.mysql_config.multi_statements =
            tc_get<mdlf_tag::multi_stmt_tag>(_mdl);
    }

    if constexpr (has_mdlf(_F, mdlf_local_infile))
    {
        cfg.mysql_config.use_local_infile =
            tc_get<mdlf_tag::local_infile_tag>(_mdl);
    }

    if constexpr (has_mdlf(_F, mdlf_connect_attrs))
    {
        cfg.mysql_config.connect_attributes =
            tc_get<mdlf_tag::connect_attrs_tag>(_mdl);
    }

    // -- mariadb-specific overlays (cfg.*) ---------------------------
    if constexpr (has_mdlf(_F, mdlf_storage_engine))
    {
        cfg.default_storage_engine =
            tc_get<mdlf_tag::storage_engine_tag>(_mdl).value;
    }

    if constexpr (has_mdlf(_F, mdlf_galera))
    {
        const auto& g = tc_get<mdlf_tag::galera_tag>(_mdl);

        cfg.galera_wsrep_sync_wait    = g.wsrep_sync_wait;
        cfg.galera_wsrep_causal_reads = g.wsrep_causal_reads;
    }

    return cfg;
}

// mdl_reset_to_defaults
//   function: reset every field to vendor defaults and clear form
// error / submitting state.
template <unsigned _F>
void
mdl_reset_to_defaults(
    mariadb_login<_F>& _mdl
)
{
    _mdl = mariadb_login<_F>{};

    return;
}




// ===============================================================================
//  7  TRAITS
// ===============================================================================

namespace mariadb_login_traits
{
NS_INTERNAL

    // is_mariadb_login_impl
    //   trait: structural detector for instantiations of mariadb_login.
    template <typename>
    struct is_mariadb_login_impl : std::false_type
    {};

    template <unsigned _F>
    struct is_mariadb_login_impl<mariadb_login<_F>> : std::true_type
    {};

NS_END  // internal

// is_mariadb_login
//   trait: structural predicate for mariadb_login instantiations.
// Convenience wrapper; user-defined aggregates that want to pose as a
// MariaDB login form are better tested via is_database_login_v plus a
// db_type == database_type::mariadb check.
template <typename _Type>
struct is_mariadb_login : internal::is_mariadb_login_impl<_Type>
{};

template <typename _T>
inline constexpr bool is_mariadb_login_v =
    is_mariadb_login<_T>::value;


}   // namespace mariadb_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MARIADB_LOGIN_
