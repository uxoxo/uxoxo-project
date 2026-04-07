/*******************************************************************************
* uxoxo [component]                                              table_view.hpp
*
*   Framework-agnostic adapter that bridges the compile-time table model
* (table.hpp, table_traits.hpp) to runtime UI concepts. A renderer (Qt,
* TUI, imgui, web) reads this struct and draws it however it likes.
*
*   The table model has fixed compile-time dimensions, config-driven regions
* (header/footer/total rows and columns), and merged cells (spans). This
* adapter translates all of that into a uniform runtime interface.
*
*   Structure:
*     I.    enums
*     II.   core structs
*     III.  cell query functions
*     IV.   navigation
*     V.    selection operations
*     VI.   editing operations
*     VII.  sort operations
*     VIII. binding
*     IX.   visible iteration
*     X.    traits
*
* path:      /inc/uxoxo/component/table/table_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.28
*******************************************************************************/

#ifndef UXOXO_COMPONENT_TABLE_VIEW_
#define UXOXO_COMPONENT_TABLE_VIEW_ 1

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT

// cell_region
//   enum: structural region classification for a cell.
enum class cell_region : std::uint8_t
{
    header,
    data,
    total,
    footer
};

// cell_role
//   enum: semantic role classification for styling and behavior.
enum class cell_role : std::uint8_t
{
    label,
    value,
    aggregate,
    blank,
    covered
};

#ifndef UXOXO_COMPONENT_VIEW_COMMON_
// sort_order
//   enum: sort direction state for a column.
enum class sort_order : std::uint8_t
{
    none,
    ascending,
    descending
};

// text_alignment
//   enum: horizontal text alignment for a column.
enum class text_alignment : std::uint8_t
{
    left,
    center,
    right
};
#endif  // UXOXO_COMPONENT_VIEW_COMMON_

// table_column
//   struct: runtime descriptor for a visible table column.
struct table_column
{
    std::string    name;
    int            width      = 0;
    int            min_width  = 30;
    int            max_width  = 0;
    float          flex       = 1.0f;
    text_alignment align      = text_alignment::left;
    sort_order     sort       = sort_order::none;
    bool           visible    = true;
    bool           resizable  = true;
    bool           sortable   = true;
};

// cell_span
//   struct: runtime mirror of a merged-cell span.
struct cell_span
{
    std::size_t row      = 0;
    std::size_t col      = 0;
    std::size_t row_span = 0;
    std::size_t col_span = 0;
};

// table_selection_style
//   enum: selection mode for the current table view.
enum class table_selection_style : std::uint8_t
{
    none,
    cell,
    row,
    column,
    range
};

// cell_pos
//   struct: row/column coordinate pair.
struct cell_pos
{
    std::size_t row = 0;
    std::size_t col = 0;

    D_CONSTEXPR bool
    operator==(const cell_pos& _other) const
    {
        return ( (row == _other.row) &&
                 (col == _other.col) );
    }

    D_CONSTEXPR bool
    operator!=(const cell_pos& _other) const
    {
        return !(*this == _other);
    }
};

// cell_range
//   struct: inclusive rectangular range of cells.
struct cell_range
{
    cell_pos start;
    cell_pos end;

    [[nodiscard]] bool
    contains(std::size_t _row,
             std::size_t _col) const
    {
        const auto row_start = std::min(start.row,
                                        end.row);
        const auto row_end   = std::max(start.row,
                                        end.row);
        const auto col_start = std::min(start.col,
                                        end.col);
        const auto col_end   = std::max(start.col,
                                        end.col);

        return ( (_row >= row_start) &&
                 (_row <= row_end)   &&
                 (_col >= col_start) &&
                 (_col <= col_end) );
    }

    [[nodiscard]] std::size_t
    row_count() const
    {
        return std::max(start.row,
                        end.row) -
               std::min(start.row,
                        end.row) + 1;
    }

    [[nodiscard]] std::size_t
    col_count() const
    {
        return std::max(start.col,
                        end.col) -
               std::min(start.col,
                        end.col) + 1;
    }
};

