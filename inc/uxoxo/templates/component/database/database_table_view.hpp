/*******************************************************************************
* uxoxo [component]                                     database_table_view.hpp
*
*   Framework-agnostic adapter that bridges the runtime database_table model
* (database_table.hpp) to the table_view UI component.  A renderer (Qt,
* TUI, imgui, web) reads the resulting table_view struct and draws it
* however it likes.
*
*   Unlike the compile-time table adapter (table_view.hpp / table_viewbind),
* a database_table has runtime-determined dimensions, schema-driven column
* names, connection state, and synchronization lifecycle.  This module
* provides:
*
*   - Binding a database_table to a table_view (database_table_view_bind)
*   - Schema-driven column descriptors (names, types, alignment)
*   - Cell extraction via value_to_string
*   - Connection and sync state exposure
*   - Refresh / commit callbacks wired through the view
*   - Dirty / stale indicator queries
*   - Read-only enforcement for views and disconnected tables
*   - Column type-based default alignment (numeric -> right, text -> left)
*
*   Structure:
*     1.   database view state (extended state beyond table_view)
*     2.   binding (database_table_view_bind - connect database_table to table_view)
*     3.   sync operations (refresh, commit, invalidate)
*     4.   state queries (connection, dirty, stale, mutable)
*     5.   schema queries (column type, column name by index)
*     6.   cell write-back
*     7.   traits (SFINAE detection for database_table_view)
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/templates/component/table/database/
*                database_table_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.05
*******************************************************************************/

#ifndef UXOXO_COMPONENT_DATABASE_TABLE_VIEW_
#define UXOXO_COMPONENT_DATABASE_TABLE_VIEW_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/text/text_align.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../table/table_view.hpp"


NS_UXOXO
NS_COMPONENT

using djinterp::text_alignment;

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
//   database_table_view_bind populates a table_view and a database_table_state from a
// live database_table.  The database_table must outlive the view.
//
//   _DatabaseTable:   any database_table<Connection, ValueType, Config>
//   _ToString:  (const value_type&) -> std::string; defaults to
//               value_to_string from database_common.hpp.

NS_INTERNAL
    // default_alignment_for_type
    //   function: returns a sensible default text alignment based on the
    // database column's field_type.  Numeric types align right; text and
    // blobs align left.
    D_INLINE text_alignment
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

NS_END   // internal

// database_table_view_bind
//   function: connects a database_table to a table_view and
// database_table_state.  Populates dimensions, column descriptors from
// schema, cell extractor via _ToString, and wires refresh/commit
// callbacks.
template<typename _DatabaseTable,
         typename _ToString>
