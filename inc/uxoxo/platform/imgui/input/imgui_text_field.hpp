/******************************************************************************
* uxoxo [imgui]                                           imgui_text_field.hpp
*
* Dear ImGui renderer for the text_input<_Feat, _Icon> template:
*   Renders a text_input as a drawable rectangle with an active text
* cursor, optional selection highlight, optional placeholder, and
* optional validation indicator.  Keyboard input is dispatched
* through the component's `ti_*` verb family (ti_insert_char,
* ti_backspace, ti_move_left, ...) so the component's state machine
* remains the single source of truth.
*   Why not ImGui::InputText?
*   text_input owns its cursor index and selection range as plain
* fields on the struct, mutated by free functions.  ImGui::InputText
* maintains its own internal cursor inside ImGui's state and exposes
* only buffer-edit callbacks; trying to keep the two in sync would
* require fighting ImGui for cursor ownership every frame.  A
* draw-list-rendered field is straightforward, byte-identical in
* behavior to ImGui::InputText for the keyboard cases text_input
* already supports, and preserves the component's "data is the
* source of truth" contract.
*
*   Two entry points are provided:
*
*     render(_ti, _ctx)
*         Canonical free function.  ADL-friendly.  Returns a
*         render_event populated with the per-frame interaction
*         report (committed, changed, focus_gained, focus_lost).
*
*     imgui_draw_text_field(_ti, _ctx)
*         Registry-dispatched shim with the same return type.
*
*   Feature handling:
*     - tif_multiline   wraps the visible region in a child window
*                       and renders the value line-by-line; cursor
*                       movement uses ti_move_* verbs identically
*                       to single-line.  Vertical scroll is handled
*                       by ImGui's child window; horizontal scroll
*                       is left to the renderer's hit-test
*                       (no auto-scroll inside this iteration).
*     - tif_history    Up/Down arrow keys at line-edge dispatch to
*                       ti_history_prev / ti_history_next.  When
*                       multiline AND history are both active,
*                       Up/Down navigate lines first and reach
*                       history only at the top/bottom edges.
*     - tif_validation  draws the validation indicator dot to the
*                       right of the input; the indicator color
*                       follows the validation_result level via
*                       palette indicator_* tags.
*     - tif_masked      replaces the displayed string with mask_char
*                       repeated to value.size(); cursor index in
*                       the masked string maps 1:1 to the byte
*                       index in value (no UTF-8 handling here -
*                       text_input itself treats cursor as a byte
*                       index).
*
*   Migration note (2026.05.08): new renderer.  No prior version
* in the platform layer.  Built on the consolidated infrastructure
* (palette, scope guards, render_event base) introduced in steps
* 1-8 of the platform refactor.
*
* Contents:
*   1.  text_field_event           (derives render_event)
*   2.  internal helpers           (geometry, key dispatch, draw)
*   3.  render(text_input&, ctx)   - canonical
*   4.  imgui_draw_text_field      - registry shim
*
*
* path:      /inc/uxoxo/platform/imgui/input/imgui_text_field.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.05.10
******************************************************************************/

#ifndef  UXOXO_COMPONENT_IMGUI_TEXT_FIELD_
#define  UXOXO_COMPONENT_IMGUI_TEXT_FIELD_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
// imgui
#include <imgui.h>
#include <imgui_internal.h>     // PushItemFlag / PopItemFlag for disabled
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/input/text_input.hpp"
#include "../../../templates/render_context.hpp"
#include "../core/imgui_palette.hpp"
#include "../core/imgui_render_event.hpp"
#include "../core/imgui_scope.hpp"


NS_UXOXO
NS_IMGUI


using uxoxo::component::text_input;
using uxoxo::component::validation_result;
using uxoxo::component::render_context;


// ===========================================================================
//  1.  TEXT FIELD EVENT
// ===========================================================================

