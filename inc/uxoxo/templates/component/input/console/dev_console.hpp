/*******************************************************************************
* uxoxo [component]                                             dev_console.hpp
*
* Framework-agnostic developer console:
*   A dev_console is, at its core, a text_input.  On top of that core,
* optional modules may be composed via compile-time feature flags:
*
*     dcf_output        scrollable output window (text_output)
*     dcf_autocomplete  inline tab-completion via autocomplete<>
*     dcf_autosuggest   live suggestion list via autosuggest<>
*     dcf_history       command history navigation
*     dcf_submit_btn    a submit button alongside the input
*     dcf_timestamps    per-entry timestamp on output lines
*     dcf_log_levels    per-entry severity + active filter level
*     dcf_badges        per-entry source badge
*
*   The minimum viable console is a text_input with none of these
* features.  Enable dcf_output and you get the Source-Engine-style
* scrollback log.  Enable dcf_autosuggest and you get a dropdown
* (or ghost text, or floating panel — the renderer decides).  Enable
* dcf_history and the console can browse prior commands.  Enable
* dcf_submit_btn and the renderer draws a "Submit" button.  Every
* combination is valid, including all of them or none of them.
*
*   The console is a pure data aggregate — no rendering, no observer
* coupling, no framework dependency.  All mutation is via free functions
* prefixed `dc_`.
*
*   The console does NOT wire its input to its optional modules.  That
* wiring is the framework's job.  How history is stored (memory, file,
* database), how autosuggest is displayed (dropdown, ghost, popup),
* how autocomplete is triggered (tab, ctrl+space, menu) — all of
* these are implementation decisions left to the framework/renderer.
*
*   dc_submit() takes the submitted value as a std::string parameter.
* The framework extracts the committed value from whatever input type
* is in use and passes it in.  This keeps the submit path input-
* agnostic.
*
*   Structure:
*     1.   Feature flags
*     2.   Entry types (log entry with optional mixins)
*     3.   Entry-level EBO mixins (timestamp, badge)
*     4.   Console-level EBO mixins (output, history, autosuggest,
*          autocomplete, submit button, log level)
*     5.   dev_console struct (with visit_components for compositional
*          forwarding)
*     6.   Submit
*     7.   Output operations
*     8.   History operations
*     9.   Suggestion operations
*     10.  Completion operations
*     11.  Log level operations
*     12.  Visibility (legacy wrappers)
*     13.  Traits
*
*
* path:      /inc/uxoxo/templates/component/input/console/dev_console.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_DEV_CONSOLE_
#define  UXOXO_COMPONENT_DEV_CONSOLE_ 1

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
#include "../../../../uxoxo.hpp"
#include "../../component_traits.hpp"
#include "../../component_common.hpp"
#include "../../autosuggest.hpp"
#include "../../autocomplete.hpp"
#include "../../button/button.hpp"
#include "../../history/history_view.hpp"
#include "../../output/text_output.hpp"
#include "../text_input.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  FEATURE FLAGS
// ===============================================================================

// dev_console_feat
//   enum: compile-time feature flags for optional dev_console modules.
enum dev_console_feat : unsigned
{
    dcf_none         = 0,
    dcf_output       = 1u << 20,
    dcf_autocomplete = 1u << 21,
    dcf_autosuggest  = 1u << 22,
    dcf_history      = 1u << 23,
    dcf_submit_btn   = 1u << 24,
    dcf_timestamps   = 1u << 25,
    dcf_log_levels   = 1u << 26,
    dcf_badges       = 1u << 27,

    // convenience presets
    dcf_basic        = dcf_output | dcf_history | dcf_submit_btn,
    dcf_full         = dcf_output       | dcf_autocomplete
                     | dcf_autosuggest  | dcf_history
                     | dcf_submit_btn   | dcf_timestamps
                     | dcf_log_levels   | dcf_badges,

    dcf_all          = dcf_full
};

constexpr dev_console_feat
operator|(dev_console_feat _a,
          dev_console_feat _b) noexcept
{
    return static_cast<dev_console_feat>(
        static_cast<unsigned>(_a) | static_cast<unsigned>(_b));
}

constexpr bool
has_dcf(unsigned         _flags,
        dev_console_feat _bit) noexcept
{
    return (_flags & static_cast<unsigned>(_bit)) != 0;
}

// ===============================================================================
//  2.  ENTRY TYPES
// ===============================================================================

// entry_kind
//   enum: classification tag for individual log entries.
enum class entry_kind : std::uint8_t
{
    command,
    output,
    error,
    warning,
    info,
    debug,
    separator
};

// log_level
//   enum: severity filter threshold for log output.
enum class log_level : std::uint8_t
{
    all     = 0,
    debug   = 1,
    info    = 2,
    warning = 3,
    error   = 4,
    none    = 255
};

constexpr log_level
entry_kind_to_level(entry_kind _kind) noexcept
{
    switch (_kind)
    {
        case entry_kind::debug:     return log_level::debug;
        case entry_kind::info:      return log_level::info;
        case entry_kind::warning:   return log_level::warning;
        case entry_kind::error:     return log_level::error;
        default:                    return log_level::all;
    }
}

// time_point
//   type: alias for steady_clock time points.
using time_point = std::chrono::steady_clock::time_point;

// ===============================================================================
//  3.  ENTRY-LEVEL EBO MIXINS
// ===============================================================================

namespace console_mixin {

    // timestamp_data
    template<bool _Enable>
    struct timestamp_data
    {};

    template<>
    struct timestamp_data<true>
    {
        time_point timestamp = std::chrono::steady_clock::now();
    };

    // badge_data
    template<bool _Enable>
    struct badge_data
    {};

    template<>
    struct badge_data<true>
    {
        std::string badge;
    };

}   // namespace console_mixin


// log_entry
//   struct: a single entry in the console output log.
template<unsigned _Feat = dcf_none>
struct log_entry
    : console_mixin::timestamp_data <has_dcf(_Feat, dcf_timestamps)>
    , console_mixin::badge_data     <has_dcf(_Feat, dcf_badges)>
{
    static constexpr unsigned features       = _Feat;
    static constexpr bool     has_timestamps = has_dcf(_Feat, dcf_timestamps);
    static constexpr bool     has_badges     = has_dcf(_Feat, dcf_badges);

    std::string text;
    entry_kind  kind = entry_kind::output;

    log_entry() = default;

    log_entry(
            std::string _text,
            entry_kind  _kind
        )
            : text(std::move(_text)),
              kind(_kind)
        {}
};

// ===============================================================================
//  4.  CONSOLE-LEVEL EBO MIXINS
// ===============================================================================

namespace console_mixin {

    // -- output window ------------------------------------------------
    template<bool _Enable, unsigned _Feat>
    struct output_mixin
    {};

    template<unsigned _Feat>
    struct output_mixin<true, _Feat>
    {
        // the text_output features are derived from the console's
        // own flags — if the console has timestamps, the output
        // gets tof_timestamps, etc.
        static constexpr unsigned output_feats =
            tof_line_buffer |
            tof_color       |
            tof_selectable  |
            (has_dcf(_Feat, dcf_timestamps)
                ? static_cast<unsigned>(tof_timestamps) : 0u);

        text_output<output_feats> output;
    };

    // -- history ------------------------------------------------------
    template<bool _Enable>
    struct history_mixin
    {};

    template<>
    struct history_mixin<true>
    {
        history_view<std::string> cmd_history;
        std::string               saved_input;
    };

    // -- autosuggest --------------------------------------------------
    template<bool _Enable>
    struct suggest_mixin
    {};

    template<>
    struct suggest_mixin<true>
    {
        autosuggest<> suggest;
    };

    // -- autocomplete -------------------------------------------------
    template<bool _Enable>
    struct complete_mixin
    {};

    template<>
    struct complete_mixin<true>
    {
        autocomplete<> complete;
    };

    // -- submit button ------------------------------------------------
    template<bool _Enable>
    struct submit_btn_mixin
    {};

    template<>
    struct submit_btn_mixin<true>
    {
        button<bf_tooltip> submit_btn { "Submit" };
    };

    // -- log level ----------------------------------------------------
    template<bool _Enable>
    struct level_mixin
    {};

    template<>
    struct level_mixin<true>
    {
        log_level active_level = log_level::all;
    };

}   // namespace console_mixin

// ===============================================================================
//  5.  DEV CONSOLE
// ===============================================================================
//   _InputFeat  bitwise OR of text_input_feat / view_feat flags for the
//               input element.  Defaults to plain single-line input.
//   _Feat       bitwise OR of dev_console_feat flags for optional modules.

// dev_console
//   struct: framework-agnostic developer console.  At its core a
// text_input; optional modules composed via EBO mixins.
template<unsigned _InputFeat = tif_none,
         unsigned _Feat      = dcf_none>
struct dev_console
    : console_mixin::output_mixin   <has_dcf(_Feat, dcf_output), _Feat>,
      console_mixin::history_mixin  <has_dcf(_Feat, dcf_history)>,
      console_mixin::suggest_mixin  <has_dcf(_Feat, dcf_autosuggest)>,
      console_mixin::complete_mixin <has_dcf(_Feat, dcf_autocomplete)>,
      console_mixin::submit_btn_mixin <has_dcf(_Feat, dcf_submit_btn)>,
      console_mixin::level_mixin    <has_dcf(_Feat, dcf_log_levels)>
{
    using input_type = text_input<_InputFeat>;
    using entry_type = log_entry<_Feat>;

    static constexpr unsigned input_features   = _InputFeat;
    static constexpr unsigned features         = _Feat;
    static constexpr bool     has_output       = has_dcf(_Feat, dcf_output);
    static constexpr bool     has_autocomplete = has_dcf(_Feat, dcf_autocomplete);
    static constexpr bool     has_autosuggest  = has_dcf(_Feat, dcf_autosuggest);
    static constexpr bool     has_history      = has_dcf(_Feat, dcf_history);
    static constexpr bool     has_submit_btn   = has_dcf(_Feat, dcf_submit_btn);
    static constexpr bool     has_timestamps   = has_dcf(_Feat, dcf_timestamps);
    static constexpr bool     has_log_levels   = has_dcf(_Feat, dcf_log_levels);
    static constexpr bool     has_badges       = has_dcf(_Feat, dcf_badges);
    static constexpr bool     focusable        = true;

    // -- input --------------------------------------------------------
    input_type  input;

    // -- prompt -------------------------------------------------------
    std::string prompt = "> ";

    // -- visibility ---------------------------------------------------
    bool visible = true;

    // -- submit callback ----------------------------------------------
    //   the framework invokes this after dc_submit().  The callback
    // receives the submitted command string and can return an
    // optional response (empty = no response to print).
    using submit_fn = std::function<std::string(const std::string&)>;
    submit_fn on_submit;

    // -- compositional forwarding -------------------------------------
    //   Exposes sub-components for use with for_each_sub() and the
    // shared ADL free functions.  Enables:
    //
    //     for_each_sub(console, [](auto& sub) { disable(sub); });
    //     enable_all(console);
    //     show(console.input);
    //
    // rather than writing dc_-prefixed forwarding wrappers for
    // every shared operation.
    template <typename _Fn>
    void visit_components(_Fn&& _fn)
    {
        _fn(input);

        if constexpr (has_output)
        {
            _fn(this->output);
        }

        if constexpr (has_autosuggest)
        {
            _fn(this->suggest);
        }

        if constexpr (has_autocomplete)
        {
            _fn(this->complete);
        }

        if constexpr (has_submit_btn)
        {
            _fn(this->submit_btn);
        }

        return;
    }
};

// ===============================================================================
//  6.  SUBMIT
// ===============================================================================

// dc_submit (forward declarations)
template<unsigned _IF, unsigned _F>
void dc_print(dev_console<_IF, _F>& _dc,
              std::string           _text,
              entry_kind            _kind);

/*
dc_submit
  Commits a command string to the console: echoes to the output (if
present), records to history (if enabled), dismisses suggestions and
completions, and invokes the submit callback.

Parameter(s):
  _dc:  the console to submit to.
  _cmd: the command string to submit.
Return:
  the submitted command string, or empty if _cmd was empty.
*/
template<unsigned _IF, unsigned _F>
std::string
dc_submit(dev_console<_IF, _F>& _dc,
          std::string            _cmd)
{
    if (_cmd.empty())
    {
        return {};
    }

    // echo to output
    if constexpr (has_dcf(_F, dcf_output))
    {
        dc_print(_dc,
                 _dc.prompt + _cmd,
                 entry_kind::command);
    }

    // record to history
    if constexpr (has_dcf(_F, dcf_history))
    {
        if ( (_dc.cmd_history.empty())                   ||
             (_dc.cmd_history.at_live_position())        ||
             (*(hv_current(_dc.cmd_history)) != _cmd) )
        {
            hv_record(_dc.cmd_history, _cmd);
        }
        else
        {
            hv_go_to_live(_dc.cmd_history);
        }

        _dc.saved_input.clear();
    }

    // dismiss suggestions
    if constexpr (has_dcf(_F, dcf_autosuggest))
    {
        as_clear(_dc.suggest);
    }

    // dismiss completion
    if constexpr (has_dcf(_F, dcf_autocomplete))
    {
        ac_clear(_dc.complete);
    }

    // invoke submit callback
    if (_dc.on_submit)
    {
        std::string response = _dc.on_submit(_cmd);

        if ( (!response.empty()) &&
             (has_dcf(_F, dcf_output)) )
        {
            dc_print(_dc,
                     response,
                     entry_kind::output);
        }
    }

    return _cmd;
}

