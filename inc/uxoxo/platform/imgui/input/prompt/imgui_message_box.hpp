/******************************************************************************
* uxoxo [imgui]                                          imgui_message_box.hpp
*
* ImGui backend for message_box:
*   A single free function `render(message_box&)` that fully drives a
* message box through Dear ImGui's modal-popup API.  The renderer
* opens the popup the first frame after mb_show, draws the chrome
* (title bar, severity icon, message text, optional detail body,
* optional remember-choice toggle, optional busy / progress
* indicator), renders the default and cancel action buttons, wires
* keyboard activation (Enter -> default, Esc -> cancel), and returns
* a `message_box_event` struct describing what happened during the
* frame.
*
*   The renderer does not iterate the box's full action slot pack —
* the slot bits and tags are user-defined and there is no general way
* to derive a button label or click target from a slot's value type
* alone.  Instead, the box's `default_action_bit` and
* `cancel_action_bit` fields name two slots (set via mb_set_default<>
* / mb_set_cancel<>) and the renderer draws labeled buttons for
* them.  Action slots that are neither default nor cancel must be
* rendered by the caller via a custom button followed by a manual
* mb_trigger<> invocation.
*
*   The 95th-percentile message box is "OK / Cancel" or "Yes / No",
* both of which the default+cancel pair covers natively.  Multi-
* action dialogs (Save / Discard / Cancel) supply their own per-slot
* buttons in addition to the default+cancel pair the renderer draws.
*
*   Severity → icon mapping is intentionally simple — a leading
* glyph character followed by a space.  Renderers that want a richer
* style (image, color tint, system icon) should supply a wrapper
* that draws the chrome themselves and calls render() with severity
* none, suppressing the built-in icon.
*
*   ImGui dependencies:
*     - <imgui.h> for the core API.  No imgui_stdlib dependency —
*       all string fields are read-only inside this renderer.
*
*
* path:      /inc/uxoxo/platform/imgui/input/prompt/imgui_message_box.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.20
******************************************************************************/

#ifndef  UXOXO_COMPONENT_IMGUI_MESSAGE_BOX_
#define  UXOXO_COMPONENT_IMGUI_MESSAGE_BOX_ 1

// std
#include <cfloat>
#include <cstdint>
#include <string>
#include <utility>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../input/prompt/message_box.hpp"
#include "../../../platform/imgui/core/imgui_render_event.hpp"


NS_UXOXO
NS_IMGUI


using uxoxo::component::message_box;
using uxoxo::component::DSeverity;


// ===============================================================================
//  1  EVENT STRUCT
// ===============================================================================

// message_box_event
//   struct: per-frame event report returned from rendering a
// message_box.  All flags default to false; `triggered_bit` defaults
// to 0 (the "no action" sentinel used throughout message_box).
// Callers may inspect any subset or ignore the entire struct — the
// box's own state (visible, dismissed, result_bit) is the source of
// truth for cross-frame queries.
//
//   Inherits the shared render_event base.  `dismissed` is
// inherited (same name, same semantics, no caller breakage); the
// renderer continues to write `evt.dismissed = true` and reads
// resolve to the inherited member.  `triggered` is kept as the
// canonical "any action button fired" surface for message_box; the
// renderer also writes `evt.committed = evt.triggered` so generic
// callers can probe the base's `committed` flag if they don't care
// about the specific action bit.
//
//   Migration note (2026.05.08): the previous standalone `dismissed`
// member is now inherited from render_event.  No caller-visible
// change.  The renderer additionally populates base `committed` (=
// triggered) and calls summarise() at end-of-frame.
struct message_box_event : render_event
{
    bool          triggered      = false;  // any action fired this frame
    std::uint32_t triggered_bit  = 0u;     // slot bit of the fired action
    bool          enter_pressed  = false;  // Enter was pressed in the popup
    bool          escape_pressed = false;  // Esc was pressed in the popup
};




// ===============================================================================
//  2  SEVERITY ICON
// ===============================================================================

