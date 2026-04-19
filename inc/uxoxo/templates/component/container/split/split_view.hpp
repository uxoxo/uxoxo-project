/*******************************************************************************
* uxoxo [component]                                               split_view.hpp
*
* Split view component:
*   A framework-agnostic, pure-data split-view template.  Divides available
* space among an ordered collection of N panes, with N-1 splitters between
* them.  Unlike the two-way `split_view` in the ui layer, this template
* supports arbitrary N, per-pane size constraints, weighted proportional
* redistribution, programmatic and interactive splitter manipulation,
* snap points, collapse/expand, transition animation, and keyboard
* navigation — all as opt-in feature flags with zero storage cost when
* disabled.
*
*   The component is split into two template levels, matching tab_control
* and stacked_view:
*
*     split_pane<_PaneFeat>
*       A single pane slot descriptor.  Core state is the pane's current
*       size along the main axis, its min/max constraints, its resize
*       weight, and a user_id for content routing.  Optional features
*       add string id, title, collapse state, and user tag.
*
*     split_view<_PaneFeat, _CtrlFeat>
*       The split container.  Owns a vector of panes, orientation, the
*       splitter thickness, the global resizable flag, and the shared
*       callbacks.  Opts in to interaction drag state, per-splitter
*       state overrides, snap points, animation state, double-click
*       policy, and keyboard navigation via _CtrlFeat.
*
*   The template prescribes NOTHING about rendering.  Pane sizes are
* pixel values along the main axis; the integrating renderer lays out
* accordingly.  The component carries enough state to support an
* interactive splitter-drag gesture without any help from the renderer
* — the renderer's sole responsibility is to wire pointer deltas into
* spl_begin_drag / spl_update_drag / spl_end_drag.
*
*   Feature composition follows the same EBO-mixin bitfield pattern used
* throughout the uxoxo component layer.  Disabled features cost zero
* bytes thanks to empty-base-optimization on empty `_Enable = false`
* specializations.
*
* Contents:
*   1.   Pane feature flags (pane_feat)
*   2.   View feature flags (split_feat)
*   3.   Enums (split_orientation, split_resize_policy, split_drag_mode,
*         splitter_double_click)
*   4.   Per-splitter state struct
*   5.   Pane EBO mixins (namespace pane_mixin)
*   6.   split_pane struct
*   7.   View EBO mixins (namespace split_view_mixin)
*   8.   split_view struct
*   9.   Pane free functions (pn_*)
*   10.  View free functions (spl_*)
*   11.  Traits (namespace split_traits)
*
*
* path:      /inc/uxoxo/templates/component/split/split_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.17
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_SPLIT_VIEW_
#define  UXOXO_COMPONENT_SPLIT_VIEW_ 1

// std
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
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


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  PANE FEATURE FLAGS
// ===============================================================================
//   Per-pane features.  These control what optional data each
// split_pane carries.  Combine with bitwise OR:
//
//   split_pane<pf_id | pf_title | pf_collapsible>

enum pane_feat : unsigned
{
    pf_none         = 0,
    pf_id           = 1u << 0,     // per-pane string id for named lookup
    pf_title        = 1u << 1,     // per-pane title (splitter label, a11y)
    pf_collapsible  = 1u << 2,     // collapsed flag + saved_size for restore
    pf_user_tag     = 1u << 3,     // per-pane arbitrary user string tag

    pf_standard     = pf_title | pf_collapsible,
    pf_all          = pf_id        | pf_title
                    | pf_collapsible | pf_user_tag
};

constexpr unsigned
operator|(pane_feat _a,
          pane_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool
has_pf(unsigned  _f,
       pane_feat _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


// ===============================================================================
//  2.  VIEW FEATURE FLAGS
// ===============================================================================
//   Control-level features.  These govern what optional data and
// behaviour the split_view itself carries.

enum split_feat : unsigned
{
    sf_none             = 0,
    sf_drag_state       = 1u << 0,     // hover / active splitter + drag offset
    sf_resize_policy    = 1u << 1,     // policy enum for container resize
    sf_snap_points      = 1u << 2,     // per-splitter snap positions
    sf_splitter_state   = 1u << 3,     // per-splitter lock/hide/thickness
    sf_animated         = 1u << 4,     // animated size transitions
    sf_double_click     = 1u << 5,     // double-click policy on splitter
    sf_keyboard_nav     = 1u << 6,     // focused splitter + step

    sf_standard         = sf_drag_state  | sf_resize_policy,
    sf_interactive      = sf_drag_state  | sf_snap_points | sf_double_click
                        | sf_keyboard_nav,
    sf_all              = sf_drag_state    | sf_resize_policy
                        | sf_snap_points   | sf_splitter_state
                        | sf_animated      | sf_double_click
                        | sf_keyboard_nav
};

constexpr unsigned
operator|(split_feat _a,
          split_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool
has_sf(unsigned   _f,
       split_feat _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


// ===============================================================================
//  3.  ENUMS
// ===============================================================================

// split_orientation
//   enum: which axis the panes are laid out along.  `horizontal` stacks
// panes left-to-right (splitters are vertical lines); `vertical` stacks
// panes top-to-bottom (splitters are horizontal lines).
enum class split_orientation : std::uint8_t
{
    horizontal,
    vertical
};

// split_resize_policy
//   enum: how pane sizes are redistributed when the container's total
// size changes.  Used by spl_resize under the sf_resize_policy feature.
enum class split_resize_policy : std::uint8_t
{
    proportional,       // scale each pane by (new_total / old_total)
    weighted,           // distribute delta proportionally to pane weights
    stretch_first,      // first pane absorbs the whole delta
    stretch_last,       // last pane absorbs the whole delta
    rigid               // no auto-redistribute; caller sets sizes directly
};

// split_drag_mode
//   enum: interactive splitter drag lifecycle.  Used by the sf_drag_state
// mixin.
enum class split_drag_mode : std::uint8_t
{
    idle,
    hovering,           // cursor over a splitter handle
    dragging            // actively dragging a splitter
};

// splitter_double_click
//   enum: action taken when a splitter is double-clicked.  Used by the
// sf_double_click mixin.  `none` means no special behaviour; others
// operate relative to the splitter that was double-clicked (first =
// the pane before the splitter, second = the pane after).
enum class splitter_double_click : std::uint8_t
{
    none,
    collapse_first,
    collapse_second,
    toggle_collapse_first,
    toggle_collapse_second,
    reset_default,          // redistribute to stored default sizes
    distribute_evenly       // set all panes to equal size
};


// ===============================================================================
//  4.  PER-SPLITTER STATE STRUCT
// ===============================================================================
//   Optional per-splitter overrides.  One entry per splitter (there are
// pane_count - 1 splitters in a split_view with pane_count panes).
// Only present when the sf_splitter_state feature is enabled.

// split_splitter
//   struct: per-splitter interaction overrides.
struct split_splitter
{
    bool  locked             = false;   // cannot be dragged
    bool  visible            = true;    // splitter line is rendered
    float thickness_override = 0.0f;    // 0 = use view's splitter_thickness
};


// ===============================================================================
//  5.  PANE EBO MIXINS
// ===============================================================================
//   Each mixin is a struct template parameterised on a bool.  The
// `false` specialisation is empty; the `true` specialisation holds
// per-pane data.  EBO guarantees zero storage overhead when disabled.

namespace pane_mixin {

    // -- id ---------------------------------------------------------------
    template <bool _Enable>
    struct id_data
    {};

    template <>
    struct id_data<true>
    {
        std::string id;
    };

    // -- title ------------------------------------------------------------
    template <bool _Enable>
    struct title_data
    {};

    template <>
    struct title_data<true>
    {
        std::string title;
    };

    // -- collapsible ------------------------------------------------------
    //   When collapsed, the pane's `size` is driven to 0 regardless of
    // `min_size`.  `saved_size` records the pre-collapse size so
    // pn_expand can restore it.
    template <bool _Enable>
    struct collapse_data
    {};

    template <>
    struct collapse_data<true>
    {
        bool  collapsed  = false;
        float saved_size = 0.0f;
    };

    // -- user tag ---------------------------------------------------------
    //   Arbitrary free-form user string.  Distinct from pf_id (which
    // must be unique for indexed lookup patterns) and from user_id
    // (opaque size_t for content routing).
    template <bool _Enable>
    struct user_tag_data
    {};

    template <>
    struct user_tag_data<true>
    {
        std::string user_tag;
    };

}   // namespace pane_mixin


// ===============================================================================
//  6.  SPLIT PANE
// ===============================================================================
//   A single pane slot.  The actual pane content lives elsewhere —
// routed via user_id by the integrating layer.  The pane itself is
// pure sizing/constraint state.
//
//   Core state covers the fields that essentially every split_view
// implementation needs: current size, min/max constraints, weight
// for proportional redistribution, user_id for content routing, and
// the universal enabled/visible flags.  Anything further (titles,
// ids, collapse state, user tags) is feature-gated.

template <unsigned _Feat = pf_none>
struct split_pane
    : pane_mixin::id_data        <has_pf(_Feat, pf_id)>
    , pane_mixin::title_data     <has_pf(_Feat, pf_title)>
    , pane_mixin::collapse_data  <has_pf(_Feat, pf_collapsible)>
    , pane_mixin::user_tag_data  <has_pf(_Feat, pf_user_tag)>
{
    using self_type = split_pane<_Feat>;

    static constexpr unsigned features = _Feat;

    // compile-time feature queries
    static constexpr bool has_id          = has_pf(_Feat, pf_id);
    static constexpr bool has_title       = has_pf(_Feat, pf_title);
    static constexpr bool has_collapsible = has_pf(_Feat, pf_collapsible);
    static constexpr bool has_user_tag    = has_pf(_Feat, pf_user_tag);

    // -- sizing (core) ----------------------------------------------------
    //   size is the current pixel extent of this pane along the view's
    // main axis.  min_size and max_size are constraints; max_size == 0
    // means unbounded above.  weight drives proportional distribution
    // when split_resize_policy::weighted is in effect and defaults to
    // 1.0 (equal share).
    float       size     = 0.0f;
    float       min_size = 0.0f;
    float       max_size = 0.0f;        // 0 = unbounded
    float       weight   = 1.0f;

    // -- state (core) -----------------------------------------------------
    bool        enabled  = true;        // splitters on this pane's edges
                                        // honour drag; false freezes them
    bool        visible  = true;        // pane participates in layout

    // -- user data --------------------------------------------------------
    //   Opaque integer identifier for associating panes with content in
    // the integrating layer.  The framework does not interpret it.
    std::size_t user_id  = 0;

    // -- construction -----------------------------------------------------
    split_pane() = default;

    explicit split_pane(
            std::size_t _uid
        ) noexcept
            : user_id(_uid)
        {}

    split_pane(
            std::size_t _uid,
            float       _size
        ) noexcept
            : size(_size),
              user_id(_uid)
        {}
};


// ===============================================================================
//  7.  VIEW EBO MIXINS
// ===============================================================================

namespace split_view_mixin {

    // -- drag state -------------------------------------------------------
    //   Interactive state for splitter drag.  The renderer sets
    // `hovered` on mouse-over and drives the active-splitter state
    // via spl_begin_drag / spl_update_drag / spl_end_drag.  SIZE_MAX
    // sentinel values indicate "no splitter".
    template <bool _Enable>
    struct drag_data
    {};

    template <>
    struct drag_data<true>
    {
        split_drag_mode mode             = split_drag_mode::idle;
        std::size_t     hovered_splitter = static_cast<std::size_t>(-1);
        std::size_t     active_splitter  = static_cast<std::size_t>(-1);
        float           drag_start_pos   = 0.0f;      // pointer at drag begin
        float           drag_accumulator = 0.0f;      // sub-pixel carry
        std::vector<float> sizes_at_drag_begin;       // for cancel/revert
    };

    // -- resize policy ----------------------------------------------------
    template <bool _Enable>
    struct policy_data
    {};

    template <>
    struct policy_data<true>
    {
        split_resize_policy policy =
            split_resize_policy::proportional;
    };

    // -- snap points ------------------------------------------------------
    //   Per-splitter list of positions (in pixels from the main-axis
    // origin) that dragged splitters should snap to when within
    // `snap_tolerance` pixels.  The outer vector is indexed by
    // splitter; the inner vector holds snap positions for that
    // splitter.  An empty inner vector means no snap points on that
    // splitter.
    template <bool _Enable>
    struct snap_data
    {};

    template <>
    struct snap_data<true>
    {
        std::vector<std::vector<float>> snap_points;
        float                           snap_tolerance = 8.0f;
    };

    // -- per-splitter state -----------------------------------------------
    //   Individual lock / hide / thickness overrides per splitter.
    // Callers keep this vector sized to pane_count - 1; the view free
    // functions do not auto-resize it.  Missing entries (vector is
    // smaller than splitter count) behave as defaults.
    template <bool _Enable>
    struct splitter_state_data
    {};

    template <>
    struct splitter_state_data<true>
    {
        std::vector<split_splitter> splitters;
    };

    // -- animation --------------------------------------------------------
    //   Smooth interpolation from `start_sizes` to `target_sizes` over
    // `anim_duration` seconds.  The renderer advances `anim_progress`
    // each frame; spl_animation_tick is a helper.
    template <bool _Enable>
    struct anim_data
    {};

    template <>
    struct anim_data<true>
    {
        std::vector<float> start_sizes;
        std::vector<float> target_sizes;
        float              anim_progress = 0.0f;    // 0 → 1
        float              anim_duration = 0.15f;   // seconds
        bool               anim_active   = false;
    };

    // -- double-click policy ----------------------------------------------
    template <bool _Enable>
    struct double_click_data
    {};

    template <>
    struct double_click_data<true>
    {
        splitter_double_click double_click =
            splitter_double_click::none;

        std::vector<float> default_sizes;      // for reset_default
    };

    // -- keyboard navigation ----------------------------------------------
    template <bool _Enable>
    struct keyboard_data
    {};

    template <>
    struct keyboard_data<true>
    {
        std::size_t focused_splitter = 0;
        float       keyboard_step    = 10.0f;     // pixels per arrow press
    };

}   // namespace split_view_mixin


// ===============================================================================
//  8.  SPLIT VIEW
// ===============================================================================
//   _PaneFeat   bitwise OR of pane_feat flags for per-pane features.
//   _CtrlFeat   bitwise OR of split_feat flags for control features.

template <unsigned _PaneFeat = pf_none,
          unsigned _CtrlFeat = sf_none>
struct split_view
    : split_view_mixin::drag_data             <has_sf(_CtrlFeat, sf_drag_state)>
    , split_view_mixin::policy_data           <has_sf(_CtrlFeat, sf_resize_policy)>
    , split_view_mixin::snap_data             <has_sf(_CtrlFeat, sf_snap_points)>
    , split_view_mixin::splitter_state_data   <has_sf(_CtrlFeat, sf_splitter_state)>
    , split_view_mixin::anim_data             <has_sf(_CtrlFeat, sf_animated)>
    , split_view_mixin::double_click_data     <has_sf(_CtrlFeat, sf_double_click)>
    , split_view_mixin::keyboard_data         <has_sf(_CtrlFeat, sf_keyboard_nav)>
{
    using pane_type = split_pane<_PaneFeat>;
    using size_type = std::size_t;

    static constexpr unsigned pane_features = _PaneFeat;
    static constexpr unsigned features      = _CtrlFeat;

    // compile-time feature queries (pane level)
    static constexpr bool panes_have_id          = has_pf(_PaneFeat, pf_id);
    static constexpr bool panes_have_title       = has_pf(_PaneFeat, pf_title);
    static constexpr bool panes_have_collapse    = has_pf(_PaneFeat, pf_collapsible);
    static constexpr bool panes_have_tag         = has_pf(_PaneFeat, pf_user_tag);

    // compile-time feature queries (control level)
    static constexpr bool has_drag_state         = has_sf(_CtrlFeat, sf_drag_state);
    static constexpr bool has_resize_policy      = has_sf(_CtrlFeat, sf_resize_policy);
    static constexpr bool has_snap               = has_sf(_CtrlFeat, sf_snap_points);
    static constexpr bool has_splitter_state     = has_sf(_CtrlFeat, sf_splitter_state);
    static constexpr bool is_animated            = has_sf(_CtrlFeat, sf_animated);
    static constexpr bool has_double_click       = has_sf(_CtrlFeat, sf_double_click);
    static constexpr bool has_keyboard           = has_sf(_CtrlFeat, sf_keyboard_nav);

    // component identity
    //   The view itself accepts keyboard focus only when it has
    // keyboard-navigable splitters; otherwise interaction is mouse-
    // only and the view is not focusable.
    static constexpr bool focusable =
        has_sf(_CtrlFeat, sf_keyboard_nav);

    // -- panes ------------------------------------------------------------
    std::vector<pane_type> panes;

    // -- layout (core) ----------------------------------------------------
    split_orientation orientation       = split_orientation::horizontal;
    float             splitter_thickness = 4.0f;   // pixels
    bool              resizable          = true;   // splitters accept drag

    // -- state ------------------------------------------------------------
    bool enabled = true;
    bool visible = true;

    // -- callbacks --------------------------------------------------------
    using splitter_moved_fn =
        std::function<void(size_type splitter_index)>;
    using pane_resized_fn =
        std::function<void(size_type pane_index, float new_size)>;
    using pane_collapsed_fn =
        std::function<void(size_type pane_index, bool collapsed)>;

    splitter_moved_fn on_splitter_moved; // fired after drag end or programmatic move
    pane_resized_fn   on_pane_resized;   // fired per-pane on size change
    pane_collapsed_fn on_pane_collapsed; // fired on collapse/expand

    // -- construction -----------------------------------------------------
    split_view() = default;

    explicit split_view(
            split_orientation _orientation
        ) noexcept
            : orientation(_orientation)
        {}

    // -- queries ----------------------------------------------------------
    [[nodiscard]] bool
    empty() const noexcept
    {
        return panes.empty();
    }

    [[nodiscard]] size_type
    count() const noexcept
    {
        return panes.size();
    }

    [[nodiscard]] size_type
    splitter_count() const noexcept
    {
        return panes.empty() ? 0 : panes.size() - 1;
    }

    [[nodiscard]] bool
    valid_index(size_type _idx) const noexcept
    {
        return _idx < panes.size();
    }

    [[nodiscard]] bool
    valid_splitter(size_type _idx) const noexcept
    {
        return (_idx + 1) < panes.size();
    }

    // -- compositional forwarding ----------------------------------------
    //   Enables integration with component_common.hpp's for_each_sub,
    // enable_all, and disable_all.
    template <typename _Fn>
    void
    visit_components(_Fn&& _fn)
    {
        for (auto& pane : panes)
        {
            _fn(pane);
        }

        return;
    }
};


// ===============================================================================
//  9.  PANE FREE FUNCTIONS
// ===============================================================================
//   Per-pane operations.  Shared operations (enable, disable) work on
// split_pane directly via the ADL functions in component_common.hpp.

// pn_set_size
//   function: sets the pane's size, clamped to min_size / max_size.
// Returns the actual size after clamping.
template <unsigned _F>
float
pn_set_size(split_pane<_F>& _pane,
            float           _size)
{
    if (_size < _pane.min_size)
    {
        _size = _pane.min_size;
    }
    if ( (_pane.max_size > 0.0f) &&
         (_size > _pane.max_size) )
    {
        _size = _pane.max_size;
    }

    _pane.size = _size;

    return _size;
}

// pn_clamp_size
//   function: returns `_size` clamped into [min_size, max_size].
// Does not mutate the pane.
template <unsigned _F>
[[nodiscard]] float
pn_clamp_size(const split_pane<_F>& _pane,
              float                 _size) noexcept
{
    if (_size < _pane.min_size)
    {
        _size = _pane.min_size;
    }
    if ( (_pane.max_size > 0.0f) &&
         (_size > _pane.max_size) )
    {
        _size = _pane.max_size;
    }

    return _size;
}

// pn_set_constraints
template <unsigned _F>
void
pn_set_constraints(split_pane<_F>& _pane,
                   float           _min,
                   float           _max)
{
    _pane.min_size = _min;
    _pane.max_size = _max;

    // re-clamp current size into new range
    pn_set_size(_pane, _pane.size);

    return;
}

// pn_set_weight
template <unsigned _F>
void
pn_set_weight(split_pane<_F>& _pane,
              float           _weight)
{
    _pane.weight = _weight;

    return;
}

// pn_set_id
template <unsigned _F>
void
pn_set_id(split_pane<_F>& _pane,
          std::string     _id)
{
    static_assert(has_pf(_F, pf_id),
                  "requires pf_id");

    _pane.id = std::move(_id);

    return;
}

// pn_set_title
template <unsigned _F>
void
pn_set_title(split_pane<_F>& _pane,
             std::string     _title)
{
    static_assert(has_pf(_F, pf_title),
                  "requires pf_title");

    _pane.title = std::move(_title);

    return;
}

// pn_collapse
//   function: collapses the pane to size 0, saving its current size
// for later restore.  Returns false if the pane is already collapsed.
template <unsigned _F>
bool
pn_collapse(split_pane<_F>& _pane)
{
    static_assert(has_pf(_F, pf_collapsible),
                  "requires pf_collapsible");

    if (_pane.collapsed)
    {
        return false;
    }

    _pane.saved_size = _pane.size;
    _pane.collapsed  = true;
    _pane.size       = 0.0f;

    return true;
}

// pn_expand
//   function: restores a collapsed pane to its saved size.  Returns
// false if the pane was not collapsed.
template <unsigned _F>
bool
pn_expand(split_pane<_F>& _pane)
{
    static_assert(has_pf(_F, pf_collapsible),
                  "requires pf_collapsible");

    if (!_pane.collapsed)
    {
        return false;
    }

    _pane.size       = pn_clamp_size(_pane, _pane.saved_size);
    _pane.collapsed  = false;
    _pane.saved_size = 0.0f;

    return true;
}

// pn_toggle_collapse
template <unsigned _F>
bool
pn_toggle_collapse(split_pane<_F>& _pane)
{
    static_assert(has_pf(_F, pf_collapsible),
                  "requires pf_collapsible");

    if (_pane.collapsed)
    {
        return pn_expand(_pane);
    }

    return pn_collapse(_pane);
}

// pn_is_collapsed
template <unsigned _F>
[[nodiscard]] bool
pn_is_collapsed(const split_pane<_F>& _pane) noexcept
{
    static_assert(has_pf(_F, pf_collapsible),
                  "requires pf_collapsible");

    return _pane.collapsed;
}


// ===============================================================================
//  10.  VIEW FREE FUNCTIONS
// ===============================================================================

// -- pane management ------------------------------------------------------

/*
spl_add_pane
  Appends a new pane to the view and returns a reference to it.  The
new pane starts with size 0; call spl_distribute_evenly, spl_set_sizes,
or pn_set_size to give it extent.

Parameter(s):
  _sv: the split_view to append into.
Return:
  A reference to the newly-added pane.
*/
template <unsigned _PF, unsigned _CF>
split_pane<_PF>&
spl_add_pane(split_view<_PF, _CF>& _sv)
{
    _sv.panes.emplace_back();

    return _sv.panes.back();
}