// ===============================================================================
//  7.  OUTPUT OPERATIONS  (dcf_output)
// ===============================================================================

// dc_print
//   function: appends a line to the output window with the given
// entry kind.  Maps entry_kind to output_color_tag for rendering.
template<unsigned _IF, unsigned _F>
void
dc_print(dev_console<_IF, _F>& _dc,
         std::string           _text,
         entry_kind            _kind)
{
    static_assert(has_dcf(_F, dcf_output), "requires dcf_output");

    // map entry_kind to color tag
    output_color_tag color = output_color_tag::normal;

    switch (_kind)
    {
        case entry_kind::command:   color = output_color_tag::muted;     break;
        case entry_kind::error:     color = output_color_tag::error;     break;
        case entry_kind::warning:   color = output_color_tag::warning;   break;
        case entry_kind::info:      color = output_color_tag::info;      break;
        case entry_kind::debug:     color = output_color_tag::debug;     break;
        case entry_kind::output:    color = output_color_tag::normal;    break;
        case entry_kind::separator: color = output_color_tag::muted;     break;
    }

    // log level filter (skip if below threshold)
    if constexpr (has_dcf(_F, dcf_log_levels))
    {
        if ( (_kind != entry_kind::command)   &&
             (_kind != entry_kind::separator) )
        {
            auto level = entry_kind_to_level(_kind);

            if ( static_cast<int>(level) <
                 static_cast<int>(_dc.active_level) )
            {
                return;
            }
        }
    }

    to_append_colored(_dc.output,
                      std::move(_text),
                      color);

    return;
}

