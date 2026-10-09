/*******************************************************************************
* uxoxo [core]                                                 element_table.hpp
*
* Table elements: a grid of cells with headers, and its rows.
*   A table holds table_rows; each row holds one child per column, any
* element. "headers" names the columns, one per line, and fixes their
* number. The grid lines are the table's to choose: "borders" is "all",
* "inner", "outer", "rows", "columns" or "none"; "row_bg" stripes
* the rows and "resizable" lets the user drag the column edges. "height"
* above zero gives the table a scrolling body with the header kept in view.
* "widths", one per line like "headers", fixes a column at that many pixels;
* a column given 0, or none, shares what is left.
*
* path:      /inc/uxoxo/element_table.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_TABLE_HPP
#define UXOXO_ELEMENT_TABLE_HPP 1

// std
#include <cstddef>  // std::size_t
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_table / archetype_table_row
//   constants: the render archetypes for a table and for one of its rows.
inline constexpr const char* archetype_table     = "table";
inline constexpr const char* archetype_table_row = "table_row";


NS_COMPONENT

    // table_type
    //   the descriptor for the table element, owned by this module.
    D_NODISCARD
    inline const element_type* table_type()
    {
        static const element_type descriptor =
            element_type{
                "table",
                archetype_table,
                option_set{ { { "id",        ::djinterp::option_value(
                                                 std::string()) },
                              { "headers",   ::djinterp::option_value(
                                                 std::string()) },
                              { "borders",   ::djinterp::option_value(
                                                 std::string("inner")) },
                              { "row_bg",    ::djinterp::option_value(true) },
                              { "resizable", ::djinterp::option_value(true) },
                              { "height",    ::djinterp::option_value(
                                                 0.0) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    // table_row_type
    //   the descriptor for one row of a table, owned by this module.
    D_NODISCARD
    inline const element_type* table_row_type()
    {
        static const element_type descriptor =
            element_type{
                "table_row",
                archetype_table_row,
                option_set{},
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    table
      Builds a `table` template over its rows.

    Parameter(s):
      _id:      a name unique among sibling tables ("id").
      _headers: the column titles, left to right ("headers").
      _rows:    the table_row templates, top to bottom.
      _attrs:   optional extra attributes (borders, row_bg, resizable,
                height), overlaid on top (caller wins).
    Return:
      An element_template wrapping a table node over _rows.
    */
    D_NODISCARD
    inline element_template table(
        const std::string&              _id,
        const std::vector<std::string>& _headers,
        std::vector<element_template>   _rows,
        option_set                      _attrs = option_set{}
    )
    {
        std::string joined;

        // one title per line
        for (std::size_t i = 0; i < _headers.size(); ++i)
        {
            if (i != 0)
            {
                joined += '\n';
            }

            joined += _headers[i];
        }

        option_set resolved = table_type()->defaults;
        resolved.set("id", _id);
        resolved.set("headers", joined);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(table_type(), resolved, std::move(_rows));
    }

    /*
    table_row
      Builds a `table_row` template: one cell per column.

    Parameter(s):
      _cells: the cells, left to right.
      _attrs: optional extra attributes, overlaid on top (caller wins).
    Return:
      An element_template wrapping a table_row node over _cells.
    */
    D_NODISCARD
    inline element_template table_row(
        std::vector<element_template> _cells,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = ::djinterp::overlay(table_row_type()->defaults,
                                                  _attrs);

        return make_element(table_row_type(), resolved, std::move(_cells));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_TABLE_HPP