// table_view
//   struct: runtime adapter for presenting a compile-time table model.
struct table_view
{
    using cell_extractor = std::function<std::string(std::size_t,
                                                     std::size_t)>;
    using cell_writer    = std::function<bool(std::size_t,
                                              std::size_t,
                                              const std::string&)>;
    using sort_callback  = std::function<void(std::size_t,
                                              sort_order)>;

    static D_CONSTEXPR bool focusable  = true;
    static D_CONSTEXPR bool scrollable = true;

    std::size_t num_rows    = 0;
    std::size_t num_cols    = 0;
    std::size_t header_rows = 0;
    std::size_t header_cols = 0;
    std::size_t footer_rows = 0;
    std::size_t footer_cols = 0;
    std::size_t total_rows  = 0;
    std::size_t total_cols  = 0;

    std::vector<table_column> columns;
    std::vector<cell_span>    spans;

    cell_extractor get_cell;

    table_selection_style sel_style     = table_selection_style::cell;
    cell_pos              cursor        = { 0, 0 };
    cell_range            selection     = { { 0, 0 }, { 0, 0 } };
    bool                  has_selection = false;

    bool        editing     = false;
    cell_pos    edit_cell   = { 0, 0 };
    std::string edit_buffer;
    std::size_t edit_cursor = 0;

    cell_writer set_cell;

    std::size_t scroll_row = 0;
    std::size_t scroll_col = 0;
    std::size_t page_rows  = 20;
    std::size_t page_cols  = 10;

    std::size_t sort_column = 0;
    sort_order  sort_dir    = sort_order::none;

    sort_callback on_sort;

    bool stripe_rows = true;
    bool show_grid   = true;
};

// tv_cell_text
//   function: returns the display string for a cell.
D_INLINE std::string
tv_cell_text(const table_view& _table_view,
             std::size_t       _row,
             std::size_t       _col)
{
    if (_table_view.get_cell)
    {
        return _table_view.get_cell(_row,
                                    _col);
    }

    return {};
}

// tv_cell_region
//   function: classifies a cell's structural region.
D_INLINE cell_region
tv_cell_region(const table_view& _table_view,
               std::size_t       _row,
               std::size_t       _col)
{
    // check header row region
    if (_row < _table_view.header_rows)
    {
        return cell_region::header;
    }

    // check footer row region
    if ( (_table_view.footer_rows > 0) &&
         (_row >= _table_view.num_rows - _table_view.footer_rows) )
    {
        return cell_region::footer;
    }

    // check total row region
    if ( (_table_view.total_rows > 0) &&
         (_row >= _table_view.num_rows -
                  _table_view.footer_rows -
                  _table_view.total_rows) &&
         (_row < _table_view.num_rows - _table_view.footer_rows) )
    {
        return cell_region::total;
    }

    // check header column region
    if (_col < _table_view.header_cols)
    {
        return cell_region::header;
    }

    // check footer column region
    if ( (_table_view.footer_cols > 0) &&
         (_col >= _table_view.num_cols - _table_view.footer_cols) )
    {
        return cell_region::footer;
    }

    // check total column region
    if ( (_table_view.total_cols > 0) &&
         (_col >= _table_view.num_cols -
                  _table_view.footer_cols -
                  _table_view.total_cols) &&
         (_col < _table_view.num_cols - _table_view.footer_cols) )
    {
        return cell_region::total;
    }

    return cell_region::data;
}

// tv_cell_role
//   function: determines the semantic role of a cell.
D_INLINE cell_role
tv_cell_role(const table_view& _table_view,
             std::size_t       _row,
             std::size_t       _col)
{
    // check if the cell is covered by a span
    for (const auto& _span : _table_view.spans)
    {
        if ( (_row >= _span.row)                  &&
             (_row <  _span.row + _span.row_span) &&
             (_col >= _span.col)                  &&
             (_col <  _span.col + _span.col_span) )
        {
            // span origin breaks out to further classification
            if ( (_row == _span.row) && (_col == _span.col) )
            {
                break;
            }

            return cell_role::covered;
        }
    }

    const auto region = tv_cell_region(_table_view,
                                       _row,
                                       _col);

    // header-row/header-col intersection is blank
    if ( (_row < _table_view.header_rows) &&
         (_col < _table_view.header_cols) )
    {
        return cell_role::blank;
    }

    // header cells are labels
    if (region == cell_region::header)
    {
        return cell_role::label;
    }

    // total cells are aggregates
    if (region == cell_region::total)
    {
        return cell_role::aggregate;
    }

    return cell_role::value;
}