// spl_add_pane (with user_id)
template <unsigned _PF, unsigned _CF>
split_pane<_PF>&
spl_add_pane(split_view<_PF, _CF>& _sv,
             std::size_t           _user_id)
{
    _sv.panes.emplace_back(_user_id);

    return _sv.panes.back();
}

// spl_add_pane (with user_id and initial size)
template <unsigned _PF, unsigned _CF>
split_pane<_PF>&
spl_add_pane(split_view<_PF, _CF>& _sv,
             std::size_t           _user_id,
             float                 _size)
{
    _sv.panes.emplace_back(_user_id, _size);

    return _sv.panes.back();
}

/*
spl_insert_pane
  Inserts a new pane at the given index.  Clamps out-of-range indices
to the end.  When the sf_splitter_state feature is enabled, a default
split_splitter entry is inserted into the splitters vector to keep it
in sync.

Parameter(s):
  _sv:    the split_view to insert into.
  _index: the target index.
Return:
  A reference to the newly-inserted pane.
*/
template <unsigned _PF, unsigned _CF>
split_pane<_PF>&
spl_insert_pane(split_view<_PF, _CF>& _sv,
                std::size_t           _index)
{
    if (_index > _sv.panes.size())
    {
        _index = _sv.panes.size();
    }

    auto it = _sv.panes.emplace(
        _sv.panes.begin() + static_cast<std::ptrdiff_t>(_index));

    // keep per-splitter state vector in sync if present
    if constexpr (has_sf(_CF, sf_splitter_state))
    {
        // splitter count is panes.size() - 1; we just added one pane, so
        // the splitter count grew by one (except when going from 0 → 1
        // panes, which produces 0 splitters still).
        if (_sv.panes.size() >= 2)
        {
            std::size_t splitter_idx = (_index == 0) ? 0 : (_index - 1);
            if (splitter_idx > _sv.splitters.size())
            {
                splitter_idx = _sv.splitters.size();
            }
            _sv.splitters.emplace(
                _sv.splitters.begin()
                    + static_cast<std::ptrdiff_t>(splitter_idx));
        }
    }

    return *it;
}

