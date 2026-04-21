/*******************************************************************************
* uxoxo [component]                                        mysql_table_view.hpp
*
*   Framework-agnostic adapter that extends database_table_view with
* Oracle-MySQL-specific vendor state and operations.  Exposes MySQL
* features through a renderer-neutral interface so that a TUI, Qt,
* imgui, or web front-end can drive them without linking against
* MySQL types directly:
*
*   - optimizer hint injection (SELECT /*+ ... *\/) with get / set /
*     clear
*   - native JSON binary column awareness (MYSQL_TYPE_JSON / 0xF5),
*     surfaced as a per-column flag vector the renderer can consult
*     to style JSON cells distinctly from plain text
*   - table maintenance operations: ANALYZE, OPTIMIZE, CHECK
*   - compile-time json_is_native query (always true for MySQL, false
*     for MariaDB's LONGTEXT alias — useful when rendering shared
*     MySQL-family UI)
*
*   mysql_table_state publicly inherits from database_table_state.
* Any function that accepts a database_table_state& accepts a
* mysql_table_state& unchanged.  This is a zero-overhead extension:
* no accessor indirection, direct member access, and an EBO-eligible
* base.
*
*   Structure:
*     1.   mysql view state
*     2.   binding  (mysql_table_view_bind)
*     3.   optimizer hint operations
*     4.   maintenance operations  (ANALYZE / OPTIMIZE / CHECK)
*     5.   JSON column queries
*     6.   status text
*     7.   compile-time feature queries
*     8.   traits
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/templates/component/table/database/
*                mysql_table_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef UXOXO_COMPONENT_MYSQL_TABLE_VIEW_
#define UXOXO_COMPONENT_MYSQL_TABLE_VIEW_ 1

// std
#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./database_table_view.hpp"


NS_UXOXO
NS_COMPONENT


// =============================================================================
//  1.  MYSQL VIEW STATE
// =============================================================================
//   Extended state carried by an Oracle-MySQL-backed table view
// beyond the generic database_table_state.  Inherits publicly so
// that every database_table_view_* function (refresh, commit,
// status queries, guarded editing) accepts a mysql_table_state&
// without modification.

// mysql_table_state
//   struct: vendor-specific state for an Oracle-MySQL-backed table
// view.
struct mysql_table_state : public database_table_state
{
    // current optimizer hint (without /*+  */ delimiters)
    std::string optimizer_hint;

    // per-column JSON flag, parallel to table_view.columns.
    //   Entry c is true iff column c is a native JSON binary column.
    // Renderers can consult this to present JSON cells distinctly
    // (pretty-printed, syntax-coloured, collapsible, etc.).
    std::vector<bool> json_columns;

    // last CHECK TABLE result, populated by mysql_table_view_check.
    std::string last_check_result;

    // vendor-specific callbacks
    //   wired by mysql_table_view_bind to the underlying
    // mysql_table's maintenance and optimizer-hint methods.
    using set_hint_fn      = std::function<void(const std::string&)>;
    using clear_hint_fn    = std::function<void()>;
    using analyze_fn       = std::function<bool()>;
    using optimize_fn      = std::function<bool()>;
    using check_fn         = std::function<std::string()>;

    set_hint_fn   on_set_optimizer_hint;
    clear_hint_fn on_clear_optimizer_hint;
    analyze_fn    on_analyze;
    optimize_fn   on_optimize;
    check_fn      on_check;
};


// =============================================================================
//  2.  BINDING
// =============================================================================
//   mysql_table_view_bind delegates the generic wiring to
// database_table_view_bind, then overlays MySQL-specific state and
// callbacks.  The mysql_table must outlive the view.

// mysql_table_view_bind
//   function: connects a mysql_table to a table_view and
// mysql_table_state.  Populates the generic fields via the base
// bind, then wires MySQL-specific state (optimizer hint, JSON
// column flags) and callbacks (optimizer hint updates, ANALYZE /
// OPTIMIZE / CHECK).
template<typename _MysqlTable,
         typename _ToString>
