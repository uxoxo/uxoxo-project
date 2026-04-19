/*******************************************************************************
* uxoxo [component]                                            text_output.hpp
*
* Read-only text output:
*   A framework-agnostic, pure-data text output component.  This is the
* read-only counterpart to text_input — where text_input accepts
* keystrokes, text_output displays a stream of text lines.  Typical uses:
* log windows, terminal output panes, status displays, chat histories.
*
*   Zero-cost features via EBO (separate flag space from text_input):
*     tof_line_buffer     line-oriented with bounded history eviction
*     tof_color           per-line color/style tag
*     tof_selectable      text selection for copy
*     tof_timestamps      per-line timestamp
*     tof_filterable      runtime line filter (e.g. log level)
*
*   text_output is a scrollable, read-only aggregate.  All mutation is
* via free functions prefixed `to_`.  The struct prescribes no rendering.
*
* Contents:
*   1  Feature flags (text_output_feat)
*   2  Line entry struct
*   3  EBO mixins
*   4  text_output struct
*   5  Free functions (append, clear, scroll)
*   6  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/output/text_output.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_TEXT_OUTPUT_
#define  UXOXO_COMPONENT_TEXT_OUTPUT_ 1

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../view_common.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  TEXT OUTPUT FEATURE FLAGS
// ===============================================================================
//   Start at bit 8 to avoid colliding with view_feat (0–7).

enum text_output_feat : unsigned
{
    tof_none        = 0,
    tof_line_buffer = 1u << 8,      // bounded line eviction
    tof_color       = 1u << 9,      // per-line color tag
    tof_selectable  = 1u << 10,     // text selection for copy
    tof_timestamps  = 1u << 11,     // per-line timestamp
    tof_filterable  = 1u << 12,     // runtime filter predicate

    tof_all         = tof_line_buffer | tof_color | tof_selectable
                    | tof_timestamps  | tof_filterable
};

constexpr unsigned operator|(text_output_feat _a,
                             text_output_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool has_tof(unsigned          _f,
                       text_output_feat  _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


/*****************************************************************************/

// ===============================================================================
//  2  LINE ENTRY
// ===============================================================================
//   A single line in the output buffer.  Optional fields (color,
// timestamp) are composed via EBO mixins on the line entry itself.

// output_color_tag
//   enum: semantic color category for a line.  The renderer maps
// these to actual colors.
enum class output_color_tag : std::uint8_t
{
    normal,
    info,
    warning,
    error,
    debug,
    success,
    muted,
    highlight,
    custom          // renderer uses custom_color fields
};

namespace output_line_mixin {

    // -- color --------------------------------------------------------
    template <bool _Enable>
    struct color_data
    {};

    template <>
    struct color_data<true>
    {
        output_color_tag color_tag = output_color_tag::normal;

        // custom RGBA (used when color_tag == custom)
        float custom_r = 1.0f;
        float custom_g = 1.0f;
        float custom_b = 1.0f;
        float custom_a = 1.0f;
    };

    // -- timestamp ----------------------------------------------------
    template <bool _Enable>
    struct timestamp_data
    {};

    template <>
    struct timestamp_data<true>
    {
        std::chrono::steady_clock::time_point timestamp =
            std::chrono::steady_clock::now();
    };

}   // namespace output_line_mixin


// output_line
//   struct: a single line in the text_output buffer with optional
// color and timestamp data.
template <unsigned _Feat = tof_none>
struct output_line
    : output_line_mixin::color_data     <has_tof(_Feat, tof_color)>
    , output_line_mixin::timestamp_data <has_tof(_Feat, tof_timestamps)>
{
    static constexpr unsigned features       = _Feat;
    static constexpr bool     has_color      = has_tof(_Feat, tof_color);
    static constexpr bool     has_timestamps = has_tof(_Feat, tof_timestamps);

    std::string text;

    output_line() = default;

    explicit output_line(
            std::string _text
        )
            : text(std::move(_text))
        {}
};


/*****************************************************************************/

// ===============================================================================
//  3  TEXT OUTPUT EBO MIXINS
// ===============================================================================

namespace text_output_mixin {