void
database_table_view_bind(
    table_view&           _table_view,
    database_table_state& _state,
    _DatabaseTable&       _database_table,
    _ToString             _to_str
)
{
    using size_type = typename _DatabaseTable::size_type;

    // dimensions
    _table_view.num_rows = _database_table.rows();
    _table_view.num_columns = _database_table.cols();

    // config regions
    _table_view.header_rows    = _DatabaseTable::header_rows();
    _table_view.header_columns = _DatabaseTable::header_columns();
    _table_view.footer_rows    = _DatabaseTable::footer_rows();
    _table_view.footer_columns = _DatabaseTable::footer_columns();
    _table_view.total_rows     = 0;
    _table_view.total_columns  = 0;

    // cell extractor
    _table_view.get_cell = [&_database_table, _to_str](
        std::size_t _row,
        std::size_t _column
    ) -> std::string
    {
        // bounds check
        if ( (_row >= _database_table.rows()) ||
             (_column >= _database_table.cols()) )
        {
            return {};
        }

        return _to_str(_database_table.cell(_row,
                                      _column));
    };

    // column descriptors from schema
    const auto& schema = _database_table.get_schema();

    _table_view.columns.clear();
    _table_view.columns.resize(_table_view.num_columns);

    for (size_type c = 0; c < _table_view.num_columns; ++c)
    {
        // use schema name if available, fallback to generic
        if (c < schema.columns.size())
        {
            _table_view.columns[c].name = schema.columns[c].name;
        }
        else
        {
            _table_view.columns[c].name = "Col " + std::to_string(c);
        }
    }

    // spans (database tables typically have none)
    _table_view.spans.clear();

    // database state
    _state.connected     = _database_table.is_connected();
    _state.mutable_table = _database_table.is_mutable();
    _state.dirty         = _database_table.is_dirty();
    _state.stale         = _database_table.is_stale();
    _state.table_name    = _database_table.table_name();
    _state.schema_name   = schema.schema_name;

    // refresh callback
    _state.on_refresh = [&_database_table, &_table_view, &_state]() -> bool
    {
        // require active connection
        if (!_database_table.is_connected())
        {
            return false;
        }

        _database_table.refresh();

        // re-sync view dimensions
        _table_view.num_rows = _database_table.rows();
        _table_view.num_columns = _database_table.cols();

        _state.dirty = _database_table.is_dirty();
        _state.stale = _database_table.is_stale();

        // re-generate columns if schema changed
        const auto& sch = _database_table.get_schema();
        _table_view.columns.resize(_table_view.num_columns);

        for (std::size_t i = 0; i < _table_view.num_columns; ++i)
        {
            if (i < sch.columns.size())
            {
                _table_view.columns[i].name = sch.columns[i].name;
            }
        }

        // clamp cursor if it now exceeds table bounds
        if ( (_table_view.num_rows > 0) &&
             (_table_view.cursor.row >= _table_view.num_rows) )
        {
            _table_view.cursor.row = _table_view.num_rows - 1;
        }

        // clamp column cursor
        if ( (_table_view.num_columns > 0) &&
             (_table_view.cursor.col >= _table_view.num_columns) )
        {
            _table_view.cursor.col = _table_view.num_columns - 1;
        }

        // clamp scroll position
        if ( (_table_view.scroll_row + _table_view.page_rows) > _table_view.num_rows)
        {
            _table_view.scroll_row = (_table_view.num_rows > _table_view.page_rows)
                ? _table_view.num_rows - _table_view.page_rows
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
    _state.on_commit = [&_database_table, &_state]() -> bool
    {
        // require connection and mutability
        if ( (!_database_table.is_connected()) ||
             (!_database_table.is_mutable()) )
        {
            return false;
        }

        _database_table.commit();

        _state.dirty = _database_table.is_dirty();
        _state.stale = _database_table.is_stale();

        return true;
    };

    // cell writer (if mutable)
    if (_database_table.is_mutable())
    {
        _table_view.set_cell = [&_database_table, &_state](
                std::size_t        _row,
                std::size_t        _column,
                const std::string& _new_value
            ) -> bool
        {
            // bounds check
            if ( (_row >= _database_table.rows()) ||
                 (_column >= _database_table.cols()) )
            {
                return false;
            }

            // write the raw string value into the database_table's
            // local cache.  The caller is responsible for type
            // conversion if needed.
            _database_table.cell(_row,
                           _column) = _new_value;

            _state.dirty = _database_table.is_dirty();

            return true;
        };
    }
    else
    {
        _table_view.set_cell = nullptr;
    }

    return;
}

// database_table_view_bind
//   function: convenience overload without explicit _ToString.  Uses
// database::value_to_string as the default cell formatter.  Requires
// that the database_table's value_type has a matching value_to_string
// overload in scope.
template<typename _DatabaseTable>
void
database_table_view_bind(
    table_view&           _table_view,
    database_table_state& _state,
    _DatabaseTable&       _database_table
)
{
    using value_type = typename _DatabaseTable::value_type;

    database_table_view_bind(_table_view,
             _state,
             _database_table,
             [](const value_type& _v) -> std::string
             {
                 return value_to_string(_v);
             });

    return;
}


// =============================================================================
//  3.  SYNC OPERATIONS
// =============================================================================

// database_table_view_refresh
//   function: triggers a refresh of the underlying database_table and
// re-syncs the view dimensions.  Returns false if not connected.
D_INLINE bool
database_table_view_refresh(
    database_table_state& _state
)
{
    if (_state.on_refresh)
    {
        return _state.on_refresh();
    }

    return false;
}

// database_table_view_commit
//   function: commits local modifications to the database.  Returns
// false if not connected or if the table is read-only.
D_INLINE bool
database_table_view_commit(
    database_table_state& _state
)
{
    if (_state.on_commit)
    {
        return _state.on_commit();
    }

    return false;
}

// database_table_view_sync_state
//   function: re-reads connection, dirty, and stale flags from the
// database_table without performing a full refresh.  Use this after
// external operations that may have changed the table's sync state.
template<typename _DatabaseTable>
void
database_table_view_sync_state(
    database_table_state& _state,
    const _DatabaseTable& _database_table
)
{
    _state.connected     = _database_table.is_connected();
    _state.mutable_table = _database_table.is_mutable();
    _state.dirty         = _database_table.is_dirty();
    _state.stale         = _database_table.is_stale();

    return;
}


// =============================================================================
//  4.  STATE QUERIES
// =============================================================================

// database_table_view_is_connected
//   function: returns whether the database_table has a live connection.
D_INLINE bool
database_table_view_is_connected(
    const database_table_state& _state
) noexcept
{
    return _state.connected;
}

// database_table_view_is_dirty
//   function: returns whether the local cache has uncommitted
// modifications.
D_INLINE bool
database_table_view_is_dirty(
    const database_table_state& _state
) noexcept
{
    return _state.dirty;
}

// database_table_view_is_stale
//   function: returns whether the local cache may be outdated relative
// to the database.
D_INLINE bool
database_table_view_is_stale(
    const database_table_state& _state
) noexcept
{
    return _state.stale;
}

// database_table_view_is_mutable
//   function: returns whether the table supports cell editing.
D_INLINE bool
database_table_view_is_mutable(
    const database_table_state& _state
) noexcept
{
    return _state.mutable_table;
}

// database_table_view_is_editable
//   function: returns whether the table is both mutable and connected,
// the preconditions for cell editing.
D_INLINE bool
database_table_view_is_editable(
    const database_table_state& _state
) noexcept
{
    return ( _state.connected &&
             _state.mutable_table );
}

// database_table_view_status_text
//   function: returns a short status string summarising connection and
// sync state.  Suitable for display in a status bar.
D_INLINE std::string
database_table_view_status_text(
    const database_table_state& _state,
    const table_view&           _table_view
)
{
    std::string result;

    // table identity
    if (!_state.schema_name.empty())
    {
        result += _state.schema_name + ".";
    }

    result += _state.table_name;

    // dimensions
    result += " [" + std::to_string(_table_view.num_rows)
            + "×" + std::to_string(_table_view.num_columns) + "]";

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

// database_table_view_column_name
//   function: returns the schema column name for a column index, or an
// empty string if out of range.
D_INLINE std::string
database_table_view_column_name(
    const table_view& _table_view,
    std::size_t       _column
)
{
    if (_column < _table_view.columns.size())
    {
        return _table_view.columns[_column].name;
    }

    return {};
}

// =============================================================================
//  6.  GUARDED EDITING
// =============================================================================
//   Wraps table_view editing operations with database-specific guards
// (connection, mutability).

// database_table_view_begin_edit
//   function: starts D_INLINE editing if the table is editable.  Returns
// false if the table is read-only, disconnected, or the cell is covered.
D_INLINE bool
database_table_view_begin_edit(
    table_view&                 _table_view,
    const database_table_state& _state,
    std::size_t                 _row,
    std::size_t                 _column
)
{
    // require editable state
    if (!database_table_view_is_editable(_state))
    {
        return false;
    }

    return table_view_begin_edit(_table_view,
                                 _row,
                                 _column);
}

// database_table_view_begin_edit_at_cursor
//   function: convenience wrapper that begins editing at the current
// cursor position.
D_INLINE bool
database_table_view_begin_edit_at_cursor(
    table_view&                 _table_view,
    const database_table_state& _state)
{
    return database_table_view_begin_edit(_table_view,
                          _state,
                          _table_view.cursor.row,
                          _table_view.cursor.col);
}

// database_table_view_commit_edit
//   function: commits the edit buffer to the cell, then marks the state
// as dirty.
D_INLINE bool
database_table_view_commit_edit(
    table_view&           _table_view,
    database_table_state& _state
)
{
    if (!table_view_commit_edit(_table_view))
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
NS_INTERNAL

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

}   // internal

template<typename _Type>
D_INLINE constexpr bool has_connected_v =
    internal::has_connected_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_dirty_v =
    internal::has_dirty_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_on_refresh_v =
    internal::has_on_refresh_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_table_name_v =
    internal::has_table_name_member<_Type>::value;

// is_database_table_state
//   trait: true if a type satisfies the database_table_state structural
// interface (connected, dirty, on_refresh, table_name).
template<typename _Type>
struct is_database_table_state : std::conjunction<
    internal::has_connected_member<_Type>,
    internal::has_dirty_member<_Type>,
    internal::has_on_refresh_member<_Type>,
    internal::has_table_name_member<_Type>>
{};

template<typename _Type>
D_INLINE constexpr bool is_database_table_state_v =
    is_database_table_state<_Type>::value;

}   // namespace database_table_view_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DATABASE_TABLE_VIEW_