NS_INTERNAL

    // severity_glyph
    //   function: maps a DSeverity to a leading glyph rendered before
    // the message body.  Returns nullptr for DSeverity::none so the
    // renderer can skip the icon row entirely.  The glyphs use only
    // ASCII punctuation so they render correctly with the default
    // ImGui font; renderers wanting richer icons should suppress the
    // built-in icon and draw their own.
    inline const char*
    severity_glyph(
        DSeverity _sev
    ) noexcept
    {
        switch (_sev)
        {
            case DSeverity::info:     return "[i]";
            case DSeverity::success:  return "[+]";
            case DSeverity::question: return "[?]";
            case DSeverity::warning:  return "[!]";
            case DSeverity::error:    return "[x]";

            case DSeverity::none:
            default:
                return nullptr;
        }
    }

NS_END  // internal




// ===============================================================================
//  3  RENDER
// ===============================================================================

/*
render
  Draws the message box for one ImGui frame and dispatches user
input back into the box via the mb_* mutators.  The box is
rendered as an ImGui popup — modal when `_mb.modal == true`, plain
otherwise — opened via ImGui::OpenPopup the first frame after
mb_show set `visible` to true.

  Chrome rendered, in order:
    1. Severity icon glyph (skipped when severity == none).
    2. Primary `message` body via ImGui::TextWrapped.
    3. Detail body via ImGui::TextWrapped, if mbf_detail_text is
       selected and `detail_text` is non-empty.
    4. Busy indicator via ImGui::ProgressBar, if mbf_busy is
       selected and `busy == true`.
    5. Remember-choice checkbox, if mbf_remember_choice is selected.
    6. Default and cancel action buttons on a single row, labeled
       with the user-supplied strings.

  Input handling:
    - Clicking the default button fires the action whose slot bit
      matches `default_action_bit` via the message_box dispatch
      path (records result_bit, fires on_action, dismisses if
      auto_dismiss_on_action is true).
    - Clicking the cancel button does the same for cancel_action_bit;
      if no cancel is set the renderer falls back to mb_dismiss
      directly.
    - Pressing Enter inside the popup triggers the default action.
    - Pressing Esc inside the popup triggers the cancel action (or
      falls back to mb_dismiss as above).
    - The popup closing externally (window close, ImGui's built-in
      Esc handling on modal popups) calls mb_dismiss to keep the
      box's state coherent.

Parameter(s):
  _mb:            the message box to render.  All mb_* mutations
                  performed by the renderer are applied directly;
                  the box is the source of truth across frames.
  _default_label: button label for the default action.  Only
                  rendered when `default_action_bit != 0`.
  _cancel_label:  button label for the cancel action.  Only
                  rendered when `cancel_action_bit != 0`.
Return:
  A message_box_event describing what changed during the frame.
*/
template <unsigned    _M,
          unsigned    _SF,
          typename... _S>
