/*******************************************************************************
* uxoxo [component]                                          database_login.hpp
*
* Generic database login form:
*   A framework-agnostic, pure-data database login template.  Built
* directly on the generic form / composite machinery in form_builder.hpp
* and template_component.hpp: every field (username, password, host,
* port, database name, schema, SSL, URI, charset) is a slot gated on a
* feature-flag bit; disabled slots EBO-collapse to zero bytes; every
* access resolves at compile time to a direct member reference.
*
*   The template is parameterized on database_type from
* database_common.hpp so that vendor_traits can supply compile-time
* defaults (default port, SSL support, charset, schema).  A free
* function dbl_to_config() extracts a djinterp::database::connection_config
* from a database_login for hand-off to the connection layer.
*
*   Vendor-specific forms (sqlite_database_login, mariadb_database_login,
* etc.) are trivial aliases over database_login with the appropriate
* feature-flag combination.
*
*   Zero overhead:
*     - Disabled slots contribute zero bytes (field_storage<false, ...>).
*     - Every feature check resolves at compile time (static constexpr +
*       if constexpr).
*     - No virtual dispatch, no type erasure, no allocation beyond
*       whatever the user's own field types and std::function<...>
*       impose.
*
* Contents:
*   1  value-type aggregates (ssl_settings, uri_settings)
*   2  feature flags (database_login_feat)
*   3  field tags
*   4  slot-list alias
*   5  database_login struct
*   6  free functions (setters, validate, to_config, reset, toggles)
*   7  traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/database_login.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_DATABASE_LOGIN_
#define  UXOXO_COMPONENT_DATABASE_LOGIN_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/db/database_connection_template.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../template_component.hpp"
#include "../form_builder.hpp"
#include "../input/text_input.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  VALUE-TYPE AGGREGATES
// ===============================================================================
//   Bundled value types for composite fields whose logical unit is larger
// than a single primitive.  Grouping them into one struct means one tag,
// one bit, one slot - and disabled, still zero bytes.

// ssl_settings
//   struct: SSL/TLS configuration bundle stored as a single field value.
struct ssl_settings
{
    bool        enable = false;
    bool        verify = false;
    std::string ca;
    std::string cert;
    std::string key;
};

// uri_settings
//   struct: connection-string URI plus active-mode toggle stored as a
// single field value.  When `active` is true, dbl_validate and
// dbl_to_config treat the URI as authoritative and bypass the discrete
// target fields.
struct uri_settings
{
    text_input<tif_validation> uri;
    bool                       active = false;
};




// ===============================================================================
//  2  DATABASE LOGIN FEATURE FLAGS
// ===============================================================================
//   Consolidated into a single unsigned bitmask.  Identity fields occupy
// bits 0–7; connection-target fields occupy bits 8–15.  The non-
// overlapping ranges reserve room for future growth and leave room above
// bit 16 for higher-level composites (dialogs, wizards) that embed a
// database_login as a sub-field.

enum database_login_feat : unsigned
{
    dlf_none            = 0,

    // -- login identity ----------------------------------------------
    dlf_username        = 1u << 0,
    dlf_password        = 1u << 1,
    dlf_remember_me     = 1u << 2,
    dlf_show_password   = 1u << 3,

    // -- connection target -------------------------------------------
    dlf_host            = 1u << 8,
    dlf_port            = 1u << 9,
    dlf_database_name   = 1u << 10,
    dlf_schema          = 1u << 11,
    dlf_ssl             = 1u << 12,
    dlf_uri             = 1u << 13,
    dlf_charset         = 1u << 14,

    // -- common presets ----------------------------------------------
    dlf_user_pass       = dlf_username | dlf_password,
    dlf_network         = dlf_host | dlf_port | dlf_database_name,
    dlf_network_full    = dlf_network | dlf_schema | dlf_ssl | dlf_charset,
    dlf_embedded        = dlf_database_name,
    dlf_default         = dlf_user_pass | dlf_network,

    dlf_all             = dlf_username      | dlf_password
                        | dlf_remember_me   | dlf_show_password
                        | dlf_host          | dlf_port
                        | dlf_database_name | dlf_schema
                        | dlf_ssl           | dlf_uri
                        | dlf_charset
};

constexpr unsigned
operator|(
    database_login_feat _a,
    database_login_feat _b
) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

// has_dlf
//   function: compile-time test of a database_login feature bit against
// a raw feature-mask.
constexpr bool
has_dlf(
    unsigned            _f,
    database_login_feat _bit
) noexcept
{
    return ( (_f & static_cast<unsigned>(_bit)) != 0u );
}




// ===============================================================================
//  3  FIELD TAGS
// ===============================================================================
//   Empty structs used purely for overload resolution in tc_get<_Tag>().
// Grouped in a nested namespace so callers can write
// `dbl_tag::username_tag` when disambiguation from identically-named
// tags in other composites matters.

namespace dbl_tag
{
    struct username_tag      {};
    struct password_tag      {};
    struct remember_me_tag   {};
    struct show_password_tag {};
    struct host_tag          {};
    struct port_tag          {};
    struct database_name_tag {};
    struct schema_tag        {};
    struct ssl_tag           {};
    struct uri_tag           {};
    struct charset_tag       {};

}   // namespace dbl_tag




// ===============================================================================
//  4  SLOT-LIST ALIAS
// ===============================================================================
//   The complete field catalogue for database_login, expressed once as a
// form<_Feat, slots...>.  database_login inherits from this alias so it
// gains EBO-collapsed slot storage, tag-based accessors, and the full
// fm_* / tc_* aggregate operation vocabulary for free.

NS_INTERNAL

    // dbl_form_base
    //   alias: the form<_Feat, ...slots...> base type of database_login.
    template <unsigned _Feat>
    using dbl_form_base = form<_Feat,
        slot<dlf_username,       field<dbl_tag::username_tag,
                                       text_input<tif_validation>>>,
        slot<dlf_password,       field<dbl_tag::password_tag,
                                       text_input<tif_masked | tif_validation>>>,
        slot<dlf_remember_me,    field<dbl_tag::remember_me_tag,    bool>>,
        slot<dlf_show_password,  field<dbl_tag::show_password_tag,  bool>>,
        slot<dlf_host,           field<dbl_tag::host_tag,
                                       text_input<tif_validation>>>,
        slot<dlf_port,           field<dbl_tag::port_tag,           std::uint16_t>>,
        slot<dlf_database_name,  field<dbl_tag::database_name_tag,
                                       text_input<tif_validation>>>,
        slot<dlf_schema,         field<dbl_tag::schema_tag,         text_input<>>>,
        slot<dlf_ssl,            field<dbl_tag::ssl_tag,            ssl_settings>>,
        slot<dlf_uri,            field<dbl_tag::uri_tag,            uri_settings>>,
        slot<dlf_charset,        field<dbl_tag::charset_tag,        std::string>>
    >;

    // is_text_input
    //   trait: structural detector for instantiations of text_input.
    template <typename>
    struct is_text_input : std::false_type
    {};

    template <unsigned _F,
              typename _I>
    struct is_text_input<text_input<_F, _I>> : std::true_type
    {};

    // form_append_slots
    //   metafunction: produces a new form<_F, _S..., _Extra...> by
    // appending _Extra slots to an existing form<_F, _S...>.  Used by
    // vendor-specific login forms (mysql_login, mariadb_login, ...)
    // to extend dbl_form_base without restating its slot pack.
    template <typename _Form,
              typename... _Extra>
    struct form_append_slots;

    template <unsigned    _F,
              typename... _S,
              typename... _Extra>
    struct form_append_slots<form<_F, _S...>, _Extra...>
    {
        using type = form<_F, _S..., _Extra...>;
    };

    // form_append_slots_t
    //   alias: convenience alias that names the extended form directly.
    template <typename _Form,
              typename... _Extra>
    using form_append_slots_t =
        typename form_append_slots<_Form, _Extra...>::type;

NS_END  // internal




// ===============================================================================
//  5  DATABASE LOGIN
// ===============================================================================
//   _Vendor   djinterp::database::database_type enum value.  Selects the
//             vendor_traits specialization used to seed defaults
//             (default_port, supports_ssl, make_default_config()).
//   _Feat     bitwise-OR of database_login_feat flags.  Unified across
//             identity and connection-target fields; the slot list above
//             dispatches each bit to its corresponding field.
//
//   At least one of dlf_username, dlf_password, or dlf_database_name
// must be set (enforced by static_assert below).

// database_login
//   struct: vendor-parameterized login + connection-target form.
// Derives from form<_Feat, ...slots...>; every disabled slot contributes
// zero bytes and every enabled access resolves to a direct member
// reference.  Vendor defaults are seeded at construction.
template <djinterp::database::database_type _Vendor,
          unsigned                    _Feat = dlf_default>
struct database_login : internal::dbl_form_base<_Feat>
{
    using base_type = internal::dbl_form_base<_Feat>;
    using self_type = database_login<_Vendor, _Feat>;
    using vendor    = djinterp::database::vendor_traits<_Vendor>;

    static constexpr djinterp::database::database_type  db_type     = _Vendor;
    static constexpr unsigned                     db_features = _Feat;

    static constexpr bool has_username       = has_dlf(_Feat, dlf_username);
    static constexpr bool has_password       = has_dlf(_Feat, dlf_password);
    static constexpr bool has_remember_me    = has_dlf(_Feat, dlf_remember_me);
    static constexpr bool has_show_password  = has_dlf(_Feat, dlf_show_password);
    static constexpr bool has_host           = has_dlf(_Feat, dlf_host);
    static constexpr bool has_port           = has_dlf(_Feat, dlf_port);
    static constexpr bool has_database_name  = has_dlf(_Feat, dlf_database_name);
    static constexpr bool has_schema         = has_dlf(_Feat, dlf_schema);
    static constexpr bool has_ssl            = has_dlf(_Feat, dlf_ssl);
    static constexpr bool has_uri            = has_dlf(_Feat, dlf_uri);
    static constexpr bool has_charset        = has_dlf(_Feat, dlf_charset);

    static_assert( (has_username || has_password || has_database_name),
                   "database_login requires at least one of dlf_username, "
                   "dlf_password, or dlf_database_name." );

    // -- construction -------------------------------------------------
    database_login()
    {
        apply_vendor_defaults_();

        return;
    }

private:
    /*
    apply_vendor_defaults_
      Seeds the enabled fields from vendor_traits<_Vendor> at
    construction.  Only the slots enabled by _Feat are touched; every
    other branch short-circuits at compile time via if constexpr and
    contributes no code.

    Parameter(s):
      none.
    Return:
      none.
    */
    void
    apply_vendor_defaults_()
    {
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

        if constexpr (has_ssl)
        {
            tc_get<dbl_tag::ssl_tag>(*this).enable = vendor::supports_ssl;
        }

        if constexpr (has_charset)
        {
            auto cfg = vendor::make_default_config();

            if (cfg.charset.has_value())
            {
                tc_get<dbl_tag::charset_tag>(*this) =
                    std::move(cfg.charset.value());
            }
        }

        if constexpr (has_schema)
        {
            auto cfg = vendor::make_default_config();

            if (cfg.schema.has_value())
            {
                ti_set_value(tc_get<dbl_tag::schema_tag>(*this),
                             std::move(cfg.schema.value()));
            }
        }

        return;
    }
};