/*
spl_remove_pane
  Removes the pane at the given index.  When the sf_splitter_state
feature is enabled, the adjacent splitter_state entry is also
removed.  Out-of-range indices return false without mutation.

Parameter(s):
  _sv:    the split_view to remove from.
  _index: the index of the pane to remove.
Return:
  true if a pane was removed, false if the index was out of range.
*/
template <unsigned _PF, unsigned _CF>
bool
spl_remove_pane(split_view<_PF, _CF>& _sv,
                std::size_t           _index)
{
    if (_index >= _sv.panes.size())
    {
        return false;
    }

    _sv.panes.erase(
        _sv.panes.begin() + static_cast<std::ptrdiff_t>(_index));

    // keep per-splitter state vector in sync
    if constexpr (has_sf(_CF, sf_splitter_state))
    {
        if (!_sv.splitters.empty())
        {
            std::size_t splitter_idx =
                (_index == 0) ? 0 : (_index - 1);
            if (splitter_idx < _sv.splitters.size())
            {
                _sv.splitters.erase(
                    _sv.splitters.begin()
                        + static_cast<std::ptrdiff_t>(splitter_idx));
            }
        }
    }

    return true;
}

// spl_remove_all
//   function: clears all panes and associated state.
template <unsigned _PF, unsigned _CF>
void
spl_remove_all(split_view<_PF, _CF>& _sv)
{
    _sv.panes.clear();

    if constexpr (has_sf(_CF, sf_splitter_state))
    {
        _sv.splitters.clear();
    }
    if constexpr (has_sf(_CF, sf_snap_points))
    {
        _sv.snap_points.clear();
    }

    return;
}


