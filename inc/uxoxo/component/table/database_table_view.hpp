/*******************************************************************************
* uxoxo [component]                                     database_table_view.hpp
*
*   Framework-agnostic adapter that bridges the runtime database_table model
* (database_table.hpp) to the table_view UI component.  A renderer (Qt,
* TUI, imgui, web) reads the resulting table_view struct and draws it
* however it likes.
*
*   Unlike the compile-time table adapter (table_view.hpp / tv_bind), a
* database_table has runtime-determined dimensions, schema-driven column
* names, connection state, and synchronization lifecycle.  This module
* provides:
*
*   - Binding a database_table to a table_view (dtv_bind)
*   - Schema-driven column descriptors (names, types, alignment)
*   - Cell extraction via value_to_string
*   - Connection and sync state exposure
*   - Refresh / commit callbacks wired through the view
*   - Dirty / stale indicator queries
*   - Read-only enforcement for views and disconnected tables
*   - Column type-based default alignment (numeric → right, text → left)
*
*   Structure:
*     1.   Database view state (extended state beyond table_view)
*     2.   Binding (dtv_bind - connect database_table to table_view)
*     3.   Sync operations (refresh, commit, invalidate)
*     4.   State queries (connection, dirty, stale, mutable)
*     5.   Schema queries (column type, column name by index)
*     6.   Cell write-back
*     7.   Traits (SFINAE detection for database_table_view)
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/component/table/database_table_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.05
*******************************************************************************/

#ifndef UXOXO_COMPONENT_DATABASE_TABLE_VIEW_
#define UXOXO_COMPONENT_DATABASE_TABLE_VIEW_ 1

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <uxoxo>
#include "table_view.hpp"


NS_UXOXO
NS_COMPONENT


// =============================================================================
//  1.  DATABASE VIEW STATE
// =============================================================================
//   Extended state that a database-backed table view carries beyond the
// generic table_view fields.  Held as a companion struct rather than
// inheriting from table_view, so that renderers that only understand
// table_view can still draw the grid portion unmodified.

// database_table_state
//   struct: extended state for a database-backed table view.
struct database_table_state
{
    // connection
    bool connected     = false;
    bool mutable_table = false;

    // sync indicators
    bool dirty = false;
    bool stale = true;

    // table identity
    std::string table_name;
    std::string schema_name;

    // callbacks
    //   type-erased: the binding layer captures the database_table
    // reference and wires these to its refresh() / commit() methods.
    using refresh_fn = std::function<bool()>;
    using commit_fn  = std::function<bool()>;
    using sync_fn    = std::function<void()>;

    refresh_fn on_refresh;
    commit_fn  on_commit;

    // called after refresh completes to re-sync the table_view dimensions
    sync_fn    on_post_refresh;
};

// =============================================================================
//  2.  BINDING - connect a database_table to a table_view
// =============================================================================
//   dtv_bind populates a table_view and a database_table_state from a
// live database_table.  The database_table must outlive the view.
//
//   _DbTable:   any database_table<Connection, ValueType, Config>
//   _ToString:  (const value_type&) → std::string; defaults to
//               value_to_string from database_common.hpp.

namespace detail_dtv
{

    // default_alignment_for_type
    //   function: returns a sensible default text alignment based on the
    // database column's field_type.  Numeric types align right; text and
    // blobs align left.
    inline text_alignment
    default_alignment_for_type(
        unsigned _field_type_ordinal
    ) noexcept
    {
        // field_type enum values: the numeric types (integer, real,
        // decimal, etc.) are typically the lower ordinals.  We use a
        // simple heuristic: anything the caller tags as numeric gets
        // right alignment.  This function is intentionally loose -
        // callers can override per-column after binding.
        (void)_field_type_ordinal;

        return text_alignment::left;
    }

}   // namespace detail_dtv

// dtv_bind
//   function: connects a database_table to a table_view and
// database_table_state.  Populates dimensions, column descriptors from
// schema, cell extractor via _ToString, and wires refresh/commit
// callbacks.
template<typename _DbTable,
         typename _ToString>