// dc_print_output
//   convenience: prints with entry_kind::output.
template<unsigned _IF, unsigned _F>
void
dc_print_output(dev_console<_IF, _F>& _dc,
                std::string           _text)
{
    dc_print(_dc,
             std::move(_text),
             entry_kind::output);

    return;
}

// dc_print_error
//   convenience: prints with entry_kind::error.
template<unsigned _IF, unsigned _F>
void
dc_print_error(dev_console<_IF, _F>& _dc,
               std::string           _text)
{
    dc_print(_dc,
             std::move(_text),
             entry_kind::error);

    return;
}

// dc_clear_output
//   function: clears the output window.
template<unsigned _IF, unsigned _F>
void
dc_clear_output(dev_console<_IF, _F>& _dc)
{
    static_assert(has_dcf(_F, dcf_output), "requires dcf_output");

    to_clear(_dc.output);

    return;
}

// dc_scroll_to_bottom
//   function: scrolls the output window to the most recent line.
template<unsigned _IF, unsigned _F>
void
dc_scroll_to_bottom(dev_console<_IF, _F>& _dc)
{
    static_assert(has_dcf(_F, dcf_output), "requires dcf_output");

    to_scroll_to_bottom(_dc.output);

    return;
}

// ===============================================================================
//  8.  HISTORY OPERATIONS  (dcf_history)
// ===============================================================================