// -- sizing queries -------------------------------------------------------

// spl_total_pane_size
//   function: returns the sum of pane sizes (excluding splitters).
template <unsigned _PF, unsigned _CF>
[[nodiscard]] float
spl_total_pane_size(const split_view<_PF, _CF>& _sv) noexcept
{
    float total;

    total = 0.0f;

    for (const auto& pane : _sv.panes)
    {
        total += pane.size;
    }

    return total;
}

// spl_total_splitter_size
//   function: returns the total pixel extent consumed by splitters.
template <unsigned _PF, unsigned _CF>
[[nodiscard]] float
spl_total_splitter_size(const split_view<_PF, _CF>& _sv) noexcept
{
    std::size_t n;

    n = _sv.panes.size();
    if (n < 2)
    {
        return 0.0f;
    }

    return static_cast<float>(n - 1) * _sv.splitter_thickness;
}

// spl_total_size
//   function: returns the total main-axis extent of the view —
// pane sizes plus splitter thicknesses.
template <unsigned _PF, unsigned _CF>
[[nodiscard]] float
spl_total_size(const split_view<_PF, _CF>& _sv) noexcept
{
    return spl_total_pane_size(_sv)
         + spl_total_splitter_size(_sv);
}

// spl_min_total_pane_size
template <unsigned _PF, unsigned _CF>
[[nodiscard]] float
spl_min_total_pane_size(const split_view<_PF, _CF>& _sv) noexcept
{
    float total;

    total = 0.0f;

    for (const auto& pane : _sv.panes)
    {
        total += pane.min_size;
    }

    return total;
}

// spl_splitter_position
//   function: returns the pixel position (from the main-axis origin)
// of the given splitter's leading edge.  Returns NaN if the splitter
// index is out of range.
template <unsigned _PF, unsigned _CF>
[[nodiscard]] float
spl_splitter_position(const split_view<_PF, _CF>& _sv,
                      std::size_t                 _splitter_idx) noexcept
{
    float       pos;
    std::size_t i;

    if (_splitter_idx + 1 >= _sv.panes.size())
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    pos = 0.0f;

    for (i = 0; i <= _splitter_idx; ++i)
    {
        pos += _sv.panes[i].size;
        if (i < _splitter_idx)
        {
            pos += _sv.splitter_thickness;
        }
    }

    return pos;
}


// -- size mutation --------------------------------------------------------

/*
spl_set_sizes
  Sets every pane's size from the given vector.  Any trailing panes
beyond the vector length keep their current size; any trailing vector
entries beyond the pane count are ignored.  Sizes are clamped into
each pane's [min_size, max_size] range.

Parameter(s):
  _sv:    the split_view.
  _sizes: the pane sizes to apply.
Return:
  none.
*/
template <unsigned _PF, unsigned _CF>
void
spl_set_sizes(split_view<_PF, _CF>&     _sv,
              const std::vector<float>& _sizes)
{
    std::size_t n;
    std::size_t i;

    n = std::min(_sv.panes.size(), _sizes.size());

    for (i = 0; i < n; ++i)
    {
        pn_set_size(_sv.panes[i], _sizes[i]);
    }

    return;
}

// spl_get_sizes
//   function: returns a vector of current pane sizes.
template <unsigned _PF, unsigned _CF>
[[nodiscard]] std::vector<float>
spl_get_sizes(const split_view<_PF, _CF>& _sv)
{
    std::vector<float> sizes;

    sizes.reserve(_sv.panes.size());

    for (const auto& pane : _sv.panes)
    {
        sizes.push_back(pane.size);
    }

    return sizes;
}

/*
spl_distribute_evenly
  Sets all panes to equal size such that the sum of pane sizes plus
splitter thicknesses equals `_total_size`.  Sizes are clamped to each
pane's min/max constraints, which may produce a final total less than
requested if the constraints cannot be satisfied.

Parameter(s):
  _sv:         the split_view.
  _total_size: total main-axis extent to fit into.
Return:
  none.
*/
template <unsigned _PF, unsigned _CF>
void
spl_distribute_evenly(split_view<_PF, _CF>& _sv,
                      float                 _total_size)
{
    float       per_pane;
    float       usable;
    std::size_t n;

    n = _sv.panes.size();
    if (n == 0)
    {
        return;
    }

    usable   = _total_size - spl_total_splitter_size(_sv);
    per_pane = usable / static_cast<float>(n);

    if (per_pane < 0.0f)
    {
        per_pane = 0.0f;
    }

    for (auto& pane : _sv.panes)
    {
        pn_set_size(pane, per_pane);
    }

    return;
}