    // -- line buffer --------------------------------------------------
    template <bool _Enable>
    struct line_buffer_data
    {};

    template <>
    struct line_buffer_data<true>
    {
        std::size_t max_lines = 4096;
    };

    // -- selectable ---------------------------------------------------
    template <bool _Enable>
    struct selectable_data
    {};

    template <>
    struct selectable_data<true>
    {
        bool        has_selection  = false;
        std::size_t sel_line_start = 0;
        std::size_t sel_col_start  = 0;
        std::size_t sel_line_end   = 0;
        std::size_t sel_col_end    = 0;
    };

    // -- filterable ---------------------------------------------------
    template <bool _Enable>
    struct filterable_data
    {};

    template <>
    struct filterable_data<true>
    {
        // the filter predicate type is left as std::function so
        // the user can capture any state.  Returns true to show
        // the line, false to hide it.
        using filter_fn = std::function<bool(const std::string&)>;

        filter_fn   filter;
        bool        filter_active = false;
    };

}   // namespace text_output_mixin


/*****************************************************************************/

// ===============================================================================
//  4  TEXT OUTPUT
// ===============================================================================

template <unsigned _Feat = tof_none>
struct text_output
    : text_output_mixin::line_buffer_data <has_tof(_Feat, tof_line_buffer)>
    , text_output_mixin::selectable_data  <has_tof(_Feat, tof_selectable)>
    , text_output_mixin::filterable_data  <has_tof(_Feat, tof_filterable)>
{
    using line_type = output_line<_Feat>;

    static constexpr unsigned features       = _Feat;
    static constexpr bool has_line_buffer    = has_tof(_Feat, tof_line_buffer);
    static constexpr bool has_color          = has_tof(_Feat, tof_color);
    static constexpr bool is_selectable      = has_tof(_Feat, tof_selectable);
    static constexpr bool has_timestamps     = has_tof(_Feat, tof_timestamps);
    static constexpr bool is_filterable      = has_tof(_Feat, tof_filterable);

    static constexpr bool focusable  = false;
    static constexpr bool scrollable = true;

    // -- lines --------------------------------------------------------
    std::vector<line_type> lines;

    // -- scroll -------------------------------------------------------
    std::size_t scroll_offset = 0;
    std::size_t page_size     = 20;
    bool        auto_scroll   = true;

    // -- visibility ---------------------------------------------------
    bool visible = true;
    bool enabled = true;

    // -- queries ------------------------------------------------------
    [[nodiscard]] std::size_t
    line_count() const noexcept
    {
        return lines.size();
    }

    [[nodiscard]] bool
    empty() const noexcept
    {
        return lines.empty();
    }
};


/*****************************************************************************/

// ===============================================================================
//  5  FREE FUNCTIONS
// ===============================================================================

// to_append
//   function: appends a line to the output.  If tof_line_buffer is
// enabled and the buffer is full, the oldest line is evicted.
template <unsigned _F>
void to_append(text_output<_F>& _to,
               std::string      _text)
{
    // evict oldest if at capacity
    if constexpr (has_tof(_F, tof_line_buffer))
    {
        if (_to.lines.size() >= _to.max_lines)
        {
            _to.lines.erase(_to.lines.begin());

            // adjust scroll offset
            if (_to.scroll_offset > 0)
            {
                --_to.scroll_offset;
            }
        }
    }

    _to.lines.emplace_back(std::move(_text));

    // auto-scroll to bottom
    if (_to.auto_scroll)
    {
        if (_to.lines.size() > _to.page_size)
        {
            _to.scroll_offset = _to.lines.size() - _to.page_size;
        }
    }

    return;
}

// to_append_colored
//   function: appends a line with a color tag (requires tof_color).
template <unsigned _F>
void to_append_colored(text_output<_F>& _to,
                       std::string      _text,
                       output_color_tag _color)
{
    static_assert(has_tof(_F, tof_color),
                  "requires tof_color");

    // evict if needed
    if constexpr (has_tof(_F, tof_line_buffer))
    {
        if (_to.lines.size() >= _to.max_lines)
        {
            _to.lines.erase(_to.lines.begin());

            if (_to.scroll_offset > 0)
            {
                --_to.scroll_offset;
            }
        }
    }

    typename text_output<_F>::line_type line(std::move(_text));
    line.color_tag = _color;

    _to.lines.push_back(std::move(line));

    if (_to.auto_scroll)
    {
        if (_to.lines.size() > _to.page_size)
        {
            _to.scroll_offset = _to.lines.size() - _to.page_size;
        }
    }

    return;
}

