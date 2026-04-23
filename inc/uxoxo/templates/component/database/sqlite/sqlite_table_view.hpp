/*******************************************************************************
* uxoxo [component]                                       sqlite_table_view.hpp
*
*   SQLite-specific adapter that extends database_table_view with knobs
* and indicators unique to the SQLite runtime.  It does NOT replace
* database_table_view: renderers that only understand the generic
* database_table_state still work unchanged.  Renderers that know
* about SQLite can additionally consult sqlite_table_state to display
* the file path, journal mode, attached-database alias, WITHOUT ROWID
* flag, STRICT flag, and invoke maintenance operations (VACUUM,
* ANALYZE, REINDEX) through typed callbacks.
*
*   Unlike server-backed RDBMS vendors, SQLite surfaces several
* table-adjacent concerns (journal mode, the attached-database
* namespace, WAL checkpointing) that callers often want to reflect
* in the UI.  This header maps those concerns into renderer-friendly
* fields and callbacks.
*
*   Structure:
*     1.   SQLite view state (extended state beyond database_table_state)
*     2.   Binding (sqlite_table_view_bind, plus probing traits)
*     3.   Maintenance operations
*     4.   State queries
*     5.   Status text
*     6.   Traits (SFINAE detection for sqlite_table_state)
*
*   REQUIRES: C++17 or later.
*
*   NOTE: the bind function is duck-typed on the SQLite table type so
* that test doubles and alternative SQLite wrappers can be bound
* against it without inheriting from the concrete sqlite_table
* template.
*
*
* path:      /inc/uxoxo/templates/component/table/database/sqlite/
*                sqlite_table_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.23
*******************************************************************************/

#ifndef UXOXO_COMPONENT_SQLITE_TABLE_VIEW_
#define UXOXO_COMPONENT_SQLITE_TABLE_VIEW_ 1

// std
#include <cstddef>
#include <cstdint>
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
//  1.  SQLITE VIEW STATE
// =============================================================================
//   Extended state carried alongside a database_table_state for SQLite-
// backed views.  Composition (not inheritance) so that renderers which
// only understand database_table_state continue to function, reading
// only the .base field when they do not know about SQLite.

// sqlite_journal_mode
//   enum: SQLite journal mode.  The default is DELETE on disk-backed
// databases and MEMORY on in-memory databases.  WAL is the common
// choice for concurrent-reader workloads.
enum class sqlite_journal_mode : std::uint8_t
{
    unknown  = 0,
    delete_  = 1,  // trailing underscore: `delete` is a C++ keyword
    truncate = 2,
    persist  = 3,
    memory   = 4,
    wal      = 5,
    off      = 6
};

// sqlite_table_state
//   struct: extended state for a SQLite-backed table view.  The
// generic database_table_state lives in .base; SQLite-specific
// additions are members of this struct directly.
struct sqlite_table_state
{
    // generic database view state
    database_table_state base;

    // file-level identity
    //   the SQLite "database" is a single file (or ":memory:" /
    // empty).  Surfacing it here lets renderers show the file path
    // as a title affix.
    std::string file_path;
    bool        in_memory = false;

    // attached-database qualifier
    //   empty means the table lives in the main database.  Otherwise
    // this is the ATTACH alias under which the backing table was
    // found.
    std::string attach_alias;

    // DDL modifiers
    bool without_rowid = false;
    bool strict        = false;

    // journal mode (informational)
    sqlite_journal_mode journal_mode = sqlite_journal_mode::unknown;

    // maintenance callbacks
    //   type-erased wrappers over sqlite_table::vacuum_database(),
    // analyze_table(), and reindex_table().  A renderer that wants
    // to expose a "Maintenance" menu can invoke these without any
    // compile-time dependency on the SQLite driver.
    using maintenance_fn = std::function<void()>;

    maintenance_fn on_vacuum;
    maintenance_fn on_analyze;
    maintenance_fn on_reindex;
};


// =============================================================================
//  2.  BINDING - connect a sqlite_table to a table_view
// =============================================================================
//   sqlite_table_view_bind first delegates to database_table_view_bind
// for the generic grid binding, then populates the SQLite-specific
// fields from the table.  The sqlite_table must outlive the view.
//
//   _SqliteTable:  any sqlite_table<_Config> (duck-typed — no
//                  hard compile-time dependency on sqlite_table).
//   _ToString:     (const value_type&) -> std::string; forwarded to
//                  database_table_view_bind.

