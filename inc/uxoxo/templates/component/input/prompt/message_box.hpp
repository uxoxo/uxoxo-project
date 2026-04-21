/*******************************************************************************
* uxoxo [component]                                              message_box.hpp
*
* Generic message box / popup builder:
*   A framework-agnostic, pure-data composite template for modal-or-
* modeless prompts that present a message and a small, fixed set of
* actions: confirmations, alerts, errors, "are you sure?" guards,
* "saved!" toasts, "please wait" busy boxes, and anything else the
* user-interface dimension calls a "message box", "popup", "alert",
* or "dialog".  Built atop the generic `composite<_Feat, _Slots...>`
* foundation in template_component.hpp so that actions are declared
* once as (tag, bit, value-type) triples and the builder handles
* conditional storage, access, iteration, and dispatch.
*
*   Where form_builder treats slots as input fields the user fills in,
* message_box treats slots as named actions the user can trigger to
* dismiss the box.  The action's value type is unconstrained — a
* `button<...>`, a `std::function<void(message_box&)>`, a custom
* command struct, or anything else.  When the renderer (or input
* handler) determines an action has been chosen, it calls
* `mb_trigger<some_tag>(box)`, which invokes the action callback
* (button-style on_click or direct invocation), records the action's
* slot bit into `box.result_bit`, fires the box-level on_action
* callback if installed, and (by default) dismisses the box.
*
*   Two feature bitfields keep concerns separated:
*     _MbFeat   — mbf_* bits for box-level optional features (title,
*                 detail body, lifecycle callbacks, busy state, ...).
*                 Each bit gates one EBO mixin so unselected features
*                 contribute zero bytes.
*     _SlotFeat — user-defined slot bits gating which action slots
*                 are enabled in this instantiation.  Identical
*                 meaning to the composite base's _Feat in form.
*
*   Always-present state (so renderers and the shared free functions
* in component_common.hpp dispatch structurally without reaching for
* a feature bit):
*     - enabled, visible, dismissed, modal, auto_dismiss_on_action
*     - message               (the primary body text)
*     - severity              (info / warning / error / ...)
*     - result_bit            (which action's slot bit fired, 0 if none)
*     - default_action_bit    (slot bit triggered by Enter / primary
*                              activation, 0 if none)
*     - cancel_action_bit     (slot bit triggered by Esc / outside
*                              click, 0 if none)
*
*   Zero overhead by construction — same guarantees as form_builder:
*     - Disabled action slots occupy zero bytes via EBO.
*     - Disabled mbf_* mixins occupy zero bytes via EBO.
*     - Every feature check resolves at compile time.
*     - No virtual dispatch, no type erasure, no allocation beyond
*       whatever the user's action types and std::function<...>
*       impose.
*
*   Usage pattern:
*
*     struct ok_tag      {};
*     struct cancel_tag  {};
*
*     constexpr unsigned cba_ok     = 1u << 0;
*     constexpr unsigned cba_cancel = 1u << 1;
*
*     template <unsigned _Slots = (cba_ok | cba_cancel),
*               unsigned _MbF   = (mbf_titled | mbf_dismiss_callback)>
*     using confirm_box = message_box<_MbF, _Slots,
*         slot<cba_ok,     field<ok_tag,     button<>>>,
*         slot<cba_cancel, field<cancel_tag, button<>>>>;
*
*     confirm_box<> box;
*     mb_set_title  (box, "Discard changes?");
*     mb_set_message(box, "Your edits will be lost.");
*     mb_set_default<ok_tag>    (box);
*     mb_set_cancel <cancel_tag>(box);
*     box.on_dismiss = [&box]
*     {
*         if (mb_was_triggered<ok_tag>(box))
*         {
*             discard_edits();
*         }
*     };
*
*     mb_show(box);
*     // ... later, when the user presses Enter:
*     mb_trigger_default(box);
*     // box.result_bit == cba_ok, box.dismissed == true.
*
* Contents:
*   1.   DSeverity                  — semantic severity level
*   2.   mbf_* feature flags        — bitmask for box-level features
*   3.   message_box_mixin          — EBO data mixins for optional features
*   4.   message_box                — composite + box-level state
*   5.   state mutators             — mb_enable / mb_disable / mb_set_modal
*   6.   visibility / lifecycle     — mb_show / mb_hide / mb_dismiss / queries
*   7.   message / title / severity — content setters
*   8.   action triggering          — mb_trigger<Tag> / mb_set_default<Tag>
*   9.   result inspection          — mb_was_triggered<Tag> / mb_result_bit
*   10.  aggregate operations       — mb_for_each_action / mb_reset
*   11.  traits                     — is_message_box_v / is_dismissable_v
*
*
* path:      /inc/uxoxo/templates/component/input/prompt/message_box.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_MESSAGE_BOX_
#define  UXOXO_MESSAGE_BOX_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../../uxoxo.hpp"
#include "../../component_traits.hpp"
#include "../../component_common.hpp"
#include "../../component_mixin.hpp"
#include "../../template_component.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  SEVERITY
// ===============================================================================

// DSeverity
//   enum: semantic severity / intent of the message being presented.
// Renderers map this to concrete styling (icon glyph, accent color,
// alert sound).  Lives here rather than in component_types.hpp because
// at present message_box is the sole consumer; if a second component
// (a status bar badge, a toast surface) takes a dependency, this enum
// should graduate to component_types.hpp per the convention noted at
// the top of that file.
enum class DSeverity : std::uint8_t
{
    none     = 0,  // no decoration; renderer picks neutral styling
    info     = 1,  // informational notice
    success  = 2,  // positive confirmation
    question = 3,  // user input solicited (yes/no, ok/cancel, ...)
    warning  = 4,  // caution; user should think before proceeding
    error    = 5   // failure / destructive condition
};




// ===============================================================================
//  2  FEATURE FLAGS
// ===============================================================================
//   Bitmask opt-in for box-level optional features.  Each bit gates
// exactly one EBO mixin base — unselected bits collapse to empty
// bases and contribute nothing to sizeof(message_box<>).  Slot bits
// live in a separate _SlotFeat parameter and are user-defined.

// mbf_none
//   constant: no optional box-level features.  The box still carries
// its always-present state (message, severity, modal, result_bit,
// ...) and any action slots declared in _Slots.
D_INLINE constexpr unsigned mbf_none             = 0u;

// mbf_titled
//   constant: box carries a `title` string (the header bar text).
// Pulls in component_mixin::label_data — the same shared mixin used
// by collapsible_panel and the input controls, so renderers that
// already know how to draw a labeled component need no special
// case.
D_INLINE constexpr unsigned mbf_titled           = 1u << 0;

// mbf_detail_text
//   constant: box carries a secondary `detail_text` body, rendered
// below the primary message.  Useful for long error explanations,
// stack traces in dev dialogs, or "read more" expansion bodies.
D_INLINE constexpr unsigned mbf_detail_text      = 1u << 1;

// mbf_show_callback
//   constant: box carries an `on_show()` callable, fired from mb_show
// when the box transitions from hidden to visible.  Takes no
// arguments — the application that called mb_show already has a
// reference to the box and can capture it in the lambda body.
D_INLINE constexpr unsigned mbf_show_callback    = 1u << 2;

// mbf_dismiss_callback
//   constant: box carries an `on_dismiss()` callable, fired from
// mb_dismiss when the box transitions to dismissed.  Takes no
// arguments; the lambda body inspects result_bit, remember_choice,
// or any other state through a captured reference to the box.
D_INLINE constexpr unsigned mbf_dismiss_callback = 1u << 3;

// mbf_action_callback
//   constant: box carries an `on_action(std::uint32_t bit)` callable,
// fired from mb_trigger every time an action is invoked.  The
// argument is the slot bit of the triggered action.  Distinct from
// on_dismiss: on_action fires per action even when
// auto_dismiss_on_action is false (e.g. an "Apply" button that
// reveals further controls without closing the box).
D_INLINE constexpr unsigned mbf_action_callback  = 1u << 4;

// mbf_remember_choice
//   constant: box carries a `remember_choice` boolean tracking
// whether the user has ticked a "don't show this again" checkbox.
// The box itself does not persist the choice — it only records the
// user's intent so the application can serialize it.
D_INLINE constexpr unsigned mbf_remember_choice  = 1u << 5;

// mbf_busy
//   constant: box carries a `busy` flag and `progress` value (0.0
// .. 1.0) for asynchronous workflows that present a "please wait"
// box while a long operation runs.  is_enabled() returns false
// while busy, suppressing accidental action triggers.
D_INLINE constexpr unsigned mbf_busy             = 1u << 6;

// mbf_default
//   constant: the typical feature set — a titled box with show /
// dismiss / action callbacks.  Suitable for most interactive
// confirmations and alerts.
D_INLINE constexpr unsigned mbf_default          =
    ( mbf_titled           |
      mbf_show_callback    |
      mbf_dismiss_callback |
      mbf_action_callback  );

// mbf_standard
//   constant: mbf_default plus a detail body and remember-choice
// support — the full-featured configuration suitable for desktop
// GUI dialogs.
D_INLINE constexpr unsigned mbf_standard         =
    ( mbf_default          |
      mbf_detail_text      |
      mbf_remember_choice  );




// ===============================================================================
//  3  MESSAGE BOX MIXINS
// ===============================================================================
//   EBO data mixins local to message_box.  Each follows the same
// primary-empty + true-specialization pattern as component_mixin.
// Shared mixins (label_data for the title) are pulled in from
// component_mixin; only the box-specific ones live here.

namespace message_box_mixin {

// detail_text_data
//   mixin: secondary body text shown beneath the primary message.
template <bool _Enable>
struct detail_text_data
{};

template <>
struct detail_text_data<true>
{
    std::string detail_text;
};


// show_callback_data
//   mixin: on_show callable, invoked when the box transitions from
// hidden to visible.  Takes no arguments — the caller (typically
// the application code that is showing the box) already has a
// reference to the box and can capture it in the lambda if needed.
// Same convention as popover's lifecycle_fn.
template <bool _Enable>
struct show_callback_data
{};

template <>
struct show_callback_data<true>
{
    std::function<void()> on_show;
};


// dismiss_callback_data
//   mixin: on_dismiss callable, invoked when the box transitions
// to dismissed.  Takes no arguments; the application captures
// whichever box it needs to inspect in the lambda body, where
// it can read result_bit, remember_choice, etc.
template <bool _Enable>
struct dismiss_callback_data
{};

template <>
struct dismiss_callback_data<true>
{
    std::function<void()> on_dismiss;
};


// action_callback_data
//   mixin: on_action callable, invoked every time an action is
// triggered via mb_trigger.  The std::uint32_t argument is the
// slot bit of the triggered action — the only piece of per-call
// information the box itself can supply without a reference to
// the (incomplete-during-mixin-instantiation) box type.
template <bool _Enable>
struct action_callback_data
{};

template <>
struct action_callback_data<true>
{
    std::function<void(std::uint32_t)> on_action;
};


// remember_choice_data
//   mixin: tracks the state of a "don't show this again" toggle.
// The application is responsible for persisting and consulting
// this value across sessions.
template <bool _Enable>
struct remember_choice_data
{};

template <>
struct remember_choice_data<true>
{
    bool remember_choice = false;
};


// busy_data
//   mixin: async-workflow state.  When `busy` is true, the box
// suppresses action triggering (mb_trigger short-circuits) and
// renderers are expected to surface a progress indicator using
// `progress` (0.0 == none, 1.0 == complete; values outside [0, 1]
// indicate indeterminate progress).
template <bool _Enable>
struct busy_data
{};

template <>
struct busy_data<true>
{
    bool  busy     = false;
    float progress = -1.0f;  // -1 == indeterminate
};

}   // namespace message_box_mixin




// ===============================================================================
//  4  MESSAGE BOX
// ===============================================================================
//   The composite half (EBO storage for each action slot) comes from
// the `composite<_SlotFeat, _Slots...>` base; the box-level mixin
// stack is layered alongside.  Always-present box state is added as
// direct members.
//
//   _MbFeat   — bitwise-OR of mbf_* bits selecting which optional
//               box-level features are present.
//   _SlotFeat — bitwise-OR of user-defined slot bits selecting which
//               action slots are present.  Identical meaning to the
//               composite base's _Feat in form.
//   _Slots    — variadic pack of `slot<...>` declarations binding
//               action tags to action value types.

// message_box
//   class: generic multi-action prompt with title, message, severity,
// dismissal lifecycle, and per-action triggering.  Derives from
// composite to gain conditional EBO-collapsed storage for its action
// slots, and from the message_box_mixin chain to gain conditional
// storage for box-level optional features.
template <unsigned    _MbFeat,
          unsigned    _SlotFeat,
          typename... _Slots>
struct message_box
    : composite<_SlotFeat, _Slots...>,
      component_mixin::label_data<(_MbFeat & mbf_titled) != 0u>,
      message_box_mixin::detail_text_data<
          (_MbFeat & mbf_detail_text) != 0u>
    , message_box_mixin::show_callback_data<
          (_MbFeat & mbf_show_callback) != 0u>
    , message_box_mixin::dismiss_callback_data<
          (_MbFeat & mbf_dismiss_callback) != 0u>
    , message_box_mixin::action_callback_data<
          (_MbFeat & mbf_action_callback) != 0u>
    , message_box_mixin::remember_choice_data<
          (_MbFeat & mbf_remember_choice) != 0u>
    , message_box_mixin::busy_data<
          (_MbFeat & mbf_busy) != 0u>
{
    // -- type aliases -------------------------------------------------
    using self_type = message_box<_MbFeat, _SlotFeat, _Slots...>;
    using base_type = composite<_SlotFeat, _Slots...>;

    // -- compile-time feature surface ---------------------------------
    static constexpr unsigned mb_features   = _MbFeat;
    static constexpr unsigned slot_features = _SlotFeat;
    static constexpr bool     focusable     = true;

    static constexpr bool has_title           =
        (_MbFeat & mbf_titled)           != 0u;
    static constexpr bool has_detail          =
        (_MbFeat & mbf_detail_text)      != 0u;
    static constexpr bool has_on_show         =
        (_MbFeat & mbf_show_callback)    != 0u;
    static constexpr bool has_on_dismiss      =
        (_MbFeat & mbf_dismiss_callback) != 0u;
    static constexpr bool has_on_action       =
        (_MbFeat & mbf_action_callback)  != 0u;
    static constexpr bool has_remember_choice =
        (_MbFeat & mbf_remember_choice)  != 0u;
    static constexpr bool has_busy            =
        (_MbFeat & mbf_busy)             != 0u;

    // -- always-present box state -------------------------------------
    bool          enabled                = true;
    bool          visible                = false;
    bool          dismissed              = false;
    bool          modal                  = true;
    bool          auto_dismiss_on_action = true;
    DSeverity     severity               = DSeverity::none;
    std::string   message;

    // -- result tracking ----------------------------------------------
    //   `result_bit` holds the slot bit of the action that caused
    // the most recent dismissal (or 0 if none has fired yet).
    //   `default_action_bit` and `cancel_action_bit` name the slots
    // whose actions are triggered by mb_trigger_default and
    // mb_trigger_cancel respectively.  Set via mb_set_default<Tag>
    // / mb_set_cancel<Tag>; 0 means "no default / cancel mapping".
    std::uint32_t result_bit             = 0u;
    std::uint32_t default_action_bit     = 0u;
    std::uint32_t cancel_action_bit      = 0u;

    // -- construction -------------------------------------------------
    message_box() = default;

    // is_enabled
    //   query: the box is "interactive-enabled" only when the raw
    // enabled flag is set, the box is visible, and (when mbf_busy
    // is selected) the box is not mid-async-workflow.
    [[nodiscard]] bool
    is_enabled() const noexcept
    {
        if constexpr (has_busy)
        {
            return ( enabled && visible && !this->busy );
        }
        else
        {
            return ( enabled && visible );
        }
    }
};




// ===============================================================================
//  5  STATE MUTATORS
// ===============================================================================

// mb_enable
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_enable(
    message_box<_M, _SF, _S...>& _mb
)
{
    _mb.enabled = true;

    return;
}

// mb_disable
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_disable(
    message_box<_M, _SF, _S...>& _mb
)
{
    _mb.enabled = false;

    return;
}

// mb_set_modal
//   function: marks the box as modal (true) or modeless (false).
// The box itself does not enforce modality — the renderer is
// responsible for blocking input to surrounding UI.  This flag
// merely records the intent.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_modal(
    message_box<_M, _SF, _S...>& _mb,
    bool                         _modal
)
{
    _mb.modal = _modal;

    return;
}

// mb_set_auto_dismiss
//   function: controls whether mb_trigger automatically dismisses
// the box after invoking the chosen action.  Default true (the
// usual confirmation-box behavior); set false for boxes where
// actions are non-terminal (e.g. "Apply" buttons that should not
// close the dialog).
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_auto_dismiss(
    message_box<_M, _SF, _S...>& _mb,
    bool                         _auto
)
{
    _mb.auto_dismiss_on_action = _auto;

    return;
}




// ===============================================================================
//  6  VISIBILITY / LIFECYCLE
// ===============================================================================

NS_INTERNAL

    // fire_show
    //   function: invokes the box's on_show callback when the
    // mbf_show_callback feature is selected and a callable is
    // installed.  No-op otherwise.
    template <unsigned    _M,
              unsigned    _SF,
              typename... _S>
    void
    fire_show(
        message_box<_M, _SF, _S...>& _mb
    )
    {
        if constexpr ((_M & mbf_show_callback) != 0u)
        {
            if (_mb.on_show)
            {
                _mb.on_show();
            }
        }

        return;
    }

    // fire_dismiss
    //   function: invokes the box's on_dismiss callback when the
    // mbf_dismiss_callback feature is selected and a callable is
    // installed.  No-op otherwise.
    template <unsigned    _M,
              unsigned    _SF,
              typename... _S>
    void
    fire_dismiss(
        message_box<_M, _SF, _S...>& _mb
    )
    {
        if constexpr ((_M & mbf_dismiss_callback) != 0u)
        {
            if (_mb.on_dismiss)
            {
                _mb.on_dismiss();
            }
        }

        return;
    }

    // fire_action
    //   function: invokes the box's on_action callback when the
    // mbf_action_callback feature is selected and a callable is
    // installed.  The triggered slot bit is passed as the sole
    // argument.  No-op otherwise.
    template <unsigned    _M,
              unsigned    _SF,
              typename... _S>
    void
    fire_action(
        message_box<_M, _SF, _S...>& _mb,
        std::uint32_t                _bit
    )
    {
        if constexpr ((_M & mbf_action_callback) != 0u)
        {
            if (_mb.on_action)
            {
                _mb.on_action(_bit);
            }
        }

        return;
    }

NS_END  // internal


/*
mb_show
  Presents the box to the user.  Resets transient state (result_bit,
dismissed) so the box can be re-used across multiple show / dismiss
cycles, sets visible to true, and fires on_show iff mbf_show_callback
is selected and a callable is installed.

  Calling mb_show on an already-visible box is idempotent: the box
remains visible, the result is reset, and on_show fires again.  This
mirrors the behavior of HTML <dialog>.showModal().

Parameter(s):
  _mb: the box to show.
Return:
  none.
*/
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_show(
    message_box<_M, _SF, _S...>& _mb
)
{
    _mb.visible    = true;
    _mb.dismissed  = false;
    _mb.result_bit = 0u;

    internal::fire_show(_mb);

    return;
}