// to_clear
//   function: removes all lines and resets scroll.
template <unsigned _F>
void to_clear(text_output<_F>& _to)
{
    _to.lines.clear();
    _to.scroll_offset = 0;

    if constexpr (has_tof(_F, tof_selectable))
    {
        _to.has_selection = false;
    }

    return;
}

// to_scroll_to_bottom
//   function: scrolls to the most recent line.
template <unsigned _F>
void to_scroll_to_bottom(text_output<_F>& _to)
{
    if (_to.lines.size() > _to.page_size)
    {
        _to.scroll_offset = _to.lines.size() - _to.page_size;
    }
    else
    {
        _to.scroll_offset = 0;
    }

    return;
}

// to_scroll_to_top
//   function: scrolls to the oldest line.
template <unsigned _F>
void to_scroll_to_top(text_output<_F>& _to)
{
    _to.scroll_offset = 0;

    return;
}

// to_scroll_up
//   function: scrolls up by one page.
template <unsigned _F>
void to_scroll_up(text_output<_F>& _to)
{
    _to.scroll_offset =
        (_to.scroll_offset > _to.page_size)
            ? _to.scroll_offset - _to.page_size
            : 0;

    return;
}

// to_scroll_down
//   function: scrolls down by one page.
template <unsigned _F>
void to_scroll_down(text_output<_F>& _to)
{
    std::size_t max_offset =
        (_to.lines.size() > _to.page_size)
            ? _to.lines.size() - _to.page_size
            : 0;

    _to.scroll_offset = std::min(
        _to.scroll_offset + _to.page_size,
        max_offset);

    return;
}

// to_visible_range
//   function: returns the visible line range as [start, end).
template <unsigned _F>
std::pair<std::size_t, std::size_t>
to_visible_range(const text_output<_F>& _to)
{
    std::size_t start = _to.scroll_offset;
    std::size_t end   = std::min(
        _to.scroll_offset + _to.page_size,
        _to.lines.size());

    return { start, end };
}

// to_set_filter
//   function: sets the filter predicate (requires tof_filterable).
template <unsigned _F>
void to_set_filter(
    text_output<_F>&                                            _to,
    typename text_output_mixin::filterable_data<true>::filter_fn _fn)
{
    static_assert(has_tof(_F, tof_filterable),
                  "requires tof_filterable");

    _to.filter        = std::move(_fn);
    _to.filter_active = static_cast<bool>(_to.filter);

    return;
}

// to_clear_filter
//   function: removes the active filter (requires tof_filterable).
template <unsigned _F>
void to_clear_filter(text_output<_F>& _to)
{
    static_assert(has_tof(_F, tof_filterable),
                  "requires tof_filterable");

    _to.filter        = nullptr;
    _to.filter_active = false;

    return;
}

// to_select_all
//   function: selects all text (requires tof_selectable).
template <unsigned _F>
void to_select_all(text_output<_F>& _to)
{
    static_assert(has_tof(_F, tof_selectable),
                  "requires tof_selectable");

    if (_to.lines.empty())
    {
        return;
    }

    _to.has_selection  = true;
    _to.sel_line_start = 0;
    _to.sel_col_start  = 0;
    _to.sel_line_end   = _to.lines.size() - 1;
    _to.sel_col_end    = _to.lines.back().text.size();

    return;
}

// to_clear_selection
//   function: clears the text selection (requires tof_selectable).
template <unsigned _F>
void to_clear_selection(text_output<_F>& _to)
{
    static_assert(has_tof(_F, tof_selectable),
                  "requires tof_selectable");

    _to.has_selection = false;

    return;
}