/*
spl_resize
  Redistributes pane sizes so the total main-axis extent matches
`_new_total`.  The redistribution strategy is selected by the
sf_resize_policy feature; when the feature is absent the default
is `split_resize_policy::proportional`.  The policy is best-effort
with respect to constraints — panes clamp to min/max and any
residual is absorbed by the remaining flexible panes.

Parameter(s):
  _sv:        the split_view to resize.
  _new_total: the target total main-axis extent (panes + splitters).
Return:
  none.
*/
template <unsigned _PF, unsigned _CF>
void
spl_resize(split_view<_PF, _CF>& _sv,
           float                 _new_total)
{
    split_resize_policy policy;
    float               splitter_total;
    float               usable_new;
    float               usable_old;
    float               delta;
    std::size_t         n;

    n = _sv.panes.size();
    if (n == 0)
    {
        return;
    }

    // initialize
    if constexpr (has_sf(_CF, sf_resize_policy))
    {
        policy = _sv.policy;
    }
    else
    {
        policy = split_resize_policy::proportional;
    }

    splitter_total = spl_total_splitter_size(_sv);
    usable_new     = _new_total - splitter_total;
    usable_old     = spl_total_pane_size(_sv);

    if (usable_new < 0.0f)
    {
        usable_new = 0.0f;
    }

    // rigid: leave everything alone
    if (policy == split_resize_policy::rigid)
    {
        return;
    }

    delta = usable_new - usable_old;
    if (delta == 0.0f)
    {
        return;
    }

    switch (policy)
    {
        case split_resize_policy::proportional:
        {
            float scale =
                (usable_old > 0.0f)
                    ? (usable_new / usable_old)
                    : (1.0f / static_cast<float>(n));
            for (auto& pane : _sv.panes)
            {
                pn_set_size(pane, pane.size * scale);
            }
            break;
        }

        case split_resize_policy::weighted:
        {
            float total_weight = 0.0f;
            for (const auto& pane : _sv.panes)
            {
                total_weight += pane.weight;
            }
            if (total_weight <= 0.0f)
            {
                total_weight = static_cast<float>(n);
            }
            for (auto& pane : _sv.panes)
            {
                float share = delta * (pane.weight / total_weight);
                pn_set_size(pane, pane.size + share);
            }
            break;
        }

        case split_resize_policy::stretch_first:
        {
            pn_set_size(_sv.panes.front(),
                        _sv.panes.front().size + delta);
            break;
        }

        case split_resize_policy::stretch_last:
        {
            pn_set_size(_sv.panes.back(),
                        _sv.panes.back().size + delta);
            break;
        }

        case split_resize_policy::rigid:
            // handled above
            break;
    }

    return;
}


/*
spl_move_splitter
  Moves splitter `_splitter_idx` by `_delta` pixels along the main
axis.  Positive delta shrinks the pane-before and grows the
pane-after; negative does the opposite.  The actual applied delta is
constrained by:
  - the adjacent panes' min/max constraints,
  - per-splitter locking (when sf_splitter_state is enabled),
  - the global `resizable` flag.
Fires on_splitter_moved with the splitter index after successful
application.

Parameter(s):
  _sv:           the split_view.
  _splitter_idx: the splitter to move.
  _delta:        requested pixel delta.
Return:
  The actual delta applied after clamping; 0 if no motion occurred.
*/
template <unsigned _PF, unsigned _CF>
float
spl_move_splitter(split_view<_PF, _CF>& _sv,
                  std::size_t           _splitter_idx,
                  float                 _delta)
{
    float max_pos;
    float max_neg;
    float inc_a;
    float dec_b;
    float dec_a;
    float inc_b;

    // parameter validation
    if (!_sv.resizable)
    {
        return 0.0f;
    }
    if (_splitter_idx + 1 >= _sv.panes.size())
    {
        return 0.0f;
    }

    // per-splitter lock check
    if constexpr (has_sf(_CF, sf_splitter_state))
    {
        if ( (_splitter_idx < _sv.splitters.size()) &&
             (_sv.splitters[_splitter_idx].locked) )
        {
            return 0.0f;
        }
    }

    auto& pa = _sv.panes[_splitter_idx];
    auto& pb = _sv.panes[_splitter_idx + 1];

    if ( (!pa.enabled) ||
         (!pb.enabled) )
    {
        return 0.0f;
    }

    // compute clamped deltas in each direction
    //   positive delta: grow pa, shrink pb
    inc_a   = (pa.max_size > 0.0f)
              ? (pa.max_size - pa.size)
              : std::numeric_limits<float>::infinity();
    dec_b   = pb.size - pb.min_size;
    max_pos = std::min(inc_a, dec_b);

    //   negative delta: shrink pa, grow pb
    dec_a   = pa.size - pa.min_size;
    inc_b   = (pb.max_size > 0.0f)
              ? (pb.max_size - pb.size)
              : std::numeric_limits<float>::infinity();
    max_neg = std::min(dec_a, inc_b);

    if (_delta > 0.0f)
    {
        if (_delta > max_pos) { _delta = max_pos; }
        if (_delta < 0.0f)    { _delta = 0.0f;    }
    }
    else if (_delta < 0.0f)
    {
        if (-_delta > max_neg) { _delta = -max_neg; }
        if (_delta > 0.0f)     { _delta = 0.0f;     }
    }

    if (_delta == 0.0f)
    {
        return 0.0f;
    }

    pa.size += _delta;
    pb.size -= _delta;

    if (_sv.on_pane_resized)
    {
        _sv.on_pane_resized(_splitter_idx,     pa.size);
        _sv.on_pane_resized(_splitter_idx + 1, pb.size);
    }

    if (_sv.on_splitter_moved)
    {
        _sv.on_splitter_moved(_splitter_idx);
    }

    return _delta;
}

// spl_set_splitter_position
//   function: moves the splitter so its absolute pixel position from
// the main-axis origin equals `_absolute_pos`.  Computes the required
// delta and delegates to spl_move_splitter.  Returns the actual
// position after clamping.
template <unsigned _PF, unsigned _CF>
float
spl_set_splitter_position(split_view<_PF, _CF>& _sv,
                          std::size_t           _splitter_idx,
                          float                 _absolute_pos)
{
    float current;
    float delta;

    current = spl_splitter_position(_sv, _splitter_idx);
    if (current != current)          // NaN check
    {
        return current;
    }

    delta = _absolute_pos - current;
    spl_move_splitter(_sv, _splitter_idx, delta);

    return spl_splitter_position(_sv, _splitter_idx);
}


// -- collapse / expand ----------------------------------------------------

/*
spl_collapse_pane
  Collapses the given pane (size → 0, saving current size for later
restore) and redistributes its space to neighboring panes using the
view's resize policy.  Requires pf_collapsible on the pane type.

Parameter(s):
  _sv:    the split_view.
  _index: the pane to collapse.
Return:
  true if the pane was collapsed, false if out of range or already
  collapsed.
*/
template <unsigned _PF, unsigned _CF>
bool
spl_collapse_pane(split_view<_PF, _CF>& _sv,
                  std::size_t           _index)
{
    static_assert(has_pf(_PF, pf_collapsible),
                  "requires pf_collapsible on panes");

    float reclaimed;
    float distribute;

    if (_index >= _sv.panes.size())
    {
        return false;
    }
    if (_sv.panes[_index].collapsed)
    {
        return false;
    }

    // collapse and capture the space we just freed
    reclaimed = _sv.panes[_index].size;
    pn_collapse(_sv.panes[_index]);

    // redistribute the reclaimed space to the adjacent pane(s)
    //   prefer the right neighbour, fall back to the left
    distribute = reclaimed;
    if ( (_index + 1 < _sv.panes.size()) &&
         (!_sv.panes[_index + 1].collapsed) )
    {
        _sv.panes[_index + 1].size =
            pn_clamp_size(_sv.panes[_index + 1],
                          _sv.panes[_index + 1].size + distribute);
    }
    else if ( (_index > 0) &&
              (!_sv.panes[_index - 1].collapsed) )
    {
        _sv.panes[_index - 1].size =
            pn_clamp_size(_sv.panes[_index - 1],
                          _sv.panes[_index - 1].size + distribute);
    }

    if (_sv.on_pane_collapsed)
    {
        _sv.on_pane_collapsed(_index, true);
    }

    return true;
}