void
mysql_table_view_bind(
    table_view&        _table_view,
    mysql_table_state& _state,
    _MysqlTable&       _mysql_table,
    _ToString          _to_str
)
{
    using size_type = typename _MysqlTable::size_type;

    // generic wiring first (table_view + base state fields)
    database_table_view_bind(_table_view,
                             _state,
                             _mysql_table,
                             _to_str);

    // MySQL-specific state
    _state.optimizer_hint = _mysql_table.get_optimizer_hint();

    // mirror JSON column flags
    _state.json_columns.clear();
    _state.json_columns.resize(_table_view.num_columns,
                               false);

    for (size_type c = 0; c < _table_view.num_columns; ++c)
    {
        _state.json_columns[c] = _mysql_table.is_json_column(c);
    }

    // optimizer hint setter
    _state.on_set_optimizer_hint =
        [&_mysql_table, &_state](const std::string& _hint)
    {
        _mysql_table.set_optimizer_hint(_hint);
        _state.optimizer_hint = _hint;

        return;
    };

    // optimizer hint clear
    _state.on_clear_optimizer_hint =
        [&_mysql_table, &_state]()
    {
        _mysql_table.clear_optimizer_hint();
        _state.optimizer_hint.clear();

        return;
    };

    // ANALYZE TABLE
    _state.on_analyze =
        [&_mysql_table]() -> bool
    {
        if (!_mysql_table.is_connected())
        {
            return false;
        }

        _mysql_table.analyze_table();

        return true;
    };

    // OPTIMIZE TABLE
    _state.on_optimize =
        [&_mysql_table, &_state]() -> bool
    {
        if (!_mysql_table.is_connected())
        {
            return false;
        }

        _mysql_table.optimize_table();

        // optimize can shift physical layout; mark stale so callers
        // know to refresh.
        _state.stale = true;

        return true;
    };

    // CHECK TABLE
    _state.on_check =
        [&_mysql_table, &_state]() -> std::string
    {
        if (!_mysql_table.is_connected())
        {
            _state.last_check_result.clear();

            return {};
        }

        _state.last_check_result = _mysql_table.check_table();

        return _state.last_check_result;
    };

    return;
}

// mysql_table_view_bind
//   function: convenience overload using database::value_to_string
// as the default cell formatter.
template<typename _MysqlTable>
void
mysql_table_view_bind(
    table_view&        _table_view,
    mysql_table_state& _state,
    _MysqlTable&       _mysql_table
)
{
    using value_type = typename _MysqlTable::value_type;

    mysql_table_view_bind(_table_view,
                          _state,
                          _mysql_table,
                          [](const value_type& _v) -> std::string
                          {
                              return value_to_string(_v);
                          });

    return;
}


// =============================================================================
//  3.  OPTIMIZER HINT OPERATIONS
// =============================================================================

// mysql_table_view_set_optimizer_hint
//   function: sets the optimizer hint string injected into SELECT
// queries.  The hint must not include the /*+  */ delimiters —
// they are supplied by the underlying query builder.
D_INLINE void
mysql_table_view_set_optimizer_hint(
    mysql_table_state& _state,
    const std::string& _hint
)
{
    if (_state.on_set_optimizer_hint)
    {
        _state.on_set_optimizer_hint(_hint);
    }
    else
    {
        // no binding: keep the view state in sync regardless
        _state.optimizer_hint = _hint;
    }

    return;
}

// mysql_table_view_clear_optimizer_hint
//   function: removes any active optimizer hint.
D_INLINE void
mysql_table_view_clear_optimizer_hint(
    mysql_table_state& _state
)
{
    if (_state.on_clear_optimizer_hint)
    {
        _state.on_clear_optimizer_hint();
    }
    else
    {
        _state.optimizer_hint.clear();
    }

    return;
}

// mysql_table_view_get_optimizer_hint
//   function: returns the currently active optimizer hint, or an
// empty string if none is set.
D_INLINE const std::string&
mysql_table_view_get_optimizer_hint(
    const mysql_table_state& _state
) noexcept
{
    return _state.optimizer_hint;
}

// mysql_table_view_has_optimizer_hint
//   function: returns whether an optimizer hint is currently active.
D_INLINE bool
mysql_table_view_has_optimizer_hint(
    const mysql_table_state& _state
) noexcept
{
    return !_state.optimizer_hint.empty();
}


// =============================================================================
//  4.  MAINTENANCE OPERATIONS
// =============================================================================

// mysql_table_view_analyze
//   function: runs ANALYZE TABLE on the underlying table.  Returns
// false if the table is not connected.
D_INLINE bool
mysql_table_view_analyze(
    mysql_table_state& _state
)
{
    if (_state.on_analyze)
    {
        return _state.on_analyze();
    }

    return false;
}

// mysql_table_view_optimize
//   function: runs OPTIMIZE TABLE on the underlying table.  Marks
// the view stale on success so that the renderer can prompt for
// refresh.  Returns false if the table is not connected.
D_INLINE bool
mysql_table_view_optimize(
    mysql_table_state& _state
)
{
    if (_state.on_optimize)
    {
        return _state.on_optimize();
    }

    return false;
}

// mysql_table_view_check
//   function: runs CHECK TABLE and returns the concatenated result
// message(s).  Also stored in _state.last_check_result.  Returns an
// empty string if the table is not connected.
D_INLINE std::string
mysql_table_view_check(
    mysql_table_state& _state
)
{
    if (_state.on_check)
    {
        return _state.on_check();
    }

    return {};
}

// mysql_table_view_last_check_result
//   function: returns the result of the most recent CHECK TABLE
// invocation, or an empty string if none has been run.
D_INLINE const std::string&
mysql_table_view_last_check_result(
    const mysql_table_state& _state
) noexcept
{
    return _state.last_check_result;
}


// =============================================================================
//  5.  JSON COLUMN QUERIES
// =============================================================================

// mysql_table_view_is_json_column
//   function: returns whether the column at the given index is a
// native JSON binary column.  Out-of-range indices return false.
D_INLINE bool
mysql_table_view_is_json_column(
    const mysql_table_state& _state,
    std::size_t              _column
)
{
    if (_column >= _state.json_columns.size())
    {
        return false;
    }

    return _state.json_columns[_column];
}