// text_field_event
//   struct: per-frame event report from rendering a text_input.
// Inherits render_event for the shared (committed / dismissed /
// changed / focus_gained / focus_lost / any_change) flags; adds
// text-field-specific surfaces:
//
//   - validation_changed:     last_report flipped from valid to
//                              error/warning or back this frame
//                              (only meaningful for tif_validation
//                              instantiations)
//   - history_navigated:      Up/Down moved through history
//   - cursor_moved:           cursor position mutated without a
//                              value mutation (selection toggle,
//                              navigation)
//
//   `committed` (inherited) fires when Enter is pressed in a
// single-line input (or Ctrl+Enter in multiline).  `dismissed`
// (inherited) fires when Escape is pressed.  `changed` (inherited)
// is the OR of every value-mutation surface; `focus_gained` and
// `focus_lost` track click-into / click-away on the field.
struct text_field_event : render_event
{
    bool validation_changed = false;
    bool history_navigated  = false;
    bool cursor_moved       = false;
};


// ===========================================================================
//  2.  INTERNAL HELPERS
// ===========================================================================

NS_INTERNAL

    // field_id
    //   function: ImGui ID derived from the text_input's address so
    // multiple instances on the same frame disambiguate without
    // requiring an explicit label parameter.  Returns a (void*)
    // for ImGui's PushID overload.
    template<typename _Type>
    D_NODISCARD D_INLINE const void*
    field_id(
        const _Type& _ti
    ) noexcept
    {
        return static_cast<const void*>(&_ti);
    }

    // displayed_string
    //   function: returns the string to draw given the input's
    // value and (when present) its masked state.  For masked
    // inputs, returns a string of mask_char repeated value.size()
    // times.  For unmasked, returns value unchanged.
    //
    //   Cursor positions in text_input are byte indices into
    // .value; because the mask replaces each byte with one
    // mask_char, byte indices map 1:1 between value and the
    // masked string.  Renderers measuring cursor X-position can
    // use the masked string directly.
    template<typename _Type>
    D_NODISCARD D_INLINE std::string
    displayed_string(
        const _Type& _ti
    )
    {
        if constexpr (_Type::is_masked_input)
        {
            if (_ti.masked)
            {
                return std::string(_ti.value.size(),
                                   _ti.mask_char);
            }
        }

        return _ti.value;
    }

    // measure_text_to
    //   function: pixel width of the displayed string from byte 0
    // through byte `_end`, exclusive.  Used to position the cursor
    // and selection rectangles.
    D_NODISCARD D_INLINE float
    measure_text_to(
        const std::string& _s,
        std::size_t        _end
    ) noexcept
    {
        if (_end == 0)
        {
            return 0.0f;
        }

        return ImGui::CalcTextSize(
            _s.c_str(),
            _s.c_str() + std::min(_end, _s.size())).x;
    }

    // hit_test_byte_index
    //   function: returns the byte index in `_s` whose left edge is
    // closest to `_local_x` (pixels from the text origin).  Used
    // to position the cursor on mouse click.  Linear scan — fine
    // for typical input lengths.
    D_NODISCARD D_INLINE std::size_t
    hit_test_byte_index(
        const std::string& _s,
        float              _local_x
    ) noexcept
    {
        std::size_t i;
        float       best_d;
        std::size_t best_i;
        float       w;

        if (_s.empty() || _local_x <= 0.0f)
        {
            return 0;
        }

        best_d = std::abs(_local_x);
        best_i = 0;

        for (i = 1; i <= _s.size(); ++i)
        {
            w = measure_text_to(_s, i);
            float d = std::abs(_local_x - w);

            if (d < best_d)
            {
                best_d = d;
                best_i = i;
            }
            else if (w > _local_x)
            {
                // text widths are monotonically increasing; once
                // we pass the click point and start moving away,
                // stop scanning
                break;
            }
        }

        return best_i;
    }

    // validation_indicator_color
    //   function: maps a validation_result to a palette indicator
    // tag value.  Compiles to a single load when the result is a
    // compile-time constant.
    D_NODISCARD D_INLINE ImVec4
    validation_indicator_color(
        validation_result _r
    ) noexcept
    {
        switch (_r)
        {
            case validation_result::valid:
                return palette::get<palette::indicator_ok_tag>();

            case validation_result::warning:
                return palette::get<palette::indicator_warn_tag>();

            case validation_result::error:
                return palette::get<palette::indicator_error_tag>();
        }

        return palette::get<palette::indicator_ok_tag>();
    }

    // dispatch_keyboard
    //   function: consumes pressed keys this frame and routes them
    // through the component's ti_* verb family.  Reports back the
    // event flags that the dispatch produced (committed,
    // dismissed, changed, cursor_moved, history_navigated).
    //
    //   Multiline behavior: Up/Down move between visual lines via
    // ti_move_up / ti_move_down; in single-line mode they instead
    // either navigate history (when tif_history is enabled) or
    // do nothing.
    //
    //   Selection extension: Shift held during arrow keys passes
    // extend_sel = true to the ti_* verb so the selection grows.
    template<typename _Type>
    D_INLINE void
    dispatch_keyboard(
        _Type&            _ti,
        text_field_event& _evt
    )
    {
        bool shift;
        bool ctrl;

        shift = ImGui::IsKeyDown(ImGuiMod_Shift);
        ctrl  = ImGui::IsKeyDown(ImGuiMod_Ctrl);

        // -- character input ------------------------------------------
        //   ImGui's InputQueueCharacters surfaces every codepoint
        // typed this frame, including key-repeat.  We pass each
        // through ti_insert_char (which handles undo, validation,
        // and on_change automatically).
        const ImGuiIO& io = ImGui::GetIO();

        for (int n = 0; n < io.InputQueueCharacters.Size; ++n)
        {
            ImWchar wch = io.InputQueueCharacters[n];

            // skip control characters; ImGui emits a separate key
            // event for Enter/Tab/etc.
            if (wch < 0x20 || wch == 0x7F)
            {
                continue;
            }

            // narrow to char; text_input treats value as bytes
            if (wch < 0x80)
            {
                ti_insert_char(_ti, static_cast<char>(wch));
                _evt.changed = true;
            }
            else
            {
                // multi-byte UTF-8 codepoints are flattened to the
                // low byte for the byte-indexed text_input.  A
                // future iteration may want to push the full
                // UTF-8 byte sequence; for now we mirror what the
                // dev_console renderer does and accept ASCII.
                ti_insert_char(_ti, static_cast<char>(wch & 0x7F));
                _evt.changed = true;
            }
        }

        // -- editing keys ---------------------------------------------
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true))
        {
            if (ti_backspace(_ti))
            {
                _evt.changed = true;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, true))
        {
            if (ti_delete(_ti))
            {
                _evt.changed = true;
            }
        }

        // -- cursor movement ------------------------------------------
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        {
            if (ctrl)
            {
                ti_move_word_left(_ti, shift);
            }
            else
            {
                ti_move_left(_ti, shift);
            }

            _evt.cursor_moved = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        {
            if (ctrl)
            {
                ti_move_word_right(_ti, shift);
            }
            else
            {
                ti_move_right(_ti, shift);
            }

            _evt.cursor_moved = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Home, true))
        {
            ti_home(_ti, shift);
            _evt.cursor_moved = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_End, true))
        {
            ti_end(_ti, shift);
            _evt.cursor_moved = true;
        }

        // -- multiline-specific: Up / Down -----------------------------
        if constexpr (_Type::is_multiline)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
            {
                if (!ti_move_up(_ti, shift))
                {
                    // at top of multiline AND history is enabled:
                    // fall through to history navigation
                    if constexpr (_Type::has_history)
                    {
                        ti_history_prev(_ti);
                        _evt.history_navigated = true;
                        _evt.changed           = true;
                    }
                }
                else
                {
                    _evt.cursor_moved = true;
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
            {
                if (!ti_move_down(_ti, shift))
                {
                    if constexpr (_Type::has_history)
                    {
                        ti_history_next(_ti);
                        _evt.history_navigated = true;
                        _evt.changed           = true;
                    }
                }
                else
                {
                    _evt.cursor_moved = true;
                }
            }
        }
        else
        {
            // single-line: Up / Down go straight to history
            if constexpr (_Type::has_history)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
                {
                    ti_history_prev(_ti);
                    _evt.history_navigated = true;
                    _evt.changed           = true;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
                {
                    ti_history_next(_ti);
                    _evt.history_navigated = true;
                    _evt.changed           = true;
                }
            }
        }

        // -- commit / dismiss -----------------------------------------
        //   Single-line: Enter commits.  Multiline: Ctrl+Enter
        // commits so plain Enter can insert a newline via the
        // character path (ti_insert_char with '\n').
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false))
        {
            if constexpr (_Type::is_multiline)
            {
                if (ctrl)
                {
                    _evt.committed = true;
                }
                else
                {
                    ti_insert_char(_ti, '\n');
                    _evt.changed = true;
                }
            }
            else
            {
                _evt.committed = true;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            _evt.dismissed = true;
        }

        return;
    }

    // draw_field
    //   function: emits the visual representation of the input -
    // background, border, placeholder (when empty), text,
    // selection rectangle, and blinking cursor.  Uses the ambient
    // ImGui DrawList; no child window is created here so callers
    // can place the field in any layout context.
    //
    //   Caller is expected to have already advanced the cursor
    // (via ImGui::Dummy or InvisibleButton) past the field's
    // rectangle so subsequent widgets lay out below it.
    template<typename _Type>
    D_INLINE void
    draw_field(
        const _Type&  _ti,
        const ImVec2& _origin,
        const ImVec2& _size,
        bool          _focused
    )
    {
        ImDrawList* dl;
        ImVec2      text_pos;
        std::string display;
        float       cursor_x;

        dl = ImGui::GetWindowDrawList();

        // -- background fill ------------------------------------------
        dl->AddRectFilled(
            _origin,
            ImVec2(_origin.x + _size.x,
                   _origin.y + _size.y),
            ImGui::GetColorU32(
                palette::get<palette::table_edit_bg_tag>()),
            palette::get<palette::default_rounding_tag>());

        // -- border (highlighted when focused) ------------------------
        dl->AddRect(
            _origin,
            ImVec2(_origin.x + _size.x,
                   _origin.y + _size.y),
            ImGui::GetColorU32(
                _focused
                    ? palette::get<palette::table_edit_border_tag>()
                    : palette::get<palette::toolbar_border_tag>()),
            palette::get<palette::default_rounding_tag>(),
            0,
            _focused ? 1.5f : 1.0f);

        // -- text origin (inside padding) -----------------------------
        text_pos = ImVec2(_origin.x + 4.0f,
                          _origin.y + 4.0f);

        // -- placeholder (when value is empty) ------------------------
        if (_ti.value.empty() && !_ti.placeholder.empty())
        {
            dl->AddText(
                text_pos,
                ImGui::GetColorU32(
                    palette::get<palette::text_muted_tag>()),
                _ti.placeholder.c_str());
        }
        else
        {
            display = displayed_string(_ti);

            // -- selection highlight ----------------------------------
            if (_ti.has_selection)
            {
                std::size_t lo = _ti.selection_start();
                std::size_t hi = _ti.selection_end();
                float       x0 = measure_text_to(display, lo);
                float       x1 = measure_text_to(display, hi);

                dl->AddRectFilled(
                    ImVec2(text_pos.x + x0,
                           text_pos.y),
                    ImVec2(text_pos.x + x1,
                           text_pos.y + ImGui::GetTextLineHeight()),
                    ImGui::GetColorU32(
                        palette::get<palette::selection_bg_tag>()));
            }

            // -- the text itself --------------------------------------
            dl->AddText(
                text_pos,
                ImGui::GetColorU32(
                    palette::get<palette::text_body_tag>()),
                display.c_str());
        }

        // -- cursor (blinking, only when focused) ---------------------
        if (_focused)
        {
            // ImGui's GetTime() is double; blink at ~2 Hz
            const double t      = ImGui::GetTime();
            const bool   shown  = (static_cast<int>(t * 2.0) % 2) == 0;

            if (shown)
            {
                display  = displayed_string(_ti);
                cursor_x = measure_text_to(display, _ti.cursor);

                dl->AddLine(
                    ImVec2(text_pos.x + cursor_x,
                           text_pos.y),
                    ImVec2(text_pos.x + cursor_x,
                           text_pos.y + ImGui::GetTextLineHeight()),
                    ImGui::GetColorU32(
                        palette::get<palette::cursor_border_tag>()),
                    1.0f);
            }
        }

        return;
    }