NS_INTERNAL

    // has_get_file_path_impl
    //   trait: detects _Type::get_file_path().  Probed by
    // sqlite_table_view_bind to optionally populate file_path.
    template<typename _Type,
             typename = void>
    struct has_get_file_path_impl : std::false_type
    {};

    template<typename _Type>
    struct has_get_file_path_impl<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type&>().get_file_path())>>
        : std::true_type
    {};

    // has_query_journal_mode_impl
    //   trait: detects _Type::query_journal_mode().  Probed by
    // sqlite_table_view_bind to optionally detect the journal mode.
    template<typename _Type,
             typename = void>
    struct has_query_journal_mode_impl : std::false_type
    {};

    template<typename _Type>
    struct has_query_journal_mode_impl<
        _Type,
        std::void_t<decltype(
            std::declval<_Type&>().query_journal_mode())>>
        : std::true_type
    {};

    // sqlite_detect_journal_mode
    //   function: maps a SQLite journal-mode string (as returned by
    // `PRAGMA journal_mode`) to the sqlite_journal_mode enum.  Case
    // insensitive; unrecognised values return unknown.
    D_INLINE sqlite_journal_mode
    sqlite_detect_journal_mode(
        const std::string& _mode_str
    ) noexcept
    {
        std::string lc;
        lc.reserve(_mode_str.size());

        for (char c : _mode_str)
        {
            lc += static_cast<char>(
                (c >= 'A' && c <= 'Z') ? (c + 32) : c);
        }

        if (lc == "delete")
        {
            return sqlite_journal_mode::delete_;
        }

        if (lc == "truncate")
        {
            return sqlite_journal_mode::truncate;
        }

        if (lc == "persist")
        {
            return sqlite_journal_mode::persist;
        }

        if (lc == "memory")
        {
            return sqlite_journal_mode::memory;
        }

        if (lc == "wal")
        {
            return sqlite_journal_mode::wal;
        }

        if (lc == "off")
        {
            return sqlite_journal_mode::off;
        }

        return sqlite_journal_mode::unknown;
    }

NS_END   // internal

// has_get_file_path
//   trait: variable-template alias for detecting get_file_path().
template<typename _Type>
inline constexpr bool has_get_file_path =
    internal::has_get_file_path_impl<_Type>::value;

// has_query_journal_mode
//   trait: variable-template alias for detecting query_journal_mode().
template<typename _Type>
inline constexpr bool has_query_journal_mode =
    internal::has_query_journal_mode_impl<_Type>::value;

// sqlite_table_view_bind
//   function: connects a sqlite_table to a table_view and
// sqlite_table_state.  Populates the generic database_table_state
// via database_table_view_bind, then populates SQLite-specific
// fields (attach alias, DDL modifiers) and wires maintenance
// callbacks.
template<typename _SqliteTable,
         typename _ToString>
void
sqlite_table_view_bind(
    table_view&         _table_view,
    sqlite_table_state& _state,
    _SqliteTable&       _sqlite_table,
    _ToString           _to_str
)
{
    // generic binding populates _state.base
    database_table_view_bind(_table_view,
                             _state.base,
                             _sqlite_table,
                             _to_str);

    // SQLite-specific configuration knobs
    _state.attach_alias  = _sqlite_table.get_attach_alias();
    _state.without_rowid = _sqlite_table.is_without_rowid();
    _state.strict        = _sqlite_table.is_strict();

    // file-level identity.  SQLite connections expose a file path
    // via get_file_path() on the connection; retrieve it via the
    // bound connection reference.  In-memory is detected by the
    // sentinel ":memory:" or an empty path.
    if constexpr (has_get_file_path<_SqliteTable>)
    {
        _state.file_path = _sqlite_table.get_file_path();

        _state.in_memory = ( _state.file_path.empty() ||
                             (_state.file_path == ":memory:") );
    }

    // journal mode: only available on connected tables.  Skipped
    // silently on disconnected tables — renderers should treat
    // journal_mode::unknown as "not yet queried".
    if constexpr (has_query_journal_mode<_SqliteTable>)
    {
        if (_state.base.connected)
        {
            _state.journal_mode =
                internal::sqlite_detect_journal_mode(
                    _sqlite_table.query_journal_mode());
        }
    }

    // wire maintenance callbacks
    _state.on_vacuum = [&_sqlite_table]()
    {
        _sqlite_table.vacuum_database();
    };

    _state.on_analyze = [&_sqlite_table]()
    {
        _sqlite_table.analyze_table();
    };

    _state.on_reindex = [&_sqlite_table]()
    {
        _sqlite_table.reindex_table();
    };

    return;
}

