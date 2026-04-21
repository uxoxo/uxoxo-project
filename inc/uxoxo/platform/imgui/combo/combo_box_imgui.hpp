/*******************************************************************************
* uxoxo [component]                                          combo_box_imgui.hpp
*
* ImGui backend for combo_box:
*   A single free function `render(combo_box&)` that fully drives the
* combo box through Dear ImGui's immediate-mode API.  The renderer
* takes an interactive turn for the box: it draws the anchor button,
* opens / closes the option panel, dispatches user input by calling
* the cmb_* mutators on the box (cmb_select, cmb_toggle_selection,
* cmb_set_filter_text, cmb_apply_filter, cmb_commit_highlighted,
* cmb_close, ...), and returns a `combo_box_event` struct describing
* what happened during the frame.
*
*   Consumers may inspect the event or ignore it — the box itself is
* always left in the correct post-interaction state, with on_change
* and on_commit callbacks fired exactly as they would be for direct
* cmb_* calls.  The event struct exists to spare callers from
* manually diffing pre- and post-render state when they only want
* to know "was something committed this frame?".
*
*   Layout strategy:
*     - The anchor is rendered as an ImGui::Button whose label is
*       derived from the current selection (or the edit buffer for
*       editable combos in compose mode).
*     - The option panel is an ImGui popup opened beside the anchor;
*       the `_c.direction` field controls placement (down / up /
*       auto, the latter currently treated as down).
*     - The renderer is uniform across the template-parameter cube:
*       multi-select, editable, and filterable variants all use the
*       same Button + BeginPopup chassis with `if constexpr` branches
*       gating the optional surface (filter input, edit input,
*       checkbox vs selectable item rendering).
*
*   Item labels:
*     The combo box's `_Item` payload is opaque; ImGui needs a string
*   per row.  Callers pass a label-extraction callable as the second
*   render() argument.  An overload without the callable is provided
*   for `_Item == std::string` (used directly) and for arithmetic
*   item types (passed through std::to_string).  All other item types
*   require an explicit extractor.
*
*   ImGui dependencies:
*     - <imgui.h> for the core API
*     - <misc/cpp/imgui_stdlib.h> for the std::string* overload of
*       ImGui::InputText, used by the edit and filter inputs.
*
*
* path:      /inc/uxoxo/templates/component/imgui/combo_box_imgui.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_COMBO_BOX_IMGUI_
#define  UXOXO_COMPONENT_COMBO_BOX_IMGUI_ 1

// std
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
// imgui
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../combo_box.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  EVENT STRUCT
// ===============================================================================

// combo_box_event
//   struct: per-frame event report returned from rendering a combo_box.
// All flags default to false; `committed_index` defaults to the
// no_selection sentinel.  Callers may inspect any subset of fields or
// ignore the entire struct — the box's own state is the source of
// truth.
struct combo_box_event
{
    bool        opened            = false;  // panel transitioned closed->open
    bool        closed            = false;  // panel transitioned open->closed
    bool        committed         = false;  // single-select commit fired
    bool        selection_changed = false;  // value mutated this frame
    bool        filter_changed    = false;  // filter text edited this frame
    bool        edit_changed      = false;  // edit buffer edited this frame
    std::size_t committed_index   = static_cast<std::size_t>(-1);
};




// ===============================================================================
//  2  ITEM LABEL HELPERS
// ===============================================================================
//   The combo_box's `_Item` payload is opaque, but ImGui needs a
// string per row.  These helpers cover the two common cases —
// std::string-convertible items and arithmetic items — so callers
// can use the no-extractor overload of render() without writing a
// trivial lambda.

NS_INTERNAL

    // default_label_of
    //   function: best-effort string extraction for arithmetic and
    // string-convertible item types.  Anything else triggers a
    // static_assert directing the caller to the explicit-extractor
    // overload of render().
    template <typename _Item>
    std::string
    default_label_of(
        const _Item& _item
    )
    {
        if constexpr (std::is_convertible_v<_Item, std::string>)
        {
            return std::string(_item);
        }
        else if constexpr (std::is_arithmetic_v<_Item>)
        {
            return std::to_string(_item);
        }
        else
        {
            static_assert(std::is_convertible_v<_Item, std::string>,
                          "combo_box render(): _Item is not "
                          "string-convertible or arithmetic; pass an "
                          "explicit label-extractor lambda as the "
                          "second argument.");

            return std::string{};
        }
    }

NS_END  // internal




// ===============================================================================
//  3  RENDER  (with explicit label extractor)
// ===============================================================================

/*
render
  Draws the combo box for one ImGui frame and dispatches user input
back into the box via the cmb_* mutators.  The anchor is an
ImGui::Button; clicking it toggles the option panel, which is
implemented as an ImGui popup opened either beneath or above the
anchor according to `_c.direction`.  Inside the panel:
  - editable combos render an ImGui::InputText bound to the edit
    buffer; edits are written back to `_c.edit_buffer` and reported
    via `event.edit_changed`.
  - filterable combos render an ImGui::InputText bound to the filter
    text; changes call cmb_set_filter_text + cmb_apply_filter and
    are reported via `event.filter_changed`.
  - single-select combos render each option as an ImGui::Selectable;
    a click calls cmb_set_highlight + cmb_commit_highlighted, fires
    on_commit, dismisses the panel, and is reported via
    `event.committed` + `event.committed_index`.
  - multi-select combos render each option as an ImGui::Checkbox;
    a click calls cmb_toggle_selection and is reported via
    `event.selection_changed`.

  The popup also reports closure caused by ImGui (Esc key, click
outside): when ImGui's BeginPopup returns false while `_c.open` is
still true, the renderer calls cmb_close to sync state and reports
`event.closed`.

Parameter(s):
  _c:        the combo box to render.  All cmb_* mutations performed
             by the renderer are applied directly; the box is the
             source of truth for all state across frames.
  _label_of: a callable `std::string(const _Item&)` returning the
             display string for an item.  Required for opaque _Item
             types; see the no-extractor overload below for the
             string and arithmetic shortcuts.
Return:
  A combo_box_event describing what changed during the frame.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable,
          typename _LabelFn>
combo_box_event
render(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c,
    _LabelFn               _label_of
)
{
    using combo_t = combo_box<_Item,
                              _MultiSelect,
                              _Editable,
                              _Filterable,
                              _HasLabel,
                              _Clearable,
                              _Undoable>;

    combo_box_event evt;
    typename combo_t::value_type pre_value;
    bool                         was_open;
    std::string                  anchor_label;
    const char*                  popup_id;

    // -- early out for hidden combos ---------------------------------
    if (!_c.visible)
    {
        return evt;
    }

    // -- snapshot pre-render state for transition reporting ----------
    pre_value = _c.value;
    was_open  = _c.open;
    popup_id  = "##uxoxo_combo_popup";

    // -- build the anchor label --------------------------------------
    //   editable + currently composing: show the live edit buffer.
    //   multi-select: show "(N selected)" or "(none)".
    //   single-select: show the chosen item's label, or a placeholder
    //   when nothing is selected.
    if constexpr (_Editable)
    {
        if (_c.editing)
        {
            anchor_label = _c.edit_buffer;
        }
    }

    if (anchor_label.empty())
    {
        if constexpr (_MultiSelect)
        {
            if (_c.value.empty())
            {
                anchor_label = "(none)";
            }
            else
            {
                anchor_label = "(" + std::to_string(_c.value.size())
                               + " selected)";
            }
        }
        else
        {
            if ( (_c.value == combo_t::no_selection) ||
                 (_c.value >= _c.items.size()) )
            {
                anchor_label = "(select...)";
            }
            else
            {
                anchor_label = _label_of(_c.items[_c.value]);
            }
        }
    }

    // -- ID scope: distinguishes multiple combos in the same window --
    ImGui::PushID(static_cast<const void*>(&_c));

    // -- optional label rendered to the left of the anchor -----------
    if constexpr (_HasLabel)
    {
        if (!_c.label.empty())
        {
            ImGui::TextUnformatted(_c.label.c_str());
            ImGui::SameLine();
        }
    }

    // -- disabled scope wraps the anchor and popup -------------------
    if (!_c.enabled)
    {
        ImGui::BeginDisabled();
    }

    // -- sync ImGui popup state with our `open` field ----------------
    //   Catches the case where the box was opened programmatically
    //   (cmb_open from outside the renderer) since the last frame.
    if ( (_c.open) &&
         (!ImGui::IsPopupOpen(popup_id)) )
    {
        ImGui::OpenPopup(popup_id);
    }

    // -- anchor button -----------------------------------------------
    if (ImGui::Button(anchor_label.c_str()))
    {
        if (!_c.read_only)
        {
            if (was_open)
            {
                cmb_close(_c);
            }
            else
            {
                cmb_open(_c);
                ImGui::OpenPopup(popup_id);
            }
        }
    }

    // -- popup contents ----------------------------------------------
    if (_c.open)
    {
        // direction: dropup pivots the popup so its bottom-left lands
        // on the anchor's top-left.  down and auto_ defer to ImGui's
        // default placement (just below the anchor).
        if (_c.direction == DComboDirection::up)
        {
            ImVec2 anchor_top_left = ImGui::GetItemRectMin();
            ImGui::SetNextWindowPos(anchor_top_left,
                                    ImGuiCond_Always,
                                    ImVec2(0.0f, 1.0f));
        }

        if (ImGui::BeginPopup(popup_id))
        {
            // -- edit input ------------------------------------------
            if constexpr (_Editable)
            {
                if (ImGui::InputText("##uxoxo_combo_edit",
                                     &_c.edit_buffer))
                {
                    _c.editing       = true;
                    evt.edit_changed = true;
                }
            }

            // -- filter input ----------------------------------------
            if constexpr (_Filterable)
            {
                if (ImGui::InputText("##uxoxo_combo_filter",
                                     &_c.filter_text))
                {
                    cmb_apply_filter(_c);
                    evt.filter_changed = true;
                }

                // ensure the filtered-index cache is current before
                // we walk it to render rows
                if (_c.filter_dirty)
                {
                    cmb_apply_filter(_c);
                }
            }

            // -- determine the visible row set -----------------------
            std::size_t row_count;
            std::size_t v;
            std::size_t i;

            if constexpr (_Filterable)
            {
                row_count = _c.filtered_indices.size();
            }
            else
            {
                row_count = _c.items.size();
            }

            // -- option rows -----------------------------------------
            for (v = 0; v < row_count; ++v)
            {
                if constexpr (_Filterable)
                {
                    i = _c.filtered_indices[v];
                }
                else
                {
                    i = v;
                }

                // per-row ID guards against duplicate labels
                ImGui::PushID(static_cast<int>(i));

                std::string row_label = _label_of(_c.items[i]);

                if constexpr (_MultiSelect)
                {
                    bool checked = cmb_is_selected(_c, i);

                    if (ImGui::Checkbox(row_label.c_str(), &checked))
                    {
                        cmb_toggle_selection(_c, i);
                        evt.selection_changed = true;
                    }
                }
                else
                {
                    bool selected = (_c.value == i);

                    if (ImGui::Selectable(row_label.c_str(), selected))
                    {
                        cmb_set_highlight(_c, i);
                        cmb_commit_highlighted(_c);
                        evt.committed       = true;
                        evt.committed_index = i;
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndPopup();
        }
        else
        {
            // popup was closed externally (Esc, outside-click): sync
            cmb_close(_c);
        }
    }

    if (!_c.enabled)
    {
        ImGui::EndDisabled();
    }

    ImGui::PopID();

    // -- compute transition events -----------------------------------
    if ( (!was_open) &&
         (_c.open) )
    {
        evt.opened = true;
    }

    if ( (was_open) &&
         (!_c.open) )
    {
        evt.closed = true;
    }

    if ( (_c.value != pre_value) &&
         (!evt.committed) )
    {
        evt.selection_changed = true;
    }

    return evt;
}




// ===============================================================================
//  4  RENDER  (no-extractor overload)
// ===============================================================================

/*
render
  Convenience overload for combo boxes whose `_Item` is either
std::string-convertible or an arithmetic type.  Forwards to the
extractor-taking overload above with internal::default_label_of as
the label callable.

  For opaque item types, use the extractor-taking overload directly
and pass a lambda such as `[](const my_item& i) { return i.name; }`.

Parameter(s):
  _c: the combo box to render.
Return:
  A combo_box_event describing what changed during the frame.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
combo_box_event
render(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    return render(_c, &internal::default_label_of<_Item>);
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_COMBO_BOX_IMGUI_
