/*******************************************************************************
* uxoxo [component]                                      mariadb_table_view.hpp
*
*   Framework-agnostic adapter that extends database_table_view with
* MariaDB-specific vendor state and operations.  Exposes MariaDB
* features through a renderer-neutral interface so that a TUI, Qt,
* imgui, or web front-end can drive them without linking against
* MariaDB types directly:
*
*   - system-versioned (temporal) table queries: AS OF, BETWEEN, ALL
*     VERSIONS, and a "clear temporal" operation that returns to the
*     live snapshot
*   - Galera cluster-aware sync-on-commit toggle
*   - compile-time vendor feature queries (RETURNING clause,
*     system-versioned tables, sequences, INET6 / UUID types) that the
*     renderer can use to gate UI affordances
*
*   mariadb_table_state publicly inherits from database_table_state.
* Any function that accepts a database_table_state& accepts a
* mariadb_table_state& unchanged.  This is a zero-overhead extension:
* no accessor indirection, direct member access, and an EBO-eligible
* base.
*
*   Structure:
*     1.   mariadb view state
*     2.   binding  (mariadb_table_view_bind)
*     3.   temporal operations  (AS OF / BETWEEN / ALL / clear)
*     4.   Galera sync
*     5.   state queries
*     6.   status text
*     7.   compile-time feature queries
*     8.   traits
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/templates/component/table/database/
*                mariadb_table_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef UXOXO_COMPONENT_MARIADB_TABLE_VIEW_
#define UXOXO_COMPONENT_MARIADB_TABLE_VIEW_ 1

// std
#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./database_table_view.hpp"


NS_UXOXO
NS_COMPONENT


// =============================================================================
//  1.  MARIADB VIEW STATE
// =============================================================================
//   Extended state carried by a MariaDB-backed table view beyond the
// generic database_table_state.  Inherits publicly so that every
// database_table_view_* function (refresh, commit, status queries,
// guarded editing) accepts a mariadb_table_state& without modification.

// mariadb_table_state
//   struct: vendor-specific state for a MariaDB-backed table view.
struct mariadb_table_state : public database_table_state
{
    // capability (detected at bind time via INFORMATION_SCHEMA)
    bool system_versioned      = false;

    // sync policy
    bool galera_sync_on_commit = false;

    // temporal query state
    //   true whenever the live cache reflects an AS OF / BETWEEN /
    // ALL VERSIONS query rather than the current snapshot.
    bool        temporal_active = false;

    // human-readable description of the active temporal clause for
    // display in status bars, e.g. "AS OF '2024-01-01 00:00:00'".
    std::string temporal_text;

    // vendor-specific callbacks
    //   wired by mariadb_table_view_bind to the underlying
    // mariadb_table's temporal / Galera methods.
    using refresh_as_of_fn          = std::function<bool(const std::string&)>;
    using refresh_between_fn        = std::function<bool(const std::string&,
                                                         const std::string&)>;
    using refresh_all_versions_fn   = std::function<bool()>;
    using clear_temporal_fn         = std::function<bool()>;
    using galera_sync_set_fn        = std::function<void(bool)>;

    refresh_as_of_fn        on_refresh_as_of;
    refresh_between_fn      on_refresh_between;
    refresh_all_versions_fn on_refresh_all_versions;
    clear_temporal_fn       on_clear_temporal;
    galera_sync_set_fn      on_galera_sync_change;
};


// =============================================================================
//  2.  BINDING
// =============================================================================
//   mariadb_table_view_bind delegates the generic wiring to
// database_table_view_bind, then overlays MariaDB-specific state and
// callbacks.  The mariadb_table must outlive the view.

// mariadb_table_view_bind
//   function: connects a mariadb_table to a table_view and
// mariadb_table_state.  Populates the generic fields via the base
// bind, then wires MariaDB-specific state (system versioning,
// Galera) and callbacks (temporal refresh variants, Galera sync
// toggle).
template<typename _MariadbTable,
         typename _ToString>
void
mariadb_table_view_bind(
    table_view&          _table_view,
    mariadb_table_state& _state,
    _MariadbTable&       _mariadb_table,
    _ToString            _to_str
)
{
    // generic wiring first (table_view + base state fields)
    database_table_view_bind(_table_view,
                             _state,
                             _mariadb_table,
                             _to_str);

    // MariaDB-specific state
    _state.system_versioned      = _mariadb_table.is_system_versioned();
    _state.galera_sync_on_commit = _mariadb_table.is_galera_sync_on_commit();
    _state.temporal_active       = false;
    _state.temporal_text.clear();

    // AS OF callback
    _state.on_refresh_as_of =
        [&_mariadb_table, &_table_view, &_state]
        (const std::string& _timestamp) -> bool
    {
        // require active connection and system versioning
        if ( (!_mariadb_table.is_connected()) ||
             (!_mariadb_table.is_system_versioned()) )
        {
            return false;
        }

        _mariadb_table.refresh_as_of(_timestamp);

        _state.temporal_active = true;
        _state.temporal_text   = "AS OF " + _timestamp;

        // re-sync view dimensions
        _table_view.num_rows    = _mariadb_table.rows();
        _table_view.num_columns = _mariadb_table.cols();
        _state.dirty            = _mariadb_table.is_dirty();
        _state.stale            = _mariadb_table.is_stale();

        return true;
    };

    // BETWEEN callback
    _state.on_refresh_between =
        [&_mariadb_table, &_table_view, &_state]
        (const std::string& _from,
         const std::string& _to) -> bool
    {
        if ( (!_mariadb_table.is_connected()) ||
             (!_mariadb_table.is_system_versioned()) )
        {
            return false;
        }

        _mariadb_table.refresh_between(_from,
                                       _to);

        _state.temporal_active = true;
        _state.temporal_text   = "BETWEEN " + _from + " AND " + _to;

        _table_view.num_rows    = _mariadb_table.rows();
        _table_view.num_columns = _mariadb_table.cols();
        _state.dirty            = _mariadb_table.is_dirty();
        _state.stale            = _mariadb_table.is_stale();

        return true;
    };

    // ALL VERSIONS callback
    _state.on_refresh_all_versions =
        [&_mariadb_table, &_table_view, &_state]() -> bool
    {
        if ( (!_mariadb_table.is_connected()) ||
             (!_mariadb_table.is_system_versioned()) )
        {
            return false;
        }

        _mariadb_table.refresh_all_versions();

        _state.temporal_active = true;
        _state.temporal_text   = "ALL VERSIONS";

        _table_view.num_rows    = _mariadb_table.rows();
        _table_view.num_columns = _mariadb_table.cols();
        _state.dirty            = _mariadb_table.is_dirty();
        _state.stale            = _mariadb_table.is_stale();

        return true;
    };

    // clear temporal callback
    //   falls back to the standard refresh path to restore the live
    // snapshot.
    _state.on_clear_temporal =
        [&_mariadb_table, &_table_view, &_state]() -> bool
    {
        if (!_mariadb_table.is_connected())
        {
            return false;
        }

        _state.temporal_active = false;
        _state.temporal_text.clear();

        _mariadb_table.refresh();

        _table_view.num_rows    = _mariadb_table.rows();
        _table_view.num_columns = _mariadb_table.cols();
        _state.dirty            = _mariadb_table.is_dirty();
        _state.stale            = _mariadb_table.is_stale();

        return true;
    };

    // Galera sync toggle callback
    _state.on_galera_sync_change =
        [&_mariadb_table, &_state](bool _enabled)
    {
        _mariadb_table.set_galera_sync_on_commit(_enabled);
        _state.galera_sync_on_commit = _enabled;

        return;
    };

    return;
}

// mariadb_table_view_bind
//   function: convenience overload using database::value_to_string as
// the default cell formatter.
template<typename _MariadbTable>
void
mariadb_table_view_bind(
    table_view&          _table_view,
    mariadb_table_state& _state,
    _MariadbTable&       _mariadb_table
)
{
    using value_type = typename _MariadbTable::value_type;

    mariadb_table_view_bind(_table_view,
                            _state,
                            _mariadb_table,
                            [](const value_type& _v) -> std::string
                            {
                                return value_to_string(_v);
                            });

    return;
}


// =============================================================================
//  3.  TEMPORAL OPERATIONS
// =============================================================================
//   Front-end entry points for system-versioned queries.  Each returns
// false if the underlying table is not connected or not
// system-versioned.

// mariadb_table_view_refresh_as_of
//   function: refreshes the view with data as it existed at the given
// timestamp expression.  The timestamp string is passed verbatim into
// the SQL clause, so callers must quote it appropriately (e.g.
// "'2024-01-01 00:00:00'" or "TRANSACTION 42").
D_INLINE bool
mariadb_table_view_refresh_as_of(
    mariadb_table_state& _state,
    const std::string&   _timestamp
)
{
    if (_state.on_refresh_as_of)
    {
        return _state.on_refresh_as_of(_timestamp);
    }

    return false;
}

// mariadb_table_view_refresh_between
//   function: refreshes the view with rows visible at any point in
// the given temporal range.
D_INLINE bool
mariadb_table_view_refresh_between(
    mariadb_table_state& _state,
    const std::string&   _from,
    const std::string&   _to
)
{
    if (_state.on_refresh_between)
    {
        return _state.on_refresh_between(_from,
                                         _to);
    }

    return false;
}

// mariadb_table_view_refresh_all_versions
//   function: refreshes the view with all historical and current row
// versions.
D_INLINE bool
mariadb_table_view_refresh_all_versions(
    mariadb_table_state& _state
)
{
    if (_state.on_refresh_all_versions)
    {
        return _state.on_refresh_all_versions();
    }

    return false;
}

// mariadb_table_view_clear_temporal
//   function: cancels any active temporal query and refreshes the
// view with the current live snapshot.
D_INLINE bool
mariadb_table_view_clear_temporal(
    mariadb_table_state& _state
)
{
    if (_state.on_clear_temporal)
    {
        return _state.on_clear_temporal();
    }

    return false;
}


// =============================================================================
//  4.  GALERA SYNC
// =============================================================================

// mariadb_table_view_set_galera_sync
//   function: enables or disables Galera cluster sync-on-commit.
// When enabled, the underlying table will issue SET wsrep_sync_wait
// after each commit.
D_INLINE void
mariadb_table_view_set_galera_sync(
    mariadb_table_state& _state,
    bool                 _enabled
)
{
    if (_state.on_galera_sync_change)
    {
        _state.on_galera_sync_change(_enabled);
    }
    else
    {
        // no binding: keep the view state in sync regardless
        _state.galera_sync_on_commit = _enabled;
    }

    return;
}

// mariadb_table_view_is_galera_sync
//   function: returns whether Galera sync-on-commit is currently
// enabled.
D_INLINE bool
mariadb_table_view_is_galera_sync(
    const mariadb_table_state& _state
) noexcept
{
    return _state.galera_sync_on_commit;
}


// =============================================================================
//  5.  STATE QUERIES
// =============================================================================

// mariadb_table_view_is_system_versioned
//   function: returns whether the underlying table uses MariaDB
// system versioning.
D_INLINE bool
mariadb_table_view_is_system_versioned(
    const mariadb_table_state& _state
) noexcept
{
    return _state.system_versioned;
}

// mariadb_table_view_is_temporal_active
//   function: returns whether a temporal query (AS OF / BETWEEN / ALL
// VERSIONS) is currently in effect.
D_INLINE bool
mariadb_table_view_is_temporal_active(
    const mariadb_table_state& _state
) noexcept
{
    return _state.temporal_active;
}

// mariadb_table_view_temporal_text
//   function: returns a human-readable description of the currently
// active temporal clause, or an empty string if none is active.
D_INLINE const std::string&
mariadb_table_view_temporal_text(
    const mariadb_table_state& _state
) noexcept
{
    return _state.temporal_text;
}


// =============================================================================
//  6.  STATUS TEXT
// =============================================================================

// mariadb_table_view_status_text
//   function: extends the base status text with MariaDB-specific
// indicators (temporal clause, Galera sync).  Suitable for display in
// a status bar.
D_INLINE std::string
mariadb_table_view_status_text(
    const mariadb_table_state& _state,
    const table_view&          _table_view
)
{
    std::string result =
        database_table_view_status_text(_state,
                                        _table_view);

    // temporal indicator
    if (_state.temporal_active)
    {
        result += " [" + _state.temporal_text + "]";
    }
    else if (_state.system_versioned)
    {
        result += " [versioned]";
    }

    // Galera indicator
    if (_state.galera_sync_on_commit)
    {
        result += " [galera-sync]";
    }

    return result;
}


// =============================================================================
//  7.  COMPILE-TIME FEATURE QUERIES
// =============================================================================
//   Pass-through wrappers around the static constexpr feature
// predicates on mariadb_table.  Allow renderers to gate UI
// affordances (RETURNING buttons, temporal controls, sequence
// widgets) at compile time without including mariadb_table.hpp.

// mariadb_table_view_has_returning_support
//   function: returns whether the RETURNING clause is supported by
// the target MariaDB version.
template<typename _MariadbTable>
static D_CONSTEXPR bool
mariadb_table_view_has_returning_support() noexcept
{
    return _MariadbTable::has_returning_support();
}

// mariadb_table_view_has_system_versioning_support
//   function: returns whether system-versioned tables are supported.
template<typename _MariadbTable>
static D_CONSTEXPR bool
mariadb_table_view_has_system_versioning_support() noexcept
{
    return _MariadbTable::has_system_versioning_support();
}

// mariadb_table_view_has_sequences_support
//   function: returns whether CREATE SEQUENCE is supported.
template<typename _MariadbTable>
static D_CONSTEXPR bool
mariadb_table_view_has_sequences_support() noexcept
{
    return _MariadbTable::has_sequences_support();
}

// mariadb_table_view_has_inet6_type_support
//   function: returns whether the INET6 data type is supported.
template<typename _MariadbTable>
static D_CONSTEXPR bool
mariadb_table_view_has_inet6_type_support() noexcept
{
    return _MariadbTable::has_inet6_type_support();
}

// mariadb_table_view_has_uuid_type_support
//   function: returns whether the native UUID data type is supported.
template<typename _MariadbTable>
static D_CONSTEXPR bool
mariadb_table_view_has_uuid_type_support() noexcept
{
    return _MariadbTable::has_uuid_type_support();
}


// =============================================================================
//  8.  TRAITS (SFINAE detection for mariadb_table_view usage)
// =============================================================================

namespace mariadb_table_view_traits
{
NS_INTERNAL

    // has_system_versioned_member
    //   trait: detects mariadb_table_state's system_versioned field.
    template<typename _Type,
             typename = void>
    struct has_system_versioned_member : std::false_type
    {};

    template<typename _Type>
    struct has_system_versioned_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().system_versioned)>>
        : std::true_type
    {};

    // has_temporal_active_member
    //   trait: detects mariadb_table_state's temporal_active field.
    template<typename _Type,
             typename = void>
    struct has_temporal_active_member : std::false_type
    {};

    template<typename _Type>
    struct has_temporal_active_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().temporal_active)>>
        : std::true_type
    {};

    // has_galera_sync_member
    //   trait: detects mariadb_table_state's galera_sync_on_commit
    // field.
    template<typename _Type,
             typename = void>
    struct has_galera_sync_member : std::false_type
    {};

    template<typename _Type>
    struct has_galera_sync_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().galera_sync_on_commit)>>
        : std::true_type
    {};

    // has_on_refresh_as_of_member
    //   trait: detects mariadb_table_state's on_refresh_as_of
    // callback.
    template<typename _Type,
             typename = void>
    struct has_on_refresh_as_of_member : std::false_type
    {};

    template<typename _Type>
    struct has_on_refresh_as_of_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().on_refresh_as_of)>>
        : std::true_type
    {};

}   // NS_INTERNAL

template<typename _Type>
D_INLINE constexpr bool has_system_versioned_v =
    internal::has_system_versioned_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_temporal_active_v =
    internal::has_temporal_active_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_galera_sync_v =
    internal::has_galera_sync_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_on_refresh_as_of_v =
    internal::has_on_refresh_as_of_member<_Type>::value;

// is_mariadb_table_state
//   trait: true if a type satisfies the mariadb_table_state
// structural interface (system_versioned, temporal_active,
// galera_sync_on_commit, on_refresh_as_of) in addition to the base
// database_table_state interface.
template<typename _Type>
struct is_mariadb_table_state : std::conjunction<
    database_table_view_traits::is_database_table_state<_Type>,
    internal::has_system_versioned_member<_Type>,
    internal::has_temporal_active_member<_Type>,
    internal::has_galera_sync_member<_Type>,
    internal::has_on_refresh_as_of_member<_Type>>
{};

template<typename _Type>
D_INLINE constexpr bool is_mariadb_table_state_v =
    is_mariadb_table_state<_Type>::value;

}   // namespace mariadb_table_view_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MARIADB_TABLE_VIEW_