// tv_is_span_origin
//   function: returns true if a cell is the origin of a span.
D_INLINE bool
tv_is_span_origin(const table_view& _table_view,
                  std::size_t       _row,
                  std::size_t       _col)
{
    for (const auto& _span : _table_view.spans)
    {
        if ( (_span.row == _row) && (_span.col == _col) )
        {
            return true;
        }
    }

    return false;
}

// tv_get_span
//   function: returns the span containing a cell, or nullptr.
D_INLINE const cell_span*
tv_get_span(const table_view& _table_view,
            std::size_t       _row,
            std::size_t       _col)
{
    for (const auto& _span : _table_view.spans)
    {
        if ( (_row >= _span.row)                  &&
             (_row <  _span.row + _span.row_span) &&
             (_col >= _span.col)                  &&
             (_col <  _span.col + _span.col_span) )
        {
            return &_span;
        }
    }

    return nullptr;
}

// tv_data_row_start
//   function: returns the first data-row index.
D_INLINE std::size_t
tv_data_row_start(
    const table_view& _table_view
)
{
    return _table_view.header_rows;
}

// tv_data_row_end
//   function: returns the data-row end index, exclusive.
D_INLINE std::size_t
tv_data_row_end(
    const table_view& _table_view
)
{
    return _table_view.num_rows -
           _table_view.footer_rows -
           _table_view.total_rows;
}

// tv_data_col_start
//   function: returns the first data-column index.
D_INLINE std::size_t
tv_data_col_start(
    const table_view& _table_view
)
{
    return _table_view.header_cols;
}

// tv_data_col_end
//   function: returns the data-column end index, exclusive.
D_INLINE std::size_t
tv_data_col_end(
    const table_view& _table_view
)
{
    return _table_view.num_cols -
           _table_view.footer_cols -
           _table_view.total_cols;
}

// tv_data_row_count
//   function: returns the number of data rows.
D_INLINE std::size_t
tv_data_row_count(
    const table_view& _table_view
)
{
    return tv_data_row_end(_table_view) -
           tv_data_row_start(_table_view);
}

// tv_data_col_count
//   function: returns the number of data columns.
D_INLINE std::size_t
tv_data_col_count(
    const table_view& _table_view
)
{
    return tv_data_col_end(_table_view) -
           tv_data_col_start(_table_view);
}

// tv_move_up
//   function: moves the cursor up by one row.
D_INLINE bool
tv_move_up(
    table_view& _table_view
)
{
    // boundary check
    if (_table_view.cursor.row == 0)
    {
        return false;
    }

    --_table_view.cursor.row;

    // adjust scroll if cursor moved above visible area
    if (_table_view.cursor.row < _table_view.scroll_row)
    {
        _table_view.scroll_row = _table_view.cursor.row;
    }

    return true;
}

// tv_move_down
//   function: moves the cursor down by one row.
D_INLINE bool
tv_move_down(
    table_view& _table_view
)
{
    // boundary check
    if (_table_view.cursor.row + 1 >= _table_view.num_rows)
    {
        return false;
    }

    ++_table_view.cursor.row;

    // adjust scroll if cursor moved below visible area
    if (_table_view.cursor.row >= _table_view.scroll_row +
                                  _table_view.page_rows)
    {
        _table_view.scroll_row = _table_view.cursor.row -
                                 _table_view.page_rows + 1;
    }

    return true;
}

// tv_move_left
//   function: moves the cursor left by one column.
D_INLINE bool
tv_move_left(
    table_view& _table_view
)
{
    // boundary check
    if (_table_view.cursor.col == 0)
    {
        return false;
    }

    --_table_view.cursor.col;

    // adjust scroll if cursor moved left of visible area
    if (_table_view.cursor.col < _table_view.scroll_col)
    {
        _table_view.scroll_col = _table_view.cursor.col;
    }

    return true;
}