/*
spl_expand_pane
  Restores a collapsed pane to its saved size, taking space from the
neighbor that absorbed it on collapse (best-effort — the neighbor
may have shrunk in the interim, in which case the expanded size is
clamped down).  Requires pf_collapsible on the pane type.
*/
template <unsigned _PF, unsigned _CF>
bool
spl_expand_pane(split_view<_PF, _CF>& _sv,
                std::size_t           _index)
{
    static_assert(has_pf(_PF, pf_collapsible),
                  "requires pf_collapsible on panes");

    float restore;
    float available;

    if (_index >= _sv.panes.size())
    {
        return false;
    }
    if (!_sv.panes[_index].collapsed)
    {
        return false;
    }

    restore = _sv.panes[_index].saved_size;

    // take from right neighbour first
    if ( (_index + 1 < _sv.panes.size()) &&
         (!_sv.panes[_index + 1].collapsed) )
    {
        available = _sv.panes[_index + 1].size
                  - _sv.panes[_index + 1].min_size;
        if (restore > available)
        {
            restore = available;
        }
        _sv.panes[_index + 1].size -= restore;
    }
    else if ( (_index > 0) &&
              (!_sv.panes[_index - 1].collapsed) )
    {
        available = _sv.panes[_index - 1].size
                  - _sv.panes[_index - 1].min_size;
        if (restore > available)
        {
            restore = available;
        }
        _sv.panes[_index - 1].size -= restore;
    }

    _sv.panes[_index].size       = restore;
    _sv.panes[_index].collapsed  = false;
    _sv.panes[_index].saved_size = 0.0f;

    if (_sv.on_pane_collapsed)
    {
        _sv.on_pane_collapsed(_index, false);
    }

    return true;
}

// spl_any_collapsed
template <unsigned _PF, unsigned _CF>
[[nodiscard]] bool
spl_any_collapsed(const split_view<_PF, _CF>& _sv) noexcept
{
    static_assert(has_pf(_PF, pf_collapsible),
                  "requires pf_collapsible on panes");

    for (const auto& pane : _sv.panes)
    {
        if (pane.collapsed)
        {
            return true;
        }
    }

    return false;
}


// -- drag interaction (sf_drag_state) -------------------------------------

/*
spl_begin_drag
  Starts an interactive drag on the given splitter.  Stashes current
pane sizes so spl_cancel_drag can restore them, and records the
starting pointer position for delta computation.

Parameter(s):
  _sv:           the split_view.
  _splitter_idx: the splitter being dragged.
  _pointer_pos:  the current pointer position along the main axis, in
                 pixels from the view's main-axis origin.
Return:
  true if the drag was initiated, false if the splitter is invalid or
  locked.
*/
template <unsigned _PF, unsigned _CF>
bool
spl_begin_drag(split_view<_PF, _CF>& _sv,
               std::size_t           _splitter_idx,
               float                 _pointer_pos)
{
    static_assert(has_sf(_CF, sf_drag_state),
                  "requires sf_drag_state");

    if (!_sv.resizable)
    {
        return false;
    }
    if (_splitter_idx + 1 >= _sv.panes.size())
    {
        return false;
    }

    if constexpr (has_sf(_CF, sf_splitter_state))
    {
        if ( (_splitter_idx < _sv.splitters.size()) &&
             (_sv.splitters[_splitter_idx].locked) )
        {
            return false;
        }
    }

    _sv.mode             = split_drag_mode::dragging;
    _sv.active_splitter  = _splitter_idx;
    _sv.drag_start_pos   = _pointer_pos;
    _sv.drag_accumulator = 0.0f;

    _sv.sizes_at_drag_begin = spl_get_sizes(_sv);

    return true;
}

/*
spl_update_drag
  Advances an in-progress drag.  The delta is the *incremental*
pointer movement since the previous update (typically ImGui's
MouseDelta or an equivalent per-frame pointer delta).  Applies
snap-to-nearest if sf_snap_points is enabled and the resulting
splitter position lands within snap_tolerance of a snap point.

Parameter(s):
  _sv:    the split_view.
  _delta: incremental pointer delta in pixels along the main axis.
Return:
  The actual delta applied to the splitter position (possibly clamped).
*/
template <unsigned _PF, unsigned _CF>
float
spl_update_drag(split_view<_PF, _CF>& _sv,
                float                 _delta)
{
    static_assert(has_sf(_CF, sf_drag_state),
                  "requires sf_drag_state");

    float applied;

    if (_sv.mode != split_drag_mode::dragging)
    {
        return 0.0f;
    }

    applied = spl_move_splitter(_sv, _sv.active_splitter, _delta);

    // snap handling — applied after move so we know the post-move position
    if constexpr (has_sf(_CF, sf_snap_points))
    {
        if ( (_sv.active_splitter < _sv.snap_points.size()) &&
             (!_sv.snap_points[_sv.active_splitter].empty()) )
        {
            float cur  = spl_splitter_position(_sv, _sv.active_splitter);
            float best = cur;
            float best_dist =
                std::numeric_limits<float>::infinity();

            for (float snap :
                     _sv.snap_points[_sv.active_splitter])
            {
                float d = std::abs(snap - cur);
                if (d < best_dist)
                {
                    best_dist = d;
                    best      = snap;
                }
            }

            if (best_dist <= _sv.snap_tolerance)
            {
                float snap_delta = best - cur;
                applied += spl_move_splitter(
                    _sv, _sv.active_splitter, snap_delta);
            }
        }
    }

    return applied;
}

/*
spl_end_drag
  Finalises an in-progress drag, leaving pane sizes at their current
state.  Clears drag state.  The on_splitter_moved callback has already
been fired by the per-frame spl_update_drag calls; this function does
not re-fire it.
*/
template <unsigned _PF, unsigned _CF>
void
spl_end_drag(split_view<_PF, _CF>& _sv)
{
    static_assert(has_sf(_CF, sf_drag_state),
                  "requires sf_drag_state");

    _sv.mode             = split_drag_mode::idle;
    _sv.active_splitter  = static_cast<std::size_t>(-1);
    _sv.drag_start_pos   = 0.0f;
    _sv.drag_accumulator = 0.0f;
    _sv.sizes_at_drag_begin.clear();

    return;
}

/*
spl_cancel_drag
  Aborts an in-progress drag and restores all pane sizes to what they
were when spl_begin_drag was called.
*/
template <unsigned _PF, unsigned _CF>
void
spl_cancel_drag(split_view<_PF, _CF>& _sv)
{
    static_assert(has_sf(_CF, sf_drag_state),
                  "requires sf_drag_state");

    if (!_sv.sizes_at_drag_begin.empty())
    {
        spl_set_sizes(_sv, _sv.sizes_at_drag_begin);
    }

    spl_end_drag(_sv);

    return;
}

// spl_set_hovered_splitter
//   function: sets the hovered splitter index for rendering feedback.
// SIZE_MAX clears the hover state.
template <unsigned _PF, unsigned _CF>
void
spl_set_hovered_splitter(split_view<_PF, _CF>& _sv,
                         std::size_t           _splitter_idx)
{
    static_assert(has_sf(_CF, sf_drag_state),
                  "requires sf_drag_state");

    _sv.hovered_splitter = _splitter_idx;
    if ( (_sv.mode != split_drag_mode::dragging) &&
         (_splitter_idx != static_cast<std::size_t>(-1)) )
    {
        _sv.mode = split_drag_mode::hovering;
    }
    else if ( (_sv.mode == split_drag_mode::hovering) &&
              (_splitter_idx == static_cast<std::size_t>(-1)) )
    {
        _sv.mode = split_drag_mode::idle;
    }

    return;
}


// -- double-click (sf_double_click) ---------------------------------------