// mysql_table_view_json_column_count
//   function: returns the number of native JSON binary columns in
// the view.
D_INLINE std::size_t
mysql_table_view_json_column_count(
    const mysql_table_state& _state
) noexcept
{
    std::size_t count = 0;

    for (bool flag : _state.json_columns)
    {
        if (flag)
        {
            ++count;
        }
    }

    return count;
}

// mysql_table_view_has_json_columns
//   function: returns whether the view contains any native JSON
// binary columns.
D_INLINE bool
mysql_table_view_has_json_columns(
    const mysql_table_state& _state
) noexcept
{
    for (bool flag : _state.json_columns)
    {
        if (flag)
        {
            return true;
        }
    }

    return false;
}


// =============================================================================
//  6.  STATUS TEXT
// =============================================================================

// mysql_table_view_status_text
//   function: extends the base status text with MySQL-specific
// indicators (active optimizer hint, JSON column count).  Suitable
// for display in a status bar.
D_INLINE std::string
mysql_table_view_status_text(
    const mysql_table_state& _state,
    const table_view&        _table_view
)
{
    std::string result =
        database_table_view_status_text(_state,
                                        _table_view);

    // optimizer hint indicator
    if (!_state.optimizer_hint.empty())
    {
        result += " [hint: " + _state.optimizer_hint + "]";
    }

    // JSON column indicator
    const auto json_count =
        mysql_table_view_json_column_count(_state);

    if (json_count > 0)
    {
        result += " [json:" + std::to_string(json_count) + "]";
    }

    return result;
}


// =============================================================================
//  7.  COMPILE-TIME FEATURE QUERIES
// =============================================================================
//   Pass-through wrappers around the static constexpr feature
// predicates on mysql_table.  Allow renderers to gate UI
// affordances at compile time without including mysql_table.hpp.

// mysql_table_view_json_is_native
//   function: returns whether JSON columns are stored as native
// binary JSON (true for Oracle MySQL, false for MariaDB's LONGTEXT
// alias).  Useful for shared MySQL-family UI code that needs to
// choose between binary-JSON and text-JSON rendering paths.
template<typename _MysqlTable>
static D_CONSTEXPR bool
mysql_table_view_json_is_native() noexcept
{
    return _MysqlTable::json_is_native();
}


// =============================================================================
//  8.  TRAITS (SFINAE detection for mysql_table_view usage)
// =============================================================================

namespace mysql_table_view_traits
{
NS_INTERNAL

    // has_optimizer_hint_member
    //   trait: detects mysql_table_state's optimizer_hint field.
    template<typename _Type,
             typename = void>
    struct has_optimizer_hint_member : std::false_type
    {};

    template<typename _Type>
    struct has_optimizer_hint_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().optimizer_hint)>>
        : std::true_type
    {};

    // has_json_columns_member
    //   trait: detects mysql_table_state's json_columns field.
    template<typename _Type,
             typename = void>
    struct has_json_columns_member : std::false_type
    {};

    template<typename _Type>
    struct has_json_columns_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().json_columns)>>
        : std::true_type
    {};

    // has_on_analyze_member
    //   trait: detects mysql_table_state's on_analyze callback.
    template<typename _Type,
             typename = void>
    struct has_on_analyze_member : std::false_type
    {};

    template<typename _Type>
    struct has_on_analyze_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().on_analyze)>>
        : std::true_type
    {};

    // has_on_check_member
    //   trait: detects mysql_table_state's on_check callback.
    template<typename _Type,
             typename = void>
    struct has_on_check_member : std::false_type
    {};

    template<typename _Type>
    struct has_on_check_member<
        _Type,
        std::void_t<decltype(std::declval<_Type>().on_check)>>
        : std::true_type
    {};

}   // NS_INTERNAL

template<typename _Type>
D_INLINE constexpr bool has_optimizer_hint_v =
    internal::has_optimizer_hint_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_json_columns_v =
    internal::has_json_columns_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_on_analyze_v =
    internal::has_on_analyze_member<_Type>::value;

template<typename _Type>
D_INLINE constexpr bool has_on_check_v =
    internal::has_on_check_member<_Type>::value;

// is_mysql_table_state
//   trait: true if a type satisfies the mysql_table_state
// structural interface (optimizer_hint, json_columns, on_analyze,
// on_check) in addition to the base database_table_state interface.
template<typename _Type>
struct is_mysql_table_state : std::conjunction<
    database_table_view_traits::is_database_table_state<_Type>,
    internal::has_optimizer_hint_member<_Type>,
    internal::has_json_columns_member<_Type>,
    internal::has_on_analyze_member<_Type>,
    internal::has_on_check_member<_Type>>
{};

template<typename _Type>
D_INLINE constexpr bool is_mysql_table_state_v =
    is_mysql_table_state<_Type>::value;

}   // namespace mysql_table_view_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MYSQL_TABLE_VIEW_