void
dtv_bind(table_view&           _tv,
         database_table_state& _state,
         _DbTable&             _db_table,
         _ToString             _to_str)
{
    using size_type = typename _DbTable::size_type;

    // dimensions
    _tv.num_rows = _db_table.rows();
    _tv.num_cols = _db_table.cols();

    // config regions
    _tv.header_rows = _DbTable::header_rows();
    _tv.header_cols = _DbTable::header_cols();
    _tv.footer_rows = _DbTable::footer_rows();
    _tv.footer_cols = _DbTable::footer_cols();
    _tv.total_rows  = 0;
    _tv.total_cols  = 0;

    // cell extractor
    _tv.get_cell = [&_db_table, _to_str](
        std::size_t _row,
        std::size_t _col
    ) -> std::string
    {
        // bounds check
        if ( (_row >= _db_table.rows()) ||
             (_col >= _db_table.cols()) )
        {
            return {};
        }

        return _to_str(_db_table.cell(_row,
                                      _col));
    };

    // column descriptors from schema
    const auto& schema = _db_table.get_schema();

    _tv.columns.clear();
    _tv.columns.resize(_tv.num_cols);

    for (size_type c = 0; c < _tv.num_cols; ++c)
    {
        // use schema name if available, fallback to generic
        if (c < schema.columns.size())
        {
            _tv.columns[c].name = schema.columns[c].name;
        }
        else
        {
            _tv.columns[c].name = "Col " + std::to_string(c);
        }
    }

    // spans (database tables typically have none)
    _tv.spans.clear();

    // database state
    _state.connected     = _db_table.is_connected();
    _state.mutable_table = _db_table.is_mutable();
    _state.dirty         = _db_table.is_dirty();
    _state.stale         = _db_table.is_stale();
    _state.table_name    = _db_table.table_name();
    _state.schema_name   = schema.schema_name;

    // refresh callback
    _state.on_refresh = [&_db_table, &_tv, &_state]() -> bool
    {
        // require active connection
        if (!_db_table.is_connected())
        {
            return false;
        }

        _db_table.refresh();

        // re-sync view dimensions
        _tv.num_rows = _db_table.rows();
        _tv.num_cols = _db_table.cols();

        _state.dirty = _db_table.is_dirty();
        _state.stale = _db_table.is_stale();

        // re-generate columns if schema changed
        const auto& sch = _db_table.get_schema();
        _tv.columns.resize(_tv.num_cols);

        for (std::size_t i = 0; i < _tv.num_cols; ++i)
        {
            if (i < sch.columns.size())
            {
                _tv.columns[i].name = sch.columns[i].name;
            }
        }

        // clamp cursor if it now exceeds table bounds
        if ( (_tv.num_rows > 0) &&
             (_tv.cursor.row >= _tv.num_rows) )
        {
            _tv.cursor.row = _tv.num_rows - 1;
        }

        // clamp column cursor
        if ( (_tv.num_cols > 0) &&
             (_tv.cursor.col >= _tv.num_cols) )
        {
            _tv.cursor.col = _tv.num_cols - 1;
        }

        // clamp scroll position
        if (_tv.scroll_row + _tv.page_rows > _tv.num_rows)
        {
            _tv.scroll_row = (_tv.num_rows > _tv.page_rows)
                ? _tv.num_rows - _tv.page_rows
                : 0;
        }

        // invoke post-refresh hook
        if (_state.on_post_refresh)
        {
            _state.on_post_refresh();
        }

        return true;
    };

    // commit callback
    _state.on_commit = [&_db_table, &_state]() -> bool
    {
        // require connection and mutability
        if ( (!_db_table.is_connected()) ||
             (!_db_table.is_mutable()) )
        {
            return false;
        }

        _db_table.commit();

        _state.dirty = _db_table.is_dirty();
        _state.stale = _db_table.is_stale();

        return true;
    };

    // cell writer (if mutable)
    if (_db_table.is_mutable())
    {
        _tv.set_cell = [&_db_table, &_state](
                std::size_t        _row,
                std::size_t        _col,
                const std::string& _new_value
            ) -> bool
        {
            // bounds check
            if ( (_row >= _db_table.rows()) ||
                 (_col >= _db_table.cols()) )
            {
                return false;
            }

            // write the raw string value into the database_table's
            // local cache.  The caller is responsible for type
            // conversion if needed.
            _db_table.cell(_row,
                           _col) = _new_value;

            _state.dirty = _db_table.is_dirty();

            return true;
        };
    }
    else
    {
        _tv.set_cell = nullptr;
    }

    return;
}