// tv_move_right
//   function: moves the cursor right by one column.
D_INLINE bool
tv_move_right(
    table_view& _table_view
)
{
    // boundary check
    if (_table_view.cursor.col + 1 >= _table_view.num_cols)
    {
        return false;
    }

    ++_table_view.cursor.col;

    // adjust scroll if cursor moved right of visible area
    if (_table_view.cursor.col >= _table_view.scroll_col +
                                  _table_view.page_cols)
    {
        _table_view.scroll_col = _table_view.cursor.col -
                                 _table_view.page_cols + 1;
    }

    return true;
}

// tv_move_home
//   function: moves the cursor to the first visible column.
D_INLINE bool
tv_move_home(
    table_view& _table_view
)
{
    _table_view.cursor.col = 0;
    _table_view.scroll_col = 0;

    return true;
}

// tv_move_end
//   function: moves the cursor to the last column.
D_INLINE bool
tv_move_end(
    table_view& _table_view
)
{
    // empty table check
    if (_table_view.num_cols == 0)
    {
        return false;
    }

    _table_view.cursor.col = _table_view.num_cols - 1;

    // adjust scroll if cursor moved right of visible area
    if (_table_view.cursor.col >= _table_view.scroll_col +
                                  _table_view.page_cols)
    {
        _table_view.scroll_col = _table_view.cursor.col -
                                 _table_view.page_cols + 1;
    }

    return true;
}

// tv_move_top
//   function: moves the cursor to the first row.
D_INLINE bool
tv_move_top(
    table_view& _table_view
)
{
    _table_view.cursor.row = 0;
    _table_view.scroll_row = 0;

    return true;
}

// tv_move_bottom
//   function: moves the cursor to the last row.
D_INLINE bool
tv_move_bottom(
    table_view& _table_view
)
{
    // empty table check
    if (_table_view.num_rows == 0)
    {
        return false;
    }

    _table_view.cursor.row = _table_view.num_rows - 1;

    // adjust scroll if cursor moved below visible area
    if (_table_view.cursor.row >= _table_view.scroll_row +
                                  _table_view.page_rows)
    {
        _table_view.scroll_row = _table_view.cursor.row -
                                 _table_view.page_rows + 1;
    }

    return true;
}

// tv_page_up
//   function: moves the cursor up by one page.
D_INLINE void
tv_page_up(
    table_view& _table_view
)
{
    _table_view.cursor.row =
        (_table_view.cursor.row > _table_view.page_rows)
            ? _table_view.cursor.row - _table_view.page_rows
            : 0;

    // adjust scroll if cursor moved above visible area
    if (_table_view.cursor.row < _table_view.scroll_row)
    {
        _table_view.scroll_row = _table_view.cursor.row;
    }

    return;
}

// tv_page_down
//   function: moves the cursor down by one page.
D_INLINE void
tv_page_down(
    table_view& _table_view
)
{
    _table_view.cursor.row = std::min(
        _table_view.cursor.row + _table_view.page_rows,
        (_table_view.num_rows > 0)
            ? (_table_view.num_rows - 1)
            : 0
    );

    // adjust scroll if cursor moved below visible area
    if (_table_view.cursor.row >= _table_view.scroll_row +
                                  _table_view.page_rows)
    {
        _table_view.scroll_row = _table_view.cursor.row -
                                 _table_view.page_rows + 1;
    }

    return;
}

// tv_select_cell
//   function: selects a single cell.
D_INLINE void
tv_select_cell(table_view& _table_view,
               std::size_t _row,
               std::size_t _col)
{
    _table_view.selection     = { { _row, _col },
                                  { _row, _col } };
    _table_view.has_selection = true;

    return;
}

// tv_select_row
//   function: selects an entire row.
D_INLINE void
tv_select_row(table_view& _table_view,
              std::size_t _row)
{
    _table_view.selection = {
        { _row, 0 },
        { _row,
          (_table_view.num_cols > 0)
              ? (_table_view.num_cols - 1)
              : 0 }
    };
    _table_view.has_selection = true;

    return;
}

// tv_select_column
//   function: selects an entire column.
D_INLINE void
tv_select_column(table_view& _table_view,
                 std::size_t _col)
{
    _table_view.selection = {
        { 0, _col },
        { (_table_view.num_rows > 0)
              ? (_table_view.num_rows - 1)
              : 0,
          _col }
    };
    _table_view.has_selection = true;

    return;
}