template<unsigned _IF, unsigned _F>
bool
dc_history_prev(dev_console<_IF, _F>& _dc)
{
    static_assert(has_dcf(_F, dcf_history), "requires dcf_history");

    return hv_prev(_dc.cmd_history);
}

template<unsigned _IF, unsigned _F>
bool
dc_history_next(dev_console<_IF, _F>& _dc)
{
    static_assert(has_dcf(_F, dcf_history), "requires dcf_history");

    return hv_next(_dc.cmd_history);
}

template<unsigned _IF, unsigned _F>
void
dc_history_clear(dev_console<_IF, _F>& _dc)
{
    static_assert(has_dcf(_F, dcf_history), "requires dcf_history");

    hv_clear(_dc.cmd_history);
    _dc.saved_input.clear();

    return;
}

// ===============================================================================
//  9.  SUGGESTION OPERATIONS  (dcf_autosuggest)
// ===============================================================================

template<unsigned _IF, unsigned _F>
void
dc_update_suggestions(dev_console<_IF, _F>& _dc,
                      const std::string&    _input)
{
    static_assert(has_dcf(_F, dcf_autosuggest),
                  "requires dcf_autosuggest");

    as_update(_dc.suggest, _input);

    return;
}

template<unsigned _IF, unsigned _F>
void
dc_dismiss_suggestions(dev_console<_IF, _F>& _dc)
{
    static_assert(has_dcf(_F, dcf_autosuggest),
                  "requires dcf_autosuggest");

    as_dismiss(_dc.suggest);

    return;
}