// sqlite_table_view_bind
//   function: convenience overload without explicit _ToString.
// Defers cell formatting to the underlying database_table_view_bind
// default, which uses database::value_to_string.
template<typename _SqliteTable>
void
sqlite_table_view_bind(
    table_view&         _table_view,
    sqlite_table_state& _state,
    _SqliteTable&       _sqlite_table
)
{
    using value_type = typename _SqliteTable::value_type;

    sqlite_table_view_bind(_table_view,
                           _state,
                           _sqlite_table,
                           [](const value_type& _v) -> std::string
                           {
                               return value_to_string(_v);
                           });

    return;
}


// =============================================================================
//  3.  MAINTENANCE OPERATIONS
// =============================================================================
//   Thin wrappers over the type-erased callbacks in sqlite_table_state.
// Return true when the callback was invoked, false when it is unset.

// sqlite_table_view_vacuum
//   function: triggers VACUUM on the backing SQLite database.  Note
// that VACUUM is a database-level operation (not table-level) but is
// exposed here because SQLite has no table-level equivalent.
D_INLINE bool
sqlite_table_view_vacuum(
    sqlite_table_state& _state
)
{
    if (_state.on_vacuum)
    {
        _state.on_vacuum();

        return true;
    }

    return false;
}

// sqlite_table_view_analyze
//   function: triggers ANALYZE on the backing table.
D_INLINE bool
sqlite_table_view_analyze(
    sqlite_table_state& _state
)
{
    if (_state.on_analyze)
    {
        _state.on_analyze();

        return true;
    }

    return false;
}

// sqlite_table_view_reindex
//   function: rebuilds all indexes on the backing table.
D_INLINE bool
sqlite_table_view_reindex(
    sqlite_table_state& _state
)
{
    if (_state.on_reindex)
    {
        _state.on_reindex();

        return true;
    }

    return false;
}


// =============================================================================
//  4.  STATE QUERIES
// =============================================================================
//   Forwarding accessors for the generic database_table_state fields
// (so callers can operate directly on a sqlite_table_state without
// reaching into .base), plus SQLite-specific queries.

// sqlite_table_view_is_connected
//   function: forwards to database_table_view_is_connected.
D_INLINE bool
sqlite_table_view_is_connected(
    const sqlite_table_state& _state
) noexcept
{
    return database_table_view_is_connected(_state.base);
}

// sqlite_table_view_is_dirty
//   function: forwards to database_table_view_is_dirty.
D_INLINE bool
sqlite_table_view_is_dirty(
    const sqlite_table_state& _state
) noexcept
{
    return database_table_view_is_dirty(_state.base);
}

// sqlite_table_view_is_stale
//   function: forwards to database_table_view_is_stale.
D_INLINE bool
sqlite_table_view_is_stale(
    const sqlite_table_state& _state
) noexcept
{
    return database_table_view_is_stale(_state.base);
}

// sqlite_table_view_is_editable
//   function: forwards to database_table_view_is_editable.
D_INLINE bool
sqlite_table_view_is_editable(
    const sqlite_table_state& _state
) noexcept
{
    return database_table_view_is_editable(_state.base);
}

// sqlite_table_view_is_in_memory
//   function: returns whether the table's database lives entirely in
// memory (":memory:" or empty path).  SQLite-specific.
D_INLINE bool
sqlite_table_view_is_in_memory(
    const sqlite_table_state& _state
) noexcept
{
    return _state.in_memory;
}

// sqlite_table_view_is_attached
//   function: returns whether the table is accessed through an
// ATTACH alias (as opposed to the main database).
D_INLINE bool
sqlite_table_view_is_attached(
    const sqlite_table_state& _state
) noexcept
{
    return !_state.attach_alias.empty();
}

// sqlite_table_view_uses_wal
//   function: returns whether the backing database is in WAL journal
// mode.  False when journal mode is unknown.
D_INLINE bool
sqlite_table_view_uses_wal(
    const sqlite_table_state& _state
) noexcept
{
    return (_state.journal_mode == sqlite_journal_mode::wal);
}

// sqlite_table_view_journal_mode_name
//   function: returns the journal mode as a displayable string.
D_INLINE const char*
sqlite_table_view_journal_mode_name(
    const sqlite_table_state& _state
) noexcept
{
    switch (_state.journal_mode)
    {
        case sqlite_journal_mode::delete_:
            return "DELETE";
        case sqlite_journal_mode::truncate:
            return "TRUNCATE";
        case sqlite_journal_mode::persist:
            return "PERSIST";
        case sqlite_journal_mode::memory:
            return "MEMORY";
        case sqlite_journal_mode::wal:
            return "WAL";
        case sqlite_journal_mode::off:
            return "OFF";
        case sqlite_journal_mode::unknown:
        default:
            return "";
    }
}