// tv_select_range
//   function: selects an inclusive rectangular range.
D_INLINE void
tv_select_range(table_view& _table_view,
                cell_pos    _start,
                cell_pos    _end)
{
    _table_view.selection     = { _start, _end };
    _table_view.has_selection = true;

    return;
}

// tv_select_all
//   function: selects the entire table.
D_INLINE void
tv_select_all(
    table_view& _table_view
)
{
    _table_view.selection = {
        { 0, 0 },
        {
            (_table_view.num_rows > 0)
                ? (_table_view.num_rows - 1)
                : 0,
            (_table_view.num_cols > 0)
                ? (_table_view.num_cols - 1)
                : 0
        }
    };
    _table_view.has_selection = true;

    return;
}

// tv_clear_selection
//   function: clears the current selection.
D_INLINE void
tv_clear_selection(
    table_view& _table_view
)
{
    _table_view.has_selection = false;

    return;
}

// tv_is_selected
//   function: returns true if a cell lies within the active selection.
D_INLINE bool
tv_is_selected(const table_view& _table_view,
               std::size_t       _row,
               std::size_t       _col)
{
    if (!_table_view.has_selection)
    {
        return false;
    }

    return _table_view.selection.contains(_row,
                                          _col);
}

// tv_select_at_cursor
//   function: selects using the current selection style at the cursor.
D_INLINE void
tv_select_at_cursor(
    table_view& _table_view
)
{
    switch (_table_view.sel_style)
    {
        case table_selection_style::cell:
            tv_select_cell(_table_view,
                           _table_view.cursor.row,
                           _table_view.cursor.col);
            break;

        case table_selection_style::row:
            tv_select_row(_table_view,
                          _table_view.cursor.row);
            break;

        case table_selection_style::column:
            tv_select_column(_table_view,
                             _table_view.cursor.col);
            break;

        case table_selection_style::range:
            _table_view.selection.end = _table_view.cursor;
            _table_view.has_selection = true;
            break;

        case table_selection_style::none:
            break;
    }

    return;
}

// tv_selected_cells
//   function: returns every cell position within the active selection.
D_INLINE std::vector<cell_pos>
tv_selected_cells(
    const table_view& _table_view
)
{
    std::vector<cell_pos> result;

    if (!_table_view.has_selection)
    {
        return result;
    }

    const auto row_start = std::min(_table_view.selection.start.row,
                                    _table_view.selection.end.row);
    const auto row_end   = std::max(_table_view.selection.start.row,
                                    _table_view.selection.end.row);
    const auto col_start = std::min(_table_view.selection.start.col,
                                    _table_view.selection.end.col);
    const auto col_end   = std::max(_table_view.selection.start.col,
                                    _table_view.selection.end.col);

    for (std::size_t row = row_start; row <= row_end; ++row)
    {
        for (std::size_t col = col_start; col <= col_end; ++col)
        {
            result.push_back({ row, col });
        }
    }

    return result;
}

// tv_begin_edit
//   function: begins inline editing for a cell.
D_INLINE bool
tv_begin_edit(table_view& _table_view,
              std::size_t _row,
              std::size_t _col)
{
    // reject editing of covered (spanned) cells
    if (tv_cell_role(_table_view,
                     _row,
                     _col) == cell_role::covered)
    {
        return false;
    }

    _table_view.editing     = true;
    _table_view.edit_cell   = { _row, _col };
    _table_view.edit_buffer = tv_cell_text(_table_view,
                                           _row,
                                           _col);
    _table_view.edit_cursor = _table_view.edit_buffer.size();

    return true;
}

// tv_begin_edit_at_cursor
//   function: begins inline editing at the current cursor.
D_INLINE bool
tv_begin_edit_at_cursor(
    table_view& _table_view
)
{
    return tv_begin_edit(_table_view,
                         _table_view.cursor.row,
                         _table_view.cursor.col);
}

// tv_commit_edit
//   function: commits the current edit buffer to the bound writer.
D_INLINE bool
tv_commit_edit(
    table_view& _table_view
)
{
    // nothing to commit
    if (!_table_view.editing)
    {
        return false;
    }

    _table_view.editing = false;

    // write through the bound callback
    if (_table_view.set_cell)
    {
        return _table_view.set_cell(_table_view.edit_cell.row,
                                    _table_view.edit_cell.col,
                                    _table_view.edit_buffer);
    }

    return false;
}