// ===============================================================================
//  6  FREE FUNCTIONS
// ===============================================================================
//   Thin, statically-asserted setters that bundle the tag lookup with a
// compile-time feature-bit check.  Equivalent to the old direct-mixin
// API - callers who prefer direct access can use tc_get<tag>() instead.

// dbl_set_host
//   function: set the host text-input value (requires dlf_host).
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_set_host(
    database_login<_V, _F>& _dbl,
    std::string             _val
)
{
    static_assert(has_dlf(_F, dlf_host), "requires dlf_host");

    ti_set_value(tc_get<dbl_tag::host_tag>(_dbl), std::move(_val));

    return;
}

// dbl_set_port
//   function: set the numeric port value (requires dlf_port).
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_set_port(
    database_login<_V, _F>& _dbl,
    std::uint16_t           _val
)
{
    static_assert(has_dlf(_F, dlf_port), "requires dlf_port");

    tc_get<dbl_tag::port_tag>(_dbl) = _val;

    return;
}

// dbl_set_database_name
//   function: set the database/catalogue name (requires
// dlf_database_name).  For embedded databases such as SQLite this is
// the file path.
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_set_database_name(
    database_login<_V, _F>& _dbl,
    std::string             _val
)
{
    static_assert(has_dlf(_F, dlf_database_name),
                  "requires dlf_database_name");

    ti_set_value(tc_get<dbl_tag::database_name_tag>(_dbl),
                 std::move(_val));

    return;
}