/*
spl_handle_double_click
  Dispatches the double-click policy for the given splitter.  Called
by the renderer when it detects a double-click on a splitter.

Parameter(s):
  _sv:           the split_view.
  _splitter_idx: the splitter that was double-clicked.
Return:
  true if the policy performed an action, false if the policy was
  `none` or the splitter index was invalid.
*/
template <unsigned _PF, unsigned _CF>
bool
spl_handle_double_click(split_view<_PF, _CF>& _sv,
                        std::size_t           _splitter_idx)
{
    static_assert(has_sf(_CF, sf_double_click),
                  "requires sf_double_click");

    if (_splitter_idx + 1 >= _sv.panes.size())
    {
        return false;
    }

    switch (_sv.double_click)
    {
        case splitter_double_click::collapse_first:
            if constexpr (has_pf(_PF, pf_collapsible))
            {
                return spl_collapse_pane(_sv, _splitter_idx);
            }
            return false;

        case splitter_double_click::collapse_second:
            if constexpr (has_pf(_PF, pf_collapsible))
            {
                return spl_collapse_pane(_sv, _splitter_idx + 1);
            }
            return false;

        case splitter_double_click::toggle_collapse_first:
            if constexpr (has_pf(_PF, pf_collapsible))
            {
                if (_sv.panes[_splitter_idx].collapsed)
                {
                    return spl_expand_pane(_sv, _splitter_idx);
                }
                return spl_collapse_pane(_sv, _splitter_idx);
            }
            return false;

        case splitter_double_click::toggle_collapse_second:
            if constexpr (has_pf(_PF, pf_collapsible))
            {
                if (_sv.panes[_splitter_idx + 1].collapsed)
                {
                    return spl_expand_pane(_sv, _splitter_idx + 1);
                }
                return spl_collapse_pane(_sv, _splitter_idx + 1);
            }
            return false;

        case splitter_double_click::reset_default:
            if (!_sv.default_sizes.empty())
            {
                spl_set_sizes(_sv, _sv.default_sizes);
                return true;
            }
            return false;

        case splitter_double_click::distribute_evenly:
            spl_distribute_evenly(_sv, spl_total_size(_sv));
            return true;

        case splitter_double_click::none:
        default:
            return false;
    }
}


// -- animation (sf_animated) ----------------------------------------------

/*
spl_animate_to
  Begins a smooth animation from current sizes to `_target_sizes`
over `_duration` seconds.  The caller is responsible for advancing
the animation each frame via spl_animation_tick.

Parameter(s):
  _sv:           the split_view.
  _target_sizes: the sizes to animate to (one per pane).
  _duration:     the animation duration in seconds.
Return:
  none.
*/
template <unsigned _PF, unsigned _CF>
void
spl_animate_to(split_view<_PF, _CF>&     _sv,
               const std::vector<float>& _target_sizes,
               float                     _duration)
{
    static_assert(has_sf(_CF, sf_animated),
                  "requires sf_animated");

    _sv.start_sizes   = spl_get_sizes(_sv);
    _sv.target_sizes  = _target_sizes;
    _sv.anim_progress = 0.0f;
    _sv.anim_duration = (_duration > 0.0f) ? _duration : 0.15f;
    _sv.anim_active   = true;

    return;
}

/*
spl_animation_tick
  Advances the in-progress animation by `_delta_time` seconds.  On
completion (progress reaches 1.0) the animation becomes inactive,
target sizes are applied exactly, and subsequent calls are no-ops
until another spl_animate_to.

Parameter(s):
  _sv:         the split_view.
  _delta_time: elapsed seconds since the last tick.
Return:
  true if the animation is still active after this tick, false if
  it has just completed or was already inactive.
*/
template <unsigned _PF, unsigned _CF>
bool
spl_animation_tick(split_view<_PF, _CF>& _sv,
                   float                 _delta_time)
{
    static_assert(has_sf(_CF, sf_animated),
                  "requires sf_animated");

    float       t;
    std::size_t i;
    std::size_t n;

    if (!_sv.anim_active)
    {
        return false;
    }

    _sv.anim_progress += _delta_time / _sv.anim_duration;
    if (_sv.anim_progress >= 1.0f)
    {
        _sv.anim_progress = 1.0f;
        spl_set_sizes(_sv, _sv.target_sizes);
        _sv.anim_active = false;
        return false;
    }

    // linear interpolation — callers wanting ease curves can override
    // the progress value directly before calling
    t = _sv.anim_progress;
    n = std::min({_sv.panes.size(),
                  _sv.start_sizes.size(),
                  _sv.target_sizes.size()});
    for (i = 0; i < n; ++i)
    {
        float s = _sv.start_sizes[i];
        float e = _sv.target_sizes[i];
        pn_set_size(_sv.panes[i], s + (e - s) * t);
    }

    return true;
}


// -- snap points (sf_snap_points) -----------------------------------------

// spl_add_snap_point
//   function: registers a snap position (pixels from the main-axis
// origin) for the given splitter.  Resizes the snap_points outer
// vector as needed.
template <unsigned _PF, unsigned _CF>
void
spl_add_snap_point(split_view<_PF, _CF>& _sv,
                   std::size_t           _splitter_idx,
                   float                 _position)
{
    static_assert(has_sf(_CF, sf_snap_points),
                  "requires sf_snap_points");

    if (_sv.snap_points.size() <= _splitter_idx)
    {
        _sv.snap_points.resize(_splitter_idx + 1);
    }

    _sv.snap_points[_splitter_idx].push_back(_position);

    return;
}

// spl_clear_snap_points
template <unsigned _PF, unsigned _CF>
void
spl_clear_snap_points(split_view<_PF, _CF>& _sv,
                      std::size_t           _splitter_idx)
{
    static_assert(has_sf(_CF, sf_snap_points),
                  "requires sf_snap_points");

    if (_splitter_idx < _sv.snap_points.size())
    {
        _sv.snap_points[_splitter_idx].clear();
    }

    return;
}


// -- keyboard navigation (sf_keyboard_nav) --------------------------------

// spl_focus_next_splitter
template <unsigned _PF, unsigned _CF>
void
spl_focus_next_splitter(split_view<_PF, _CF>& _sv)
{
    static_assert(has_sf(_CF, sf_keyboard_nav),
                  "requires sf_keyboard_nav");

    std::size_t n;

    n = _sv.splitter_count();
    if (n == 0)
    {
        return;
    }

    _sv.focused_splitter = (_sv.focused_splitter + 1) % n;

    return;
}

// spl_focus_prev_splitter
template <unsigned _PF, unsigned _CF>
void
spl_focus_prev_splitter(split_view<_PF, _CF>& _sv)
{
    static_assert(has_sf(_CF, sf_keyboard_nav),
                  "requires sf_keyboard_nav");

    std::size_t n;

    n = _sv.splitter_count();
    if (n == 0)
    {
        return;
    }

    _sv.focused_splitter = (_sv.focused_splitter == 0)
                           ? (n - 1)
                           : (_sv.focused_splitter - 1);

    return;
}

// spl_keyboard_step
//   function: nudges the focused splitter by (_sign * keyboard_step)
// pixels.  `_sign` is typically +1 or -1.
template <unsigned _PF, unsigned _CF>
float
spl_keyboard_step(split_view<_PF, _CF>& _sv,
                  int                   _sign)
{
    static_assert(has_sf(_CF, sf_keyboard_nav),
                  "requires sf_keyboard_nav");

    float delta;

    delta = static_cast<float>(_sign) * _sv.keyboard_step;

    return spl_move_splitter(_sv, _sv.focused_splitter, delta);
}


// -- id lookup (pf_id) ----------------------------------------------------

// spl_find_by_id
//   function: linear search for a pane with the given id.  Returns
// SIZE_MAX if not found.
template <unsigned _PF, unsigned _CF>
[[nodiscard]] std::size_t
spl_find_by_id(const split_view<_PF, _CF>& _sv,
               const std::string&          _id)
{
    static_assert(has_pf(_PF, pf_id),
                  "requires pf_id on panes");

    std::size_t i;

    for (i = 0; i < _sv.panes.size(); ++i)
    {
        if (_sv.panes[i].id == _id)
        {
            return i;
        }
    }

    return static_cast<std::size_t>(-1);
}