// tv_cancel_edit
//   function: cancels the current edit operation.
D_INLINE void
tv_cancel_edit(
    table_view& _table_view
)
{
    _table_view.editing = false;
    _table_view.edit_buffer.clear();

    return;
}

// tv_sort_by_column
//   function: updates sort state and invokes the bound sort callback.
D_INLINE void
tv_sort_by_column(table_view& _table_view,
                  std::size_t _col,
                  sort_order  _dir)
{
    // clear existing sort indicators
    for (auto& _column : _table_view.columns)
    {
        _column.sort = sort_order::none;
    }

    // apply sort to the target column if valid and sortable
    if (_col < _table_view.columns.size())
    {
        if (!_table_view.columns[_col].sortable)
        {
            return;
        }

        _table_view.columns[_col].sort = _dir;
    }

    _table_view.sort_column = _col;
    _table_view.sort_dir    = _dir;

    // invoke the bound sort callback
    if (_table_view.on_sort)
    {
        _table_view.on_sort(_col,
                            _dir);
    }

    return;
}

// tv_toggle_sort
//   function: cycles a column's sort state.
D_INLINE void
tv_toggle_sort(
	table_view& _table_view,
	std::size_t _col
)
{
    sort_order next = sort_order::ascending;

    // cycle through none -> ascending -> descending -> none
    if (_table_view.sort_column == _col)
    {
        switch (_table_view.sort_dir)
        {
            case sort_order::none:
                next = sort_order::ascending;
                break;

            case sort_order::ascending:
                next = sort_order::descending;
                break;

            case sort_order::descending:
            default:
                next = sort_order::none;
                break;
        }
    }

    tv_sort_by_column(_table_view,
                      _col,
                      next);

    return;
}

namespace detail_tv
{
    // has_header_rows
    //   trait: detects a static header_rows member.
    template<typename _Type,
             typename = void>
    struct has_header_rows : std::false_type
    {};

    template<typename _Type>
    struct has_header_rows<_Type,
                           std::void_t<decltype(_Type::header_rows)>>
        : std::true_type
    {};

    // has_header_cols
    //   trait: detects a static header_cols member.
    template<typename _Type,
             typename = void>
    struct has_header_cols : std::false_type
    {};

    template<typename _Type>
    struct has_header_cols<_Type,
                           std::void_t<decltype(_Type::header_cols)>>
        : std::true_type
    {};

    // has_footer_rows
    //   trait: detects a static footer_rows member.
    template<typename _Type,
             typename = void>
    struct has_footer_rows : std::false_type
    {};

    template<typename _Type>
    struct has_footer_rows<_Type,
                           std::void_t<decltype(_Type::footer_rows)>>
        : std::true_type
    {};

    // has_footer_cols
    //   trait: detects a static footer_cols member.
    template<typename _Type,
             typename = void>
    struct has_footer_cols : std::false_type
    {};

    template<typename _Type>
    struct has_footer_cols<_Type,
                           std::void_t<decltype(_Type::footer_cols)>>
        : std::true_type
    {};

    // has_total_rows
    //   trait: detects a static total_rows member.
    template<typename _Type,
             typename = void>
    struct has_total_rows : std::false_type
    {};

    template<typename _Type>
    struct has_total_rows<_Type,
                          std::void_t<decltype(_Type::total_rows)>>
        : std::true_type
    {};

    // has_total_cols
    //   trait: detects a static total_cols member.
    template<typename _Type,
             typename = void>
    struct has_total_cols : std::false_type
    {};

    template<typename _Type>
    struct has_total_cols<_Type,
                          std::void_t<decltype(_Type::total_cols)>>
        : std::true_type
    {};

    // has_spans_alias
    //   trait: detects a public spans type alias.
    template<typename _Type,
             typename = void>
    struct has_spans_alias : std::false_type
    {};

    template<typename _Type>
    struct has_spans_alias<_Type,
                           std::void_t<typename _Type::spans>>
        : std::true_type
    {};
}

// tv_bind
//   function: binds a table-like object to a table_view.
template<typename _Table,
         typename _ToString>