// mb_hide
//   function: hides the box without marking it as dismissed.  Useful
// for transiently obscuring a box (e.g. behind a settings pane) that
// should remain logically open.  Does NOT fire on_dismiss.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_hide(
    message_box<_M, _SF, _S...>& _mb
)
{
    _mb.visible = false;

    return;
}

/*
mb_dismiss
  Closes the box for good.  Sets visible to false, marks dismissed
true, and fires on_dismiss iff mbf_dismiss_callback is selected and
a callable is installed.  Does NOT touch result_bit — callers that
arrive here through mb_trigger see the triggered action's bit; direct
mb_dismiss calls (e.g. from outside-click handling) leave result_bit
at 0, signalling "dismissed without choosing an action".

  Calling mb_dismiss on an already-dismissed box is idempotent: state
remains dismissed, but on_dismiss does NOT re-fire.  This prevents
double-firing when the user triggers an action and the renderer also
detects an outside click on the same frame.

Parameter(s):
  _mb: the box to dismiss.
Return:
  true if the box transitioned from visible to dismissed (and
  on_dismiss fired); false if the box was already dismissed.
*/
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
bool
mb_dismiss(
    message_box<_M, _SF, _S...>& _mb
)
{
    if (_mb.dismissed)
    {
        return false;
    }

    _mb.visible   = false;
    _mb.dismissed = true;

    internal::fire_dismiss(_mb);

    return true;
}