// dbl_set_schema
//   function: set the schema/namespace (requires dlf_schema).
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_set_schema(
    database_login<_V, _F>& _dbl,
    std::string             _val
)
{
    static_assert(has_dlf(_F, dlf_schema), "requires dlf_schema");

    ti_set_value(tc_get<dbl_tag::schema_tag>(_dbl), std::move(_val));

    return;
}

// dbl_set_ssl
//   function: enable or disable SSL on the ssl slot (requires dlf_ssl).
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_set_ssl(
    database_login<_V, _F>& _dbl,
    bool                    _on
)
{
    static_assert(has_dlf(_F, dlf_ssl), "requires dlf_ssl");

    tc_get<dbl_tag::ssl_tag>(_dbl).enable = _on;

    return;
}

/*
dbl_set_ssl_certs
  Set the SSL CA certificate, client-certificate, and private-key
paths on the ssl slot.  All three paths may be empty.

Parameter(s):
  _dbl:  the login form to update.
  _ca:   path to the trusted CA certificate (empty for system default).
  _cert: path to the client certificate.
  _key:  path to the client private key.
Return:
  none.
*/
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_set_ssl_certs(
    database_login<_V, _F>& _dbl,
    std::string             _ca,
    std::string             _cert,
    std::string             _key
)
{
    static_assert(has_dlf(_F, dlf_ssl), "requires dlf_ssl");

    auto& s = tc_get<dbl_tag::ssl_tag>(_dbl);

    s.ca   = std::move(_ca);
    s.cert = std::move(_cert);
    s.key  = std::move(_key);

    return;
}