// to_selected_text
//   function: returns the selected text as a single string
// (requires tof_selectable).
template <unsigned _F>
std::string to_selected_text(const text_output<_F>& _to)
{
    static_assert(has_tof(_F, tof_selectable),
                  "requires tof_selectable");

    if (!_to.has_selection)
    {
        return {};
    }

    std::string result;

    for (std::size_t i = _to.sel_line_start;
         i <= _to.sel_line_end && i < _to.lines.size();
         ++i)
    {
        const auto& line = _to.lines[i].text;
        std::size_t col_start = (i == _to.sel_line_start)
            ? _to.sel_col_start : 0;
        std::size_t col_end   = (i == _to.sel_line_end)
            ? _to.sel_col_end : line.size();

        col_start = std::min(col_start, line.size());
        col_end   = std::min(col_end,   line.size());

        if (col_end > col_start)
        {
            result += line.substr(col_start,
                                  col_end - col_start);
        }

        if (i < _to.sel_line_end)
        {
            result += '\n';
        }
    }

    return result;
}


/*****************************************************************************/

// ===============================================================================
//  6  TRAITS
// ===============================================================================

namespace text_output_traits {
namespace detail {

    template <typename, typename = void>
    struct has_lines_member : std::false_type {};
    template <typename _Type>
    struct has_lines_member<_Type, std::void_t<
        decltype(std::declval<_Type>().lines)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_scroll_offset_member : std::false_type {};
    template <typename _Type>
    struct has_scroll_offset_member<_Type, std::void_t<
        decltype(std::declval<_Type>().scroll_offset)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_auto_scroll_member : std::false_type {};
    template <typename _Type>
    struct has_auto_scroll_member<_Type, std::void_t<
        decltype(std::declval<_Type>().auto_scroll)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_max_lines_member : std::false_type {};
    template <typename _Type>
    struct has_max_lines_member<_Type, std::void_t<
        decltype(std::declval<_Type>().max_lines)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_filter_member : std::false_type {};
    template <typename _Type>
    struct has_filter_member<_Type, std::void_t<
        decltype(std::declval<_Type>().filter)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_has_selection_member : std::false_type {};
    template <typename _Type>
    struct has_has_selection_member<_Type, std::void_t<
        decltype(std::declval<_Type>().has_selection)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_scrollable_flag : std::false_type {};
    template <typename _Type>
    struct has_scrollable_flag<_Type,
                               std::enable_if_t<_Type::scrollable>>
        : std::true_type {};

}   // namespace detail

template <typename _Type>
inline constexpr bool has_lines_v =
    detail::has_lines_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_scroll_offset_v =
    detail::has_scroll_offset_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_auto_scroll_v =
    detail::has_auto_scroll_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_max_lines_v =
    detail::has_max_lines_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_filter_v =
    detail::has_filter_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_selection_v =
    detail::has_has_selection_member<_Type>::value;
template <typename _Type>
inline constexpr bool is_scrollable_v =
    detail::has_scrollable_flag<_Type>::value;

// is_text_output
//   type trait: has lines + scroll_offset + auto_scroll + scrollable.
template <typename _Type>
struct is_text_output : std::conjunction<
    detail::has_lines_member<_Type>,
    detail::has_scroll_offset_member<_Type>,
    detail::has_auto_scroll_member<_Type>,
    detail::has_scrollable_flag<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_text_output_v =
    is_text_output<_Type>::value;

// is_buffered_text_output
template <typename _Type>
struct is_buffered_text_output : std::conjunction<
    is_text_output<_Type>,
    detail::has_max_lines_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_buffered_text_output_v =
    is_buffered_text_output<_Type>::value;

// is_selectable_text_output
template <typename _Type>
struct is_selectable_text_output : std::conjunction<
    is_text_output<_Type>,
    detail::has_has_selection_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_selectable_text_output_v =
    is_selectable_text_output<_Type>::value;

// is_filterable_text_output
template <typename _Type>
struct is_filterable_text_output : std::conjunction<
    is_text_output<_Type>,
    detail::has_filter_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_filterable_text_output_v =
    is_filterable_text_output<_Type>::value;

}   // namespace text_output_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_TEXT_OUTPUT_