// mb_is_visible
//   query: whether the box is currently visible.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
[[nodiscard]] bool
mb_is_visible(
    const message_box<_M, _SF, _S...>& _mb
) noexcept
{
    return _mb.visible;
}

// mb_is_dismissed
//   query: whether the box has been dismissed.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
[[nodiscard]] bool
mb_is_dismissed(
    const message_box<_M, _SF, _S...>& _mb
) noexcept
{
    return _mb.dismissed;
}




// ===============================================================================
//  7  CONTENT SETTERS  (message / title / severity / detail)
// ===============================================================================

// mb_set_message
//   function: replaces the primary body text.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_message(
    message_box<_M, _SF, _S...>& _mb,
    std::string                  _text
)
{
    _mb.message = std::move(_text);

    return;
}

// mb_set_severity
//   function: replaces the severity / intent of the message.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_severity(
    message_box<_M, _SF, _S...>& _mb,
    DSeverity                    _sev
)
{
    _mb.severity = _sev;

    return;
}

// mb_set_title
//   function: replaces the title bar text.  Compile error if the
// box was instantiated without mbf_titled.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_title(
    message_box<_M, _SF, _S...>& _mb,
    std::string                  _title
)
{
    static_assert((_M & mbf_titled) != 0u,
                  "mb_set_title requires the mbf_titled feature.");

    _mb.label = std::move(_title);

    return;
}

