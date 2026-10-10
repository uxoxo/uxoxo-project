/*******************************************************************************
* djinterp [test]                                              static_tests.cpp
*   Build:  g++ -std=c++17 -I. -o test static_tests.cpp && ./test
*******************************************************************************/

#include <container/table_view.hpp>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace djinterp::container;
namespace tvt = djinterp::container::table_view_traits;

// ═══════════════════════════════════════════════════════════════════════════════
//  COMPILE-TIME
// ═══════════════════════════════════════════════════════════════════════════════

static_assert( tvt::is_table_view_v<table_view>);
static_assert(!tvt::is_table_view_v<std::string>);
static_assert(!tvt::is_table_view_v<int>);

static_assert( tvt::has_num_rows_v<table_view>);
static_assert( tvt::has_num_cols_v<table_view>);
static_assert( tvt::has_columns_v<table_view>);
static_assert( tvt::has_get_cell_v<table_view>);
static_assert( tvt::has_spans_v<table_view>);
static_assert( tvt::has_cursor_v<table_view>);
static_assert( tvt::has_editing_v<table_view>);
static_assert( tvt::is_focusable_v<table_view>);

// cell_pos equality
static_assert(cell_pos{1,2} == cell_pos{1,2});
static_assert(cell_pos{1,2} != cell_pos{3,4});

// ── test config structs (must be at file scope for static constexpr) ─────

struct test_config {
    static constexpr std::size_t header_rows = 2;
    static constexpr std::size_t header_cols = 1;
    static constexpr std::size_t total_rows  = 1;
    static constexpr std::size_t footer_rows = 1;
};

struct empty_test_config {};

struct mock_table {
    static constexpr std::size_t num_rows = 4;
    static constexpr std::size_t num_cols = 3;
    int data[4][3] = {
        { 1,  2,  3 },
        { 4,  5,  6 },
        { 7,  8,  9 },
        { 10, 11, 12 }
    };
    int& cell(std::size_t r, std::size_t c) { return data[r][c]; }
    const int& cell(std::size_t r, std::size_t c) const { return data[r][c]; }
};

/*****************************************************************************/

// helper: build a 10×5 table view with 1 header row, 1 footer row
table_view make_sample()
{
    table_view tv;
    tv.num_rows = 10;
    tv.num_cols = 5;
    tv.header_rows = 1;
    tv.footer_rows = 1;
    tv.page_rows = 5;
    tv.page_cols = 5;

    // columns
    tv.columns = {
        { "Name",  120, 50, 300, 2.0f, text_alignment::left },
        { "Age",    60, 40, 100, 1.0f, text_alignment::right },
        { "Score", 100, 60, 200, 1.5f, text_alignment::right },
        { "Grade",  60, 40, 100, 1.0f, text_alignment::center },
        { "Notes", 200, 80,   0, 3.0f, text_alignment::left }
    };

    // mock data
    std::vector<std::vector<std::string>> data = {
        { "Name", "Age", "Score", "Grade", "Notes" },   // header
        { "Alice", "25", "95.5", "A", "excellent" },
        { "Bob",   "30", "82.0", "B", "good" },
        { "Carol", "22", "91.2", "A", "" },
        { "Dave",  "28", "76.8", "C", "needs work" },
        { "Eve",   "35", "88.4", "B", "" },
        { "Frank", "27", "93.1", "A", "strong" },
        { "Grace", "31", "79.5", "C", "" },
        { "Hank",  "24", "85.0", "B", "improving" },
        { "Total", "",   "avg", "",  "" }               // footer
    };

    tv.get_cell = [data](std::size_t r, std::size_t c) -> std::string {
        if (r < data.size() && c < data[r].size())
            return data[r][c];
        return "";
    };

    return tv;
}