void
tv_bind(
	table_view& _table_view,
	_Table&     _table,
	_ToString   _to_string
)
{
    _table_view.num_rows = _Table::num_rows;
    _table_view.num_cols = _Table::num_cols;

    _table_view.get_cell =
        [&_table, _to_string](std::size_t _row,
                              std::size_t _col) -> std::string
        {
            return _to_string(_table.cell(_row,
                                          _col));
        };

    _table_view.set_cell    = nullptr;
    _table_view.header_rows = 0;
    _table_view.header_cols = 0;
    _table_view.footer_rows = 0;
    _table_view.footer_cols = 0;
    _table_view.total_rows  = 0;
    _table_view.total_cols  = 0;

    _table_view.columns.clear();
    _table_view.columns.resize(_table_view.num_cols);

    for (std::size_t index = 0; index < _table_view.num_cols; ++index)
    {
        // use header row text as column name if available
        if ( (_table_view.header_rows > 0) && _table_view.get_cell )
        {
            _table_view.columns[index].name =
                _table_view.get_cell(0,
                                     index);
        }
        else
        {
            _table_view.columns[index].name =
                "Col " + std::to_string(index);
        }
    }

    return;
}

// tv_bind_config
//   function: applies compile-time config values to a table_view.
template<typename _Config>
void
tv_bind_config(
	table_view& _table_view
)
{
    if D_CONSTEXPR (detail_tv::has_header_rows<_Config>::value)
    {
        _table_view.header_rows = _Config::header_rows;
    }

    if D_CONSTEXPR (detail_tv::has_header_cols<_Config>::value)
    {
        _table_view.header_cols = _Config::header_cols;
    }

    if D_CONSTEXPR (detail_tv::has_footer_rows<_Config>::value)
    {
        _table_view.footer_rows = _Config::footer_rows;
    }

    if D_CONSTEXPR (detail_tv::has_footer_cols<_Config>::value)
    {
        _table_view.footer_cols = _Config::footer_cols;
    }

    if D_CONSTEXPR (detail_tv::has_total_rows<_Config>::value)
    {
        _table_view.total_rows = _Config::total_rows;
    }

    if D_CONSTEXPR (detail_tv::has_total_cols<_Config>::value)
    {
        _table_view.total_cols = _Config::total_cols;
    }

    return;
}

// tv_add_span
//   function: registers a merged-cell span.
D_INLINE void
tv_add_span(
	table_view& _table_view,
	std::size_t _row,
	std::size_t _col,
	std::size_t _row_span,
	std::size_t _col_span
)
{
    _table_view.spans.push_back({ _row,
                                  _col,
                                  _row_span,
                                  _col_span });

    return;
}

// tv_visible_rows
//   function: returns the visible row range as [start, end).
D_INLINE std::pair<std::size_t,
                   std::size_t>
tv_visible_rows(
    const table_view& _table_view
)
{
    const auto start = _table_view.scroll_row;
    const auto end   = std::min(_table_view.scroll_row +
                                    _table_view.page_rows,
                                _table_view.num_rows);

    return { start, end };
}

// tv_visible_cols
//   function: returns the visible column range as [start, end).
D_INLINE std::pair<std::size_t, 
                   std::size_t>
tv_visible_cols(
    const table_view& _table_view
)
{
    const auto start = _table_view.scroll_col;
    const auto end   = std::min(_table_view.scroll_col +
                                    _table_view.page_cols,
                                _table_view.num_cols);

    return { start, end };
}

// tv_for_each_visible_cell
//   function: visits each visible, non-covered cell.
template<typename _Fn>
void
tv_for_each_visible_cell(const table_view& _table_view,
                         _Fn&&             _fn)
{
    const auto [row_start, row_end] = tv_visible_rows(_table_view);
    const auto [col_start, col_end] = tv_visible_cols(_table_view);

    for (std::size_t row = row_start; row < row_end; ++row)
    {
        for (std::size_t col = col_start; col < col_end; ++col)
        {
            const auto role = tv_cell_role(_table_view,
                                           row,
                                           col);

            // skip cells covered by a span
            if (role == cell_role::covered)
            {
                continue;
            }

            const auto region = tv_cell_region(_table_view,
                                               row,
                                               col);
            const auto text   = tv_cell_text(_table_view,
                                             row,
                                             col);
            _fn(row,
                col,
                text,
                region,
                role);
        }
    }

    return;
}