// mb_set_detail
//   function: replaces the secondary body text.  Compile error if
// the box was instantiated without mbf_detail_text.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_detail(
    message_box<_M, _SF, _S...>& _mb,
    std::string                  _detail
)
{
    static_assert((_M & mbf_detail_text) != 0u,
                  "mb_set_detail requires the mbf_detail_text feature.");

    _mb.detail_text = std::move(_detail);

    return;
}

// mb_set_busy
//   function: marks the box as busy / not-busy and updates the
// progress hint.  Pass progress < 0 for indeterminate.  Compile
// error if the box was instantiated without mbf_busy.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_busy(
    message_box<_M, _SF, _S...>& _mb,
    bool                         _busy,
    float                        _progress = -1.0f
)
{
    static_assert((_M & mbf_busy) != 0u,
                  "mb_set_busy requires the mbf_busy feature.");

    _mb.busy     = _busy;
    _mb.progress = _progress;

    return;
}




// ===============================================================================
//  8  ACTION TRIGGERING
// ===============================================================================
//   The core dispatch path: name an action by tag, invoke its
// payload, record which bit fired, fire the box-level on_action
// hook, and (by default) dismiss the box.

NS_INTERNAL

    // -- has_on_click detector (local, narrow)  -----------------------
    //   button.hpp's button_traits namespace provides a similar
    // detector but pulling the whole header in for this single check
    // would be overreach.  This local detector matches any type that
    // exposes an `on_click` callable member.
    template <typename, typename = void>
    struct mb_has_on_click : std::false_type
    {};

    template <typename _Type>
    struct mb_has_on_click<_Type, std::void_t<
        decltype(std::declval<_Type&>().on_click)
    >> : std::true_type
    {};


    /*
    invoke_action
      Invokes an action payload using the best available calling
    convention, in preference order:
      1. If the payload has an `on_click` callable member (button-
         style), invoke it with the payload itself as the argument
         (matching button<>'s click_fn signature `void(self_type&)`).
      2. If the payload is invocable with no arguments, invoke it
         directly (matches std::function<void()>, plain function
         pointers, lambdas, etc.).
      3. If the payload is invocable with the box reference, invoke
         it that way (for rich actions that want to inspect box
         state).
      4. Otherwise, do nothing — the action is opaque-data-only and
         exists purely so the renderer can present it; the on_action
         box-level callback is the dispatch path in that case.

    Parameter(s):
      _action: the action payload to invoke.
      _box:    the owning message_box, passed to box-aware actions.
    Return:
      none.
    */
    template <typename _Action,
              typename _Box>
    void
    invoke_action(
        _Action& _action,
        _Box&    _box
    )
    {
        if constexpr (mb_has_on_click<_Action>::value)
        {
            if (_action.on_click)
            {
                _action.on_click(_action);
            }
        }
        else if constexpr (std::is_invocable_v<_Action&>)
        {
            _action();
        }
        else if constexpr (std::is_invocable_v<_Action&, _Box&>)
        {
            _action(_box);
        }
        else
        {
            (void)_action;
            (void)_box;
        }

        return;
    }