int main()
{
    std::printf("── sizeof ──────────────────────────────────\n");
    std::printf("  table_view     : %zu\n", sizeof(table_view));
    std::printf("  table_column   : %zu\n", sizeof(table_column));
    std::printf("  cell_span      : %zu\n", sizeof(cell_span));
    std::printf("  cell_pos       : %zu\n", sizeof(cell_pos));
    std::printf("  cell_range     : %zu\n", sizeof(cell_range));
    std::printf("  ✓ sizeof checks\n");

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── cell text ──────────────────────────────\n");
    {
        auto tv = make_sample();
        assert(tv_cell_text(tv, 0, 0) == "Name");
        assert(tv_cell_text(tv, 1, 0) == "Alice");
        assert(tv_cell_text(tv, 1, 1) == "25");
        assert(tv_cell_text(tv, 9, 0) == "Total");
        std::printf("  ✓ cell text extraction via get_cell\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── cell region classification ─────────────\n");
    {
        auto tv = make_sample();

        // header row
        assert(tv_cell_region(tv, 0, 0) == cell_region::header);
        assert(tv_cell_region(tv, 0, 4) == cell_region::header);

        // data rows
        assert(tv_cell_region(tv, 1, 0) == cell_region::data);
        assert(tv_cell_region(tv, 5, 3) == cell_region::data);
        assert(tv_cell_region(tv, 8, 4) == cell_region::data);

        // footer row (last row)
        assert(tv_cell_region(tv, 9, 0) == cell_region::footer);
        assert(tv_cell_region(tv, 9, 4) == cell_region::footer);

        // data range helpers
        assert(tv_data_row_start(tv) == 1);
        assert(tv_data_row_end(tv) == 9);   // 10 - 1 footer - 0 total
        assert(tv_data_row_count(tv) == 8);
        assert(tv_data_col_count(tv) == 5);

        std::printf("  ✓ header, data, footer classification\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── cell region: header+total ───────────────\n");
    {
        table_view tv;
        tv.num_rows = 12;
        tv.num_cols = 6;
        tv.header_rows = 2;
        tv.header_cols = 1;
        tv.footer_rows = 1;
        tv.total_rows  = 1;

        // header rows
        assert(tv_cell_region(tv, 0, 3) == cell_region::header);
        assert(tv_cell_region(tv, 1, 3) == cell_region::header);

        // header col in data row
        assert(tv_cell_region(tv, 5, 0) == cell_region::header);

        // data
        assert(tv_cell_region(tv, 5, 3) == cell_region::data);

        // total row (row 10 = 12-1-1)
        assert(tv_cell_region(tv, 10, 3) == cell_region::total);

        // footer row (row 11)
        assert(tv_cell_region(tv, 11, 3) == cell_region::footer);

        // blank cell: intersection of header row + header col
        assert(tv_cell_role(tv, 0, 0) == cell_role::blank);
        assert(tv_cell_role(tv, 1, 0) == cell_role::blank);

        // header label
        assert(tv_cell_role(tv, 0, 3) == cell_role::label);

        // aggregate
        assert(tv_cell_role(tv, 10, 3) == cell_role::aggregate);

        // data value
        assert(tv_cell_role(tv, 5, 3) == cell_role::value);

        std::printf("  ✓ header cols, total rows, blank intersection\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── spans (merged cells) ───────────────────\n");
    {
        auto tv = make_sample();

        // add a span: merge header row 0, columns 2-4
        tv_add_span(tv, 0, 2, 1, 3);  // (0,2) spans 1 row × 3 cols

        assert(tv.spans.size() == 1);
        assert(tv_is_span_origin(tv, 0, 2));
        assert(!tv_is_span_origin(tv, 0, 3));

        // (0,3) and (0,4) are covered
        assert(tv_cell_role(tv, 0, 3) == cell_role::covered);
        assert(tv_cell_role(tv, 0, 4) == cell_role::covered);

        // (0,2) is the origin — NOT covered
        assert(tv_cell_role(tv, 0, 2) != cell_role::covered);

        // get_span
        auto* sp = tv_get_span(tv, 0, 3);
        assert(sp != nullptr);
        assert(sp->row == 0 && sp->col == 2);
        assert(sp->row_span == 1 && sp->col_span == 3);

        // no span at (1, 0)
        assert(tv_get_span(tv, 1, 0) == nullptr);

        std::printf("  ✓ add_span, is_origin, covered, get_span\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── navigation ─────────────────────────────\n");
    {
        auto tv = make_sample();
        assert(tv.cursor.row == 0 && tv.cursor.col == 0);

        tv_move_down(tv);
        assert(tv.cursor.row == 1);

        tv_move_right(tv);
        assert(tv.cursor.col == 1);

        tv_move_end(tv);
        assert(tv.cursor.col == 4);

        tv_move_home(tv);
        assert(tv.cursor.col == 0);

        tv_move_bottom(tv);
        assert(tv.cursor.row == 9);

        tv_move_top(tv);
        assert(tv.cursor.row == 0);

        // boundaries
        assert(!tv_move_up(tv));
        assert(!tv_move_left(tv));

        tv_move_bottom(tv);
        assert(!tv_move_down(tv));

        tv_move_end(tv);
        assert(!tv_move_right(tv));

        // page
        tv_move_top(tv);
        tv_page_down(tv);
        assert(tv.cursor.row == 5);
        tv_page_up(tv);
        assert(tv.cursor.row == 0);

        // scroll tracking
        tv.scroll_row = 0;
        tv.cursor.row = 0;
        for (int i = 0; i < 6; ++i) tv_move_down(tv);
        assert(tv.cursor.row == 6);
        assert(tv.scroll_row == 2);  // 6 - 5 + 1

        std::printf("  ✓ up/down/left/right/home/end/top/bottom/page, scroll\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── selection ───────────────────────────────\n");
    {
        auto tv = make_sample();

        // cell selection
        tv_select_cell(tv, 3, 2);
        assert(tv.has_selection);
        assert(tv_is_selected(tv, 3, 2));
        assert(!tv_is_selected(tv, 3, 3));

        // row selection
        tv_select_row(tv, 5);
        assert(tv_is_selected(tv, 5, 0));
        assert(tv_is_selected(tv, 5, 4));
        assert(!tv_is_selected(tv, 4, 0));

        // column selection
        tv_select_column(tv, 2);
        assert(tv_is_selected(tv, 0, 2));
        assert(tv_is_selected(tv, 9, 2));
        assert(!tv_is_selected(tv, 0, 1));

        // range selection
        tv_select_range(tv, {2, 1}, {5, 3});
        assert(tv_is_selected(tv, 3, 2));
        assert(!tv_is_selected(tv, 1, 1));
        assert(!tv_is_selected(tv, 6, 3));

        // selected_cells
        auto cells = tv_selected_cells(tv);
        assert(cells.size() == 4 * 3);  // 4 rows × 3 cols

        // select all
        tv_select_all(tv);
        assert(tv_is_selected(tv, 0, 0));
        assert(tv_is_selected(tv, 9, 4));
        auto all = tv_selected_cells(tv);
        assert(all.size() == 50);  // 10 × 5

        // clear
        tv_clear_selection(tv);
        assert(!tv.has_selection);
        assert(!tv_is_selected(tv, 0, 0));

        // select_at_cursor (cell mode)
        tv.sel_style = table_selection_style::cell;
        tv.cursor = {3, 2};
        tv_select_at_cursor(tv);
        assert(tv_is_selected(tv, 3, 2));

        // select_at_cursor (row mode)
        tv.sel_style = table_selection_style::row;
        tv.cursor = {5, 1};
        tv_select_at_cursor(tv);
        assert(tv_is_selected(tv, 5, 0));
        assert(tv_is_selected(tv, 5, 4));

        std::printf("  ✓ cell, row, column, range, all, clear, at_cursor\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── editing ────────────────────────────────\n");
    {
        auto tv = make_sample();
        bool written = false;
        tv.set_cell = [&written](std::size_t r, std::size_t c,
                                  const std::string& val) -> bool {
            written = true;
            assert(r == 2 && c == 1);
            assert(val == "31");
            return true;
        };

        assert(tv_begin_edit(tv, 2, 1));
        assert(tv.editing);
        assert(tv.edit_cell.row == 2 && tv.edit_cell.col == 1);
        assert(tv.edit_buffer == "30");  // Bob's age

        tv.edit_buffer = "31";
        assert(tv_commit_edit(tv));
        assert(!tv.editing);
        assert(written);

        // cancel
        tv_begin_edit(tv, 3, 0);
        assert(tv.editing);
        tv_cancel_edit(tv);
        assert(!tv.editing);

        // begin_edit_at_cursor
        tv.cursor = {4, 2};
        assert(tv_begin_edit_at_cursor(tv));
        assert(tv.edit_cell.row == 4 && tv.edit_cell.col == 2);

        // can't edit a covered cell
        tv_add_span(tv, 0, 2, 1, 3);
        assert(!tv_begin_edit(tv, 0, 3));  // covered

        std::printf("  ✓ begin, commit, cancel, at_cursor, covered guard\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── sorting ────────────────────────────────\n");
    {
        auto tv = make_sample();
        bool sorted = false;
        std::size_t sorted_col = 99;
        sort_order sorted_dir = sort_order::none;

        tv.on_sort = [&](std::size_t col, sort_order dir) {
            sorted = true;
            sorted_col = col;
            sorted_dir = dir;
        };

        // sort by column 2 ascending
        tv_sort_by_column(tv, 2, sort_order::ascending);
        assert(sorted);
        assert(sorted_col == 2);
        assert(sorted_dir == sort_order::ascending);
        assert(tv.columns[2].sort == sort_order::ascending);
        assert(tv.columns[0].sort == sort_order::none);  // cleared

        // toggle: ascending → descending
        tv_toggle_sort(tv, 2);
        assert(tv.sort_dir == sort_order::descending);

        // toggle: descending → none
        tv_toggle_sort(tv, 2);
        assert(tv.sort_dir == sort_order::none);

        // toggle different column: none → ascending
        tv_toggle_sort(tv, 0);
        assert(tv.sort_column == 0);
        assert(tv.sort_dir == sort_order::ascending);

        // non-sortable column
        tv.columns[3].sortable = false;
        sorted = false;
        tv_sort_by_column(tv, 3, sort_order::ascending);
        assert(!sorted);  // callback not called

        std::printf("  ✓ sort_by_column, toggle, indicators, non-sortable\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── visible cell iteration ─────────────────\n");
    {
        auto tv = make_sample();
        tv.scroll_row = 2;
        tv.scroll_col = 0;
        tv.page_rows = 3;
        tv.page_cols = 5;

        // should iterate rows 2–4, cols 0–4
        int count = 0;
        tv_for_each_visible_cell(tv, 
            [&](std::size_t r, std::size_t c, const std::string& text,
                cell_region region, cell_role role) {
                (void)text; (void)region; (void)role;
                assert(r >= 2 && r < 5);
                assert(c < 5);
                ++count;
            });
        assert(count == 15);  // 3 rows × 5 cols

        // with a span: add span at (2,1) → 2 rows × 2 cols
        tv_add_span(tv, 2, 1, 2, 2);
        count = 0;
        tv_for_each_visible_cell(tv,
            [&](std::size_t, std::size_t, const std::string&,
                cell_region, cell_role role) {
                assert(role != cell_role::covered);
                ++count;
            });
        // 15 total - 3 covered cells (span covers (2,2), (3,1), (3,2))
        assert(count == 12);

        std::printf("  ✓ visible iteration respects scroll + skips covered\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── cell_range ─────────────────────────────\n");
    {
        cell_range r = { {2, 1}, {5, 3} };
        assert(r.contains(2, 1));
        assert(r.contains(5, 3));
        assert(r.contains(3, 2));
        assert(!r.contains(1, 1));
        assert(!r.contains(6, 3));
        assert(!r.contains(3, 0));
        assert(r.row_count() == 4);
        assert(r.col_count() == 3);

        // inverted range (end < start)
        cell_range inv = { {5, 3}, {2, 1} };
        assert(inv.contains(3, 2));
        assert(inv.row_count() == 4);

        std::printf("  ✓ contains, row_count, col_count, inverted range\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── tv_bind_config ─────────────────────────\n");
    {
        table_view tv;
        tv.num_rows = 20;
        tv.num_cols = 8;
        tv_bind_config<test_config>(tv);

        assert(tv.header_rows == 2);
        assert(tv.header_cols == 1);
        assert(tv.total_rows  == 1);
        assert(tv.footer_rows == 1);
        assert(tv.footer_cols == 0);  // not in config → stays 0
        assert(tv.total_cols  == 0);

        // verify region calculations
        assert(tv_data_row_start(tv) == 2);
        assert(tv_data_row_end(tv) == 18);  // 20 - 1 footer - 1 total
        assert(tv_data_row_count(tv) == 16);

        // empty config → all zeros
        table_view tv2;
        tv2.num_rows = 10;
        tv2.num_cols = 5;
        tv_bind_config<empty_test_config>(tv2);
        assert(tv2.header_rows == 0);
        assert(tv2.footer_rows == 0);

        std::printf("  ✓ config detection, empty config, region math\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    std::printf("── tv_bind (type-erased table) ────────────\n");
    {
        mock_table mt;
        table_view tv;
        tv_bind(tv, mt, [](const int& v) { return std::to_string(v); });

        assert(tv.num_rows == 4);
        assert(tv.num_cols == 3);
        assert(tv.columns.size() == 3);
        assert(tv_cell_text(tv, 0, 0) == "1");
        assert(tv_cell_text(tv, 2, 1) == "8");
        assert(tv_cell_text(tv, 3, 2) == "12");

        std::printf("  ✓ bind mock table, cell extraction works\n");
    }

    std::printf("\n── ALL TESTS PASSED ─────────────────────────\n");
    return 0;
}