// dbl_set_connection_uri
//   function: set the connection-string URI and activate URI mode
// (requires dlf_uri).  When active, dbl_validate / dbl_to_config treat
// the URI as authoritative and skip the discrete target fields.
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_set_connection_uri(
    database_login<_V, _F>& _dbl,
    std::string             _uri
)
{
    static_assert(has_dlf(_F, dlf_uri), "requires dlf_uri");

    auto& u = tc_get<dbl_tag::uri_tag>(_dbl);

    ti_set_value(u.uri, std::move(_uri));
    u.active = true;

    return;
}

// dbl_set_charset
//   function: set the character-set string (requires dlf_charset).
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_set_charset(
    database_login<_V, _F>& _dbl,
    std::string             _val
)
{
    static_assert(has_dlf(_F, dlf_charset), "requires dlf_charset");

    tc_get<dbl_tag::charset_tag>(_dbl) = std::move(_val);

    return;
}

// dbl_toggle_show_password
//   function: toggle the show-password view flag and, if dlf_password
// is also present, propagate the inverse into the password field's
// masked flag (requires dlf_show_password).
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_toggle_show_password(
    database_login<_V, _F>& _dbl
)
{
    static_assert(has_dlf(_F, dlf_show_password),
                  "requires dlf_show_password");

    auto& show = tc_get<dbl_tag::show_password_tag>(_dbl);

    show = !show;

    if constexpr (has_dlf(_F, dlf_password))
    {
        tc_get<dbl_tag::password_tag>(_dbl).masked = !show;
    }

    return;
}