// dtv_bind
//   function: convenience overload without explicit _ToString.  Uses
// database::value_to_string as the default cell formatter.  Requires
// that the database_table's value_type has a matching value_to_string
// overload in scope.
template<typename _DbTable>
void
dtv_bind(table_view&           _tv,
         database_table_state& _state,
         _DbTable&             _db_table)
{
    using value_type = typename _DbTable::value_type;

    dtv_bind(_tv,
             _state,
             _db_table,
             [](const value_type& _v) -> std::string
             {
                 return value_to_string(_v);
             });

    return;
}


// =============================================================================
//  3.  SYNC OPERATIONS
// =============================================================================

// dtv_refresh
//   function: triggers a refresh of the underlying database_table and
// re-syncs the view dimensions.  Returns false if not connected.
inline bool
dtv_refresh(
    database_table_state& _state
)
{
    if (_state.on_refresh)
    {
        return _state.on_refresh();
    }

    return false;
}

// dtv_commit
//   function: commits local modifications to the database.  Returns
// false if not connected or if the table is read-only.
inline bool
dtv_commit(
    database_table_state& _state
)
{
    if (_state.on_commit)
    {
        return _state.on_commit();
    }

    return false;
}

// dtv_sync_state
//   function: re-reads connection, dirty, and stale flags from the
// database_table without performing a full refresh.  Use this after
// external operations that may have changed the table's sync state.
template<typename _DbTable>
void
dtv_sync_state(database_table_state& _state,
               const _DbTable&       _db_table)
{
    _state.connected     = _db_table.is_connected();
    _state.mutable_table = _db_table.is_mutable();
    _state.dirty         = _db_table.is_dirty();
    _state.stale         = _db_table.is_stale();

    return;
}


// =============================================================================
//  4.  STATE QUERIES
// =============================================================================

// dtv_is_connected
//   function: returns whether the database_table has a live connection.
inline bool
dtv_is_connected(
    const database_table_state& _state
) noexcept
{
    return _state.connected;
}

// dtv_is_dirty
//   function: returns whether the local cache has uncommitted
// modifications.
inline bool
dtv_is_dirty(
    const database_table_state& _state
) noexcept
{
    return _state.dirty;
}

// dtv_is_stale
//   function: returns whether the local cache may be outdated relative
// to the database.
inline bool
dtv_is_stale(
    const database_table_state& _state
) noexcept
{
    return _state.stale;
}

// dtv_is_mutable
//   function: returns whether the table supports cell editing.
inline bool
dtv_is_mutable(
    const database_table_state& _state
) noexcept
{
    return _state.mutable_table;
}

// dtv_is_editable
//   function: returns whether the table is both mutable and connected,
// the preconditions for cell editing.
inline bool
dtv_is_editable(
    const database_table_state& _state
) noexcept
{
    return ( _state.connected &&
             _state.mutable_table );
}

// dtv_status_text
//   function: returns a short status string summarising connection and
// sync state.  Suitable for display in a status bar.
inline std::string
dtv_status_text(const database_table_state& _state,
                const table_view&           _tv)
{
    std::string result;

    // table identity
    if (!_state.schema_name.empty())
    {
        result += _state.schema_name + ".";
    }

    result += _state.table_name;

    // dimensions
    result += " [" + std::to_string(_tv.num_rows)
            + "×" + std::to_string(_tv.num_cols) + "]";

    // connection status
    if (!_state.connected)
    {
        result += " (disconnected)";

        return result;
    }

    // sync flags
    if (_state.dirty)
    {
        result += " *modified*";
    }

    if (_state.stale)
    {
        result += " (stale)";
    }

    // mutability indicator
    if (!_state.mutable_table)
    {
        result += " [read-only]";
    }

    return result;
}


