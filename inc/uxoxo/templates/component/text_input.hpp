/*******************************************************************************
* uxoxo [component]                                              text_input.hpp
*
*   Single-line and multi-line editable text input.  Pure data aggregate —
* no rendering, no observer coupling.  All editing is via free functions so
* the struct stays a plain data description.
*
*   Zero-cost features via EBO (same bitfield as tree/list):
*     vf_checkable   --> ignored (meaningless for text input)
*     vf_icons       --> prefix/suffix icon
*     vf_context     --> per-input context action bitfield
*     vf_renamable   --> ignored (the entire point IS editing)
*     vf_collapsible --> ignored
*
*   text_input_feat adds text-specific feature flags that do not overlap
*   with view_feat, starting at bit 8:
*     tif_multiline    multi-line with vertical scroll
*     tif_history      command-line history ring
*     tif_validation   per-input validators
*     tif_masked       password dot display
*
*
* path:      /inc/uxoxo/component/text_input.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.26
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_TEXT_INPUT_
#define  UXOXO_COMPONENT_TEXT_INPUT_ 1

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <uxoxo>
#include <component/view_common.hpp>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  TEXT-SPECIFIC FEATURE FLAGS
// ═══════════════════════════════════════════════════════════════════════════════
//   Start at bit 8 to avoid colliding with view_feat.

enum text_input_feat : unsigned
{
    tif_none        = 0,
    tif_multiline   = 1u << 8,
    tif_history     = 1u << 9,
    tif_validation  = 1u << 10,
    tif_masked      = 1u << 11,

    tif_all         = tif_multiline | tif_history | tif_validation | tif_masked
};

constexpr unsigned operator|(text_input_feat a, text_input_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}
constexpr unsigned operator|(view_feat a, text_input_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}
constexpr unsigned operator|(text_input_feat a, view_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_tif(unsigned f, text_input_feat bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  VALIDATION
// ═══════════════════════════════════════════════════════════════════════════════

enum class validation_result : std::uint8_t
{
    valid,
    warning,        // accept but show warning indicator
    error           // reject input
};

struct validation_report
{
    validation_result result  = validation_result::valid;
    std::string       message;
};

using validator_fn = std::function<validation_report(const std::string& value)>;


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  TEXT-SPECIFIC EBO MIXINS
// ═══════════════════════════════════════════════════════════════════════════════

namespace text_mixin {

    // ── multiline state ──────────────────────────────────────────────────
    template <bool _Enable>
    struct multiline_data {};

    template <>
    struct multiline_data<true>
    {
        std::size_t scroll_row    = 0;
        std::size_t scroll_col    = 0;
        std::size_t visible_rows  = 1;      // set by renderer
        std::size_t visible_cols  = 80;     // set by renderer
        bool        word_wrap     = true;
    };

    // ── history state ────────────────────────────────────────────────────
    template <bool _Enable>
    struct history_data {};

    template <>
    struct history_data<true>
    {
        std::vector<std::string> history;
        std::size_t              history_pos  = 0;
        std::size_t              history_max  = 256;
        std::string              saved_input;   // current input before browsing
    };

    // ── validation state ─────────────────────────────────────────────────
    template <bool _Enable>
    struct validation_data {};

    template <>
    struct validation_data<true>
    {
        std::vector<validator_fn> validators;
        validation_report         last_report;
        bool                      validate_on_change = true;
    };

    // ── masked state ─────────────────────────────────────────────────────
    template <bool _Enable>
    struct masked_data {};

    template <>
    struct masked_data<true>
    {
        bool  masked      = false;      // active masking
        char  mask_char   = '*';        // display character
        bool  show_last   = false;      // briefly show last typed char
    };

}   // namespace text_mixin


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  TEXT INPUT
// ═══════════════════════════════════════════════════════════════════════════════

template <unsigned _Feat = tif_none,
          typename _Icon = int>
struct text_input
    : entry_mixin::icon_data    <has_feat(_Feat, vf_icons), _Icon>
    , entry_mixin::context_data <has_feat(_Feat, vf_context)>
    , text_mixin::multiline_data  <has_tif(_Feat, tif_multiline)>
    , text_mixin::history_data    <has_tif(_Feat, tif_history)>
    , text_mixin::validation_data <has_tif(_Feat, tif_validation)>
    , text_mixin::masked_data     <has_tif(_Feat, tif_masked)>
{
    using icon_type = _Icon;

    static constexpr unsigned features = _Feat;
    static constexpr bool has_icons       = has_feat(_Feat, vf_icons);
    static constexpr bool has_context     = has_feat(_Feat, vf_context);
    static constexpr bool is_multiline    = has_tif(_Feat, tif_multiline);
    static constexpr bool has_history     = has_tif(_Feat, tif_history);
    static constexpr bool has_validation  = has_tif(_Feat, tif_validation);
    static constexpr bool is_masked_input = has_tif(_Feat, tif_masked);
    static constexpr bool focusable  = true;
    static constexpr bool scrollable = false;  // multiline sets its own scroll

    // ── data ─────────────────────────────────────────────────────────────
    std::string value;
    std::size_t cursor       = 0;       // byte position in value
    std::string placeholder;
    bool        enabled      = true;
    bool        read_only    = false;
    std::size_t max_length   = 0;       // 0 = unlimited

    // selection: anchor is the fixed end, cursor is the moving end
    std::size_t sel_anchor   = 0;
    bool        has_selection = false;

    // ── construction ─────────────────────────────────────────────────────
    text_input() = default;

    explicit text_input(std::string initial)
        : value(std::move(initial))
        , cursor(value.size())
    {}

    text_input(std::string initial, std::string ph)
        : value(std::move(initial))
        , cursor(value.size())
        , placeholder(std::move(ph))
    {}

    // ── queries ──────────────────────────────────────────────────────────
    [[nodiscard]] bool empty() const noexcept { return value.empty(); }
    [[nodiscard]] std::size_t length() const noexcept { return value.size(); }

    [[nodiscard]] bool at_start() const noexcept { return cursor == 0; }
    [[nodiscard]] bool at_end()   const noexcept { return cursor >= value.size(); }

    // selected_text
    //   Returns the selected substring, or empty if no selection.
    [[nodiscard]] std::string selected_text() const
    {
        if (!has_selection)
        {
            return {};
        }
        auto lo = std::min(sel_anchor, cursor);
        auto hi = std::max(sel_anchor, cursor);
        if (lo >= value.size())
        {
            return {};
        }
        hi = std::min(hi, value.size());
        return value.substr(lo, hi - lo);
    }

    [[nodiscard]] std::size_t selection_start() const
    {
        return has_selection ? std::min(sel_anchor, cursor) : cursor;
    }

    [[nodiscard]] std::size_t selection_end() const
    {
        return has_selection ? std::max(sel_anchor, cursor) : cursor;
    }

    [[nodiscard]] std::size_t selection_length() const
    {
        if (!has_selection)
        {
            return 0;
        }
        return std::max(sel_anchor, cursor) - std::min(sel_anchor, cursor);
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  CURSOR MOVEMENT  (free functions)
// ═══════════════════════════════════════════════════════════════════════════════

// ti_move_left
template <unsigned _F,
          typename _I>
bool ti_move_left(text_input<_F, _I>& ti, bool extend_sel = false)
{
    if (ti.cursor == 0)
    {
        return false;
    }

    if (extend_sel)
    {
        if (!ti.has_selection)
        {
            ti.sel_anchor = ti.cursor; ti.has_selection = true;
        }
    }
    else
    {
        if (ti.has_selection)
        {
            ti.cursor = ti.selection_start();
            ti.has_selection = false;
            return true;
        }
    }
    --ti.cursor;
    return true;
}

// ti_move_right
template <unsigned _F,
          typename _I>
bool ti_move_right(text_input<_F, _I>& ti, bool extend_sel = false)
{
    if (ti.cursor >= ti.value.size())
    {
        return false;
    }

    if (extend_sel)
    {
        if (!ti.has_selection)
        {
            ti.sel_anchor = ti.cursor; ti.has_selection = true;
        }
    }
    else
    {
        if (ti.has_selection)
        {
            ti.cursor = ti.selection_end();
            ti.has_selection = false;
            return true;
        }
    }
    ++ti.cursor;
    return true;
}

// ti_home
template <unsigned _F,
          typename _I>
bool ti_home(text_input<_F, _I>& ti, bool extend_sel = false)
{
    if (ti.cursor == 0)
    {
        return false;
    }
    if (extend_sel)
    {
        if (!ti.has_selection)
        {
            ti.sel_anchor = ti.cursor; ti.has_selection = true;
        }
    }
    else
    {
        ti.has_selection = false;
    }
    ti.cursor = 0;
    return true;
}

// ti_end
template <unsigned _F,
          typename _I>
bool ti_end(text_input<_F, _I>& ti, bool extend_sel = false)
{
    if (ti.cursor >= ti.value.size())
    {
        return false;
    }
    if (extend_sel)
    {
        if (!ti.has_selection)
        {
            ti.sel_anchor = ti.cursor; ti.has_selection = true;
        }
    }
    else
    {
        ti.has_selection = false;
    }
    ti.cursor = ti.value.size();
    return true;
}

// ti_move_word_left
//   Moves cursor to the beginning of the previous word boundary.
template <unsigned _F,
          typename _I>
bool ti_move_word_left(text_input<_F, _I>& ti, bool extend_sel = false)
{
    if (ti.cursor == 0)
    {
        return false;
    }
    if (extend_sel)
    {
        if (!ti.has_selection)
        {
            ti.sel_anchor = ti.cursor; ti.has_selection = true;
        }
    }
    else
    {
        ti.has_selection = false;
    }

    auto& v = ti.value;
    std::size_t pos = ti.cursor;

    // skip whitespace backwards
    while (pos > 0 && (v[pos - 1] == ' ' || v[pos - 1] == '\t'))
    {
        --pos;
    }
    // skip word chars backwards
    while (pos > 0 && v[pos - 1] != ' ' && v[pos - 1] != '\t')
    {
        --pos;
    }

    ti.cursor = pos;
    return true;
}

// ti_move_word_right
template <unsigned _F,
          typename _I>
bool ti_move_word_right(text_input<_F, _I>& ti, bool extend_sel = false)
{
    if (ti.cursor >= ti.value.size())
    {
        return false;
    }
    if (extend_sel)
    {
        if (!ti.has_selection)
        {
            ti.sel_anchor = ti.cursor; ti.has_selection = true;
        }
    }
    else
    {
        ti.has_selection = false;
    }

    auto& v = ti.value;
    std::size_t pos = ti.cursor;

    // skip word chars forward
    while (pos < v.size()
    {
        && v[pos] != ' ' && v[pos] != '\t')
    }
        ++pos;
    // skip whitespace forward
    while (pos < v.size()
    {
        && (v[pos] == ' ' || v[pos] == '\t'))
    }
        ++pos;

    ti.cursor = pos;
    return true;
}

// ti_select_all
template <unsigned _F,
          typename _I>
void ti_select_all(text_input<_F, _I>& ti)
{
    if (ti.value.empty())
    {
        return;
    }
    ti.sel_anchor    = 0;
    ti.cursor        = ti.value.size();
    ti.has_selection = true;
}

// ti_deselect
template <unsigned _F,
          typename _I>
void ti_deselect(text_input<_F, _I>& ti)
{
    ti.has_selection = false;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §6  EDITING  (free functions)
// ═══════════════════════════════════════════════════════════════════════════════

namespace detail_ti {
    // delete selected text, leave cursor at start of deleted range
    template <unsigned _F,
              typename _I>
    void delete_selection(text_input<_F, _I>& ti)
    {
        if (!ti.has_selection)
        {
            return;
        }
        auto lo = ti.selection_start();
        auto hi = ti.selection_end();
        hi = std::min(hi, ti.value.size());
        ti.value.erase(lo, hi - lo);
        ti.cursor = lo;
        ti.has_selection = false;
    }

    template <unsigned _F,
              typename _I>
    bool check_length(const text_input<_F, _I>& ti, std::size_t add)
    {
        if (ti.max_length == 0)
        {
            return true;
        }
        return ti.value.size() + add <= ti.max_length;
    }
}

// ti_insert
//   Inserts text at cursor.  Replaces selection if active.
template <unsigned _F,
          typename _I>
bool ti_insert(text_input<_F, _I>& ti, const std::string& text)
{
    if (ti.read_only || !ti.enabled)
    {
        return false;
    }
    if (text.empty())
    {
        return false;
    }

    // delete selection first (frees space for max_length check)
    detail_ti::delete_selection(ti);

    if (!detail_ti::check_length(ti, text.size()))
    {
        return false;
    }

    ti.cursor = std::min(ti.cursor, ti.value.size());
    ti.value.insert(ti.cursor, text);
    ti.cursor += text.size();
    return true;
}

// ti_insert_char
template <unsigned _F,
          typename _I>
bool ti_insert_char(text_input<_F, _I>& ti, char ch)
{
    if (ti.read_only || !ti.enabled)
    {
        return false;
    }
    detail_ti::delete_selection(ti);
    if (!detail_ti::check_length(ti, 1))
    {
        return false;
    }

    ti.cursor = std::min(ti.cursor, ti.value.size());
    ti.value.insert(ti.cursor, 1, ch);
    ++ti.cursor;
    return true;
}

// ti_backspace
template <unsigned _F,
          typename _I>
bool ti_backspace(text_input<_F, _I>& ti)
{
    if (ti.read_only || !ti.enabled)
    {
        return false;
    }
    if (ti.has_selection)
    {
        detail_ti::delete_selection(ti);
        return true;
    }
    if (ti.cursor == 0)
    {
        return false;
    }
    --ti.cursor;
    ti.value.erase(ti.cursor, 1);
    return true;
}

// ti_delete_forward
template <unsigned _F,
          typename _I>
bool ti_delete_forward(text_input<_F, _I>& ti)
{
    if (ti.read_only || !ti.enabled)
    {
        return false;
    }
    if (ti.has_selection)
    {
        detail_ti::delete_selection(ti);
        return true;
    }
    if (ti.cursor >= ti.value.size())
    {
        return false;
    }
    ti.value.erase(ti.cursor, 1);
    return true;
}

// ti_delete_word_back
template <unsigned _F,
          typename _I>
bool ti_delete_word_back(text_input<_F, _I>& ti)
{
    if (ti.read_only || !ti.enabled)
    {
        return false;
    }
    if (ti.has_selection)
    {
        detail_ti::delete_selection(ti); return true;
    }
    if (ti.cursor == 0)
    {
        return false;
    }

    std::size_t end = ti.cursor;
    // skip whitespace
    while (ti.cursor > 0 && (ti.value[ti.cursor - 1] == ' ' || ti.value[ti.cursor - 1] == '\t'))
    {
        --ti.cursor;
    }
    // skip word
    while (ti.cursor > 0 && ti.value[ti.cursor - 1] != ' ' && ti.value[ti.cursor - 1] != '\t')
    {
        --ti.cursor;
    }

    ti.value.erase(ti.cursor, end - ti.cursor);
    return true;
}

// ti_delete_word_forward
template <unsigned _F,
          typename _I>
bool ti_delete_word_forward(text_input<_F, _I>& ti)
{
    if (ti.read_only || !ti.enabled)
    {
        return false;
    }
    if (ti.has_selection)
    {
        detail_ti::delete_selection(ti); return true;
    }
    if (ti.cursor >= ti.value.size())
    {
        return false;
    }

    std::size_t start = ti.cursor;
    std::size_t pos = ti.cursor;
    while (pos < ti.value.size()
    {
        && ti.value[pos] != ' ' && ti.value[pos] != '\t')
    }
        ++pos;
    while (pos < ti.value.size()
    {
        && (ti.value[pos] == ' ' || ti.value[pos] == '\t'))
    }
        ++pos;

    ti.value.erase(start, pos - start);
    return true;
}

// ti_delete_to_start
template <unsigned _F,
          typename _I>
bool ti_delete_to_start(text_input<_F, _I>& ti)
{
    if (ti.read_only || !ti.enabled)
    {
        return false;
    }
    if (ti.cursor == 0)
    {
        return false;
    }
    ti.has_selection = false;
    ti.value.erase(0, ti.cursor);
    ti.cursor = 0;
    return true;
}

// ti_delete_to_end
template <unsigned _F,
          typename _I>
bool ti_delete_to_end(text_input<_F, _I>& ti)
{
    if (ti.read_only || !ti.enabled)
    {
        return false;
    }
    if (ti.cursor >= ti.value.size())
    {
        return false;
    }
    ti.has_selection = false;
    ti.value.erase(ti.cursor);
    return true;
}

// ti_clear
template <unsigned _F,
          typename _I>
void ti_clear(text_input<_F, _I>& ti)
{
    ti.value.clear();
    ti.cursor = 0;
    ti.has_selection = false;
}

// ti_set_value
//   Replaces entire value and resets cursor/selection.
template <unsigned _F,
          typename _I>
void ti_set_value(text_input<_F, _I>& ti, std::string val)
{
    ti.value = std::move(val);
    ti.cursor = ti.value.size();
    ti.has_selection = false;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §7  CLIPBOARD  (free functions — no system clipboard, just data ops)
// ═══════════════════════════════════════════════════════════════════════════════
//   These return/accept the clipboard content as a string.  The application
// bridges to the actual system clipboard.

// ti_copy
//   Returns selected text without modifying the input.
template <unsigned _F,
          typename _I>
std::string ti_copy(const text_input<_F, _I>& ti)
{
    return ti.selected_text();
}

// ti_cut
//   Returns selected text and deletes it.
template <unsigned _F,
          typename _I>
std::string ti_cut(text_input<_F, _I>& ti)
{
    if (ti.read_only || !ti.enabled)
    {
        return {};
    }
    auto text = ti.selected_text();
    detail_ti::delete_selection(ti);
    return text;
}

// ti_paste
//   Inserts text at cursor (replacing selection if active).
template <unsigned _F,
          typename _I>
bool ti_paste(text_input<_F, _I>& ti, const std::string& text)
{
    return ti_insert(ti, text);
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §8  HISTORY  (tif_history)
// ═══════════════════════════════════════════════════════════════════════════════

// ti_history_push
//   Appends the current value to the history ring and clears input.
template <unsigned _F,
          typename _I>
void ti_history_push(text_input<_F, _I>& ti)
{
    static_assert(has_tif(_F, tif_history), "requires tif_history");
    if (ti.value.empty())
    {
        return;
    }
    ti.history.push_back(ti.value);
    if (ti.history.size()
    {
        > ti.history_max)
    }
        ti.history.erase(ti.history.begin());
    ti.history_pos = ti.history.size();
    ti_clear(ti);
}

// ti_history_prev
//   Navigate to the previous history entry.
template <unsigned _F,
          typename _I>
bool ti_history_prev(text_input<_F, _I>& ti)
{
    static_assert(has_tif(_F, tif_history), "requires tif_history");
    if (ti.history.empty())
    {
        return false;
    }

    // save current input when first entering history
    if (ti.history_pos == ti.history.size())
    {
        ti.saved_input = ti.value;
    }

    if (ti.history_pos == 0)
    {
        return false;
    }
    --ti.history_pos;
    ti.value  = ti.history[ti.history_pos];
    ti.cursor = ti.value.size();
    ti.has_selection = false;
    return true;
}

// ti_history_next
template <unsigned _F,
          typename _I>
bool ti_history_next(text_input<_F, _I>& ti)
{
    static_assert(has_tif(_F, tif_history), "requires tif_history");
    if (ti.history_pos >= ti.history.size())
    {
        return false;
    }

    ++ti.history_pos;
    if (ti.history_pos == ti.history.size())
    {
        // restore saved input
        ti.value  = ti.saved_input;
        ti.saved_input.clear();
    }
    else
    {
        ti.value = ti.history[ti.history_pos];
    }
    ti.cursor = ti.value.size();
    ti.has_selection = false;
    return true;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §9  VALIDATION  (tif_validation)
// ═══════════════════════════════════════════════════════════════════════════════

// ti_validate
//   Runs all validators.  Returns the most severe result.
template <unsigned _F,
          typename _I>
validation_report ti_validate(text_input<_F, _I>& ti)
{
    static_assert(has_tif(_F, tif_validation), "requires tif_validation");
    validation_report worst = { validation_result::valid, {} };

    for (const auto& v : ti.validators)
    {
        auto r = v(ti.value);
        if (static_cast<int>(r.result)
        {
            > static_cast<int>(worst.result))
        }
            worst = r;
    }
    ti.last_report = worst;
    return worst;
}

// ti_add_validator
template <unsigned _F,
          typename _I>
void ti_add_validator(text_input<_F, _I>& ti, validator_fn fn)
{
    static_assert(has_tif(_F, tif_validation), "requires tif_validation");
    ti.validators.push_back(std::move(fn));
}

// ti_clear_validators
template <unsigned _F,
          typename _I>
void ti_clear_validators(text_input<_F, _I>& ti)
{
    static_assert(has_tif(_F, tif_validation), "requires tif_validation");
    ti.validators.clear();
    ti.last_report = { validation_result::valid, {} };
}

// ── built-in validator factories ─────────────────────────────────────────

inline validator_fn make_max_length_validator(std::size_t max)
{
    return [max](const std::string& v) -> validation_report {
        if (v.size()
        {
            > max)
        }
            return { validation_result::error, "Exceeds maximum length" };
        return { validation_result::valid, {} };
    };
}

inline validator_fn make_not_empty_validator()
{
    return [](const std::string& v) -> validation_report {
        if (v.empty())
        {
            return { validation_result::error, "Cannot be empty" };
        }
        return { validation_result::valid, {} };
    };
}

inline validator_fn make_pattern_validator(
    std::function<bool(const std::string&)> test,
    std::string error_msg = "Invalid format")
{
    return [test = std::move(test), msg = std::move(error_msg)]
           (const std::string& v) -> validation_report {
        if (!test(v))
        {
            return { validation_result::error, msg };
        }
        return { validation_result::valid, {} };
    };
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §10  TEXT INPUT TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace text_input_traits {
namespace detail {
    template <typename,
              typename = void>
    struct has_value_member : std::false_type {};
    template <typename _T>
    struct has_value_member<_T, std::void_t<
        decltype(std::declval<_T>().value)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_cursor_member : std::false_type {};
    template <typename _T>
    struct has_cursor_member<_T, std::void_t<
        decltype(std::declval<_T>().cursor)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_placeholder_member : std::false_type {};
    template <typename _T>
    struct has_placeholder_member<_T, std::void_t<
        decltype(std::declval<_T>().placeholder)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_sel_anchor_member : std::false_type {};
    template <typename _T>
    struct has_sel_anchor_member<_T, std::void_t<
        decltype(std::declval<_T>().sel_anchor)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_read_only_member : std::false_type {};
    template <typename _T>
    struct has_read_only_member<_T, std::void_t<
        decltype(std::declval<_T>().read_only)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_history_member : std::false_type {};
    template <typename _T>
    struct has_history_member<_T, std::void_t<
        decltype(std::declval<_T>().history)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_validators_member : std::false_type {};
    template <typename _T>
    struct has_validators_member<_T, std::void_t<
        decltype(std::declval<_T>().validators)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_masked_member : std::false_type {};
    template <typename _T>
    struct has_masked_member<_T, std::void_t<
        decltype(std::declval<_T>().masked)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_word_wrap_member : std::false_type {};
    template <typename _T>
    struct has_word_wrap_member<_T, std::void_t<
        decltype(std::declval<_T>().word_wrap)
    >> : std::true_type {};
}

template <typename _T> inline constexpr bool has_value_v       = detail::has_value_member<_T>::value;
template <typename _T> inline constexpr bool has_cursor_v      = detail::has_cursor_member<_T>::value;
template <typename _T> inline constexpr bool has_placeholder_v = detail::has_placeholder_member<_T>::value;
template <typename _T> inline constexpr bool has_sel_anchor_v  = detail::has_sel_anchor_member<_T>::value;
template <typename _T> inline constexpr bool has_read_only_v   = detail::has_read_only_member<_T>::value;
template <typename _T> inline constexpr bool has_history_v     = detail::has_history_member<_T>::value;
template <typename _T> inline constexpr bool has_validators_v  = detail::has_validators_member<_T>::value;
template <typename _T> inline constexpr bool has_masked_v      = detail::has_masked_member<_T>::value;
template <typename _T> inline constexpr bool has_word_wrap_v   = detail::has_word_wrap_member<_T>::value;

// is_text_input
//   Has value + cursor + placeholder + sel_anchor + focusable.
template <typename _Type>
struct is_text_input : std::conjunction<
    detail::has_value_member<_Type>,
    detail::has_cursor_member<_Type>,
    detail::has_placeholder_member<_Type>,
    detail::has_sel_anchor_member<_Type>,
    view_traits::detail::has_focusable_flag<_Type>
> {};
template <typename _T> inline constexpr bool is_text_input_v = is_text_input<_T>::value;

// is_multiline_input
template <typename _Type>
struct is_multiline_input : std::conjunction<
    is_text_input<_Type>,
    detail::has_word_wrap_member<_Type>
> {};
template <typename _T> inline constexpr bool is_multiline_input_v = is_multiline_input<_T>::value;

// is_history_input
template <typename _Type>
struct is_history_input : std::conjunction<
    is_text_input<_Type>,
    detail::has_history_member<_Type>
> {};
template <typename _T> inline constexpr bool is_history_input_v = is_history_input<_T>::value;

// is_validated_input
template <typename _Type>
struct is_validated_input : std::conjunction<
    is_text_input<_Type>,
    detail::has_validators_member<_Type>
> {};
template <typename _T> inline constexpr bool is_validated_input_v = is_validated_input<_T>::value;

// is_masked_text_input
template <typename _Type>
struct is_masked_text_input : std::conjunction<
    is_text_input<_Type>,
    detail::has_masked_member<_Type>
> {};
template <typename _T> inline constexpr bool is_masked_text_input_v = is_masked_text_input<_T>::value;

}   // namespace text_input_traits


NS_END  // component
NS_END  // uxoxo

#endif  // UXOXO_COMPONENT_TEXT_INPUT_