// =============================================================================
//  5.  STATUS TEXT
// =============================================================================

// sqlite_table_view_status_text
//   function: returns a short status string summarising connection,
// sync, and SQLite-specific state.  Extends
// database_table_view_status_text with attach-alias, in-memory,
// journal-mode, WITHOUT ROWID, and STRICT affixes.
D_INLINE std::string
sqlite_table_view_status_text(
    const sqlite_table_state& _state,
    const table_view&         _table_view
)
{
    // start with the generic status string
    std::string result =
        database_table_view_status_text(_state.base, _table_view);

    // in-memory indicator
    if (_state.in_memory)
    {
        result += " [:memory:]";
    }
    else if (!_state.file_path.empty())
    {
        result += " (" + _state.file_path + ")";
    }

    // attach alias indicator
    if (!_state.attach_alias.empty())
    {
        result += " [attached: " + _state.attach_alias + "]";
    }

    // DDL modifier indicators
    if (_state.without_rowid)
    {
        result += " [WITHOUT ROWID]";
    }

    if (_state.strict)
    {
        result += " [STRICT]";
    }

    // journal mode (omitted when unknown or in-memory where MEMORY is
    // implied)
    if ( (_state.journal_mode != sqlite_journal_mode::unknown) &&
         (!_state.in_memory) )
    {
        result += " {journal: ";
        result += sqlite_table_view_journal_mode_name(_state);
        result += "}";
    }

    return result;
}


// =============================================================================
//  6.  TRAITS
// =============================================================================
//   SFINAE detection for sqlite_table_state and for the optional
// sqlite_table members (get_file_path, query_journal_mode) that the
// binding function probes for.

NS_INTERNAL

    // has_attach_alias_member
    //   trait: detects sqlite_table_state's attach_alias field.
    template<typename _Type,
             typename = void>
    struct has_attach_alias_member : std::false_type
    {};

    template<typename _Type>
    struct has_attach_alias_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().attach_alias)>>
        : std::true_type
    {};

    // has_without_rowid_member
    //   trait: detects sqlite_table_state's without_rowid field.
    template<typename _Type,
             typename = void>
    struct has_without_rowid_member : std::false_type
    {};

    template<typename _Type>
    struct has_without_rowid_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().without_rowid)>>
        : std::true_type
    {};

    // has_strict_member
    //   trait: detects sqlite_table_state's strict field.
    template<typename _Type,
             typename = void>
    struct has_strict_member : std::false_type
    {};

    template<typename _Type>
    struct has_strict_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().strict)>>
        : std::true_type
    {};

    // has_journal_mode_member
    //   trait: detects sqlite_table_state's journal_mode field.
    template<typename _Type,
             typename = void>
    struct has_journal_mode_member : std::false_type
    {};

    template<typename _Type>
    struct has_journal_mode_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().journal_mode)>>
        : std::true_type
    {};

    // has_on_vacuum_member
    //   trait: detects sqlite_table_state's on_vacuum callback.
    template<typename _Type,
             typename = void>
    struct has_on_vacuum_member : std::false_type
    {};

    template<typename _Type>
    struct has_on_vacuum_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().on_vacuum)>>
        : std::true_type
    {};

}   // internal

template<typename _Type>
inline constexpr bool has_attach_alias_v =
    internal::has_attach_alias_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_without_rowid_v =
    internal::has_without_rowid_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_strict_v =
    internal::has_strict_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_journal_mode_v =
    internal::has_journal_mode_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_on_vacuum_v =
    internal::has_on_vacuum_member<_Type>::value;

// is_sqlite_table_state
//   trait: true if a type satisfies the sqlite_table_state structural
// interface (attach_alias, without_rowid, strict, journal_mode,
// on_vacuum).  Does NOT require the .base member to be a
// database_table_state, so testable fakes can omit the heavier
// generic portion.
template<typename _Type>
struct is_sqlite_table_state : std::conjunction<
    internal::has_attach_alias_member<_Type>,
    internal::has_without_rowid_member<_Type>,
    internal::has_strict_member<_Type>,
    internal::has_journal_mode_member<_Type>,
    internal::has_on_vacuum_member<_Type>>
{};

template<typename _Type>
inline constexpr bool is_sqlite_table_state_v =
    is_sqlite_table_state<_Type>::value;



NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_SQLITE_TABLE_VIEW_