// ===============================================================================
//  10.  COMPLETION OPERATIONS  (dcf_autocomplete)
// ===============================================================================

template<unsigned _IF, unsigned _F>
void
dc_trigger_complete(dev_console<_IF, _F>& _dc,
                    const std::string&    _input)
{
    static_assert(has_dcf(_F, dcf_autocomplete),
                  "requires dcf_autocomplete");

    ac_complete(_dc.complete, _input);

    return;
}

template<unsigned _IF, unsigned _F>
void
dc_dismiss_complete(dev_console<_IF, _F>& _dc)
{
    static_assert(has_dcf(_F, dcf_autocomplete),
                  "requires dcf_autocomplete");

    ac_clear(_dc.complete);

    return;
}

// ===============================================================================
//  11.  LOG LEVEL OPERATIONS  (dcf_log_levels)
// ===============================================================================

template<unsigned _IF, unsigned _F>
void
dc_set_log_level(dev_console<_IF, _F>& _dc,
                 log_level             _level)
{
    static_assert(has_dcf(_F, dcf_log_levels),
                  "requires dcf_log_levels");

    _dc.active_level = _level;

    return;
}

// ===============================================================================
//  12.  VISIBILITY (legacy wrappers)
// ===============================================================================
//   These dc_-prefixed functions are retained for backward
// compatibility.  New code should prefer the ADL-dispatched
// equivalents in component_common.hpp:
//
//     dc_show(dc)    ->  show(dc)
//     dc_hide(dc)    ->  hide(dc)
//     dc_toggle(dc)  ->  toggle_visible(dc)

template<unsigned _IF, unsigned _F>
void dc_show(dev_console<_IF, _F>& _dc)
{
    show(_dc);

    return;
}

template<unsigned _IF, unsigned _F>
void dc_hide(dev_console<_IF, _F>& _dc)
{
    hide(_dc);

    return;
}

template<unsigned _IF, unsigned _F>
void dc_toggle(dev_console<_IF, _F>& _dc)
{
    toggle_visible(_dc);

    return;
}

// ===============================================================================
//  13.  TRAITS
// ===============================================================================

namespace console_traits {
NS_INTERNAL

    // -- console-specific detectors -----------------------------------

    template<typename, typename = void>
    struct has_input_member : std::false_type {};
    template<typename _Type>
    struct has_input_member<_Type, std::void_t<
        decltype(std::declval<_Type>().input)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_prompt_member : std::false_type {};
    template<typename _Type>
    struct has_prompt_member<_Type, std::void_t<
        decltype(std::declval<_Type>().prompt)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_output_member : std::false_type {};
    template<typename _Type>
    struct has_output_member<_Type, std::void_t<
        decltype(std::declval<_Type>().output)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_cmd_history_member : std::false_type {};
    template<typename _Type>
    struct has_cmd_history_member<_Type, std::void_t<
        decltype(std::declval<_Type>().cmd_history)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_suggest_member : std::false_type {};
    template<typename _Type>
    struct has_suggest_member<_Type, std::void_t<
        decltype(std::declval<_Type>().suggest)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_complete_member : std::false_type {};
    template<typename _Type>
    struct has_complete_member<_Type, std::void_t<
        decltype(std::declval<_Type>().complete)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_submit_btn_member : std::false_type {};
    template<typename _Type>
    struct has_submit_btn_member<_Type, std::void_t<
        decltype(std::declval<_Type>().submit_btn)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_active_level_member : std::false_type {};
    template<typename _Type>
    struct has_active_level_member<_Type, std::void_t<
        decltype(std::declval<_Type>().active_level)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_on_submit_member : std::false_type {};
    template<typename _Type>
    struct has_on_submit_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_submit)
    >> : std::true_type {};

}   // NS_INTERNAL