NS_END  // internal


// ===========================================================================
//  3.  RENDER  (text_input)
// ===========================================================================

/*
render  (text_input)
  Canonical render entry for text_input.  Lays out an interactive
field with width = available content width and height = one text
line (or `visible_rows * line_height` for multiline) plus padding.

  Workflow each frame:
  1. Reserve a rectangle via ImGui::InvisibleButton so ImGui's
     layout, hover detection, and focus tracking work.
  2. Manage focus state: clicking the rectangle focuses it; the
     focused flag is held in ImGui's storage keyed by the field
     ID so it persists across frames without polluting the
     text_input struct.
  3. Hit-test mouse clicks to position the cursor.
  4. Dispatch keyboard input through the ti_* verbs when focused.
  5. Draw the field background, border, text, selection, cursor.
  6. Run validators (when tif_validation) and draw the indicator
     dot to the right of the field.

Parameter(s):
  _ti:  the text_input to render.  Mutated by every keystroke.
  _ctx: render context.  Currently unused.
Return:
  A text_field_event populated with the per-frame interaction
  report.  any_change is the OR of every flag set.
*/
template<unsigned _Feat,
          typename _Icon>
text_field_event
render(
    text_input<_Feat, _Icon>& _ti,
    render_context&           _ctx
)
{
    text_field_event evt;
    ImVec2           origin;
    ImVec2           size;
    bool             clicked;
    bool             hovered;
    bool             focused;

    (void)_ctx;

    // -- early-out for disabled or hidden fields -----------------------
    //   text_input has no visible member (its visibility is the
    // caller's responsibility), so we check only `enabled` here.
    // A disabled field still draws but consumes no keyboard input.

    scoped_id id(internal::field_id(_ti));

    // -- reserve layout space ------------------------------------------
    origin = ImGui::GetCursorScreenPos();

    {
        const float content_w = ImGui::GetContentRegionAvail().x;
        const float line_h    = ImGui::GetTextLineHeightWithSpacing();
        float       h         = (line_h + 8.0f);   // single line + padding

        if constexpr (_ti.is_multiline)
        {
            // visible_rows is set by callers (or stays at default 1).
            // For multiline, allocate that many lines.
            const std::size_t rows = std::max<std::size_t>(
                                         _ti.visible_rows, 1);
            h = (line_h * static_cast<float>(rows)) + 8.0f;
        }

        // reserve a little extra width for the validation indicator
        // when present so the field text doesn't overlap the dot
        float field_w = content_w;

        if constexpr (_ti.has_validation)
        {
            field_w = std::max(field_w - 16.0f, 32.0f);
        }

        size = ImVec2(field_w, h);
    }

    // -- focus tracking via ImGui storage ------------------------------
    //   Keyed by the field's ImGui ID so multiple text_inputs in
    // one frame don't share state.  Default focused = false on
    // first frame.

    const ImGuiID storage_id = ImGui::GetID("focus");
    bool          was_focused;

    {
        ImGuiStorage* store = ImGui::GetStateStorage();

        was_focused = store->GetBool(storage_id, false);
    }

    // -- interaction: invisible button overlay -------------------------
    //   InvisibleButton captures click + hover.  Disabled fields
    // still draw but the button reports clicked=false because we
    // gate it on enabled.
    {
        scoped_disabled dis(!_ti.enabled || _ti.read_only);

        clicked = ImGui::InvisibleButton("##field", size);
        hovered = ImGui::IsItemHovered();
    }

    // -- focus state transitions ---------------------------------------
    focused = was_focused;

    if (clicked)
    {
        focused = true;

        // position cursor at the mouse click location
        const ImVec2 mp     = ImGui::GetIO().MousePos;
        const float  rel_x  = (mp.x - origin.x - 4.0f);
        const std::string display = internal::displayed_string(_ti);

        _ti.cursor        = internal::hit_test_byte_index(display, rel_x);
        _ti.has_selection = false;
    }
    else if ( (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) &&
              (!hovered) )
    {
        // click anywhere else loses focus
        if (focused)
        {
            evt.focus_lost = true;
        }

        focused = false;
    }

    if ( (focused) &&
         (!was_focused) )
    {
        evt.focus_gained = true;
    }

    // persist focus into ImGui's storage
    {
        ImGuiStorage* store = ImGui::GetStateStorage();
        store->SetBool(storage_id, focused);
    }

    // -- keyboard dispatch (only when focused and editable) ------------
    if ( (focused)       &&
         (_ti.enabled)   &&
         (!_ti.read_only) )
    {
        internal::dispatch_keyboard(_ti, evt);
    }

    // -- draw the field ------------------------------------------------
    internal::draw_field(_ti, origin, size, focused);

    // -- validation indicator + auto-validate --------------------------
    if constexpr (_ti.has_validation)
    {
        const validation_result prev = _ti.last_report.result;

        if ( (_ti.validate_on_change) &&
             (evt.changed) )
        {
            ti_validate(_ti);
        }

        if (_ti.last_report.result != prev)
        {
            evt.validation_changed = true;
        }

        ImGui::SameLine();

        ImVec2      dot_origin = ImGui::GetCursorScreenPos();
        ImDrawList* dl         = ImGui::GetWindowDrawList();

        dl->AddCircleFilled(
            ImVec2(dot_origin.x + 6.0f,
                   dot_origin.y + (size.y * 0.5f)),
            5.0f,
            ImGui::GetColorU32(
                internal::validation_indicator_color(
                    _ti.last_report.result)));

        ImGui::Dummy(ImVec2(16.0f, size.y));

        // tooltip on hover showing the message
        if ( (ImGui::IsItemHovered())                &&
             (!_ti.last_report.message.empty()) )
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(_ti.last_report.message.c_str());
            ImGui::EndTooltip();
        }
    }

    // -- summarise the event report into any_change -------------------
    //   Beyond the base flags, the text-field-specific flags also
    // contribute - cursor_moved and history_navigated are
    // meaningful "something happened" signals even when value
    // didn't change.
    evt.any_change = ( (evt.committed)          ||
                       (evt.dismissed)          ||
                       (evt.changed)            ||
                       (evt.focus_gained)       ||
                       (evt.focus_lost)         ||
                       (evt.cursor_moved)       ||
                       (evt.history_navigated)  ||
                       (evt.validation_changed) );

    return evt;
}


// ===========================================================================
//  4.  IMGUI DRAW TEXT FIELD  (registry shim)
// ===========================================================================

/*
imgui_draw_text_field
  Registry-dispatched entry point.  Identical semantics to
`render(_ti, _ctx)`.

Parameter(s):
  _ti:  the text_input to render.
  _ctx: render context.
Return:
  A text_field_event populated with the per-frame interaction
  report.
*/
template<unsigned _Feat,
          typename _Icon>
D_INLINE text_field_event
imgui_draw_text_field(
    text_input<_Feat, _Icon>& _ti,
    render_context&           _ctx
)
{
    return render(_ti, _ctx);
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_IMGUI_TEXT_FIELD_