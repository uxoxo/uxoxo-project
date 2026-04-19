/*******************************************************************************
* uxoxo [component]                                             tab_control.hpp
*
* Tab control component:
*   A framework-agnostic, pure-data tab/tab-bar template.  Manages an
* ordered collection of tab entries and the selection, scrolling, and
* layout state that any renderer needs to draw a tabbed interface.
*
*   The component is split into two template levels:
*
*     tab_entry<_TabFeat, _Icon>
*       A single tab.  Analogous to a button with domain-specific
*       extensions: closable, pinnable, badged, context-menu-equipped.
*       Each tab has a label, enabled/visible flags, and zero-cost
*       optional features via EBO mixins.
*
*     tab_control<_TabFeat, _CtrlFeat, _Icon>
*       The tab bar / tab strip.  Owns a vector of tab_entry, manages
*       selected index, placement (top/bottom/left/right), scroll
*       state, multi-row wrapping, drag-to-reorder, overflow menus,
*       and an add-tab button.  Participates in the component
*       infrastructure via enabled/visible and visit_components.
*
*   The template prescribes NOTHING about rendering.  A renderer
* discovers capabilities via tab_traits:: and dispatches with
* if constexpr.  The tab_control is a pure data aggregate — all
* mutation is via free functions prefixed `tab_` (domain-specific)
* or the shared ADL functions from component_common.hpp.
*
*   Feature composition follows the same EBO-mixin bitfield pattern
* used throughout the uxoxo component layer.
*
* Contents:
*   1.   Tab entry feature flags (tab_feat)
*   2.   Tab control feature flags (tab_control_feat)
*   3.   Enums (placement, overflow, close policy, drag state)
*   4.   Tab entry EBO mixins
*   5.   tab_entry struct
*   6.   Tab control EBO mixins
*   7.   tab_control struct
*   8.   Tab entry free functions
*   9.   Tab control free functions
*   10.  Traits
*
*
* path:      /inc/uxoxo/templates/component/tab/tab_control.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.15
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_TAB_CONTROL_
#define  UXOXO_COMPONENT_TAB_CONTROL_ 1

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
#include "../../component_traits.hpp"
#include "../../component_common.hpp"


NS_UXOXO
NS_COMPONENT

// ===============================================================================
//  1.  TAB ENTRY FEATURE FLAGS
// ===============================================================================

//   Per-tab features.  These control what optional data each
// tab_entry carries.
enum tab_feat : unsigned
{
    tf_none      = 0,
    tf_closable  = 1u << 0,     // per-tab close button
    tf_icon      = 1u << 1,     // per-tab icon
    tf_tooltip   = 1u << 2,     // per-tab hover tooltip
    tf_badge     = 1u << 3,     // per-tab badge counter overlay
    tf_context   = 1u << 4,     // per-tab context-menu actions
    tf_pinnable  = 1u << 5,     // tab can be pinned (immovable, unclosable)
    tf_color     = 1u << 6,     // per-tab color override
    tf_renamable = 1u << 7,     // tab can be renamed inline
    tf_standard  = tf_closable  | 
                   tf_icon      | 
                   tf_tooltip,
    tf_all       = tf_closable  |
                   tf_icon      | 
                   tf_tooltip   |
                   tf_badge     |
                   tf_context   | 
                   tf_pinnable  |
                   tf_color     |
                   tf_renamable
};

constexpr unsigned operator|(
    tab_feat _a,
    tab_feat _b
) D_NOEXCEPT
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool has_tf(
    unsigned _f,
    tab_feat _bit
) D_NOEXCEPT
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}

// ===============================================================================
//  2.  TAB CONTROL FEATURE FLAGS
// ===============================================================================
//   Control-level features.  These govern the tab bar's layout
// and interaction capabilities.

enum tab_control_feat : unsigned
{
    tcf_none          = 0,
    tcf_scrollable    = 1u << 0,     // tab bar scrolls on overflow
    tcf_reorderable   = 1u << 1,     // tabs can be dragged to reorder
    tcf_add_button    = 1u << 2,     // "+" button to add new tab
    tcf_overflow_menu = 1u << 3,     // dropdown for tabs that don't fit
    tcf_multirow      = 1u << 4,     // tabs wrap to multiple rows
    tcf_detachable    = 1u << 5,     // tabs can be torn off / floated
    tcf_animated      = 1u << 6,     // smooth transitions on reorder/close

    tcf_standard      = tcf_scrollable | tcf_reorderable | tcf_add_button,
    tcf_all           = tcf_scrollable    | tcf_reorderable
                      | tcf_add_button    | tcf_overflow_menu
                      | tcf_multirow      | tcf_detachable
                      | tcf_animated
};

constexpr unsigned operator|(
    tab_control_feat _a,
    tab_control_feat _b
) D_NOEXCEPT
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool has_tcf(
    unsigned          _f,
    tab_control_feat  _bit
) D_NOEXCEPT
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}

// ===============================================================================
//  3.  ENUMS
// ===============================================================================

// tab_placement
//   enum: which edge of the content area the tabs are drawn on.
enum class tab_placement : std::uint8_t
{
    top,
    bottom,
    left,
    right
};

// tab_overflow_mode
//   enum: how the tab bar handles more tabs than fit in the
// available space.  The renderer uses this as a hint.
enum class tab_overflow_mode : std::uint8_t
{
    scroll,         // scroll arrows / drag to scroll
    wrap,           // wrap to additional rows
    compress,       // shrink tab widths to fit
    hide            // hide overflow tabs (use with overflow_menu)
};

// tab_close_policy
//   enum: when the close button is shown on a tab.
enum class tab_close_policy : std::uint8_t
{
    always,         // close button always visible
    selected_only,  // close button only on active tab
    hover_only,     // close button on hover
    never           // no close button (programmatic close only)
};

// tab_size_mode
//   enum: how individual tab widths are determined.
enum class tab_size_mode : std::uint8_t
{
    fit_content,    // each tab sized to its label + icon
    equal,          // all tabs the same width
    fill,           // tabs stretch to fill available space
    compact         // minimal width, icon-only when crowded
};

// drag_state
//   enum: reorder drag lifecycle.
enum class tab_drag_state : std::uint8_t
{
    idle,
    hovering,       // cursor over a drag handle
    dragging,       // actively dragging a tab
    dropping        // about to drop (over a valid target)
};

// ===============================================================================
//  4.  TAB ENTRY EBO MIXINS
// ===============================================================================

namespace tab_mixin {

    // -- closable ---------------------------------------------------------
    template <bool _Enable>
    struct closable_data
    {};

    template <>
    struct closable_data<true>
    {
        tab_close_policy close_policy = tab_close_policy::always;
    };

    // -- icon -------------------------------------------------------------
    template <bool _Enable,
              typename _Icon = int>
    struct icon_data
    {};

    template <typename _Icon>
    struct icon_data<true, _Icon>
    {
        _Icon icon{};
    };

    // -- tooltip ----------------------------------------------------------
    template <bool _Enable>
    struct tooltip_data
    {};

    template <>
    struct tooltip_data<true>
    {
        std::string tooltip;
    };

    // -- badge ------------------------------------------------------------
    template <bool _Enable>
    struct badge_data
    {};

    template <>
    struct badge_data<true>
    {
        int  badge_count   = 0;
        bool badge_visible = false;
    };

    // -- context ----------------------------------------------------------
    template <bool _Enable>
    struct context_data
    {};

    template <>
    struct context_data<true>
    {
        unsigned context_actions = 0;
    };

    // -- pinnable ---------------------------------------------------------
    template <bool _Enable>
    struct pin_data
    {};

    template <>
    struct pin_data<true>
    {
        bool pinned = false;
    };

    // -- color override ---------------------------------------------------
    template <bool _Enable>
    struct color_data
    {};

    template <>
    struct color_data<true>
    {
        float tab_r = 0.0f;
        float tab_g = 0.0f;
        float tab_b = 0.0f;
        float tab_a = 0.0f;     // 0 = use theme default
    };

    // -- renamable --------------------------------------------------------
    template <bool _Enable>
    struct rename_data
    {};

    template <>
    struct rename_data<true>
    {
        bool renamable = true;
    };

}   // namespace tab_mixin

// ===============================================================================
//  5.  TAB ENTRY
// ===============================================================================
//   A single tab.  Pure data aggregate with zero-cost optional features.

template <unsigned _Feat = tf_none,
          typename _Icon = int>
struct tab_entry
    : tab_mixin::closable_data <has_tf(_Feat, tf_closable)>
    , tab_mixin::icon_data     <has_tf(_Feat, tf_icon), _Icon>
    , tab_mixin::tooltip_data  <has_tf(_Feat, tf_tooltip)>
    , tab_mixin::badge_data    <has_tf(_Feat, tf_badge)>
    , tab_mixin::context_data  <has_tf(_Feat, tf_context)>
    , tab_mixin::pin_data      <has_tf(_Feat, tf_pinnable)>
    , tab_mixin::color_data    <has_tf(_Feat, tf_color)>
    , tab_mixin::rename_data   <has_tf(_Feat, tf_renamable)>
{
    using self_type = tab_entry<_Feat, _Icon>;
    using icon_type = _Icon;

    static constexpr unsigned features = _Feat;

    // compile-time feature queries
    static constexpr bool is_closable   = has_tf(_Feat, tf_closable);
    static constexpr bool has_icon      = has_tf(_Feat, tf_icon);
    static constexpr bool has_tooltip   = has_tf(_Feat, tf_tooltip);
    static constexpr bool has_badge     = has_tf(_Feat, tf_badge);
    static constexpr bool has_context   = has_tf(_Feat, tf_context);
    static constexpr bool is_pinnable   = has_tf(_Feat, tf_pinnable);
    static constexpr bool has_color     = has_tf(_Feat, tf_color);
    static constexpr bool is_renamable  = has_tf(_Feat, tf_renamable);

    // -- core state -------------------------------------------------------
    std::string label;
    bool        enabled  = true;
    bool        visible  = true;

    // -- interaction state (set by renderer) ------------------------------
    bool        hovered  = false;
    bool        pressed  = false;

    // -- user data --------------------------------------------------------
    //   opaque identifier for associating tabs with content.
    // The framework does not interpret this value.
    std::size_t user_id  = 0;

    // -- construction -----------------------------------------------------
    tab_entry() = default;

    explicit tab_entry(
            std::string _label
        )
            : label(std::move(_label))
        {}

    tab_entry(
            std::string _label,
            std::size_t _user_id
        )
            : label(std::move(_label)),
              user_id(_user_id)
        {}
};

// ===============================================================================
//  6.  TAB CONTROL EBO MIXINS
// ===============================================================================

namespace tab_control_mixin {

    // -- scrollable -------------------------------------------------------
    template <bool _Enable>
    struct scroll_data
    {};

    template <>
    struct scroll_data<true>
    {
        float  scroll_offset = 0.0f;   // pixel offset along tab axis
        float  scroll_max    = 0.0f;   // max offset (set by renderer)
        float  scroll_speed  = 30.0f;  // pixels per scroll tick
        bool   can_scroll_prev = false; // set by renderer
        bool   can_scroll_next = false; // set by renderer
    };

    // -- reorderable ------------------------------------------------------
    template <bool _Enable>
    struct reorder_data
    {};

    template <>
    struct reorder_data<true>
    {
        tab_drag_state drag        = tab_drag_state::idle;
        std::size_t    drag_index  = 0;    // index of tab being dragged
        std::size_t    drop_target = 0;    // index of drop position
        float          drag_offset = 0.0f; // visual offset during drag
    };

    // -- add button -------------------------------------------------------
    template <bool _Enable>
    struct add_btn_data
    {};

    template <>
    struct add_btn_data<true>
    {
        bool        add_enabled = true;
        bool        add_hovered = false;
        std::string add_tooltip;
    };

    // -- overflow menu ----------------------------------------------------
    template <bool _Enable>
    struct overflow_data
    {};

    template <>
    struct overflow_data<true>
    {
        bool                     overflow_active = false;
        std::vector<std::size_t> overflow_indices;
    };

    // -- multirow ---------------------------------------------------------
    template <bool _Enable>
    struct multirow_data
    {};

    template <>
    struct multirow_data<true>
    {
        std::size_t max_rows     = 2;
        std::size_t current_rows = 1;   // set by renderer
    };

    // -- detachable -------------------------------------------------------
    template <bool _Enable>
    struct detach_data
    {};

    template <>
    struct detach_data<true>
    {
        bool        detach_active = false;
        std::size_t detach_index  = 0;
    };

    // -- animated ---------------------------------------------------------
    template <bool _Enable>
    struct anim_data
    {};

    template <>
    struct anim_data<true>
    {
        float anim_progress = 0.0f;     // 0.0 → 1.0 transition
        float anim_duration = 0.15f;    // seconds
    };

}   // namespace tab_control_mixin

// ===============================================================================
//  7.  TAB CONTROL
// ===============================================================================
//   _TabFeat   bitwise OR of tab_feat flags for per-tab features.
//   _CtrlFeat  bitwise OR of tab_control_feat flags for control features.
//   _Icon      icon storage type.

template <unsigned _TabFeat  = tf_none,
          unsigned _CtrlFeat = tcf_none,
          typename _Icon     = int>
struct tab_control
    : tab_control_mixin::scroll_data   <has_tcf(_CtrlFeat, tcf_scrollable)>
    , tab_control_mixin::reorder_data  <has_tcf(_CtrlFeat, tcf_reorderable)>
    , tab_control_mixin::add_btn_data  <has_tcf(_CtrlFeat, tcf_add_button)>
    , tab_control_mixin::overflow_data <has_tcf(_CtrlFeat, tcf_overflow_menu)>
    , tab_control_mixin::multirow_data <has_tcf(_CtrlFeat, tcf_multirow)>
    , tab_control_mixin::detach_data   <has_tcf(_CtrlFeat, tcf_detachable)>
    , tab_control_mixin::anim_data     <has_tcf(_CtrlFeat, tcf_animated)>
{
    using entry_type = tab_entry<_TabFeat, _Icon>;
    using icon_type  = _Icon;
    using size_type  = std::size_t;

    static constexpr unsigned tab_features  = _TabFeat;
    static constexpr unsigned features      = _CtrlFeat;

    // compile-time feature queries (tab entry level)
    static constexpr bool tabs_closable    = has_tf(_TabFeat, tf_closable);
    static constexpr bool tabs_have_icons  = has_tf(_TabFeat, tf_icon);
    static constexpr bool tabs_have_badges = has_tf(_TabFeat, tf_badge);
    static constexpr bool tabs_pinnable    = has_tf(_TabFeat, tf_pinnable);
    static constexpr bool tabs_renamable   = has_tf(_TabFeat, tf_renamable);

    // compile-time feature queries (control level)
    static constexpr bool is_scrollable    = has_tcf(_CtrlFeat, tcf_scrollable);
    static constexpr bool is_reorderable   = has_tcf(_CtrlFeat, tcf_reorderable);
    static constexpr bool has_add_button   = has_tcf(_CtrlFeat, tcf_add_button);
    static constexpr bool has_overflow     = has_tcf(_CtrlFeat, tcf_overflow_menu);
    static constexpr bool is_multirow      = has_tcf(_CtrlFeat, tcf_multirow);
    static constexpr bool is_detachable    = has_tcf(_CtrlFeat, tcf_detachable);
    static constexpr bool is_animated      = has_tcf(_CtrlFeat, tcf_animated);

    // component identity
    static constexpr bool focusable = true;

    // -- tabs -------------------------------------------------------------
    std::vector<entry_type> tabs;
    size_type               selected = 0;

    // -- layout -----------------------------------------------------------
    tab_placement     placement = tab_placement::top;
    tab_size_mode     size_mode = tab_size_mode::fit_content;
    tab_overflow_mode overflow  = tab_overflow_mode::scroll;
    float             min_tab_width = 60.0f;
    float             max_tab_width = 200.0f;

    // -- state ------------------------------------------------------------
    bool        enabled  = true;
    bool        visible  = true;

    // -- rename state (when tabs are renamable) ----------------------------
    //   Stored at the control level so the renderer can coordinate
    // with a single text_input overlay.
    bool        renaming       = false;
    size_type   rename_index   = 0;
    std::string rename_buffer;

    // -- callbacks --------------------------------------------------------
    using select_fn   = std::function<void(size_type)>;
    using close_fn    = std::function<bool(size_type)>;
    using reorder_fn  = std::function<void(size_type, size_type)>;
    using add_fn      = std::function<void()>;
    using detach_fn   = std::function<void(size_type)>;
    using rename_fn   = std::function<bool(size_type, const std::string&)>;
    using context_fn  = std::function<void(size_type, unsigned)>;

    select_fn   on_select;      // called when selection changes
    close_fn    on_close;       // called when tab close requested; return false to veto
    reorder_fn  on_reorder;     // called after drag-drop reorder completes
    add_fn      on_add;         // called when add button is clicked
    detach_fn   on_detach;      // called when tab is torn off
    rename_fn   on_rename;      // called on rename commit; return false to reject
    context_fn  on_context;     // called when context action is selected

    // -- construction -----------------------------------------------------
    tab_control() = default;

    explicit tab_control(
            tab_placement _placement
        )
            : placement(_placement)
        {}

    // -- queries ----------------------------------------------------------
    [[nodiscard]] bool
    empty() const D_NOEXCEPT
    {
        return tabs.empty();
    }

    [[nodiscard]] size_type
    count() const D_NOEXCEPT
    {
        return tabs.size();
    }

    [[nodiscard]] bool
    valid_index(size_type _idx) const D_NOEXCEPT
    {
        return _idx < tabs.size();
    }

    [[nodiscard]] entry_type*
    selected_tab() D_NOEXCEPT
    {
        if (selected < tabs.size())
        {
            return &tabs[selected];
        }

        return nullptr;
    }

    [[nodiscard]] const entry_type*
    selected_tab() const D_NOEXCEPT
    {
        if (selected < tabs.size())
        {
            return &tabs[selected];
        }

        return nullptr;
    }

    // -- compositional forwarding -----------------------------------------
    template <typename _Fn>
    void visit_components(_Fn&& _fn)
    {
        for (auto& tab : tabs)
        {
            _fn(tab);
        }

        return;
    }
};

// ===============================================================================
//  8.  TAB ENTRY FREE FUNCTIONS
// ===============================================================================
//   Domain-specific per-tab operations.  Shared operations (enable,
// disable, show, hide) work on tab entries directly via the ADL
// functions in component_common.hpp since tab_entry has enabled
// and visible members.

// tab_set_badge
template <unsigned _F, typename _I>
void tab_set_badge(tab_entry<_F, _I>& _tab,
                   int                _count)
{
    static_assert(has_tf(_F, tf_badge),
                  "requires tf_badge");

    _tab.badge_count   = _count;
    _tab.badge_visible = (_count > 0);

    return;
}

// tab_pin
template <unsigned _F, typename _I>
void tab_pin(tab_entry<_F, _I>& _tab)
{
    static_assert(has_tf(_F, tf_pinnable),
                  "requires tf_pinnable");

    _tab.pinned = true;

    return;
}

// tab_unpin
template <unsigned _F, typename _I>
void tab_unpin(tab_entry<_F, _I>& _tab)
{
    static_assert(has_tf(_F, tf_pinnable),
                  "requires tf_pinnable");

    _tab.pinned = false;

    return;
}

// tab_is_pinned
template <unsigned _F, typename _I>
bool tab_is_pinned(const tab_entry<_F, _I>& _tab)
{
    static_assert(has_tf(_F, tf_pinnable),
                  "requires tf_pinnable");

    return _tab.pinned;
}

// tab_set_icon
template <unsigned _F, typename _I>
void tab_set_icon(tab_entry<_F, _I>&                       _tab,
                  typename tab_entry<_F, _I>::icon_type     _icon)
{
    static_assert(has_tf(_F, tf_icon),
                  "requires tf_icon");

    _tab.icon = std::move(_icon);

    return;
}

// tab_set_tooltip
template <unsigned _F, typename _I>
void tab_set_tooltip(tab_entry<_F, _I>& _tab,
                     std::string        _text)
{
    static_assert(has_tf(_F, tf_tooltip),
                  "requires tf_tooltip");

    _tab.tooltip = std::move(_text);

    return;
}

// tab_set_color
template <unsigned _F, typename _I>
void tab_set_color(tab_entry<_F, _I>& _tab,
                   float _r, float _g, float _b, float _a)
{
    static_assert(has_tf(_F, tf_color),
                  "requires tf_color");

    _tab.tab_r = _r;
    _tab.tab_g = _g;
    _tab.tab_b = _b;
    _tab.tab_a = _a;

    return;
}

// ===============================================================================
//  9.  TAB CONTROL FREE FUNCTIONS
// ===============================================================================

// -- tab management -------------------------------------------------------

// tc_add_tab
//   function: appends a new tab and returns a reference to it.
template <unsigned _TF, unsigned _CF, typename _I>
tab_entry<_TF, _I>&
tc_add_tab(tab_control<_TF, _CF, _I>& _tc,
           std::string                _label)
{
    _tc.tabs.emplace_back(std::move(_label));

    return _tc.tabs.back();
}

// tc_add_tab (with user_id)
template <unsigned _TF, unsigned _CF, typename _I>
tab_entry<_TF, _I>&
tc_add_tab(tab_control<_TF, _CF, _I>& _tc,
           std::string                _label,
           std::size_t                _user_id)
{
    _tc.tabs.emplace_back(std::move(_label), _user_id);

    return _tc.tabs.back();
}

// tc_insert_tab
//   function: inserts a tab at the given index.  Adjusts selected
// index if necessary.
template <unsigned _TF, unsigned _CF, typename _I>
tab_entry<_TF, _I>&
tc_insert_tab(tab_control<_TF, _CF, _I>& _tc,
              std::size_t                _index,
              std::string                _label)
{
    if (_index > _tc.tabs.size())
    {
        _index = _tc.tabs.size();
    }

    auto it = _tc.tabs.emplace(
        _tc.tabs.begin() + static_cast<std::ptrdiff_t>(_index),
        std::move(_label));

    // adjust selected if insertion is before or at selected
    if (_index <= _tc.selected && !_tc.tabs.empty())
    {
        ++_tc.selected;
    }

    return *it;
}

// tc_close_tab
//   function: closes (removes) the tab at the given index.
// Invokes on_close if set; returns false if the close was vetoed.
// Adjusts selected index to stay in bounds.
template <unsigned _TF, unsigned _CF, typename _I>
bool tc_close_tab(tab_control<_TF, _CF, _I>& _tc,
                  std::size_t                _index)
{
    if (_index >= _tc.tabs.size())
    {
        return false;
    }

    // check pinned
    if constexpr (has_tf(_TF, tf_pinnable))
    {
        if (_tc.tabs[_index].pinned)
        {
            return false;
        }
    }

    // veto check
    if (_tc.on_close)
    {
        if (!_tc.on_close(_index))
        {
            return false;
        }
    }

    _tc.tabs.erase(
        _tc.tabs.begin() + static_cast<std::ptrdiff_t>(_index));

    // adjust selected
    if (_tc.tabs.empty())
    {
        _tc.selected = 0;
    }
    else if (_tc.selected >= _tc.tabs.size())
    {
        _tc.selected = _tc.tabs.size() - 1;
    }
    else if ( (_tc.selected > _index) &&
              (_tc.selected > 0) )
    {
        --_tc.selected;
    }

    return true;
}

// tc_close_all
//   function: closes all non-pinned tabs.  Returns the number of
// tabs closed.
template <unsigned _TF, unsigned _CF, typename _I>
std::size_t
tc_close_all(tab_control<_TF, _CF, _I>& _tc)
{
    std::size_t closed = 0;

    // iterate backwards to maintain index stability
    for (std::size_t i = _tc.tabs.size(); i > 0; --i)
    {
        std::size_t idx = i - 1;

        if constexpr (has_tf(_TF, tf_pinnable))
        {
            if (_tc.tabs[idx].pinned)
            {
                continue;
            }
        }

        if (tc_close_tab(_tc, idx))
        {
            ++closed;
        }
    }

    return closed;
}

// -- selection ------------------------------------------------------------

// tc_select
//   function: selects the tab at the given index.  Invokes
// on_select if set.  Returns false if the index is out of range
// or the tab is disabled.
template <unsigned _TF, unsigned _CF, typename _I>
bool tc_select(tab_control<_TF, _CF, _I>& _tc,
               std::size_t                _index)
{
    if (_index >= _tc.tabs.size())
    {
        return false;
    }

    if (!_tc.tabs[_index].enabled)
    {
        return false;
    }

    _tc.selected = _index;

    if (_tc.on_select)
    {
        _tc.on_select(_index);
    }

    return true;
}

// tc_select_next
//   function: selects the next enabled, visible tab.  Wraps around.
template <unsigned _TF, unsigned _CF, typename _I>
bool tc_select_next(tab_control<_TF, _CF, _I>& _tc)
{
    if (_tc.tabs.empty())
    {
        return false;
    }

    std::size_t start = (_tc.selected + 1) % _tc.tabs.size();
    std::size_t i     = start;

    do
    {
        if ( (_tc.tabs[i].enabled) &&
             (_tc.tabs[i].visible) )
        {
            return tc_select(_tc, i);
        }

        i = (i + 1) % _tc.tabs.size();
    }
    while (i != start);

    return false;
}

// tc_select_prev
//   function: selects the previous enabled, visible tab.  Wraps around.
template <unsigned _TF, unsigned _CF, typename _I>
bool tc_select_prev(tab_control<_TF, _CF, _I>& _tc)
{
    if (_tc.tabs.empty())
    {
        return false;
    }

    std::size_t start =
        (_tc.selected + _tc.tabs.size() - 1) % _tc.tabs.size();
    std::size_t i = start;

    do
    {
        if ( (_tc.tabs[i].enabled) &&
             (_tc.tabs[i].visible) )
        {
            return tc_select(_tc, i);
        }

        i = (i + _tc.tabs.size() - 1) % _tc.tabs.size();
    }
    while (i != start);

    return false;
}

// tc_select_by_id
//   function: selects the first tab with the given user_id.
template <unsigned _TF, unsigned _CF, typename _I>
bool tc_select_by_id(tab_control<_TF, _CF, _I>& _tc,
                     std::size_t                _user_id)
{
    for (std::size_t i = 0; i < _tc.tabs.size(); ++i)
    {
        if (_tc.tabs[i].user_id == _user_id)
        {
            return tc_select(_tc, i);
        }
    }

    return false;
}

// -- find -----------------------------------------------------------------

// tc_find_by_id
//   function: returns a pointer to the first tab with the given
// user_id, or nullptr.
template <unsigned _TF, unsigned _CF, typename _I>
tab_entry<_TF, _I>*
tc_find_by_id(tab_control<_TF, _CF, _I>& _tc,
              std::size_t                _user_id)
{
    for (auto& tab : _tc.tabs)
    {
        if (tab.user_id == _user_id)
        {
            return &tab;
        }
    }

    return nullptr;
}

// tc_find_by_label
//   function: returns a pointer to the first tab with a matching
// label, or nullptr.
template <unsigned _TF, unsigned _CF, typename _I>
tab_entry<_TF, _I>*
tc_find_by_label(tab_control<_TF, _CF, _I>& _tc,
                 const std::string&         _label)
{
    for (auto& tab : _tc.tabs)
    {
        if (tab.label == _label)
        {
            return &tab;
        }
    }

    return nullptr;
}

// -- reorder --------------------------------------------------------------

// tc_move_tab
//   function: moves a tab from one index to another.  Adjusts
// selected index.  Invokes on_reorder if set.
template <unsigned _TF, unsigned _CF, typename _I>
bool tc_move_tab(tab_control<_TF, _CF, _I>& _tc,
                 std::size_t                _from,
                 std::size_t                _to)
{
    if ( (_from >= _tc.tabs.size()) ||
         (_to   >= _tc.tabs.size()) ||
         (_from == _to) )
    {
        return false;
    }

    // pinned tabs cannot be moved
    if constexpr (has_tf(_TF, tf_pinnable))
    {
        if (_tc.tabs[_from].pinned)
        {
            return false;
        }
    }

    auto entry = std::move(_tc.tabs[_from]);

    _tc.tabs.erase(
        _tc.tabs.begin() + static_cast<std::ptrdiff_t>(_from));
    _tc.tabs.insert(
        _tc.tabs.begin() + static_cast<std::ptrdiff_t>(_to),
        std::move(entry));

    // adjust selected to follow the moved tab if it was selected
    if (_tc.selected == _from)
    {
        _tc.selected = _to;
    }
    else if ( (_from < _tc.selected) &&
              (_to   >= _tc.selected) )
    {
        --_tc.selected;
    }
    else if ( (_from > _tc.selected) &&
              (_to   <= _tc.selected) )
    {
        ++_tc.selected;
    }

    if (_tc.on_reorder)
    {
        _tc.on_reorder(_from, _to);
    }

    return true;
}

// -- rename ---------------------------------------------------------------

// tc_begin_rename
//   function: enters rename mode for the tab at the given index.
template <unsigned _TF, unsigned _CF, typename _I>
bool tc_begin_rename(tab_control<_TF, _CF, _I>& _tc,
                     std::size_t                _index)
{
    static_assert(has_tf(_TF, tf_renamable),
                  "requires tf_renamable on tab_feat");

    if (_index >= _tc.tabs.size())
    {
        return false;
    }

    if constexpr (has_tf(_TF, tf_renamable))
    {
        if (!_tc.tabs[_index].renamable)
        {
            return false;
        }
    }

    _tc.renaming     = true;
    _tc.rename_index = _index;
    _tc.rename_buffer = _tc.tabs[_index].label;

    return true;
}

// tc_commit_rename
//   function: commits the rename buffer to the tab label.
// Invokes on_rename if set; returns false if rejected.
template <unsigned _TF, unsigned _CF, typename _I>
bool tc_commit_rename(tab_control<_TF, _CF, _I>& _tc)
{
    if (!_tc.renaming)
    {
        return false;
    }

    if ( (_tc.on_rename) &&
         (!_tc.on_rename(_tc.rename_index, _tc.rename_buffer)) )
    {
        _tc.renaming = false;

        return false;
    }

    if (_tc.rename_index < _tc.tabs.size())
    {
        _tc.tabs[_tc.rename_index].label = _tc.rename_buffer;
    }

    _tc.renaming = false;

    return true;
}

// tc_cancel_rename
//   function: cancels rename mode without committing.
template <unsigned _TF, unsigned _CF, typename _I>
void tc_cancel_rename(tab_control<_TF, _CF, _I>& _tc)
{
    _tc.renaming = false;
    _tc.rename_buffer.clear();

    return;
}

// -- scroll (tcf_scrollable) ----------------------------------------------

// tc_scroll_to_tab
//   function: requests that the renderer scroll to make the
// tab at the given index fully visible.
template <unsigned _TF, unsigned _CF, typename _I>
void tc_scroll_to_selected(tab_control<_TF, _CF, _I>& _tc)
{
    static_assert(has_tcf(_CF, tcf_scrollable),
                  "requires tcf_scrollable");

    // the scroll_offset is set by the renderer after layout.
    // this function is a hint — the renderer reads it and
    // adjusts scroll_offset to bring _tc.selected into view.
    // We mark scroll_offset as requiring recalculation by
    // clamping to -1 (sentinel).
    _tc.scroll_offset = -1.0f;

    return;
}

// -- visible tab count ----------------------------------------------------

// tc_visible_count
//   function: returns the number of visible tabs.
template <unsigned _TF, unsigned _CF, typename _I>
std::size_t
tc_visible_count(const tab_control<_TF, _CF, _I>& _tc)
{
    std::size_t count = 0;

    for (const auto& tab : _tc.tabs)
    {
        if (tab.visible)
        {
            ++count;
        }
    }

    return count;
}

// tc_pinned_count
//   function: returns the number of pinned tabs.
template <unsigned _TF, unsigned _CF, typename _I>
std::size_t
tc_pinned_count(const tab_control<_TF, _CF, _I>& _tc)
{
    static_assert(has_tf(_TF, tf_pinnable),
                  "requires tf_pinnable");

    std::size_t count = 0;

    for (const auto& tab : _tc.tabs)
    {
        if (tab.pinned)
        {
            ++count;
        }
    }

    return count;
}

// ===============================================================================
//  10.  TRAITS
// ===============================================================================

NS_INTERNAL
    // -- tab entry detectors ----------------------------------------------
    template <typename, typename = void>
    struct has_user_id_member : std::false_type {};

    template <typename _Type>
    struct has_user_id_member<_Type, std::void_t<
        decltype(std::declval<_Type>().user_id)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_pinned_member : std::false_type {};

    template <typename _Type>
    struct has_pinned_member<_Type, std::void_t<
        decltype(std::declval<_Type>().pinned)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_close_policy_member : std::false_type {};

    template <typename _Type>
    struct has_close_policy_member<_Type, std::void_t<
        decltype(std::declval<_Type>().close_policy)
    >> : std::true_type {};

    // -- tab control detectors --------------------------------------------

    template <typename, typename = void>
    struct has_tabs_member : std::false_type {};

    template <typename _Type>
    struct has_tabs_member<_Type, std::void_t<
        decltype(std::declval<_Type>().tabs)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_selected_member : std::false_type {};

    template <typename _Type>
    struct has_selected_member<_Type, std::void_t<
        decltype(std::declval<_Type>().selected)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_placement_member : std::false_type {};

    template <typename _Type>
    struct has_placement_member<_Type, std::void_t<
        decltype(std::declval<_Type>().placement)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_scroll_offset_member : std::false_type {};

    template <typename _Type>
    struct has_scroll_offset_member<_Type, std::void_t<
        decltype(std::declval<_Type>().scroll_offset)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_drag_member : std::false_type {};

    template <typename _Type>
    struct has_drag_member<_Type, std::void_t<
        decltype(std::declval<_Type>().drag)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_add_enabled_member : std::false_type {};

    template <typename _Type>
    struct has_add_enabled_member<_Type, std::void_t<
        decltype(std::declval<_Type>().add_enabled)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_overflow_indices_member : std::false_type {};

    template <typename _Type>
    struct has_overflow_indices_member<_Type, std::void_t<
        decltype(std::declval<_Type>().overflow_indices)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_max_rows_member : std::false_type {};

    template <typename _Type>
    struct has_max_rows_member<_Type, std::void_t<
        decltype(std::declval<_Type>().max_rows)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_on_select_member : std::false_type
    {};

    template <typename _Type>
    struct has_on_select_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_select)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_on_close_member : std::false_type
    {};

    template <typename _Type>
    struct has_on_close_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_close)
    >> : std::true_type {};

NS_END  // internal

// -- tab entry value aliases ----------------------------------------------
template <typename _Type>
inline constexpr bool has_user_id_v =
    detail::has_user_id_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_pinned_v =
    detail::has_pinned_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_close_policy_v =
    detail::has_close_policy_member<_Type>::value;

// -- tab control value aliases --------------------------------------------
template <typename _Type>
inline constexpr bool has_tabs_v =
    detail::has_tabs_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_selected_v =
    detail::has_selected_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_placement_v =
    detail::has_placement_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_scroll_offset_v =
    detail::has_scroll_offset_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_drag_v =
    detail::has_drag_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_overflow_indices_v =
    detail::has_overflow_indices_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_max_rows_v =
    detail::has_max_rows_member<_Type>::value;

// -- shared aliases (delegate to component_traits) ------------------------
template <typename _Type>
inline constexpr bool has_enabled_v =
    component_traits::has_enabled_v<_Type>;
template <typename _Type>
inline constexpr bool has_visible_v =
    component_traits::has_visible_v<_Type>;
template <typename _Type>
inline constexpr bool has_label_v =
    component_traits::has_label_v<_Type>;

// -- composite traits -----------------------------------------------------

// is_tab_entry
//   trait: has label + enabled + visible + user_id.
template <typename _Type>
struct is_tab_entry : std::conjunction<
    component_traits::detail::has_label_member<_Type>,
    component_traits::detail::has_enabled_member<_Type>,
    component_traits::detail::has_visible_member<_Type>,
    detail::has_user_id_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_tab_entry_v = is_tab_entry<_Type>::value;

// is_tab_control
//   trait: has tabs + selected + placement + enabled + visible +
// focusable.
template <typename _Type>
struct is_tab_control : std::conjunction<
    detail::has_tabs_member<_Type>,
    detail::has_selected_member<_Type>,
    detail::has_placement_member<_Type>,
    component_traits::detail::has_enabled_member<_Type>,
    component_traits::detail::has_visible_member<_Type>,
    component_traits::detail::has_focusable_flag<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_tab_control_v =
    is_tab_control<_Type>::value;

// is_closable_tab_control
template <typename _Type>
struct is_closable_tab_control : std::conjunction<
    is_tab_control<_Type>,
    detail::has_on_close_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_closable_tab_control_v =
    is_closable_tab_control<_Type>::value;

// is_scrollable_tab_control
template <typename _Type>
struct is_scrollable_tab_control : std::conjunction<
    is_tab_control<_Type>,
    detail::has_scroll_offset_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_scrollable_tab_control_v =
    is_scrollable_tab_control<_Type>::value;

// is_reorderable_tab_control
template <typename _Type>
struct is_reorderable_tab_control : std::conjunction<
    is_tab_control<_Type>,
    detail::has_drag_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_reorderable_tab_control_v =
    is_reorderable_tab_control<_Type>::value;

// is_multirow_tab_control
template <typename _Type>
struct is_multirow_tab_control : std::conjunction<
    is_tab_control<_Type>,
    detail::has_max_rows_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_multirow_tab_control_v =
    is_multirow_tab_control<_Type>::value;

// is_overflow_tab_control
template <typename _Type>
struct is_overflow_tab_control : std::conjunction<
    is_tab_control<_Type>,
    detail::has_overflow_indices_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_overflow_tab_control_v =
    is_overflow_tab_control<_Type>::value;


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_TAB_CONTROL_