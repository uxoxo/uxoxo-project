/*******************************************************************************
* uxoxo [component]                                               combo_box.hpp
*
* Generic combo box component template:
*   A structurally-conforming uxoxo component that pairs a closed "anchor"
* display (the currently selected option) with a deferred option panel
* (the "dropdown") that can open toward either vertical direction.  When
* instantiated with `_MultiSelect == true` the combo behaves as a
* check-list: each option carries an implicit checkbox and `value` holds
* the full set of selected indices rather than a single index.
*
*   DComboDirection is defined in this header rather than
* component_types.hpp because no other component currently consumes it.
* If a second consumer appears (e.g. menu_button) it should graduate
* following the same rule DOrientation / DEmphasis did.
*
*   The template is a plain aggregate with no base-class overhead.
* Capability mixins (label, clearable, undoable, edit state, filter
* state) are engaged per-template-instance and contribute zero bytes
* when disabled thanks to EBO.  All operations in component_common.hpp
* (enable, disable, set_value, get_value, clear, undo, commit) work on
* this type through structural conformance.  The cmb_* helpers below
* cover only the combo-specific operations — open/close, option-panel
* navigation, per-option selection, edit-buffer handling, and filter
* application — that have no generic analogue.
*
*   The `_Item` payload is opaque to the component: the renderer (or an
* extractor function supplied by the consumer) is responsible for
* mapping _Item to a display string.  This mirrors the list_view /
* tree_view convention and keeps the data model clean.
*
* Contents:
*   1  DComboDirection
*   2  combo_mixin namespace (edit_state, filter_state)
*   3  combo_box template
*   4  Common configurations (type aliases)
*   5  Combo-specific free functions
*       5.1  open / close
*       5.2  direction
*       5.3  selection (cmb_select, cmb_deselect, cmb_toggle_selection)
*       5.4  highlight navigation
*       5.5  commit highlighted
*       5.6  filter (requires _Filterable)
*       5.7  edit buffer (requires _Editable)
*
*
* path:      /inc/uxoxo/templates/component/combo/combo_box.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.20
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_COMBO_BOX_
#define  UXOXO_COMPONENT_COMBO_BOX_ 1

// std
#include <algorithm>
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
#include "./component_mixin.hpp"
#include "./component_common.hpp"


NS_UXOXO
NS_COMPONENT

// ===============================================================================
//  1  DROPDOWN DIRECTION
// ===============================================================================

// DComboDirection
//   enum: direction the option panel opens relative to its anchor.
// `down` is conventional dropdown behavior; `up` is a dropup, suitable
// for anchors near the bottom of the viewport; `auto_` defers the
// decision to the renderer based on available space.
enum class DComboDirection : std::uint8_t
{
    down  = 0,
    up    = 1,
    auto_ = 2
};

// ===============================================================================
//  2  COMBO-SPECIFIC MIXINS
// ===============================================================================
//   Optional per-instance data engaged by the `_Editable` and
// `_Filterable` template parameters.  Each mixin follows the same EBO
// pattern used by component_mixin: empty primary template,
// data-carrying `true` specialization.  When disabled the subobject
// contributes zero bytes to the enclosing combo_box.
//
//   These mixins are local to combo_box because they have no second
// consumer.  If a future component (e.g. autocomplete, picker) wants
// the same surface, graduate them into a shared header rather than
// duplicating them.

namespace combo_mixin 
{

// -- edit state -------------------------------------------------------

// edit_state
//   trait: primary template; empty when editable mode is disabled.
template <bool _Enable>
struct edit_state
{};

// edit_state<true>
//   trait: carries the pending text buffer used by editable combos and
// a flag indicating whether the user is currently composing input.
template <>
struct edit_state<true>
{
    std::string edit_buffer;
    bool        editing = false;
};


// -- filter state -----------------------------------------------------

// filter_state
//   trait: primary template; empty when filterable mode is disabled.
template <bool     _Enable,
          typename _Item = void>
struct filter_state
{};

// filter_state<true, _Item>
//   trait: carries the filter text, the user-supplied predicate, and
// the cached set of indices into the item list that currently pass
// the filter.  `filter_dirty` indicates the cache must be rebuilt on
// next access.
template <typename _Item>
struct filter_state<true, _Item>
{
    std::string                       filter_text;
    std::function<bool(const _Item&)> filter_predicate;
    std::vector<std::size_t>          filtered_indices;
    bool                              filter_dirty = true;
};


}   // namespace combo_mixin




// ===============================================================================
//  INTERNAL: value type helper
// ===============================================================================
//   The combo's `value` is either a single index or a set of indices
// depending on `_MultiSelect`.  clearable_data and undo_data also
// need this type to store default_value / previous_value.

NS_INTERNAL

    // combo_value_t
    //   type: std::size_t for single-select combos; std::vector<std::size_t>
    // for multi-select combos.
    template <bool _MultiSelect>
    using combo_value_t =
        std::conditional_t<_MultiSelect,
                           std::vector<std::size_t>,
                           std::size_t>;

}   // NS_INTERNAL




// ===============================================================================
//  3  COMBO BOX TEMPLATE
// ===============================================================================

// combo_box
//   struct: generic combo box component parameterized by item type and
// a set of capability flags.  Inherits zero-cost EBO mixins for label,
// clearable, undoable, editable, and filterable capabilities.  The
// `value` member holds a single index (single-select) or a vector of
// indices (multi-select) into the `items` list.
template <typename _Item,
          bool     _MultiSelect = false,
          bool     _Editable    = false,
          bool     _Filterable  = false,
          bool     _HasLabel    = false,
          bool     _Clearable   = false,
          bool     _Undoable    = false>
struct combo_box
    : public component_mixin::label_data<_HasLabel>,
      public component_mixin::clearable_data<
                 _Clearable,
                 internal::combo_value_t<_MultiSelect>>,
      public component_mixin::undo_data<
                 _Undoable,
                 internal::combo_value_t<_MultiSelect>>,
      public combo_mixin::edit_state<_Editable>,
      public combo_mixin::filter_state<_Filterable, _Item>
{
    using item_type  = _Item;
    using index_type = std::size_t;
    using value_type = internal::combo_value_t<_MultiSelect>;

    static constexpr bool       focusable    = true;
    static constexpr bool       scrollable   = true;
    static constexpr bool       multi_select = _MultiSelect;
    static constexpr bool       editable     = _Editable;
    static constexpr bool       filterable   = _Filterable;

    // sentinel returned by selection queries when nothing is selected
    // in a single-select combo (multi-select uses an empty vector).
    static constexpr index_type no_selection =
        static_cast<index_type>(-1);

    // default_initial
    //   function: produces the initial `value` for the template
    // instantiation — no_selection for single-select, empty vector
    // for multi-select.  Called exactly once as a member initializer.
    static value_type default_initial()
    {
        if constexpr (_MultiSelect)
        {
            return value_type{};
        }
        else
        {
            return no_selection;
        }
    }

    // options
    std::vector<_Item>                     items;

    // current selection
    value_type                             value         = default_initial();

    // standard component surface
    bool                                   enabled       = true;
    bool                                   visible       = true;
    bool                                   read_only     = false;

    // dropdown state
    bool                                   open          = false;
    DComboDirection                        direction     =
        DComboDirection::auto_;
    index_type                             highlighted   = 0;
    index_type                             scroll_offset = 0;
    index_type                             max_visible   = 10;

    // callbacks
    std::function<void(const value_type&)> on_commit;
    std::function<void(const value_type&)> on_change;
};


/*****************************************************************************/