// -- console-specific aliases -----------------------------------------
template<typename _Type> inline constexpr bool has_input_v        = internal::has_input_member<_Type>::value;
template<typename _Type> inline constexpr bool has_prompt_v       = internal::has_prompt_member<_Type>::value;
template<typename _Type> inline constexpr bool has_output_v       = internal::has_output_member<_Type>::value;
template<typename _Type> inline constexpr bool has_cmd_history_v  = internal::has_cmd_history_member<_Type>::value;
template<typename _Type> inline constexpr bool has_suggest_v      = internal::has_suggest_member<_Type>::value;
template<typename _Type> inline constexpr bool has_complete_v     = internal::has_complete_member<_Type>::value;
template<typename _Type> inline constexpr bool has_submit_btn_v   = internal::has_submit_btn_member<_Type>::value;
template<typename _Type> inline constexpr bool has_active_level_v = internal::has_active_level_member<_Type>::value;
template<typename _Type> inline constexpr bool has_on_submit_v    = internal::has_on_submit_member<_Type>::value;

// -- shared aliases (delegate to component_traits) --------------------
template<typename _Type> inline constexpr bool has_visible_v      = has_visible_v<_Type>;
template<typename _Type> inline constexpr bool is_focusable_v     = is_focusable_v<_Type>;

// is_dev_console
//   trait: has input + prompt + visible + focusable.
template<typename _Type>
struct is_dev_console : std::conjunction<
    internal::has_input_member<_Type>,
    internal::has_prompt_member<_Type>,
    ::uxoxo::component::internal::has_visible_member<_Type>
    ::uxoxo::component::internal::has_focusable_flag<_Type>
>
{};

template<typename _Type> inline constexpr bool is_dev_console_v = is_dev_console<_Type>::value;

// is_output_console
template<typename _Type>
struct is_output_console : std::conjunction<
    is_dev_console<_Type>,
    internal::has_output_member<_Type>
>
{};

template<typename _Type> inline constexpr bool is_output_console_v = is_output_console<_Type>::value;

// is_history_console
template<typename _Type>
struct is_history_console : std::conjunction<
    is_dev_console<_Type>,
    internal::has_cmd_history_member<_Type>
>
{};

template<typename _Type> inline constexpr bool is_history_console_v = is_history_console<_Type>::value;

// is_suggest_console
template<typename _Type>
struct is_suggest_console : std::conjunction<
    is_dev_console<_Type>,
    internal::has_suggest_member<_Type>
>
{};

template<typename _Type> inline constexpr bool is_suggest_console_v = is_suggest_console<_Type>::value;

// is_complete_console
template<typename _Type>
struct is_complete_console : std::conjunction<
    is_dev_console<_Type>,
    internal::has_complete_member<_Type>
>
{};

template<typename _Type> inline constexpr bool is_complete_console_v = is_complete_console<_Type>::value;

// is_submit_btn_console
template<typename _Type>
struct is_submit_btn_console : std::conjunction<
    is_dev_console<_Type>,
    internal::has_submit_btn_member<_Type>
>
{};

template<typename _Type> inline constexpr bool is_submit_btn_console_v = is_submit_btn_console<_Type>::value;

// is_leveled_console
template<typename _Type>
struct is_leveled_console : std::conjunction<
    is_dev_console<_Type>,
    internal::has_active_level_member<_Type>
>
{};

template<typename _Type> inline constexpr bool is_leveled_console_v = is_leveled_console<_Type>::value;


NS_END  // namespace console_traits
NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DEV_CONSOLE_