NS_END  // internal


/*
mb_trigger
  Invokes the action stored at slot `_Tag`.  The dispatch is:
    1. Short-circuit with false if the box is not interactive-enabled
       (disabled, hidden, or busy).
    2. Look up the slot by tag (compile error if `_Tag` is not
       registered or its slot bit is not set in `_SlotFeat`).
    3. Invoke the action payload via internal::invoke_action.
    4. Record the slot's bit into result_bit.
    5. Fire on_action iff mbf_action_callback is selected.
    6. If auto_dismiss_on_action is true, dismiss the box (firing
       on_dismiss iff mbf_dismiss_callback is selected).

  Returning true indicates the action was triggered; false means
the box was ineligible (disabled / hidden / busy).  A `true` return
does NOT imply a payload callback fired — opaque-data actions
return true without ever invoking anything (the on_action callback
is the dispatch path in that case).

Parameter(s):
  _mb: the box to trigger an action on.
Return:
  true if the action was triggered; false if the box was ineligible.
*/
template <typename    _Tag,
          unsigned    _M,
          unsigned    _SF,
          typename... _S>
bool
mb_trigger(
    message_box<_M, _SF, _S...>& _mb
)
{
    using lookup = internal::slot_for_tag<_Tag, _S...>;

    static_assert(lookup::found,
                  "mb_trigger: tag is not registered in this message_box.");
    static_assert(((_SF & lookup::bit) != 0u),
                  "mb_trigger: tag's slot is not enabled in this message_box.");

    if (!_mb.is_enabled())
    {
        return false;
    }

    auto& action = tc_get<_Tag>(_mb);

    internal::invoke_action(action, _mb);

    _mb.result_bit = lookup::bit;

    internal::fire_action(_mb, lookup::bit);

    if (_mb.auto_dismiss_on_action)
    {
        mb_dismiss(_mb);
    }

    return true;
}