// ===============================================================================
//  4  COMMON CONFIGURATIONS
// ===============================================================================

// simple_combo_box
//   type: minimal single-select combo with no label or mixin capabilities.
template <typename _Item>
using simple_combo_box =
    combo_box<_Item, false, false, false, false, false, false>;

// labeled_combo_box
//   type: single-select combo with a label.
template <typename _Item>
using labeled_combo_box =
    combo_box<_Item, false, false, false, true,  false, false>;

// multi_combo_box
//   type: multi-select "checklist" combo.  `value` is a
// std::vector<std::size_t> of selected indices.
template <typename _Item>
using multi_combo_box =
    combo_box<_Item, true,  false, false, false, false, false>;

// editable_combo_box
//   type: single-select combo with an edit buffer for typed input.
template <typename _Item>
using editable_combo_box =
    combo_box<_Item, false, true,  false, false, false, false>;

// searchable_combo_box
//   type: single-select combo with a filter predicate over items.
template <typename _Item>
using searchable_combo_box =
    combo_box<_Item, false, false, true,  false, false, false>;

// full_combo_box
//   type: multi-select combo with every capability mixin enabled.
template <typename _Item>
using full_combo_box =
    combo_box<_Item, true,  true,  true,  true,  true,  true>;


/*****************************************************************************/

