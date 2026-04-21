/*******************************************************************************
* uxoxo [component]                                  database_login_presets.hpp
*
* Vendor-specific database login aliases:
*   Ready-made aliases over database_login<_Vendor, _Feat> for every
* supported djinterp vendor.  Each preset picks a reasonable default
* feature set for the vendor and exposes it under a short name; callers
* who want a different feature mix can either override _Feat at the use
* site (e.g. `sqlite_database_login<dlf_embedded | dlf_schema>`) or
* instantiate database_login<...> directly.
*
*   The one-line-per-vendor density is the payoff of rebasing
* database_login on the form / composite template machinery: every
* vendor alias is a template alias over the same underlying form
* instantiation, with zero duplicated storage, traits, or free-function
* boilerplate.
*
* path:      /inc/uxoxo/component/database_login_presets.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_DATABASE_LOGIN_PRESETS_
#define  UXOXO_COMPONENT_DATABASE_LOGIN_PRESETS_ 1

// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./database_login.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  PRESET FEATURE MASKS
// ===============================================================================
//   Vendor-appropriate default feature combinations.  Each is a plain
// unsigned constant, composable with the base dlf_* flags via operator|
// for further customization at the use site.

// dlf_classic_rdbms
//   preset: user/pass + host/port/db + charset + SSL.  Fits the
// traditional "network RDBMS" vendor surface (MariaDB, MySQL, DB2).
constexpr unsigned dlf_classic_rdbms =
    dlf_user_pass | dlf_network | dlf_charset | dlf_ssl;

// dlf_schema_rdbms
//   preset: classic RDBMS plus schema/namespace.  Fits vendors that
// surface first-class schemas (PostgreSQL, Oracle, MSSQL).
constexpr unsigned dlf_schema_rdbms =
    dlf_classic_rdbms | dlf_schema;

// dlf_uri_first
//   preset: user/pass + host/port + SSL + URI.  Fits vendors whose
// canonical connection spec is a URI (MongoDB, Neo4j).
constexpr unsigned dlf_uri_first =
    dlf_user_pass | dlf_host | dlf_port | dlf_ssl | dlf_uri;

// dlf_token_auth
//   preset: password-only + URI + SSL.  Fits vendors where the
// "password" slot carries an API key or token and there is no discrete
// username (Redis <6, Firebase, some REST-backed stores).
constexpr unsigned dlf_token_auth =
    dlf_password | dlf_uri | dlf_ssl;




// ===============================================================================
//  VENDOR ALIASES
// ===============================================================================

// -- embedded / file-backed -------------------------------------------

// sqlite_database_login
//   alias: SQLite — just a file path (the "database name" slot); no
// network, no credentials, no SSL.
template <unsigned _Feat = dlf_embedded>
using sqlite_database_login =
    database_login<djinterp::database::database_type::sqlite, _Feat>;


// -- network RDBMS ----------------------------------------------------

// mariadb_database_login
//   alias: MariaDB — classic network RDBMS with charset + SSL.
template <unsigned _Feat = dlf_classic_rdbms>
using mariadb_database_login =
    database_login<djinterp::database::database_type::mariadb, _Feat>;

// mysql_database_login
//   alias: MySQL — classic network RDBMS with charset + SSL.
template <unsigned _Feat = dlf_classic_rdbms>
using mysql_database_login =
    database_login<djinterp::database::database_type::mysql, _Feat>;

// postgresql_database_login
//   alias: PostgreSQL — schema-aware network RDBMS.
template <unsigned _Feat = dlf_schema_rdbms>
using postgresql_database_login =
    database_login<djinterp::database::database_type::postgresql, _Feat>;

// oracle_database_login
//   alias: Oracle — schema-aware network RDBMS.
template <unsigned _Feat = dlf_schema_rdbms>
using oracle_database_login =
    database_login<djinterp::database::database_type::oracle, _Feat>;

// mssql_database_login
//   alias: Microsoft SQL Server — schema-aware network RDBMS.
template <unsigned _Feat = dlf_schema_rdbms>
using mssql_database_login =
    database_login<djinterp::database::database_type::mssql, _Feat>;

// db2_database_login
//   alias: IBM DB2 — classic network RDBMS.
template <unsigned _Feat = dlf_classic_rdbms>
using db2_database_login =
    database_login<djinterp::database::database_type::db2, _Feat>;


// -- document / NoSQL -------------------------------------------------

// mongodb_database_login
//   alias: MongoDB — canonical form is a URI; username/password/host
// surfaced for discrete-entry UIs.
template <unsigned _Feat = dlf_uri_first>
using mongodb_database_login =
    database_login<djinterp::database::database_type::mongodb, _Feat>;

// couchdb_database_login
//   alias: CouchDB — URI-first with an optional discrete database slot.
template <unsigned _Feat = (dlf_uri_first | dlf_database_name)>
using couchdb_database_login =
    database_login<djinterp::database::database_type::couchdb, _Feat>;

// arangodb_database_login
//   alias: ArangoDB — classic network surface plus a database-name
// (tenant) slot.
template <unsigned _Feat = (dlf_user_pass | dlf_network | dlf_ssl)>
using arangodb_database_login =
    database_login<djinterp::database::database_type::arangodb, _Feat>;


// -- key-value / token-auth -------------------------------------------

// redis_database_login
//   alias: Redis — password-only auth (pre-6.0 semantics); URI
// supported for cluster endpoints.
template <unsigned _Feat = (dlf_password | dlf_host | dlf_port | dlf_ssl)>
using redis_database_login =
    database_login<djinterp::database::database_type::redis, _Feat>;

// firebase_database_login
//   alias: Firebase — API key carried in the password slot, endpoint
// in the URI slot.
template <unsigned _Feat = dlf_token_auth>
using firebase_database_login =
    database_login<djinterp::database::database_type::firebase, _Feat>;


// -- graph / wide-column ----------------------------------------------

// neo4j_database_login
//   alias: Neo4j — URI-first graph database.
template <unsigned _Feat = dlf_uri_first>
using neo4j_database_login =
    database_login<djinterp::database::database_type::neo4j, _Feat>;

// cassandra_database_login
//   alias: Cassandra — network + keyspace (database-name slot) + SSL.
template <unsigned _Feat = (dlf_user_pass | dlf_network | dlf_ssl)>
using cassandra_database_login =
    database_login<djinterp::database::database_type::cassandra, _Feat>;


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DATABASE_LOGIN_PRESETS_