// dbl_toggle_remember_me
//   function: toggle the remember-me flag (requires dlf_remember_me).
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_toggle_remember_me(
    database_login<_V, _F>& _dbl
)
{
    static_assert(has_dlf(_F, dlf_remember_me),
                  "requires dlf_remember_me");

    auto& rm = tc_get<dbl_tag::remember_me_tag>(_dbl);

    rm = !rm;

    return;
}

/*
dbl_validate
  Validate every enabled input field and return true iff all pass.
When URI mode is active, only the URI field is validated and the
discrete target / identity fields are skipped - matching the
vendor contract that a URI, when provided, is the authoritative
source of connection parameters.

  Delegates the per-field walk to fm_validate_with; the inner
predicate dispatches per field type via nested if constexpr:
validated text_inputs are run through ti_validate; every other
enabled field (bools, ssl_settings, uri_settings, plain text_input
with no validators) is considered trivially valid.

Parameter(s):
  _dbl: the login form to validate.
Return:
  true if every applicable field was valid; false otherwise.
*/
template <djinterp::database::database_type _V,
          unsigned                    _F>
bool
dbl_validate(
    database_login<_V, _F>& _dbl
)
{
    // URI mode short-circuit: when URI is active, validate only it.
    if constexpr (has_dlf(_F, dlf_uri))
    {
        auto& u = tc_get<dbl_tag::uri_tag>(_dbl);

        if (u.active)
        {
            auto r = ti_validate(u.uri);

            return ( r.result == validation_result::valid );
        }
    }

    return fm_validate_with(_dbl,
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
dbl_to_config
  Extract a djinterp::database::connection_config from the form's current
field values, starting from vendor defaults and overlaying only the
fields actually present in _Feat.  Disabled fields are elided at
compile time via if constexpr and contribute no code.

  URI mode is not handled specially here - the caller who activates
URI mode is expected to pass the raw URI string to the connection
layer directly, bypassing connection_config.

Parameter(s):
  _dbl: the login form to read.
Return:
  A fully populated connection_config ready for hand-off to the
  connection layer.
*/
template <djinterp::database::database_type _V,
          unsigned                    _F>
djinterp::database::connection_config
dbl_to_config(
    const database_login<_V, _F>& _dbl
)
{
    djinterp::database::connection_config cfg =
        djinterp::database::vendor_traits<_V>::make_default_config();

    if constexpr (has_dlf(_F, dlf_host))
    {
        cfg.host = tc_get<dbl_tag::host_tag>(_dbl).value;
    }

    if constexpr (has_dlf(_F, dlf_port))
    {
        cfg.port = tc_get<dbl_tag::port_tag>(_dbl);
    }

    if constexpr (has_dlf(_F, dlf_database_name))
    {
        cfg.database = tc_get<dbl_tag::database_name_tag>(_dbl).value;
    }

    if constexpr (has_dlf(_F, dlf_schema))
    {
        cfg.schema = tc_get<dbl_tag::schema_tag>(_dbl).value;
    }

    if constexpr (has_dlf(_F, dlf_username))
    {
        cfg.username = tc_get<dbl_tag::username_tag>(_dbl).value;
    }

    if constexpr (has_dlf(_F, dlf_password))
    {
        cfg.password = tc_get<dbl_tag::password_tag>(_dbl).value;
    }

    if constexpr (has_dlf(_F, dlf_ssl))
    {
        const auto& s = tc_get<dbl_tag::ssl_tag>(_dbl);

        cfg.enable_ssl = s.enable;
        cfg.verify_ssl = s.verify;
        cfg.ssl_ca     = s.ca;
        cfg.ssl_cert   = s.cert;
        cfg.ssl_key    = s.key;
    }

    if constexpr (has_dlf(_F, dlf_charset))
    {
        cfg.charset = tc_get<dbl_tag::charset_tag>(_dbl);
    }

    return cfg;
}

// dbl_reset_to_defaults
//   function: reset every field to vendor defaults and clear form
// error / submitting state.  Implemented as value-reassignment from a
// freshly-constructed instance, which re-runs apply_vendor_defaults_().
template <djinterp::database::database_type _V,
          unsigned                    _F>
void
dbl_reset_to_defaults(
    database_login<_V, _F>& _dbl
)
{
    _dbl = database_login<_V, _F>{};

    return;
}




// ===============================================================================
//  7  TRAITS
// ===============================================================================
//   Structural predicates.  Because database_login derives from form
// (which derives from composite), the composite's has_field<_Tag>()
// static member provides per-slot presence queries for free - the
// trait layer only needs to confirm database-login-ness (db_type
// constant + submittable-form surface) and then delegate.

namespace database_login_traits
{
NS_INTERNAL

    // has_db_type_constant
    //   trait: detects whether _Type exposes a static db_type constant.
    template <typename, typename = void>
    struct has_db_type_constant : std::false_type
    {};

    template <typename _T>
    struct has_db_type_constant<_T, std::void_t<
        decltype(_T::db_type)
    >> : std::true_type
    {};

    // has_tag
    //   trait: probe whether _Type exposes the given tag as an enabled
    // field.  Requires _Type to be composite-derived; for non-composite
    // types the primary specialization matches and yields false.
    template <typename _Type,
              typename _Tag,
              typename = void>
    struct has_tag : std::false_type
    {};

    template <typename _Type,
              typename _Tag>
    struct has_tag<_Type, _Tag, std::void_t<
        decltype(_Type::template has_field<_Tag>())
    >> : std::bool_constant<_Type::template has_field<_Tag>()>
    {};

NS_END  // internal

// is_database_login
//   trait: structural predicate for database_login instantiations and
// user-defined aggregates that replicate the same surface: a db_type
// enumerator plus the form-level state surface from form_builder.hpp
// (on_submit + submitting + error_message + enabled).
template <typename _Type>
struct is_database_login : std::conjunction<
    internal::has_db_type_constant<_Type>,
    std::bool_constant<is_submittable_form_v<_Type>>
>
{};

template <typename _T>
inline constexpr bool is_database_login_v =
    is_database_login<_T>::value;

// is_network_database_login
//   trait: database_login with host + port slots enabled.
template <typename _Type>
struct is_network_database_login : std::conjunction<
    is_database_login<_Type>,
    internal::has_tag<_Type, dbl_tag::host_tag>,
    internal::has_tag<_Type, dbl_tag::port_tag>
>
{};

template <typename _T>
inline constexpr bool is_network_database_login_v =
    is_network_database_login<_T>::value;

// is_embedded_database_login
//   trait: database_login with a database_name slot but no host slot.
template <typename _Type>
struct is_embedded_database_login : std::conjunction<
    is_database_login<_Type>,
    internal::has_tag<_Type, dbl_tag::database_name_tag>,
    std::negation<internal::has_tag<_Type, dbl_tag::host_tag>>
>
{};

template <typename _T>
inline constexpr bool is_embedded_database_login_v =
    is_embedded_database_login<_T>::value;

// is_ssl_database_login
//   trait: database_login with the ssl slot enabled.
template <typename _Type>
struct is_ssl_database_login : std::conjunction<
    is_database_login<_Type>,
    internal::has_tag<_Type, dbl_tag::ssl_tag>
>
{};

template <typename _T>
inline constexpr bool is_ssl_database_login_v =
    is_ssl_database_login<_T>::value;

// is_uri_database_login
//   trait: database_login with the uri slot enabled.
template <typename _Type>
struct is_uri_database_login : std::conjunction<
    is_database_login<_Type>,
    internal::has_tag<_Type, dbl_tag::uri_tag>
>
{};

template <typename _T>
inline constexpr bool is_uri_database_login_v =
    is_uri_database_login<_T>::value;


}   // namespace database_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DATABASE_LOGIN_