message_box_event
render(
    message_box<_M, _SF, _S...>& _mb,
    const char*                  _default_label = "OK",
    const char*                  _cancel_label  = "Cancel"
)
{
    message_box_event evt;
    const char*       window_title;
    const char*       icon;
    bool              has_default;
    bool              has_cancel;
    bool              opened_popup;

    // -- early out for hidden boxes ----------------------------------
    if (!_mb.visible)
    {
        return evt;
    }

    // -- title: pulled from the label_data mixin if mbf_titled, else
    //   a stable fallback so ImGui has a non-empty popup ID ----------
    if constexpr ((_M & mbf_titled) != 0u)
    {
        window_title = _mb.label.empty()
                       ? "##uxoxo_message_box"
                       : _mb.label.c_str();
    }
    else
    {
        window_title = "##uxoxo_message_box";
    }

    // -- ID scope: distinguishes multiple boxes simultaneously open --
    ImGui::PushID(static_cast<const void*>(&_mb));

    // -- ensure the ImGui popup is open ------------------------------
    //   This is the renderer-driven counterpart of mb_show: when
    //   _mb.visible is true but ImGui's popup state for our ID is
    //   not, open it.  Catches both the first-frame-after-mb_show
    //   case and the "popup closed by ImGui but mb_dismiss was not
    //   called" case (e.g. focus loss with NoSavedSettings flag).
    if (!ImGui::IsPopupOpen(window_title))
    {
        ImGui::OpenPopup(window_title);
    }

    // -- begin the popup ---------------------------------------------
    if (_mb.modal)
    {
        opened_popup = ImGui::BeginPopupModal(
                           window_title,
                           nullptr,
                           ImGuiWindowFlags_AlwaysAutoResize);
    }
    else
    {
        opened_popup = ImGui::BeginPopup(window_title);
    }

    if (!opened_popup)
    {
        // popup was closed by ImGui (e.g. modal Esc handling) but the
        // box still believes it is visible — sync via mb_dismiss
        if (_mb.visible)
        {
            mb_dismiss(_mb);
            evt.dismissed = true;
        }

        ImGui::PopID();

        return evt;
    }

    // -- severity icon row -------------------------------------------
    icon = internal::severity_glyph(_mb.severity);

    if (icon)
    {
        ImGui::TextUnformatted(icon);
        ImGui::SameLine();
    }

    // -- primary message body ----------------------------------------
    ImGui::TextWrapped("%s", _mb.message.c_str());

    // -- detail body -------------------------------------------------
    if constexpr ((_M & mbf_detail_text) != 0u)
    {
        if (!_mb.detail_text.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped("%s", _mb.detail_text.c_str());
        }
    }

    // -- busy / progress indicator -----------------------------------
    if constexpr ((_M & mbf_busy) != 0u)
    {
        if (_mb.busy)
        {
            ImGui::Spacing();

            // negative progress -> indeterminate (ImGui draws an
            // animated bar via the special -1.0f sentinel)
            ImGui::ProgressBar(_mb.progress,
                               ImVec2(-FLT_MIN, 0),
                               (_mb.progress < 0.0f) ? "..." : nullptr);
        }
    }

    // -- remember-choice checkbox ------------------------------------
    if constexpr ((_M & mbf_remember_choice) != 0u)
    {
        ImGui::Spacing();
        ImGui::Checkbox("Don't show this again",
                        &_mb.remember_choice);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -- action buttons ----------------------------------------------
    has_default = (_mb.default_action_bit != 0u);
    has_cancel  = (_mb.cancel_action_bit  != 0u);

    if (has_default)
    {
        if (ImGui::Button(_default_label))
        {
            if (internal::trigger_by_bit(_mb,
                                         _mb.default_action_bit))
            {
                evt.triggered     = true;
                evt.triggered_bit = _mb.default_action_bit;
            }
        }

        if (has_cancel)
        {
            ImGui::SameLine();
        }
    }

    if (has_cancel)
    {
        if (ImGui::Button(_cancel_label))
        {
            if (internal::trigger_by_bit(_mb,
                                         _mb.cancel_action_bit))
            {
                evt.triggered     = true;
                evt.triggered_bit = _mb.cancel_action_bit;
            }
        }
    }

    // -- keyboard activation -----------------------------------------
    //   IsWindowFocused gates the key checks so background popups
    //   don't intercept Enter / Esc meant for the focused window.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter,
                                /* repeat = */ false))
        {
            evt.enter_pressed = true;

            if ( (has_default) &&
                 (!evt.triggered) )
            {
                if (internal::trigger_by_bit(_mb,
                                             _mb.default_action_bit))
                {
                    evt.triggered     = true;
                    evt.triggered_bit = _mb.default_action_bit;
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape,
                                /* repeat = */ false))
        {
            evt.escape_pressed = true;

            if (!evt.triggered)
            {
                if (has_cancel)
                {
                    if (internal::trigger_by_bit(_mb,
                                                 _mb.cancel_action_bit))
                    {
                        evt.triggered     = true;
                        evt.triggered_bit = _mb.cancel_action_bit;
                    }
                }
                else if (mb_dismiss(_mb))
                {
                    evt.dismissed = true;
                }
            }
        }
    }

    ImGui::EndPopup();

    // -- post-render dismissal report --------------------------------
    //   A trigger that ran with auto_dismiss_on_action == true has
    //   already dismissed the box; reflect that in the event so
    //   callers don't have to re-check _mb.dismissed.
    if ( (evt.triggered) &&
         (_mb.dismissed) )
    {
        evt.dismissed = true;
    }

    ImGui::PopID();

    // -- populate the shared render_event base ----------------------
    //   `committed` mirrors `triggered` (any action button firing is
    // the message_box equivalent of a commit).  `dismissed` is
    // already in the base via inheritance and was set above; nothing
    // additional needed.  summarise() then ORs the base flags into
    // any_change.
    evt.committed = evt.triggered;

    summarise(evt);

    return evt;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_MESSAGE_BOX_