// ===============================================================================
//  5  COMBO-SPECIFIC FREE FUNCTIONS
// ===============================================================================
//   Generic verbs (enable, disable, show, hide, set_value, get_value,
// clear, undo, commit) come from component_common.hpp and work on
// combo_box through structural conformance.  The cmb_* helpers below
// cover only the combo-specific surface.

// ===============================================================================
//  5.1  OPEN / CLOSE
// ===============================================================================

/*
cmb_open
  Opens the combo's option panel.  The `highlighted` index is kept
in place so the user resumes at their last position.  If the combo
is disabled or read-only, the call is a no-op and returns false.

Parameter(s):
  _c: the combo box to open.
Return:
  true if the panel transitioned from closed to open; false if the
  panel was already open, or if the combo is disabled / read-only.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_open(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    if ( (!_c.enabled) ||
         (_c.read_only) )
    {
        return false;
    }

    if (_c.open)
    {
        return false;
    }

    _c.open = true;

    return true;
}


/*
cmb_close
  Closes the combo's option panel.  Does not affect `value`; the
panel is purely a presentational state.

Parameter(s):
  _c: the combo box to close.
Return:
  true if the panel transitioned from open to closed; false if it
  was already closed.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_close(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    if (!_c.open)
    {
        return false;
    }

    _c.open = false;

    return true;
}


/*
cmb_toggle_open
  Convenience: opens a closed panel, closes an open panel.  Observes
the same enabled / read-only constraints as cmb_open when opening.

Parameter(s):
  _c: the combo box whose panel state to toggle.
Return:
  true if the call produced a state transition; false otherwise.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_toggle_open(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    if (_c.open)
    {
        return cmb_close(_c);
    }

    return cmb_open(_c);
}


/*
cmb_is_open
  Non-mutating query for the panel state.

Parameter(s):
  _c: the combo box to inspect.
Return:
  true if the option panel is currently open; false otherwise.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
[[nodiscard]] bool
cmb_is_open(
    const combo_box<_Item,
                    _MultiSelect,
                    _Editable,
                    _Filterable,
                    _HasLabel,
                    _Clearable,
                    _Undoable>& _c
) noexcept
{
    return _c.open;
}




// ===============================================================================
//  5.2  DIRECTION
// ===============================================================================

/*
cmb_set_direction
  Replaces the combo's preferred open direction.  Has no immediate
visual effect; the renderer consults `direction` the next time the
panel is laid out.

Parameter(s):
  _c:   the combo box whose direction to set.
  _dir: the new direction.  `DComboDirection::auto_` defers the
        choice to the renderer based on available viewport space.
Return:
  none.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
void
cmb_set_direction(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c,
    DComboDirection       _dir
)
{
    _c.direction = _dir;

    return;
}




// ===============================================================================
//  5.3  SELECTION
// ===============================================================================
//   cmb_select / cmb_deselect / cmb_toggle_selection go through the
// generic set_value so undo state and on_change notifications fire
// exactly as they would for any other value mutation.

/*
cmb_is_selected
  Tests whether a given item index is currently selected.  For
single-select combos, returns true only when `value == _idx` (and
_idx is not the no_selection sentinel).  For multi-select combos,
returns true when _idx is present in the selected-indices vector.

Parameter(s):
  _c:   the combo box to query.
  _idx: the item index to test.
Return:
  true if _idx is selected; false otherwise (including when _idx
  is out of range).
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
[[nodiscard]] bool
cmb_is_selected(
    const combo_box<_Item,
                    _MultiSelect,
                    _Editable,
                    _Filterable,
                    _HasLabel,
                    _Clearable,
                    _Undoable>& _c,
    std::size_t                 _idx
) noexcept
{
    if (_idx >= _c.items.size())
    {
        return false;
    }

    if constexpr (_MultiSelect)
    {
        return std::find(_c.value.begin(), _c.value.end(), _idx)
               != _c.value.end();
    }
    else
    {
        return _c.value == _idx;
    }
}


/*
cmb_selected_count
  Returns the number of selected items.  For single-select combos
this is either 0 (no_selection) or 1.  For multi-select combos
this is the size of the selected-indices vector.

Parameter(s):
  _c: the combo box to query.
Return:
  the current selection count.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
[[nodiscard]] std::size_t
cmb_selected_count(
    const combo_box<_Item,
                    _MultiSelect,
                    _Editable,
                    _Filterable,
                    _HasLabel,
                    _Clearable,
                    _Undoable>& _c
) noexcept
{
    if constexpr (_MultiSelect)
    {
        return _c.value.size();
    }
    else
    {
        if (_c.value ==
            combo_box<_Item,
                      _MultiSelect,
                      _Editable,
                      _Filterable,
                      _HasLabel,
                      _Clearable,
                      _Undoable>::no_selection)
        {
            return 0;
        }

        return 1;
    }
}


/*
cmb_select
  Selects the item at the given index.  For single-select combos,
replaces the current selection.  For multi-select combos, adds
_idx to the selection if not already present; duplicate selects
are idempotent.  Out-of-range indices are rejected.  Goes through
set_value so undo state and on_change callbacks fire.

Parameter(s):
  _c:   the combo box to modify.
  _idx: the item index to select.
Return:
  true if the selection changed; false if the index was
  out of range or already selected (multi-select).
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_select(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c,
    std::size_t           _idx
)
{
    if (_idx >= _c.items.size())
    {
        return false;
    }

    if constexpr (_MultiSelect)
    {
        if (std::find(_c.value.begin(), _c.value.end(), _idx)
            != _c.value.end())
        {
            return false;
        }

        auto new_value = _c.value;
        new_value.push_back(_idx);
        std::sort(new_value.begin(), new_value.end());
        set_value(_c, std::move(new_value));

        return true;
    }
    else
    {
        if (_c.value == _idx)
        {
            return false;
        }

        set_value(_c, _idx);

        return true;
    }
}


/*
cmb_deselect
  Removes an item from the selection.  Multi-select only: for
single-select combos, use cmb_clear_selection or set_value with
no_selection.

Parameter(s):
  _c:   the combo box to modify.  Must be instantiated with
        _MultiSelect == true.
  _idx: the item index to remove from the selection.
Return:
  true if _idx was removed; false if it was not in the selection.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_deselect(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c,
    std::size_t           _idx
)
{
    static_assert(_MultiSelect,
                  "cmb_deselect requires a multi-select combo box "
                  "(_MultiSelect template parameter must be true).");

    auto it = std::find(_c.value.begin(), _c.value.end(), _idx);

    if (it == _c.value.end())
    {
        return false;
    }

    auto new_value = _c.value;
    new_value.erase(new_value.begin()
                    + std::distance(_c.value.begin(), it));
    set_value(_c, std::move(new_value));

    return true;
}


/*
cmb_toggle_selection
  Toggles the selection state of a single item.  For single-select
combos, selects _idx if not selected, otherwise clears to
no_selection.  For multi-select combos, adds _idx if absent,
removes it if present.

Parameter(s):
  _c:   the combo box to modify.
  _idx: the item index to toggle.
Return:
  true if the selection state changed; false if _idx was out of
  range.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_toggle_selection(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c,
    std::size_t           _idx
)
{
    if (_idx >= _c.items.size())
    {
        return false;
    }

    if constexpr (_MultiSelect)
    {
        if (cmb_is_selected(_c, _idx))
        {
            return cmb_deselect(_c, _idx);
        }

        return cmb_select(_c, _idx);
    }
    else
    {
        using combo_t = combo_box<_Item,
                                  _MultiSelect,
                                  _Editable,
                                  _Filterable,
                                  _HasLabel,
                                  _Clearable,
                                  _Undoable>;

        if (_c.value == _idx)
        {
            set_value(_c, combo_t::no_selection);
        }
        else
        {
            set_value(_c, _idx);
        }

        return true;
    }
}


/*
cmb_clear_selection
  Clears the entire selection.  For single-select combos, sets
value to no_selection.  For multi-select combos, empties the
selected-indices vector.  Goes through set_value so undo state
and on_change callbacks fire.

Parameter(s):
  _c: the combo box to clear.
Return:
  none.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
void
cmb_clear_selection(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    using combo_t = combo_box<_Item,
                              _MultiSelect,
                              _Editable,
                              _Filterable,
                              _HasLabel,
                              _Clearable,
                              _Undoable>;

    if constexpr (_MultiSelect)
    {
        set_value(_c, typename combo_t::value_type{});
    }
    else
    {
        set_value(_c, combo_t::no_selection);
    }

    return;
}




// ===============================================================================
//  5.4  HIGHLIGHT NAVIGATION
// ===============================================================================
//   When the option panel is open, the highlighted index tracks
// which item is currently "under the cursor" (keyboard focus within
// the panel).  These helpers advance the highlight with wrap-around
// and update scroll_offset so the highlight stays in view.

NS_INTERNAL

    // cmb_ensure_visible_
    //   function: nudges scroll_offset so that `highlighted` falls
    // within [scroll_offset, scroll_offset + max_visible).
    template <typename _Combo>
    inline void
    cmb_ensure_visible_(_Combo& _c)
    {
        if (_c.max_visible == 0)
        {
            return;
        }

        if (_c.highlighted < _c.scroll_offset)
        {
            _c.scroll_offset = _c.highlighted;
        }
        else if (_c.highlighted >= (_c.scroll_offset + _c.max_visible))
        {
            _c.scroll_offset = (_c.highlighted - _c.max_visible) + 1;
        }

        return;
    }

}   // NS_INTERNAL


/*
cmb_next_highlight
  Advances the highlight to the next item in the list, wrapping
to index 0 at the end.  Updates scroll_offset to keep the
highlight visible.

Parameter(s):
  _c: the combo box to navigate.
Return:
  true if the highlight moved; false if the item list is empty.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_next_highlight(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    if (_c.items.empty())
    {
        return false;
    }

    _c.highlighted = (_c.highlighted + 1) % _c.items.size();
    internal::cmb_ensure_visible_(_c);

    return true;
}


/*
cmb_prev_highlight
  Retreats the highlight to the previous item, wrapping to the
last item at index 0.  Updates scroll_offset to keep the highlight
visible.

Parameter(s):
  _c: the combo box to navigate.
Return:
  true if the highlight moved; false if the item list is empty.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_prev_highlight(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    if (_c.items.empty())
    {
        return false;
    }

    if (_c.highlighted == 0)
    {
        _c.highlighted = _c.items.size() - 1;
    }
    else
    {
        _c.highlighted = _c.highlighted - 1;
    }

    internal::cmb_ensure_visible_(_c);

    return true;
}


/*
cmb_set_highlight
  Jumps the highlight to a specified index.  Updates scroll_offset
to keep the highlight visible.

Parameter(s):
  _c:   the combo box to navigate.
  _idx: the target index.  Must be < _c.items.size().
Return:
  true if _idx was in range and the highlight was updated;
  false otherwise.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_set_highlight(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c,
    std::size_t           _idx
)
{
    if (_idx >= _c.items.size())
    {
        return false;
    }

    _c.highlighted = _idx;
    internal::cmb_ensure_visible_(_c);

    return true;
}




// ===============================================================================
//  5.5  COMMIT HIGHLIGHTED
// ===============================================================================

/*
cmb_commit_highlighted
  Finalizes the currently highlighted option.  For single-select
combos, sets value to `highlighted`, invokes on_commit, and
closes the panel.  For multi-select combos, toggles the
highlighted item in the selection and leaves the panel open
(the canonical "check-list" interaction).  Goes through set_value
internally so undo and on_change fire correctly.

Parameter(s):
  _c: the combo box to commit against.
Return:
  true if the commit took effect; false if the combo is disabled,
  read-only, has no items, or the highlight is out of range.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_commit_highlighted(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    if ( (!_c.enabled)              ||
         (_c.read_only)              ||
         (_c.items.empty())          ||
         (_c.highlighted >= _c.items.size()) )
    {
        return false;
    }

    if constexpr (_MultiSelect)
    {
        cmb_toggle_selection(_c, _c.highlighted);
    }
    else
    {
        set_value(_c, _c.highlighted);

        if (_c.on_commit)
        {
            _c.on_commit(_c.value);
        }

        _c.open = false;
    }

    return true;
}




// ===============================================================================
//  5.6  FILTER  (requires _Filterable)
// ===============================================================================

/*
cmb_set_filter_predicate
  Installs a user-supplied filter predicate.  The predicate takes a
const reference to an item and returns true if the item should be
visible in the option panel.  Marks the filtered-index cache
dirty; the cache is lazily rebuilt by cmb_apply_filter.

Parameter(s):
  _c:    the combo box whose filter to set.  Must be instantiated
         with _Filterable == true.
  _pred: the predicate to install.  A null / default-constructed
         std::function disables filtering (equivalent to
         "show all").
Return:
  none.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
void
cmb_set_filter_predicate(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c,
    std::function<bool(const _Item&)> _pred
)
{
    static_assert(_Filterable,
                  "cmb_set_filter_predicate requires a filterable "
                  "combo box (_Filterable template parameter must "
                  "be true).");

    _c.filter_predicate = std::move(_pred);
    _c.filter_dirty     = true;

    return;
}


/*
cmb_set_filter_text
  Updates the filter text buffer.  This is pure state — the
associated predicate is responsible for consulting filter_text
when the cache is rebuilt.  Marks the cache dirty.

Parameter(s):
  _c:    the combo box whose filter text to set.  Must be
         instantiated with _Filterable == true.
  _text: the new filter text.
Return:
  none.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
void
cmb_set_filter_text(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c,
    std::string           _text
)
{
    static_assert(_Filterable,
                  "cmb_set_filter_text requires a filterable "
                  "combo box (_Filterable template parameter must "
                  "be true).");

    _c.filter_text  = std::move(_text);
    _c.filter_dirty = true;

    return;
}


/*
cmb_apply_filter
  Rebuilds the filtered-index cache.  If no predicate is installed,
the cache degenerates to [0, items.size()) — i.e. every item
passes.  After rebuild, clears the dirty flag and clamps the
highlight to the new visible range so navigation remains coherent.

Parameter(s):
  _c: the combo box whose cache to rebuild.  Must be instantiated
      with _Filterable == true.
Return:
  none.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
void
cmb_apply_filter(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    static_assert(_Filterable,
                  "cmb_apply_filter requires a filterable combo box "
                  "(_Filterable template parameter must be true).");

    std::size_t i;

    _c.filtered_indices.clear();

    if (_c.filter_predicate)
    {
        for (i = 0; i < _c.items.size(); ++i)
        {
            if (_c.filter_predicate(_c.items[i]))
            {
                _c.filtered_indices.push_back(i);
            }
        }
    }
    else
    {
        _c.filtered_indices.reserve(_c.items.size());
        for (i = 0; i < _c.items.size(); ++i)
        {
            _c.filtered_indices.push_back(i);
        }
    }

    // clamp highlight so it points at a valid item (or 0 when empty)
    if (_c.items.empty())
    {
        _c.highlighted = 0;
    }
    else if (_c.highlighted >= _c.items.size())
    {
        _c.highlighted = _c.items.size() - 1;
    }

    _c.filter_dirty = false;

    return;
}


/*
cmb_clear_filter
  Drops the filter predicate and text, marks the cache dirty, and
restores "show all" behavior.

Parameter(s):
  _c: the combo box whose filter to clear.  Must be instantiated
      with _Filterable == true.
Return:
  none.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
void
cmb_clear_filter(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    static_assert(_Filterable,
                  "cmb_clear_filter requires a filterable combo box "
                  "(_Filterable template parameter must be true).");

    _c.filter_predicate = nullptr;
    _c.filter_text.clear();
    _c.filter_dirty     = true;

    return;
}




// ===============================================================================
//  5.7  EDIT BUFFER  (requires _Editable)
// ===============================================================================

/*
cmb_begin_edit
  Signals the start of text composition.  The renderer should route
keyboard input into edit_buffer until cmb_end_edit or cmb_cancel_edit
is called.  If the combo is disabled or read-only, the call is a
no-op and returns false.

Parameter(s):
  _c: the combo box to enter edit mode.  Must be instantiated
      with _Editable == true.
Return:
  true if edit mode was entered; false if it was already active,
  or the combo is disabled / read-only.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_begin_edit(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    static_assert(_Editable,
                  "cmb_begin_edit requires an editable combo box "
                  "(_Editable template parameter must be true).");

    if ( (!_c.enabled) ||
         (_c.read_only) )
    {
        return false;
    }

    if (_c.editing)
    {
        return false;
    }

    _c.editing = true;

    return true;
}


/*
cmb_end_edit
  Concludes text composition and keeps edit_buffer intact for
inspection by the caller.  Clients that want to append a new
item from the buffer (the canonical "type a new value" combo
behavior) should do so after this call.

Parameter(s):
  _c: the combo box whose edit mode to end.  Must be instantiated
      with _Editable == true.
Return:
  true if edit mode was active and has been concluded; false if
  edit mode was not active.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
bool
cmb_end_edit(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    static_assert(_Editable,
                  "cmb_end_edit requires an editable combo box "
                  "(_Editable template parameter must be true).");

    if (!_c.editing)
    {
        return false;
    }

    _c.editing = false;

    return true;
}


/*
cmb_cancel_edit
  Aborts text composition and discards the contents of edit_buffer.
Useful for Escape-key handling in renderers.

Parameter(s):
  _c: the combo box whose edit mode to cancel.  Must be
      instantiated with _Editable == true.
Return:
  none.
*/
template <typename _Item,
          bool     _MultiSelect,
          bool     _Editable,
          bool     _Filterable,
          bool     _HasLabel,
          bool     _Clearable,
          bool     _Undoable>
void
cmb_cancel_edit(
    combo_box<_Item,
              _MultiSelect,
              _Editable,
              _Filterable,
              _HasLabel,
              _Clearable,
              _Undoable>& _c
)
{
    static_assert(_Editable,
                  "cmb_cancel_edit requires an editable combo box "
                  "(_Editable template parameter must be true).");

    _c.editing = false;
    _c.edit_buffer.clear();

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_COMBO_BOX_