namespace table_view_traits
{
    namespace detail
    {
        // has_num_rows_member
        //   trait: detects a num_rows member.
        template<typename _Type,
                 typename = void>
        struct has_num_rows_member : std::false_type
        {};

        template<typename _Type>
        struct has_num_rows_member<
            _Type,
            std::void_t<decltype(std::declval<_Type>().num_rows)>>
            : std::true_type
        {};

        // has_num_cols_member
        //   trait: detects a num_cols member.
        template<typename _Type,
                 typename = void>
        struct has_num_cols_member : std::false_type
        {};

        template<typename _Type>
        struct has_num_cols_member<
            _Type,
            std::void_t<decltype(std::declval<_Type>().num_cols)>>
            : std::true_type
        {};

        // has_columns_member
        //   trait: detects a columns member.
        template<typename _Type,
                 typename = void>
        struct has_columns_member : std::false_type
        {};

        template<typename _Type>
        struct has_columns_member<
            _Type,
            std::void_t<decltype(std::declval<_Type>().columns)>>
            : std::true_type
        {};

        // has_get_cell_member
        //   trait: detects a get_cell member.
        template<typename _Type,
                 typename = void>
        struct has_get_cell_member : std::false_type
        {};

        template<typename _Type>
        struct has_get_cell_member<
            _Type,
            std::void_t<decltype(std::declval<_Type>().get_cell)>>
            : std::true_type
        {};

        // has_spans_member
        //   trait: detects a spans member.
        template<typename _Type,
                 typename = void>
        struct has_spans_member : std::false_type
        {};

        template<typename _Type>
        struct has_spans_member<
            _Type,
            std::void_t<decltype(std::declval<_Type>().spans)>>
            : std::true_type
        {};

        // has_cursor_member
        //   trait: detects a cursor member.
        template<typename _Type,
                 typename = void>
        struct has_cursor_member : std::false_type
        {};

        template<typename _Type>
        struct has_cursor_member<
            _Type,
            std::void_t<decltype(std::declval<_Type>().cursor)>>
            : std::true_type
        {};

        // has_editing_member
        //   trait: detects an editing member.
        template<typename _Type,
                 typename = void>
        struct has_editing_member : std::false_type
        {};

        template<typename _Type>
        struct has_editing_member<
            _Type,
            std::void_t<decltype(std::declval<_Type>().editing)>>
            : std::true_type
        {};

        // has_focusable_flag
        //   trait: detects a static focusable flag.
        template<typename _Type,
                 typename = void>
        struct has_focusable_flag : std::false_type
        {};

        template<typename _Type>
        struct has_focusable_flag<_Type,
                                  std::enable_if_t<_Type::focusable>>
            : std::true_type
        {};
    }

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool has_num_rows_v =
        detail::has_num_rows_member<_Type>::value;

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool has_num_cols_v =
        detail::has_num_cols_member<_Type>::value;

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool has_columns_v =
        detail::has_columns_member<_Type>::value;

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool has_get_cell_v =
        detail::has_get_cell_member<_Type>::value;

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool has_spans_v =
        detail::has_spans_member<_Type>::value;

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool has_cursor_v =
        detail::has_cursor_member<_Type>::value;

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool has_editing_v =
        detail::has_editing_member<_Type>::value;

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool is_focusable_v =
        detail::has_focusable_flag<_Type>::value;

    // is_table_view
    //   trait: detects whether a type satisfies the table_view surface.
    template<typename _Type>
    struct is_table_view : std::conjunction<
        detail::has_num_rows_member<_Type>,
        detail::has_num_cols_member<_Type>,
        detail::has_columns_member<_Type>,
        detail::has_get_cell_member<_Type>,
        detail::has_cursor_member<_Type>,
        detail::has_focusable_flag<_Type>>
    {};

    template<typename _Type>
    D_INLINE D_CONSTEXPR bool is_table_view_v =
        is_table_view<_Type>::value;
}


NS_END
NS_END


#endif  // UXOXO_COMPONENT_TABLE_VIEW_