/*******************************************************************************
* uxoxo [component]                                                  dialog.hpp
*
* Generic dialog:
*   A framework-agnostic, pure-data dialog template.  This is the abstract
* base for any modal or modeless popup the UI presents — message boxes,
* confirmation prompts, input dialogs, progress dialogs, about boxes,
* preferences panes, and specialized pickers like the file dialog.
*
*   A dialog captures concerns shared by every popup:
*     - A title and optional icon (compile-time gated)
*     - A result outcome (pending / accepted / cancelled / closed / custom)
*     - A button bar with role-tagged entries (OK / Cancel / Yes / No /
*       Apply / Reset / Close / Retry / Abort / Ignore / Help / custom)
*     - Lifecycle callbacks (on_open, on_accept, on_cancel, on_close,
*       on_result)
*     - Modal / modeless gate
*     - Optional position, size, and min/max bounds (compile-time gated)
*     - An opaque parent handle
*
*   Dialogs prescribe no rendering.  Concrete dialogs (file_dialog,
* message_dialog, progress_dialog, etc.) either inherit from dialog<_F>
* or compose it.  Platform adapters discover capabilities structurally
* via dialog_traits::.
*
*   Feature composition follows the same EBO-mixin bitfield pattern used
* throughout the uxoxo component layer.
*
* Contents:
*   1.  Feature flags (dialog_feat)
*   2.  Enums (dialog_result, button_role)
*   3.  dialog_button struct
*   4.  EBO mixins
*   5.  dialog struct
*   6.  Free functions
*   7.  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/dialog/dialog.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_DIALOG_
#define  UXOXO_COMPONENT_DIALOG_ 1

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
// uxoxo
#include "../../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  DIALOG FEATURE FLAGS
// ===============================================================================
//   Dialogs own their own 0-15 bit range.  Derived dialogs (file_dialog,
// file_open_save_dialog, ...) take their own feature flag parameter and
// never share bits with dialog_feat.

enum dialog_feat : unsigned
{
    df_none        = 0,
    df_titled      = 1u << 0,    // has a title string
    df_iconed      = 1u << 1,    // has an icon reference
    df_closable    = 1u << 2,    // has a close (X) affordance
    df_resizable   = 1u << 3,    // user-resizable
    df_movable     = 1u << 4,    // user-draggable
    df_positioned  = 1u << 5,    // tracks x / y position
    df_sized       = 1u << 6,    // tracks width / height
    df_bounded     = 1u << 7,    // tracks min / max size bounds
    df_help_button = 1u << 8,    // has a help (?) affordance
    df_status_line = 1u << 9,    // has a footer status text

    df_window = df_titled       | 
                df_closable     | 
                df_resizable    |
                df_movable      |
                df_positioned   | 
                df_sized,

    df_all    = df_titled       | 
                df_iconed       | 
                df_closable     |
                df_resizable    | 
                df_movable      | 
                df_positioned   |
                df_sized        | 
                df_bounded      | 
                df_help_button  |
                df_status_line
};

constexpr unsigned operator|(
    dialog_feat _a,                             
    dialog_feat _b
) noexcept
{
    return ( static_cast<unsigned>(_a) | 
             static_cast<unsigned>(_b) )
}

constexpr bool has_df(
    unsigned    _f,
    dialog_feat _bit
) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}

// ===============================================================================
//  2.  ENUMS
// ===============================================================================

// dialog_result
//   enum: final outcome of a dialog interaction.  `pending` means no
// decision yet; `custom` means a button with a custom role was fired
// and the specifics live in dialog::custom_result_code.
enum class dialog_result : std::uint8_t
{
    pending,
    accepted,
    cancelled,
    closed,
    custom
};

// button_role
//   enum: semantic role of a dialog button.  The platform adapter is
// free to substitute localized labels for the standard roles.
enum class button_role : std::uint8_t
{
    ok,
    cancel,
    yes,
    no,
    apply,
    reset,
    close,
    retry,
    abort,
    ignore,
    help,
    discard,
    save,
    open,
    custom
};


// ===============================================================================
//  3.  DIALOG BUTTON
// ===============================================================================

// dialog_button
//   struct: one entry in a dialog's button bar.  Platforms may ignore
// `label` for standard roles and use their native localized string.
struct dialog_button
{
    std::string  label;
    button_role  role          = button_role::custom;
    int          custom_code   = 0;      // meaningful when role == custom
    bool         is_default    = false;  // activated by Enter
    bool         is_cancel     = false;  // activated by Escape
    bool         enabled       = true;
    bool         visible       = true;
};


// ===============================================================================
//  4.  EBO MIXINS
// ===============================================================================

namespace dialog_mixin {

    // -- title --------------------------------------------------------
    template <bool _Enable>
    struct title_data
    {};

    template <>
    struct title_data<true>
    {
        std::string title;
    };

    // -- icon ---------------------------------------------------------
    template <bool _Enable>
    struct icon_data
    {};

    template <>
    struct icon_data<true>
    {
        int          icon_id   = -1;     // platform handle / atlas id
        std::string  icon_path;          // fallback path when id < 0
    };

    // -- position -----------------------------------------------------
    template <bool _Enable>
    struct position_data
    {};

    template <>
    struct position_data<true>
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    // -- size ---------------------------------------------------------
    template <bool _Enable>
    struct size_data
    {};

    template <>
    struct size_data<true>
    {
        float width  = 480.0f;
        float height = 320.0f;
    };

    // -- bounds -------------------------------------------------------
    template <bool _Enable>
    struct bounds_data
    {};

    template <>
    struct bounds_data<true>
    {
        float min_width  = 200.0f;
        float min_height = 120.0f;
        float max_width  = 0.0f;   // 0 => unbounded
        float max_height = 0.0f;   // 0 => unbounded
    };

    // -- status -------------------------------------------------------
    template <bool _Enable>
    struct status_data
    {};

    template <>
    struct status_data<true>
    {
        std::string status_line;
    };

}   // namespace dialog_mixin


// ===============================================================================
//  5.  DIALOG
// ===============================================================================

// dialog
//   struct: framework-agnostic dialog.  Holds button bar, result state,
// lifecycle callbacks, modality flag, visibility, and any compile-time
// gated geometry / title / icon / status state.
//
//   _Feat  bitwise OR of dialog_feat values.

template <unsigned _Feat = df_titled | df_closable>
struct dialog
    : dialog_mixin::title_data    <has_df(_Feat, df_titled)>
    , dialog_mixin::icon_data     <has_df(_Feat, df_iconed)>
    , dialog_mixin::position_data <has_df(_Feat, df_positioned)>
    , dialog_mixin::size_data     <has_df(_Feat, df_sized)>
    , dialog_mixin::bounds_data   <has_df(_Feat, df_bounded)>
    , dialog_mixin::status_data   <has_df(_Feat, df_status_line)>
{
    using result_fn   = std::function<void(dialog_result)>;
    using simple_fn   = std::function<void()>;
    using button_fn   = std::function<void(const dialog_button&)>;

    static constexpr unsigned features   = _Feat;
    static constexpr bool has_title      = has_df(_Feat, df_titled);
    static constexpr bool has_icon       = has_df(_Feat, df_iconed);
    static constexpr bool is_closable    = has_df(_Feat, df_closable);
    static constexpr bool is_resizable   = has_df(_Feat, df_resizable);
    static constexpr bool is_movable     = has_df(_Feat, df_movable);
    static constexpr bool has_position   = has_df(_Feat, df_positioned);
    static constexpr bool has_size       = has_df(_Feat, df_sized);
    static constexpr bool has_bounds     = has_df(_Feat, df_bounded);
    static constexpr bool has_help       = has_df(_Feat, df_help_button);
    static constexpr bool has_status     = has_df(_Feat, df_status_line);
    static constexpr bool focusable      = true;

    // -- core state ---------------------------------------------------
    dialog_result result             = dialog_result::pending;
    int           custom_result_code = 0;
    bool          visible            = false;
    bool          modal              = false;

    // -- buttons ------------------------------------------------------
    std::vector<dialog_button>  buttons;

    // -- lifecycle callbacks ------------------------------------------
    simple_fn  on_open;    // fired by dlg_open
    simple_fn  on_accept;  // fired by dlg_accept
    simple_fn  on_cancel;  // fired by dlg_cancel
    simple_fn  on_close;   // fired by any resolution path
    result_fn  on_result;  // fired last; receives the final result
    button_fn  on_button;  // fired whenever a button is activated

    // -- opaque parent handle -----------------------------------------
    void* parent = nullptr;

    // -- construction -------------------------------------------------
    dialog() = default;
};


// ===============================================================================
//  6.  FREE FUNCTIONS
// ===============================================================================

// dlg_open
//   shows the dialog, resets result to pending, and fires on_open.
template <unsigned _F>
void
dlg_open(
    dialog<_F>& _d
)
{
    _d.result             = dialog_result::pending;
    _d.custom_result_code = 0;
    _d.visible            = true;

    if (_d.on_open)
    {
        _d.on_open();
    }

    return;
}

// dlg_reset
//   returns the dialog to a pending / hidden state without firing
// any callbacks.  Useful before reopening a cached dialog instance.
template <unsigned _F>
void
dlg_reset(
    dialog<_F>& _d
)
{
    _d.result             = dialog_result::pending;
    _d.custom_result_code = 0;
    _d.visible            = false;

    return;
}

// dlg_accept
//   resolves the dialog as `accepted`.  Fires, in order: on_accept,
// on_button (if _source provided), on_close, on_result.
template <unsigned _F>
void
dlg_accept(
    dialog<_F>&           _d,
    const dialog_button*  _source = nullptr
)
{
    _d.result  = dialog_result::accepted;
    _d.visible = false;

    if (_d.on_accept)
    {
        _d.on_accept();
    }

    if ( (_source) && (_d.on_button) )
    {
        _d.on_button(*_source);
    }

    if (_d.on_close)
    {
        _d.on_close();
    }

    if (_d.on_result)
    {
        _d.on_result(_d.result);
    }

    return;
}

// dlg_cancel
//   resolves the dialog as `cancelled`.
template <unsigned _F>
void
dlg_cancel(
    dialog<_F>&           _d,
    const dialog_button*  _source = nullptr
)
{
    _d.result  = dialog_result::cancelled;
    _d.visible = false;

    if (_d.on_cancel)
    {
        _d.on_cancel();
    }

    if ( (_source) && (_d.on_button) )
    {
        _d.on_button(*_source);
    }

    if (_d.on_close)
    {
        _d.on_close();
    }

    if (_d.on_result)
    {
        _d.on_result(_d.result);
    }

    return;
}

// dlg_close
//   resolves the dialog as `closed` (window X / programmatic close).
// Distinct from cancel so adapters can distinguish abandoned from
// rejected.
template <unsigned _F>
void
dlg_close(
    dialog<_F>& _d
)
{
    _d.result  = dialog_result::closed;
    _d.visible = false;

    if (_d.on_close)
    {
        _d.on_close();
    }

    if (_d.on_result)
    {
        _d.on_result(_d.result);
    }

    return;
}

// dlg_close_custom
//   resolves the dialog with a custom result code.
template <unsigned _F>
void
dlg_close_custom(
    dialog<_F>&           _d,
    int                   _code,
    const dialog_button*  _source = nullptr
)
{
    _d.result             = dialog_result::custom;
    _d.custom_result_code = _code;
    _d.visible            = false;

    if ( (_source) && (_d.on_button) )
    {
        _d.on_button(*_source);
    }

    if (_d.on_close)
    {
        _d.on_close();
    }

    if (_d.on_result)
    {
        _d.on_result(_d.result);
    }

    return;
}

// dlg_activate_button
//   dispatches a button activation to the appropriate resolution
// path based on the button's role.
template <unsigned _F>
void
dlg_activate_button(
    dialog<_F>&           _d,
    const dialog_button&  _b
)
{
    if (!_b.enabled)
    {
        return;
    }

    switch (_b.role)
    {
        case button_role::ok:
        case button_role::yes:
        case button_role::apply:
        case button_role::save:
        case button_role::open:
        case button_role::retry:
            dlg_accept(_d, &_b);
            break;

        case button_role::cancel:
        case button_role::no:
        case button_role::abort:
        case button_role::ignore:
        case button_role::discard:
            dlg_cancel(_d, &_b);
            break;

        case button_role::close:
            dlg_close(_d);
            break;

        case button_role::reset:
        case button_role::help:
            if (_d.on_button)
            {
                _d.on_button(_b);
            }
            break;

        case button_role::custom:
        default:
            dlg_close_custom(_d, _b.custom_code, &_b);
            break;
    }

    return;
}

// dlg_add_button
template <unsigned _F>
dialog_button&
dlg_add_button(
    dialog<_F>&    _d,
    dialog_button  _b
)
{
    _d.buttons.push_back(std::move(_b));

    return _d.buttons.back();
}

// dlg_add_role
//   appends a button carrying only a role, with the platform-default
// label left empty (adapters fill it).
template <unsigned _F>
dialog_button&
dlg_add_role(
    dialog<_F>&  _d,
    button_role  _r,
    bool         _is_default = false,
    bool         _is_cancel  = false
)
{
    dialog_button b;
    b.role       = _r;
    b.is_default = _is_default;
    b.is_cancel  = _is_cancel;

    _d.buttons.push_back(std::move(b));

    return _d.buttons.back();
}

// dlg_clear_buttons
template <unsigned _F>
void
dlg_clear_buttons(
    dialog<_F>& _d
)
{
    _d.buttons.clear();

    return;
}

// dlg_find_by_role
//   returns a pointer to the first button with the given role, or
// nullptr.  O(N) on the button count (expected to be tiny).
template <unsigned _F>
dialog_button*
dlg_find_by_role(
    dialog<_F>&  _d,
    button_role  _r
)
{
    for (auto& b : _d.buttons)
    {
        if (b.role == _r)
        {
            return &b;
        }
    }

    return nullptr;
}

// dlg_find_default
//   returns the "default" (Enter-activated) button, or nullptr.
template <unsigned _F>
dialog_button*
dlg_find_default(
    dialog<_F>& _d
)
{
    for (auto& b : _d.buttons)
    {
        if (b.is_default)
        {
            return &b;
        }
    }

    return nullptr;
}

// dlg_find_cancel
//   returns the "cancel" (Escape-activated) button, or nullptr.
template <unsigned _F>
dialog_button*
dlg_find_cancel(
    dialog<_F>& _d
)
{
    for (auto& b : _d.buttons)
    {
        if (b.is_cancel)
        {
            return &b;
        }
    }

    return nullptr;
}

// dlg_is_open
template <unsigned _F>
bool
dlg_is_open(
    const dialog<_F>& _d
) noexcept
{
    return _d.visible;
}

// dlg_is_pending
template <unsigned _F>
bool
dlg_is_pending(
    const dialog<_F>& _d
) noexcept
{
    return (_d.result == dialog_result::pending);
}


// ===============================================================================
//  7.  TRAITS
// ===============================================================================

namespace dialog_traits {
namespace detail {

    template <typename, typename = void>
    struct has_result_member : std::false_type {};
    template <typename _Type>
    struct has_result_member<_Type, std::void_t<
        decltype(std::declval<_Type>().result)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_buttons_member : std::false_type {};
    template <typename _Type>
    struct has_buttons_member<_Type, std::void_t<
        decltype(std::declval<_Type>().buttons)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_visible_member : std::false_type {};
    template <typename _Type>
    struct has_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type>().visible)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_modal_member : std::false_type {};
    template <typename _Type>
    struct has_modal_member<_Type, std::void_t<
        decltype(std::declval<_Type>().modal)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_title_member : std::false_type {};
    template <typename _Type>
    struct has_title_member<_Type, std::void_t<
        decltype(std::declval<_Type>().title)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_icon_id_member : std::false_type {};
    template <typename _Type>
    struct has_icon_id_member<_Type, std::void_t<
        decltype(std::declval<_Type>().icon_id)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_width_member : std::false_type {};
    template <typename _Type>
    struct has_width_member<_Type, std::void_t<
        decltype(std::declval<_Type>().width)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_on_result_member : std::false_type {};
    template <typename _Type>
    struct has_on_result_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_result)
    >> : std::true_type {};

}   // namespace detail

template <typename _Type>
inline constexpr bool has_result_v =
    detail::has_result_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_buttons_v =
    detail::has_buttons_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_visible_v =
    detail::has_visible_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_modal_v =
    detail::has_modal_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_title_v =
    detail::has_title_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_icon_v =
    detail::has_icon_id_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_size_v =
    detail::has_width_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_on_result_v =
    detail::has_on_result_member<_Type>::value;

// is_dialog
//   type trait: has result + buttons + visible + modal + on_result.
template <typename _Type>
struct is_dialog : std::conjunction<
    detail::has_result_member<_Type>,
    detail::has_buttons_member<_Type>,
    detail::has_visible_member<_Type>,
    detail::has_modal_member<_Type>,
    detail::has_on_result_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_dialog_v =
    is_dialog<_Type>::value;

// is_titled_dialog
template <typename _Type>
struct is_titled_dialog : std::conjunction<
    is_dialog<_Type>,
    detail::has_title_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_titled_dialog_v =
    is_titled_dialog<_Type>::value;

// is_sized_dialog
template <typename _Type>
struct is_sized_dialog : std::conjunction<
    is_dialog<_Type>,
    detail::has_width_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_sized_dialog_v =
    is_sized_dialog<_Type>::value;

}   // namespace dialog_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DIALOG_