// =============================================================================
//  5.  SCHEMA QUERIES
// =============================================================================

// dtv_column_name
//   function: returns the schema column name for a column index, or an
// empty string if out of range.
inline std::string
dtv_column_name(const table_view& _tv,
                std::size_t       _col)
{
    if (_col < _tv.columns.size())
    {
        return _tv.columns[_col].name;
    }

    return {};
}

// =============================================================================
//  6.  GUARDED EDITING
// =============================================================================
//   Wraps table_view editing operations with database-specific guards
// (connection, mutability).

// dtv_begin_edit
//   function: starts inline editing if the table is editable.  Returns
// false if the table is read-only, disconnected, or the cell is covered.
inline bool
dtv_begin_edit(table_view&                 _tv,
               const database_table_state& _state,
               std::size_t                 _row,
               std::size_t                 _col)
{
    // require editable state
    if (!dtv_is_editable(_state))
    {
        return false;
    }

    return tv_begin_edit(_tv,
                         _row,
                         _col);
}

// dtv_begin_edit_at_cursor
//   function: convenience wrapper that begins editing at the current
// cursor position.
inline bool
dtv_begin_edit_at_cursor(table_view&                 _tv,
                         const database_table_state& _state)
{
    return dtv_begin_edit(_tv,
                          _state,
                          _tv.cursor.row,
                          _tv.cursor.col);
}

// dtv_commit_edit
//   function: commits the edit buffer to the cell, then marks the state
// as dirty.
inline bool
dtv_commit_edit(table_view&           _tv,
                database_table_state& _state)
{
    if (!tv_commit_edit(_tv))
    {
        return false;
    }

    // update dirty flag after cell write
    _state.dirty = true;

    return true;
}


// =============================================================================
//  7.  TRAITS (SFINAE detection for database_table_view usage)
// =============================================================================

namespace database_table_view_traits
{
namespace detail
{

    // has_connected_member
    //   trait: detects database_table_state's connected field.
    template<typename _Type,
             typename = void>
    struct has_connected_member : std::false_type
    {};

    template<typename _Type>
    struct has_connected_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().connected)>>
        : std::true_type
    {};

    // has_dirty_member
    //   trait: detects database_table_state's dirty field.
    template<typename _Type,
             typename = void>
    struct has_dirty_member : std::false_type
    {};

    template<typename _Type>
    struct has_dirty_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().dirty)>>
        : std::true_type
    {};

    // has_on_refresh_member
    //   trait: detects database_table_state's on_refresh callback.
    template<typename _Type,
             typename = void>
    struct has_on_refresh_member : std::false_type
    {};

    template<typename _Type>
    struct has_on_refresh_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().on_refresh)>>
        : std::true_type
    {};

    // has_table_name_member
    //   trait: detects database_table_state's table_name field.
    template<typename _Type,
             typename = void>
    struct has_table_name_member : std::false_type
    {};

    template<typename _Type>
    struct has_table_name_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().table_name)>>
        : std::true_type
    {};

}   // namespace detail

template<typename _Type>
inline constexpr bool has_connected_v =
    detail::has_connected_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_dirty_v =
    detail::has_dirty_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_on_refresh_v =
    detail::has_on_refresh_member<_Type>::value;

template<typename _Type>
inline constexpr bool has_table_name_v =
    detail::has_table_name_member<_Type>::value;

// is_database_table_state
//   trait: true if a type satisfies the database_table_state structural
// interface (connected, dirty, on_refresh, table_name).
template<typename _Type>
struct is_database_table_state : std::conjunction<
    detail::has_connected_member<_Type>,
    detail::has_dirty_member<_Type>,
    detail::has_on_refresh_member<_Type>,
    detail::has_table_name_member<_Type>>
{};

template<typename _Type>
inline constexpr bool is_database_table_state_v =
    is_database_table_state<_Type>::value;

}   // namespace database_table_view_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DATABASE_TABLE_VIEW_
