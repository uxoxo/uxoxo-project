/*******************************************************************************
* uxoxo [component]                                  color_palette_selector.hpp
*
* Discrete-color palette selector:
*   A grid of fixed (non-interpolated) colour swatches from which the
* user picks one.  Despite the name "grid", supports both 1D strips
* (rows=1 or cols=1) and 2D layouts; the model is identical, only
* `rows` and `cols` differ.  Swatches can be any of a small set of
* shapes and any per-axis spacing — including zero spacing for a
* tight grid (as in the central Grid tab of a typical colour picker)
* or non-zero spacing for a separated strip (as in the favourites /
* recent-colours row).
*
*   Two ways to populate:
*
*     1. Manual list — assign or push individual _Color values into
*        the colors vector via cps_set_colors / cps_append_color.
*     2. Generator-based — provide a callable
*           _Color(std::size_t row, std::size_t col,
*                  std::size_t rows, std::size_t cols)
*        and call cps_generate, which evaluates the callable across
*        the rows × cols cell grid and stores the results.  The
*        callable owns all colour math (hue wheel walk, gradient
*        interpolation, lookup-table indexing, anything) so the
*        palette itself stays colour-space agnostic.
*
*   The palette's `value` is the selected swatch's flat index
* (row-major: index = row * cols + col), or no_selection if nothing
* is currently picked.  Because of this, the palette structurally
* conforms to component_common.hpp: set_value selects an index,
* get_value returns it, clear() resets to default_value (a default
* index) when _Clearable, undo() reverts the last selection when
* _Undoable, request_copy() flags the selection for clipboard
* export when _Copyable, and commit() invokes on_commit with the
* current index.
*
* Contents:
*   1.  DSwatchShape                  — enum: square / circle / etc.
*   2.  color_palette_selector<>      — struct template: the selector
*   3.  Operations
*         Population
*           cps_set_colors            — replace the colour list
*           cps_set_colors_1d         — replace; set rows=1
*           cps_set_colors_2d         — replace; set rows / cols
*           cps_set_dimensions        — set rows / cols only
*           cps_append_color          — push one colour
*           cps_remove_color_at       — erase at flat index
*           cps_clear_colors          — empty the colour list
*           cps_color_count           — query colour count
*           cps_generate              — fill via user generator
*         Selection                   
*           cps_select_at             — select by flat index
*           cps_select_at_grid        — select by (row, col)
*           cps_clear_selection       — set to no_selection
*           cps_has_selection         — query whether a swatch is picked
*           cps_grid_position         — query (row, col) of selection
*           cps_color_at              — bounded color access by index
*           cps_color_at_grid         — bounded color access by (row, col)
*           cps_selected_color        — color of current selection (or null)
*         Layout                      
*           cps_set_shape             — switch swatch shape
*           cps_set_swatch_size       — swatch dimensions
*           cps_set_spacing           — per-axis spacing
*
*
* path:      /inc/uxoxo/templates/component/color/color_palette_selector.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.25
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_COLOR_PALETTE_SELECTOR_
#define  UXOXO_COMPONENT_COLOR_PALETTE_SELECTOR_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/util/color/color.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../component/component_mixin.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  SWATCH SHAPE
// ===============================================================================

// DSwatchShape
//   enum: visual shape of each swatch in the palette.
//     square          — sharp-cornered rectangle (uses
//                       swatch_width × swatch_height).
//     rounded_square  — rectangle with renderer-defined corner
//                       radius.
//     circle          — circle / ellipse fitted to the swatch box.
//     hexagon         — flat-top regular hexagon fitted to the
//                       swatch box.  Used by honeycomb / pill
//                       palettes.
//     custom          — renderer-defined.  The model carries no
//                       extra data for custom shapes; if richer
//                       shape data is needed, it's the renderer's
//                       responsibility to look it up by other
//                       means.
enum class DSwatchShape : std::uint8_t
{
    square         = 0,
    rounded_square = 1,
    circle         = 2,
    hexagon        = 3,
    custom         = 4
};

// ===============================================================================
//  2.  COLOR PALETTE SELECTOR
// ===============================================================================

// color_palette_selector
//   struct template: a grid of fixed colour swatches with single
// selection.  The selected swatch is identified by a flat row-major
// index stored in `value`; the special sentinel
// color_palette_selector::no_selection means "nothing currently
// picked".
//
//   Template parameters:
//     _Color      — type used for individual swatches.  Defaults
//                   to djinterp::color::rgb.  Any default-
//                   constructible, copyable type works; the
//                   palette never reads any channel members.
//     _Labeled    — if true, mixes in a `label` string ("Recent
//                   colours", "Material 500", whatever).
//                   Defaults to true.
//     _Clearable  — if true, mixes in `default_value` of type
//                   std::size_t; clear() resets `value` to it.
//                   Useful for "reset to first swatch" or
//                   "reset to no selection" via setting
//                   default_value = no_selection.
//     _Undoable   — if true, mixes in `previous_value` +
//                   `has_previous` so undo() reverts the last
//                   selection.
//     _Copyable   — if true, mixes in `copy_requested` so
//                   request_copy() can flag the current
//                   selection for clipboard export.  The
//                   renderer is responsible for resolving the
//                   index back to a colour at copy time.
//
//   The colour grid is row-major: cell (r, c) lives at
// colors[r * cols + c].  If colors.size() < rows * cols, trailing
// cells are unfilled — the renderer should draw them as empty
// (or skip them).  If colors.size() > rows * cols, the extras
// exist in the buffer but are not displayed; cps_generate and
// cps_set_colors_2d both keep size and dimensions in sync, so
// this only happens when the user manipulates members directly.
template <typename _Color     = djinterp::color::rgb,
          bool     _Labeled   = true,
          bool     _Clearable = false,
          bool     _Undoable  = false,
          bool     _Copyable  = false>
struct color_palette_selector
    : component_mixin::label_data<_Labeled>,
      component_mixin::clearable_data<_Clearable, std::size_t>,
      component_mixin::undo_data<_Undoable, std::size_t>,
      component_mixin::copyable_data<_Copyable>
{
    using color_type = _Color;
    using value_type = std::size_t;

    // no_selection
    //   constant: sentinel value used for `value` when no swatch
    // is currently selected.
    static constexpr std::size_t no_selection =
        static_cast<std::size_t>(-1);

    // colors
    //   member: flat row-major buffer of swatch colours.
    // colors[r * cols + c] is the colour at grid position (r, c).
    std::vector<_Color> colors;

    // rows, cols
    //   members: layout dimensions.  Defaults are rows=1, cols=0
    // (an empty horizontal strip).  Set via cps_set_dimensions or
    // by the cps_set_colors_1d / cps_set_colors_2d / cps_generate
    // convenience operations.
    std::size_t rows = 1;
    std::size_t cols = 0;

    // value
    //   member: flat index of the currently selected swatch, or
    // no_selection.  The shared get_value / set_value / clear /
    // undo all act on this index.
    std::size_t value = no_selection;

    // shape
    //   member: visual shape of each swatch.  See DSwatchShape.
    DSwatchShape shape = DSwatchShape::square;

    // swatch_width, swatch_height
    //   members: dimensions of an individual swatch in renderer
    // units.  Zero on either axis means "renderer chooses"
    // (typically based on container width / a content-fit
    // heuristic).
    int swatch_width  = 0;
    int swatch_height = 0;

    // spacing_x, spacing_y
    //   members: gap between adjacent swatches along each axis,
    // in renderer units.  Zero means a tight grid (the picture's
    // central Grid tab); non-zero gives a separated strip (the
    // picture's bottom favourites row).  The two axes are
    // independent so a 1D row of circles can have horizontal
    // spacing without affecting an unused vertical dimension.
    int spacing_x = 0;
    int spacing_y = 0;

    // enabled
    //   member: when false, the renderer should ignore user input
    // and the shared commit() is a no-op.
    bool enabled = true;

    // visible
    //   member: whether the palette's UI is shown.
    bool visible = true;

    // read_only
    //   member: when true, swatches still render but are not
    // interactable; commit() is a no-op.  Useful for "preview-
    // only" palettes (e.g. showing a colour scheme without
    // letting the user pick from it).
    bool read_only = false;

    // on_change
    //   callback: invoked by the shared set_value (and therefore
    // by cps_select_at and cps_select_at_grid, which route
    // through it) whenever the selection changes.  Receives the
    // new index; the consumer looks up the colour via
    // palette.colors[index] if needed.  The index may be
    // no_selection, signalling that the selection was cleared.
    std::function<void(const std::size_t&)> on_change;

    // on_commit
    //   callback: invoked by the shared commit() when the user
    // finalizes a selection (typically a click or Enter).
    std::function<void(const std::size_t&)> on_commit;
};

// ===============================================================================
//  3.  OPERATIONS
// ===============================================================================
//   Palette-specific operations.  Shared component operations from
// component_common.hpp (enable / disable / show / hide / set_value /
// get_value / clear / undo / request_copy / commit / set_read_only)
// work on the palette via structural conformance — no wrapping
// needed.

NS_INTERNAL
    // clamp_selection
    //   helper: ensures `_v` is either a valid index into a
    // buffer of size `_n` or the no_selection sentinel.  Used
    // after operations that change colors.size() to keep the
    // selection consistent.
    template <typename _Selector>
    D_INLINE void
    clamp_selection(
        _Selector& _p
    )
    {
        if (_p.value == _Selector::no_selection)
        {
            return;
        }

        if (_p.value >= _p.colors.size())
        {
            _p.value = _Selector::no_selection;
        }

        return;
    }

NS_END   // internal


// ----- POPULATION ---------------------------------------------------------

// cps_set_colors
//   function: replaces the palette's colour list.  Does NOT
// modify rows or cols — the caller is responsible for keeping
// dimensions consistent (or use cps_set_colors_1d /
// cps_set_colors_2d, which do it for you).  Selection is
// reset to no_selection if it would otherwise point past the
// end of the new buffer.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_set_colors(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    std::vector<_Color>                _colors
)
{
    _p.colors = std::move(_colors);

    internal::clamp_selection(_p);

    return;
}

// cps_set_colors_1d
//   function: replaces the colour list and configures the
// palette as a single horizontal strip — rows = 1,
// cols = colors.size().  This is the convenient "favourites
// row" setter.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_set_colors_1d(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    std::vector<_Color>                _colors
)
{
    _p.cols   = _colors.size();
    _p.rows   = 1;
    _p.colors = std::move(_colors);

    internal::clamp_selection(_p);

    return;
}

// cps_set_colors_2d
//   function: replaces the colour list and sets rows / cols
// explicitly.  No requirement that rows * cols == colors.size():
// if smaller, trailing cells are empty; if larger, extras are
// not displayed.  Most callers want them equal.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
cps_set_colors_2d(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    std::vector<_Color>                _colors,
    std::size_t                        _rows,
    std::size_t                        _cols
)
{
    _p.rows   = _rows;
    _p.cols   = _cols;
    _p.colors = std::move(_colors);

    internal::clamp_selection(_p);

    return;
}

// cps_set_dimensions
//   function: changes rows and cols without touching the colour
// list.  Selection is preserved if still valid.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
cps_set_dimensions(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    std::size_t                        _rows,
    std::size_t                        _cols
)
{
    _p.rows = _rows;
    _p.cols = _cols;

    return;
}

// cps_append_color
//   function: pushes a colour onto the end of the colour list.
// Does NOT change rows / cols.  Useful for "favourites" and
// "recent colours" patterns where the user accumulates
// swatches over time.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
cps_append_color(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    _Color                              _color
)
{
    _p.colors.push_back(std::move(_color));

    return;
}

// cps_remove_color_at
//   function: erases the colour at the given flat index.  No-op
// if out of range.  Does NOT change rows / cols (so the strip
// "shrinks" by leaving a trailing empty cell).  Selection is
// adjusted: if it was the removed swatch, becomes no_selection;
// if it was after the removed swatch, decrements by one.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void 
cps_remove_color_at(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    std::size_t                        _index
)
{
    using selector_t = color_palette_selector<_Color,
                                              _Labeled,
                                              _Clearable,
                                              _Undoable,
                                              _Copyable>;

    if (_index >= _p.colors.size())
    {
        return;
    }

    _p.colors.erase(_p.colors.begin() +
                    static_cast<std::ptrdiff_t>(_index));

    if (_p.value == selector_t::no_selection)
    {
        return;
    }

    if (_p.value == _index)
    {
        _p.value = selector_t::no_selection;
    }
    else if (_p.value > _index)
    {
        --_p.value;
    }

    return;
}

// cps_clear_colors
//   function: empties the colour list and resets selection.
// Distinct from the shared clear(), which resets `value` to
// default_value but does NOT empty the buffer.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_clear_colors(
    color_palette_selector<_Color,
    _Labeled,
    _Clearable,
    _Undoable,
    _Copyable>& _p
)
{
    using selector_t = color_palette_selector<_Color,
                                              _Labeled,
                                              _Clearable,
                                              _Undoable,
                                              _Copyable>;

    _p.colors.clear();
    _p.value = selector_t::no_selection;

    return;
}

// cps_color_count
//   function: returns the number of colours currently stored.
// May differ from rows * cols.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD std::size_t
cps_color_count(
    const color_palette_selector<_Color,
                                 _Labeled,
                                 _Clearable,
                                 _Undoable,
                                 _Copyable>& _p
) noexcept
{
    return _p.colors.size();
}

// cps_generate
//   function: fills the palette using a caller-provided generator.
// Sets rows and cols, then evaluates the generator at every cell
// in row-major order:
//
//     for r in [0, rows):
//         for c in [0, cols):
//             colors.push_back( generator(r, c, rows, cols) );
//
// The generator owns all colour math, which is what keeps the
// palette colour-space agnostic.  Common patterns:
//
//   - Linear ramp on an 8-bit channel (1D, integer interp):
//        cps_generate(p, 1, 16, [](auto r, auto c, auto, auto cols) {
//            unsigned v = static_cast<unsigned>(c * 255 / (cols - 1));
//            return rgb::from_8bit(v, v, v);
//        });
//
//   - Hue wheel walk (1D, sampler-style):
//        cps_generate(p, 1, 360, [](auto, auto c, auto, auto) {
//            return hsl(static_cast<channel_t>(c), 1.0, 0.5);
//        });
//
//   - 2D bilinear blend, lookup table, etc. — all the user's
//     responsibility.
//
// Selection is reset to no_selection unless it remains in range
// of the new colour count.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable,
          typename _Gen>
void
cps_generate(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    std::size_t                        _rows,
    std::size_t                        _cols,
    _Gen                               _gen
)
{
    _p.rows = _rows;
    _p.cols = _cols;

    _p.colors.clear();
    _p.colors.reserve(_rows * _cols);

    for (std::size_t r = 0; r < _rows; ++r)
    {
        for (std::size_t c = 0; c < _cols; ++c)
        {
            _p.colors.push_back(_gen(r, c, _rows, _cols));
        }
    }

    internal::clamp_selection(_p);

    return;
}

// cps_select_at
//   function: selects the swatch at the given flat index.
// Routes through the shared set_value, so on_change fires and
// undo state (when _Undoable) is recorded.  No-op if the
// palette is disabled or read-only; if the index is out of
// range, selection becomes no_selection.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_select_at(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    std::size_t                        _index
)
{
    using selector_t = color_palette_selector<_Color,
                                              _Labeled,
                                              _Clearable,
                                              _Undoable,
                                              _Copyable>;

    if ( (!_p.enabled) || 
         (_p.read_only) )
    {
        return;
    }

    const std::size_t next =
        (_index < _p.colors.size())
            ? _index
            : selector_t::no_selection;

    set_value(_p, next);

    return;
}

// cps_select_at_grid
//   function: selects the swatch at grid position (row, col).
// Computes the flat index as row * cols + col and routes
// through cps_select_at.  No-op if (row, col) is outside the
// rows × cols region.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_select_at_grid(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    std::size_t                        _row,
    std::size_t                        _col
)
{
    if ( (_row >= _p.rows) || 
         (_col >= _p.cols) )
    {
        return;
    }

    cps_select_at(_p, _row * _p.cols + _col);

    return;
}

// cps_clear_selection
//   function: deselects.  Routes through set_value so on_change
// fires.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_clear_selection(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p
)
{                          
    using selector_t = color_palette_selector<_Color,
                                              _Labeled,
                                              _Clearable,
                                              _Undoable,
                                              _Copyable>;

    set_value(_p, selector_t::no_selection);

    return;
}

// cps_has_selection
//   function: returns true iff a swatch is currently selected
// (i.e. value != no_selection AND value < colors.size()).  The
// second clause guards against stale selections after direct
// member manipulation.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD bool
cps_has_selection(
    const color_palette_selector<_Color,
                                 _Labeled,
                                 _Clearable,
                                 _Undoable,
                                 _Copyable>& _p
) noexcept
{
    using selector_t = color_palette_selector<_Color,
                                              _Labeled,
                                              _Clearable,
                                              _Undoable,
                                              _Copyable>;

    return ( _p.value != selector_t::no_selection &&
             _p.value <  _p.colors.size() );
}

// cps_grid_position
//   function: returns the (row, col) of the current selection,
// or (0, 0) if no selection exists.  Callers should check
// cps_has_selection first to disambiguate "no selection" from
// "selection at (0, 0)".
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD std::pair<std::size_t, std::size_t>
cps_grid_position(
    const color_palette_selector<_Color,
                                 _Labeled,
                                 _Clearable,
                                 _Undoable,
                                 _Copyable>& _p
) noexcept
{
    if ( (!cps_has_selection(_p)) || 
         (_p.cols == 0) )
    {
        return std::make_pair(std::size_t(0), std::size_t(0));
    }

    return std::make_pair(_p.value / _p.cols,
                          _p.value % _p.cols);
}

// cps_color_at
//   function: returns a pointer to the colour at the given flat
// index, or nullptr if out of range.  Pointer rather than
// reference so the out-of-range case is unambiguous; lifetime
// matches the palette's colors vector.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD const _Color*
cps_color_at(
    const color_palette_selector<_Color,
                                 _Labeled,
                                 _Clearable,
                                 _Undoable,
                                 _Copyable>& _p,
    std::size_t                              _index
) noexcept
{
    if (_index >= _p.colors.size())
    {
        return nullptr;
    }

    return &_p.colors[_index];
}

// cps_color_at_grid
//   function: returns a pointer to the colour at grid position
// (row, col), or nullptr if outside the rows × cols region or
// past colors.size().
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD const _Color*
cps_color_at_grid(
    const color_palette_selector<_Color,
                                 _Labeled,
                                 _Clearable,
                                 _Undoable,
                                 _Copyable>& _p,
    std::size_t                              _row,
    std::size_t                              _col) noexcept
{
    if ( (_row >= _p.rows) || 
         (_col >= _p.cols) )
    {
        return nullptr;
    }

    return cps_color_at(_p, (_row * _p.cols) + _col);
}

// cps_selected_color
//   function: returns a pointer to the currently selected
// colour, or nullptr if cps_has_selection is false.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
D_NODISCARD const _Color*
cps_selected_color(
    const color_palette_selector<_Color,
                                 _Labeled,
                                 _Clearable,
                                 _Undoable,
                                 _Copyable>& _p
) noexcept
{
    if (!cps_has_selection(_p))
    {
        return nullptr;
    }

    return &_p.colors[_p.value];
}

// cps_set_shape
//   function: switches swatch shape.  Affects rendering only.
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_set_shape(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    DSwatchShape                       _shape
)
{
    _p.shape = _shape;

    return;
}

// cps_set_swatch_size
//   function: sets per-swatch dimensions.  Negative inputs are
// treated as zero (the renderer's "auto" sentinel).
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_set_swatch_size(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    int                                _w,
    int                                _h
)
{
    _p.swatch_width  = (_w < 0) ? 0 : _w;
    _p.swatch_height = (_h < 0) ? 0 : _h;

    return;
}

// cps_set_spacing
//   function: sets per-axis spacing between swatches.  Negative
// inputs are treated as zero (a tight grid).
template <typename _Color,
          bool     _Labeled,
          bool     _Clearable,
          bool     _Undoable,
          bool     _Copyable>
void
cps_set_spacing(
    color_palette_selector<_Color,
                           _Labeled,
                           _Clearable,
                           _Undoable,
                           _Copyable>& _p,
    int                                _x,
    int                                _y
)
{
    _p.spacing_x = (_x < 0) ? 0 : _x;
    _p.spacing_y = (_y < 0) ? 0 : _y;

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_COLOR_PALETTE_SELECTOR_