/*
mb_set_default
  Records `_Tag`'s slot as the default action — the one triggered
by mb_trigger_default (typically wired to Enter / primary keyboard
activation).

  Compile errors are issued if `_Tag` is not registered in the box's
slot pack, or if the slot is registered but its feature bit is not
set in `_SlotFeat`.

Parameter(s):
  _mb: the box to update.
Return:
  none.
*/
template <typename    _Tag,
          unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_default(
    message_box<_M, _SF, _S...>& _mb
)
{
    using lookup = internal::slot_for_tag<_Tag, _S...>;

    static_assert(lookup::found,
                  "mb_set_default: tag is not registered.");
    static_assert(((_SF & lookup::bit) != 0u),
                  "mb_set_default: tag's slot is not enabled.");

    _mb.default_action_bit = lookup::bit;

    return;
}

/*
mb_set_cancel
  Records `_Tag`'s slot as the cancel action — the one triggered
by mb_trigger_cancel (typically wired to Esc / outside-click /
window-close).

  Compile errors are issued if `_Tag` is not registered in the box's
slot pack, or if the slot is registered but its feature bit is not
set in `_SlotFeat`.

Parameter(s):
  _mb: the box to update.
Return:
  none.
*/
template <typename    _Tag,
          unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_set_cancel(
    message_box<_M, _SF, _S...>& _mb
)
{
    using lookup = internal::slot_for_tag<_Tag, _S...>;

    static_assert(lookup::found,
                  "mb_set_cancel: tag is not registered.");
    static_assert(((_SF & lookup::bit) != 0u),
                  "mb_set_cancel: tag's slot is not enabled.");

    _mb.cancel_action_bit = lookup::bit;

    return;
}

NS_INTERNAL

    /*
    trigger_by_bit
      Walks the slot pack at compile time looking for the slot whose
    bit matches `_bit`, and invokes mb_trigger on the matching tag.
    Used by mb_trigger_default and mb_trigger_cancel, which know the
    bit but not the tag at the call site.

      A linear walk through the (compile-time-fixed) slot pack is
    fine here: the pack is small (typically 1-4 slots) and the
    walk is a fold of `if`s that the compiler reduces to a single
    switch.

    Parameter(s):
      _mb:  the box to trigger an action on.
      _bit: the slot bit identifying the action to trigger.
    Return:
      true if a slot with matching bit was found AND the trigger
      succeeded; false if no slot matched or the trigger was
      ineligible.
    */
    template <unsigned    _M,
              unsigned    _SF,
              typename... _S>
    bool
    trigger_by_bit(
        message_box<_M, _SF, _S...>& _mb,
        std::uint32_t                _bit
    )
    {
        bool fired;

        // bit 0 means "no mapping"; nothing to do
        if (_bit == 0u)
        {
            return false;
        }

        fired = false;

        // fold over the slot pack: invoke mb_trigger on the slot
        // whose bit matches and whose feature bit is set
        ( ( ((_SF & _S::bit) != 0u) && (_S::bit == _bit) &&
                (fired = mb_trigger<typename _S::tag_type>(_mb), true)
          ), ... );

        return fired;
    }