// ===============================================================================
//  11.  TRAITS
// ===============================================================================
//   SFINAE detectors following the tab_control / stacked_view pattern.
// Renderers and generic code query these to discover what a split_pane
// or split_view instance carries, without hard-coding specific feature
// combinations.

namespace split_traits {

NS_INTERNAL

    // -- pane-level detectors ---------------------------------------------

    template <typename, typename = void>
    struct has_size_member : std::false_type {};

    template <typename _Type>
    struct has_size_member<_Type, std::void_t<
        decltype(std::declval<_Type>().size)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_min_size_member : std::false_type {};

    template <typename _Type>
    struct has_min_size_member<_Type, std::void_t<
        decltype(std::declval<_Type>().min_size)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_max_size_member : std::false_type {};

    template <typename _Type>
    struct has_max_size_member<_Type, std::void_t<
        decltype(std::declval<_Type>().max_size)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_weight_member : std::false_type {};

    template <typename _Type>
    struct has_weight_member<_Type, std::void_t<
        decltype(std::declval<_Type>().weight)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_user_id_member : std::false_type {};

    template <typename _Type>
    struct has_user_id_member<_Type, std::void_t<
        decltype(std::declval<_Type>().user_id)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_id_member : std::false_type {};

    template <typename _Type>
    struct has_id_member<_Type, std::void_t<
        decltype(std::declval<_Type>().id)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_title_member : std::false_type {};

    template <typename _Type>
    struct has_title_member<_Type, std::void_t<
        decltype(std::declval<_Type>().title)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_collapsed_member : std::false_type {};

    template <typename _Type>
    struct has_collapsed_member<_Type, std::void_t<
        decltype(std::declval<_Type>().collapsed)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_saved_size_member : std::false_type {};

    template <typename _Type>
    struct has_saved_size_member<_Type, std::void_t<
        decltype(std::declval<_Type>().saved_size)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_user_tag_member : std::false_type {};

    template <typename _Type>
    struct has_user_tag_member<_Type, std::void_t<
        decltype(std::declval<_Type>().user_tag)
    >> : std::true_type {};

    // -- view-level detectors ---------------------------------------------

    template <typename, typename = void>
    struct has_panes_member : std::false_type {};

    template <typename _Type>
    struct has_panes_member<_Type, std::void_t<
        decltype(std::declval<_Type>().panes)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_orientation_member : std::false_type {};

    template <typename _Type>
    struct has_orientation_member<_Type, std::void_t<
        decltype(std::declval<_Type>().orientation)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_splitter_thickness_member : std::false_type {};

    template <typename _Type>
    struct has_splitter_thickness_member<_Type, std::void_t<
        decltype(std::declval<_Type>().splitter_thickness)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_resizable_member : std::false_type {};

    template <typename _Type>
    struct has_resizable_member<_Type, std::void_t<
        decltype(std::declval<_Type>().resizable)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_active_splitter_member : std::false_type {};

    template <typename _Type>
    struct has_active_splitter_member<_Type, std::void_t<
        decltype(std::declval<_Type>().active_splitter)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_policy_member : std::false_type {};

    template <typename _Type>
    struct has_policy_member<_Type, std::void_t<
        decltype(std::declval<_Type>().policy)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_snap_points_member : std::false_type {};

    template <typename _Type>
    struct has_snap_points_member<_Type, std::void_t<
        decltype(std::declval<_Type>().snap_points)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_splitters_member : std::false_type {};

    template <typename _Type>
    struct has_splitters_member<_Type, std::void_t<
        decltype(std::declval<_Type>().splitters)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_anim_progress_member : std::false_type {};

    template <typename _Type>
    struct has_anim_progress_member<_Type, std::void_t<
        decltype(std::declval<_Type>().anim_progress)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_double_click_member : std::false_type {};

    template <typename _Type>
    struct has_double_click_member<_Type, std::void_t<
        decltype(std::declval<_Type>().double_click)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_focused_splitter_member : std::false_type {};

    template <typename _Type>
    struct has_focused_splitter_member<_Type, std::void_t<
        decltype(std::declval<_Type>().focused_splitter)
    >> : std::true_type {};

NS_END  // internal


// -- pane value aliases ---------------------------------------------------
template <typename _Type>
inline constexpr bool has_size_v =
    internal::has_size_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_min_size_v =
    internal::has_min_size_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_max_size_v =
    internal::has_max_size_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_weight_v =
    internal::has_weight_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_user_id_v =
    internal::has_user_id_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_id_v =
    internal::has_id_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_title_v =
    internal::has_title_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_collapsed_v =
    internal::has_collapsed_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_saved_size_v =
    internal::has_saved_size_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_user_tag_v =
    internal::has_user_tag_member<_Type>::value;

// -- view value aliases ---------------------------------------------------
template <typename _Type>
inline constexpr bool has_panes_v =
    internal::has_panes_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_orientation_v =
    internal::has_orientation_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_splitter_thickness_v =
    internal::has_splitter_thickness_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_resizable_v =
    internal::has_resizable_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_active_splitter_v =
    internal::has_active_splitter_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_policy_v =
    internal::has_policy_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_snap_points_v =
    internal::has_snap_points_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_splitters_v =
    internal::has_splitters_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_anim_progress_v =
    internal::has_anim_progress_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_double_click_v =
    internal::has_double_click_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_focused_splitter_v =
    internal::has_focused_splitter_member<_Type>::value;


// -- composite traits -----------------------------------------------------

// is_split_pane
//   trait: structurally identifies a split_pane — has size, min_size,
// max_size, weight, enabled, visible, and user_id.  Optional per-pane
// features do not participate in the test.
template <typename _Type>
struct is_split_pane : std::conjunction<
    internal::has_size_member<_Type>,
    internal::has_min_size_member<_Type>,
    internal::has_max_size_member<_Type>,
    internal::has_weight_member<_Type>,
    internal::has_enabled_member<_Type>,
    internal::has_visible_member<_Type>,
    internal::has_user_id_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_split_pane_v =
    is_split_pane<_Type>::value;

// is_split_view
//   trait: structurally identifies a split_view — has panes,
// orientation, splitter_thickness, resizable, enabled, visible.
template <typename _Type>
struct is_split_view : std::conjunction<
    internal::has_panes_member<_Type>,
    internal::has_orientation_member<_Type>,
    internal::has_splitter_thickness_member<_Type>,
    internal::has_resizable_member<_Type>,
    internal::has_enabled_member<_Type>,
    internal::has_visible_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_split_view_v =
    is_split_view<_Type>::value;

// is_draggable_split_view
template <typename _Type>
struct is_draggable_split_view : std::conjunction<
    is_split_view<_Type>,
    internal::has_active_splitter_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_draggable_split_view_v =
    is_draggable_split_view<_Type>::value;

// is_policy_split_view
template <typename _Type>
struct is_policy_split_view : std::conjunction<
    is_split_view<_Type>,
    internal::has_policy_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_policy_split_view_v =
    is_policy_split_view<_Type>::value;

// is_snap_split_view
template <typename _Type>
struct is_snap_split_view : std::conjunction<
    is_split_view<_Type>,
    internal::has_snap_points_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_snap_split_view_v =
    is_snap_split_view<_Type>::value;

// is_animated_split_view
template <typename _Type>
struct is_animated_split_view : std::conjunction<
    is_split_view<_Type>,
    internal::has_anim_progress_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_animated_split_view_v =
    is_animated_split_view<_Type>::value;

// is_keyboard_split_view
template <typename _Type>
struct is_keyboard_split_view : std::conjunction<
    is_split_view<_Type>,
    internal::has_focused_splitter_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_keyboard_split_view_v =
    is_keyboard_split_view<_Type>::value;


}   // namespace split_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_SPLIT_VIEW_
