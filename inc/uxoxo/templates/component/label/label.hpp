/*******************************************************************************
* uxoxo [component]                                                    label.hpp
*
* Static text display component:
*   A structurally-conforming uxoxo component for displaying a string
* value with optional alignment and emphasis.  Output-like: not
* focusable, no on_commit, no read_only.  The component's `.value`
* member IS the displayed text, so all of the standard text-setting
* verbs from component_common.hpp (set_value, get_value, show, hide,
* enable, disable) work directly.
*
*   Named `label_control` rather than `label` to avoid ambiguity with
* both `uxoxo::ui::label` (the concrete plain-struct label in the ui
* namespace) and `component_mixin::label_data` (the EBO mixin that
* adds a secondary label string to other components).  The filename
* `label.hpp` matches the switch.hpp / toggle_switch precedent where
* the filename describes the concept and the type name avoids a
* keyword or naming collision.
*
*   Deliberately un-templated.  Every capability axis that could plausibly
* be mixin-gated (alignment, emphasis, on_change) is cheap enough in raw
* bytes that the EBO-optimization overhead of making each optional would
* outweigh the savings.  If a future use case demands it — say a
* zero-allocation embedded build — a templated variant can be added later.
*
* Contents:
*   1  label_control struct
*   2  Label-specific free functions (lb_append)
*
*
* path:      /inc/uxoxo/templates/component/label.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_LABEL_
#define  UXOXO_COMPONENT_LABEL_ 1

// std
#include <functional>
#include <string>
#include <string_view>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "./component_common.hpp"
#include "./component_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  LABEL CONTROL
// ===============================================================================

// label_control
//   struct: static text display component.  Output-like (not focusable,
// no on_commit).  Satisfies is_output_like_v through its .value +
// .enabled + .visible + .on_change members.
struct label_control
{
    static constexpr bool focusable = false;

    std::string                              value;
    bool                                     enabled   = true;
    bool                                     visible   = true;
    DTextAlignment                           alignment = DTextAlignment::left;
    DEmphasis                                emph      = DEmphasis::normal;

    std::function<void(const std::string&)>  on_change;
};


/*****************************************************************************/

// ===============================================================================
//  2  LABEL-SPECIFIC FREE FUNCTIONS
// ===============================================================================
//   All generic verbs (set_value, get_value, show, hide, enable,
// disable) from component_common.hpp work on label_control directly.
// The only label-specific operation that has no generic analogue is
// append, which is useful enough for log-style and streaming-text
// labels to warrant a first-class helper.

/*
lb_append
  Appends text to the label's value and fires on_change.  Equivalent to
`set_value(lb, lb.value + text)` but avoids the intermediate string
copy.

Parameter(s):
  _lb:   the label to append to.
  _text: the text to append.  Accepted as std::string_view so callers
         can pass literals, std::string, or any string-viewable type.
Return:
  none.
*/
inline void
lb_append(
    label_control&     _lb,
    std::string_view   _text
)
{
    _lb.value.append(_text);

    if (_lb.on_change)
    {
        _lb.on_change(_lb.value);
    }

    return;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_LABEL_