NS_END  // internal


/*
mb_trigger_default
  Triggers the action whose slot was registered via mb_set_default.
No-op (returns false) if no default has been set, if the bit does
not correspond to an enabled slot, or if the box is not currently
interactive-enabled.

Parameter(s):
  _mb: the box to trigger the default action on.
Return:
  true if the default action fired; false otherwise.
*/
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
bool
mb_trigger_default(
    message_box<_M, _SF, _S...>& _mb
)
{
    return internal::trigger_by_bit(_mb, _mb.default_action_bit);
}

/*
mb_trigger_cancel
  Triggers the action whose slot was registered via mb_set_cancel.
No-op (returns false) if no cancel has been set, if the bit does
not correspond to an enabled slot, or if the box is not currently
interactive-enabled.

  When no cancel action is set, callers that nonetheless need to
honor an Esc key or outside-click should fall back to mb_dismiss
directly — that path leaves result_bit at 0, signalling
"dismissed without choosing an action".

Parameter(s):
  _mb: the box to trigger the cancel action on.
Return:
  true if the cancel action fired; false otherwise.
*/
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
bool
mb_trigger_cancel(
    message_box<_M, _SF, _S...>& _mb
)
{
    return internal::trigger_by_bit(_mb, _mb.cancel_action_bit);
}




// ===============================================================================
//  9  RESULT INSPECTION
// ===============================================================================

// mb_result_bit
//   query: returns the slot bit of the most recently triggered
// action, or 0 if no action has fired since the last mb_show.
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
[[nodiscard]] std::uint32_t
mb_result_bit(
    const message_box<_M, _SF, _S...>& _mb
) noexcept
{
    return _mb.result_bit;
}

/*
mb_was_triggered
  Type-safe query: returns true iff the most recently triggered
action was the one stored at slot `_Tag`.  Compile error if `_Tag`
is not registered in the box.

Parameter(s):
  _mb: the box to query.
Return:
  true iff result_bit equals `_Tag`'s slot bit; false otherwise.
*/
template <typename    _Tag,
          unsigned    _M,
          unsigned    _SF,
          typename... _S>
[[nodiscard]] bool
mb_was_triggered(
    const message_box<_M, _SF, _S...>& _mb
) noexcept
{
    using lookup = internal::slot_for_tag<_Tag, _S...>;

    static_assert(lookup::found,
                  "mb_was_triggered: tag is not registered.");

    return ( _mb.result_bit == lookup::bit );
}




// ===============================================================================
//  10  AGGREGATE OPERATIONS
// ===============================================================================

/*
mb_for_each_action
  Thin re-export of tc_for_each_enabled scoped to message boxes.
Exists so users can write box-centric code without reaching into the
composite base namespace, and so that future box-only pre / post
processing can hook here without breaking callers.

  The callable receives each enabled action's value by reference,
in slot-declaration order.  Disabled slots are skipped at compile
time and contribute no code.  Use `if constexpr` with the component
traits inside the lambda body to dispatch per action type.

Parameter(s):
  _mb: the box to iterate.
  _fn: a callable invoked once per enabled action, receiving the
       action's value by reference.
Return:
  none.
*/
template <unsigned    _M,
          unsigned    _SF,
          typename... _S,
          typename    _Fn>
void
mb_for_each_action(
    message_box<_M, _SF, _S...>& _mb,
    _Fn&&                        _fn
)
{
    tc_for_each_enabled(_mb, std::forward<_Fn>(_fn));

    return;
}

// mb_for_each_action (const overload)
template <unsigned    _M,
          unsigned    _SF,
          typename... _S,
          typename    _Fn>
void
mb_for_each_action(
    const message_box<_M, _SF, _S...>& _mb,
    _Fn&&                              _fn
)
{
    tc_for_each_enabled(_mb, std::forward<_Fn>(_fn));

    return;
}

/*
mb_reset
  Restores the box to a fresh post-construction state without
disturbing slot payloads, callbacks, title, default / cancel bit
mappings, or the message text.  Specifically:
    - clears result_bit to 0
    - clears dismissed to false
    - clears visible to false
    - clears busy to false (if mbf_busy is selected)
    - clears remember_choice to false (if mbf_remember_choice
      is selected)

  Use this between successive presentations of the same box when
the application wants to re-show without reconstructing.  Does NOT
fire on_dismiss — reset is purely a state wipe, not a lifecycle
event.

Parameter(s):
  _mb: the box to reset.
Return:
  none.
*/
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
void
mb_reset(
    message_box<_M, _SF, _S...>& _mb
)
{
    _mb.result_bit = 0u;
    _mb.dismissed  = false;
    _mb.visible    = false;

    if constexpr ((_M & mbf_busy) != 0u)
    {
        _mb.busy     = false;
        _mb.progress = -1.0f;
    }

    if constexpr ((_M & mbf_remember_choice) != 0u)
    {
        _mb.remember_choice = false;
    }

    return;
}




// ===============================================================================
//  11  TRAITS
// ===============================================================================

NS_INTERNAL

    // is_message_box_impl
    //   trait: structural detector for instantiations of `message_box`.
    template <typename>
    struct is_message_box_impl : std::false_type
    {};

    // is_message_box_impl<message_box<...>>
    //   trait: positive specialization matched when _Type is (exactly)
    // a message_box<_MbFeat, _SlotFeat, _Slots...>.
    template <unsigned    _M,
              unsigned    _SF,
              typename... _S>
    struct is_message_box_impl<message_box<_M, _SF, _S...>> : std::true_type
    {};

    // has_message_member
    //   trait: detects whether a type has a `message` body string.
    template <typename, typename = void>
    struct has_message_member : std::false_type
    {};

    template <typename _Type>
    struct has_message_member<_Type, std::void_t<
        decltype(std::declval<_Type>().message)
    >> : std::true_type
    {};

    // has_severity_member
    //   trait: detects whether a type has a severity classifier.
    template <typename, typename = void>
    struct has_severity_member : std::false_type
    {};

    template <typename _Type>
    struct has_severity_member<_Type, std::void_t<
        decltype(std::declval<_Type>().severity)
    >> : std::true_type
    {};

    // has_dismissed_member
    //   trait: detects whether a type carries a dismissed flag.
    template <typename, typename = void>
    struct has_dismissed_member : std::false_type
    {};

    template <typename _Type>
    struct has_dismissed_member<_Type, std::void_t<
        decltype(std::declval<_Type>().dismissed)
    >> : std::true_type
    {};

    // has_result_bit_member
    //   trait: detects whether a type carries a result_bit field.
    template <typename, typename = void>
    struct has_result_bit_member : std::false_type
    {};

    template <typename _Type>
    struct has_result_bit_member<_Type, std::void_t<
        decltype(std::declval<_Type>().result_bit)
    >> : std::true_type
    {};

    // has_on_dismiss_member
    //   trait: detects whether a type has an on_dismiss callback.
    template <typename, typename = void>
    struct has_on_dismiss_member : std::false_type
    {};

    template <typename _Type>
    struct has_on_dismiss_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_dismiss)
    >> : std::true_type
    {};

NS_END  // internal

// is_message_box_v
//   trait: true iff _Type is an instantiation of `message_box`.
template <typename _Type>
D_INLINE constexpr bool is_message_box_v =
    internal::is_message_box_impl<_Type>::value;

// has_message_v
//   trait: true iff _Type carries a `message` body string.
template <typename _Type>
D_INLINE constexpr bool has_message_v =
    internal::has_message_member<_Type>::value;

// has_severity_v
//   trait: true iff _Type carries a severity classifier.
template <typename _Type>
D_INLINE constexpr bool has_severity_v =
    internal::has_severity_member<_Type>::value;

// has_dismissed_v
//   trait: true iff _Type carries a dismissed flag.
template <typename _Type>
D_INLINE constexpr bool has_dismissed_v =
    internal::has_dismissed_member<_Type>::value;

// has_result_bit_v
//   trait: true iff _Type carries a result_bit field.
template <typename _Type>
D_INLINE constexpr bool has_result_bit_v =
    internal::has_result_bit_member<_Type>::value;

// has_on_dismiss_v
//   trait: true iff _Type carries an on_dismiss callback.
template <typename _Type>
D_INLINE constexpr bool has_on_dismiss_v =
    internal::has_on_dismiss_member<_Type>::value;

// is_message_box_like
//   trait: structural "message-box-like" — has message + severity +
// dismissed + result_bit + enabled + visible.  Matches message_box<>
// and any user-defined composite that replicates the box-level state
// surface.  Useful for renderers that want to draw any "message box"
// regardless of whether it derives from this template.
template <typename _Type>
struct is_message_box_like : std::conjunction<
    internal::has_message_member<_Type>,
    internal::has_severity_member<_Type>,
    internal::has_dismissed_member<_Type>,
    internal::has_result_bit_member<_Type>,
    std::bool_constant<has_enabled_v<_Type>>,
    std::bool_constant<has_visible_v<_Type>>
>
{};

template <typename _Type>
D_INLINE constexpr bool is_message_box_like_v =
    is_message_box_like<_Type>::value;

// is_dismissable_message_box
//   trait: a message_box-like type that also carries an on_dismiss
// callback, i.e. one instantiated with mbf_dismiss_callback.
template <typename _Type>
struct is_dismissable_message_box : std::conjunction<
    is_message_box_like<_Type>,
    internal::has_on_dismiss_member<_Type>
>
{};

template <typename _Type>
D_INLINE constexpr bool is_dismissable_message_box_v =
    is_dismissable_message_box<_Type>::value;


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_MESSAGE_BOX_