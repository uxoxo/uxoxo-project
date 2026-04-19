/*******************************************************************************
* uxoxo [component]                                                  popover.hpp
*
* Generic anchored popover shell:
*   A framework-agnostic, content-agnostic floating panel.  Where
* drop_container is specifically a flat list of items expanding up
* or down, `popover` is the abstract container *around* any such
* content: a list_view, a grid of buttons, an emoji picker, a date
* grid, an autosuggest dropdown, a history browser, a context
* menu — whatever fits.
*
*   The popover owns:
*     - A `_Content` instance (the hosted widget / data)
*     - Anchor metadata (what the popover pops out of)
*     - Direction, alignment, and size hints
*     - Lifecycle state (enabled / visible / active / modal / loading)
*     - Lifecycle callbacks (on_open, on_close, on_before_close)
*     - A generic on_refresh hook the consumer installs
*
*   The popover optionally owns:
*     - A non-owning pointer to a suggest_adapter (via `_Source`
*       template parameter; default `void` = no binding, EBO-eliding)
*     - A typed sink function that drains adapter results into
*       `_Content`
*
*   The popover prescribes NOTHING about:
*     - Rendering.  Where the panel actually lands on screen, and
*       how `_Content` is drawn, is the renderer's concern.
*     - Input routing.  How keystrokes translate into selections
*       is the consumer's concern (typically delegated to whatever
*       `_Content` is — e.g. drop_container navigation ops).
*     - Eviction / persistence of the content.  The popover holds
*       the content; the content owns its own storage policy.
*
*   Typical consumption patterns:
*     - autocomplete + drop_container:
*         popover<drop_container<suggestion>, my_trie_adapter>
*     - command history browser:
*         popover<list_view<std::string>>  (on_refresh reads the
*         history<> into list_view.items)
*     - emoji picker:
*         popover<std::vector<button>>      (no source, static
*         grid of buttons)
*     - autosuggest wrapped in a popover for consistent framing:
*         popover<autosuggest<std::string>>
*     - context menu:
*         popover<drop_container<menu_action>>
*
* Contents:
*   1  DPopoverDirection   — four-way + automatic
*   2  DPopoverAlignment   — cross-axis start/center/end
*   3  popover feature flags
*   4  popover_default_policy
*   5  EBO mixins (source_plumbing, policy_carrier)
*   6  popover<>           — the component class
*   7  popover_* free functions
*   8  popover_traits      — SFINAE detection
*
*
* path:      /inc/uxoxo/templates/component/popover.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_POPOVER_
#define  UXOXO_COMPONENT_POPOVER_ 1

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
#include "../../uxoxo.hpp"
#include "./component_mixin.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  POPOVER DIRECTION
// ===============================================================================

// DPopoverDirection
//   enum: primary expansion axis of a popover relative to its anchor.
// `automatic` defers to the renderer, which picks the direction with
// the most available space.  The renderer writes its decision back
// into `resolved_direction` so downstream code sees a concrete value.
enum class DPopoverDirection : std::uint8_t
{
    down      = 0,  // panel extends below the anchor (classic dropdown)
    up        = 1,  // panel extends above (dropup)
    right     = 2,  // panel extends to the right (side flyout)
    left      = 3,  // panel extends to the left
    automatic = 4   // renderer picks based on available space
};


/*****************************************************************************/

// ===============================================================================
//  2  POPOVER ALIGNMENT
// ===============================================================================

// DPopoverAlignment
//   enum: cross-axis alignment of the popover relative to the anchor.
// The meaning of start / end follows the primary direction:
//   - direction is down or up   -> start = left edge,  end = right edge
//   - direction is right or left -> start = top edge,   end = bottom edge
// `center` always means center-aligned on the cross axis.
enum class DPopoverAlignment : std::uint8_t
{
    start  = 0,
    center = 1,
    end    = 2
};


/*****************************************************************************/

// ===============================================================================
//  3  POPOVER FEATURE FLAGS
// ===============================================================================
//   Bitmask opt-in for popover capabilities.  Combine with bitwise-or
// when instantiating, e.g.:
//
//     popover<my_grid, void, pf_labeled | pf_dismissable> emoji_pick;

enum : std::uint32_t
{
    pf_none        = 0u,
    pf_labeled     = 1u << 0,  // label_data mixin (title / header text)
    pf_dismissable = 1u << 1   // opts into user-initiated close (Esc,
                               // outside click); gates popover_dismiss()
};


/*****************************************************************************/

// ===============================================================================
//  4  DEFAULT POLICY
// ===============================================================================

// popover_default_policy
//   struct: empty default policy placeholder.  Framework-specific
// policies can define static constexpr fields, type aliases, or
// methods to express preferences (animation mode, default
// direction, focus-trap behavior, etc.).
struct popover_default_policy
{};


/*****************************************************************************/

// ===============================================================================
//  5  EBO MIXINS
// ===============================================================================

namespace popover_mixin {

    // source_plumbing
    //   mixin: carries a non-owning pointer to a suggest_adapter plus a
    // typed sink function that maps adapter results into _Content.
    // Primary template (used when _Source is a concrete adapter type).
    template <typename _Content,
              typename _Source>
    struct source_plumbing
    {
        using source_type  = _Source;
        using input_type   = typename _Source::input_type;
        using results_type = typename _Source::container_type;
        using sink_fn      = std::function<
                                 void(_Content&, results_type&&)>;

        // non-owning pointer to the adapter; nullptr if not bound.
        _Source* source = nullptr;

        // optional drain function.  When set, popover_refresh_from_source
        // calls it with the adapter's results.  When unset, the popover
        // attempts direct assignment if _Content is assignable from
        // results_type&&.
        sink_fn on_source_result;
    };

    // source_plumbing (void specialization)
    //   mixin: empty base when no adapter binding is requested.  EBO
    // collapses this to zero cost.
    template <typename _Content>
    struct source_plumbing<_Content, void>
    {};

    // policy_carrier
    //   mixin: carries a _Policy member, or inherits from _Policy when
    // it is empty (EBO).  Mirrors the pattern in history_view.hpp.
    template <typename _Policy,
              bool     _Empty = std::is_empty<_Policy>::value>
    struct policy_carrier
    {
        _Policy policy {};
    };

    template <typename _Policy>
    struct policy_carrier<_Policy, true> : _Policy
    {};

}   // namespace popover_mixin


/*****************************************************************************/

// ===============================================================================
//  6  POPOVER
// ===============================================================================

// popover
//   class: generic anchored floating panel hosting an arbitrary
// `_Content`.  Carries direction / alignment / sizing hints, a
// lifecycle surface, and optional adapter plumbing.
//
//   Template parameters:
//     _Content   the hosted widget or data (drop_container<...>,
//                list_view<...>, std::vector<button>, ...).  Must
//                be default-constructible; movable preferred.
//     _Source    suggest_adapter derivative providing the data feed.
//                Default `void` disables the binding entirely —
//                source_plumbing collapses to an empty base.
//     _Features  pf_* bitmask of optional capabilities.
//     _Policy    framework-supplied presentation policy; default is
//                the empty popover_default_policy.
template <typename      _Content,
          typename      _Source   = void,
          std::uint32_t _Features = pf_none,
          typename      _Policy   = popover_default_policy>
class popover
    : public component_mixin::label_data<(_Features & pf_labeled) != 0>
    , public popover_mixin::source_plumbing<_Content, _Source>
    , public popover_mixin::policy_carrier<_Policy>
{
public:
    using content_type = _Content;
    using source_type  = _Source;
    using policy_type  = _Policy;
    using size_type    = std::size_t;
    using refresh_fn   = std::function<void(_Content&)>;
    using veto_fn      = std::function<bool()>;
    using lifecycle_fn = std::function<void()>;

    static constexpr std::uint32_t features   = _Features;
    static constexpr bool          focusable  = true;
    static constexpr bool          scrollable = true;

    // -- content ------------------------------------------------------
    //   The hosted widget / data.  Consumers operate on this directly
    // using whatever verbs `_Content` supports (drop_highlight_next,
    // lv_select, etc.).  The popover itself is content-agnostic.
    _Content content {};

    // -- core state ---------------------------------------------------
    bool enabled = true;   // interaction permitted
    bool visible = false;  // currently rendered
    bool active  = false;  // currently receiving input focus

    // -- behavioral flags (runtime) -----------------------------------
    bool modal            = false;  // blocks interaction with content behind
    bool close_on_select  = false;  // dismiss after a selection fires
    bool focus_trap       = false;  // trap tab focus inside while open
    bool is_loading       = false;  // async source is currently fetching

    // -- layout -------------------------------------------------------
    //   `direction` is the consumer's request; `resolved_direction`
    // is what the renderer ultimately chose (equal to `direction`
    // unless `direction == automatic`).
    DPopoverDirection direction          = DPopoverDirection::down;
    DPopoverDirection resolved_direction = DPopoverDirection::down;
    DPopoverAlignment alignment          = DPopoverAlignment::start;

    // -- sizing -------------------------------------------------------
    //   max_visible_entries caps the visible row/cell count.  0 means
    // "no cap".  min_visible_entries keeps the panel from shrinking
    // below a floor (useful to prevent jitter as results change); 0
    // means "no floor".  desired_width / desired_height in
    // renderer-defined units (pixels for GUI, cells for TUI).  0
    // means "auto from content".
    size_type max_visible_entries = 10;
    size_type min_visible_entries = 0;
    size_type desired_width       = 0;
    size_type desired_height      = 0;

    // -- anchor -------------------------------------------------------
    //   anchor_id is an opaque identifier the renderer maps to a
    // concrete source position (a widget handle, a DOM node, an
    // (x, y) point).  anchor_offset_{x,y} shift the panel from its
    // computed anchor point in renderer-defined units.
    std::string anchor_id;
    int         anchor_offset_x = 0;
    int         anchor_offset_y = 0;

    // -- messaging ----------------------------------------------------
    //   empty_message is surfaced by the renderer when the content
    // reports empty; the popover does not detect that condition
    // itself (content-specific).  loading_message is surfaced when
    // is_loading is true.
    std::string empty_message;
    std::string loading_message;

    // -- callbacks ----------------------------------------------------
    //   All callbacks are optional; empty std::function is skipped.
    // on_before_close returns false to veto the close.
    lifecycle_fn on_open;
    lifecycle_fn on_close;
    veto_fn      on_before_close;

    // -- refresh hook -------------------------------------------------
    //   Installed by the consumer to regenerate `content` on demand.
    // Invoked by popover_refresh().  Orthogonal to the source binding
    // below — use this for consumer-driven refresh (e.g. pull history
    // into content, regenerate a filtered list).
    refresh_fn on_refresh;


    // -- construction -------------------------------------------------
    popover() = default;

    explicit popover(
            _Content _c
        )
            : content(std::move(_c))
        {}

    popover(
            _Content          _c,
            DPopoverDirection _dir
        )
            : content(std::move(_c)),
              direction(_dir),
              resolved_direction(_dir)
        {}


    // -- queries ------------------------------------------------------
    [[nodiscard]] bool
    is_open() const noexcept
    {
        return visible;
    }

    [[nodiscard]] bool
    has_on_refresh() const noexcept
    {
        return static_cast<bool>(on_refresh);
    }
};


/*****************************************************************************/

// ===============================================================================
//  7  FREE FUNCTIONS
// ===============================================================================
//   Shared verbs (enable, disable, is_enabled) are inherited for free
// from component_common.hpp via structural detection.  show / hide
// from component_common.hpp will also flip `visible` but will NOT
// fire on_open / on_close — use popover_open / popover_close when
// callback invocation is required.


// ---- 7.1  detail helpers ------------------------------------------------

namespace detail {

    // has_max_visible_rows_member
    //   trait: structural detector for the `.max_visible_rows` field
    // that drop_container exposes.  Used to forward popover sizing
    // caps into content when the content supports it.
    template <typename, typename = void>
    struct has_max_visible_rows_member : std::false_type
    {};

    template <typename _Type>
    struct has_max_visible_rows_member<_Type, std::void_t<
        decltype(std::declval<_Type&>().max_visible_rows)
    >> : std::true_type
    {};

    // has_max_visible_member
    //   trait: structural detector for the `.max_visible` field that
    // autosuggest exposes.  Secondary forwarding target.
    template <typename, typename = void>
    struct has_max_visible_member : std::false_type
    {};

    template <typename _Type>
    struct has_max_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type&>().max_visible)
    >> : std::true_type
    {};

}   // namespace detail


/*****************************************************************************/

// ---- 7.2  lifecycle -----------------------------------------------------

// popover_open
//   function: marks the popover visible and active, then fires
// on_open if installed.  No-op if already visible (prevents
// double-fire of the callback).
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_open(
    popover<_Content, _Source, _Features, _Policy>& _p
)
{
    if (_p.visible)
    {
        return;
    }

    _p.visible = true;
    _p.active  = true;

    if (_p.on_open)
    {
        _p.on_open();
    }

    return;
}

// popover_close
//   function: consults on_before_close (if installed) for veto
// authority, then hides the popover, clears active state, and
// fires on_close.  Returns true if the close actually happened;
// false if vetoed or already hidden.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
bool
popover_close(
    popover<_Content, _Source, _Features, _Policy>& _p
)
{
    if (!_p.visible)
    {
        return false;
    }

    // veto check
    if (_p.on_before_close && !_p.on_before_close())
    {
        return false;
    }

    _p.visible = false;
    _p.active  = false;

    if (_p.on_close)
    {
        _p.on_close();
    }

    return true;
}

// popover_toggle
//   function: flips visibility — open if closed, close if open.
// Fires the corresponding lifecycle callback.  Returns the new
// visible state.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
bool
popover_toggle(
    popover<_Content, _Source, _Features, _Policy>& _p
)
{
    if (_p.visible)
    {
        popover_close(_p);
    }
    else
    {
        popover_open(_p);
    }

    return _p.visible;
}

// popover_dismiss
//   function: user-initiated close (Esc key, outside click).
// Semantically distinct from popover_close so framework code
// can distinguish programmatic dismissal from user intent.
// Only compiled when the popover opts into pf_dismissable.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy,
          std::enable_if_t<
              (_Features & pf_dismissable) != 0,
              int> = 0>
bool
popover_dismiss(
    popover<_Content, _Source, _Features, _Policy>& _p
)
{
    return popover_close(_p);
}


/*****************************************************************************/

// ---- 7.3  layout --------------------------------------------------------

// popover_set_direction
//   function: sets the requested direction.  Does NOT resolve
// `automatic` — the renderer does that and writes the outcome
// into `resolved_direction`.  When the request is concrete,
// `resolved_direction` is synchronized here as a convenience.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_set_direction(
    popover<_Content, _Source, _Features, _Policy>& _p,
    DPopoverDirection                                _dir
)
{
    _p.direction = _dir;

    if (_dir != DPopoverDirection::automatic)
    {
        _p.resolved_direction = _dir;
    }

    return;
}

// popover_set_alignment
//   function: sets the cross-axis alignment.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_set_alignment(
    popover<_Content, _Source, _Features, _Policy>& _p,
    DPopoverAlignment                                _align
)
{
    _p.alignment = _align;

    return;
}

// popover_set_anchor
//   function: sets the anchor identifier.  The renderer maps the
// id to a concrete source position.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_set_anchor(
    popover<_Content, _Source, _Features, _Policy>& _p,
    std::string                                      _id
)
{
    _p.anchor_id = std::move(_id);

    return;
}

// popover_set_anchor_offset
//   function: sets the renderer-unit offset from the resolved
// anchor point.  Both axes set in a single call — most callers
// shift on both or neither.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_set_anchor_offset(
    popover<_Content, _Source, _Features, _Policy>& _p,
    int                                              _dx,
    int                                              _dy
)
{
    _p.anchor_offset_x = _dx;
    _p.anchor_offset_y = _dy;

    return;
}


/*****************************************************************************/

// ---- 7.4  sizing --------------------------------------------------------

// popover_set_max_entries
//   function: caps the visible entry count.  Writes to the popover
// itself and, when `_Content` exposes a compatible cap field
// (`max_visible_rows` or `max_visible`), forwards the value into
// content so the two stay synchronized.  The forward is an
// `if constexpr` compile-time choice — zero overhead when content
// exposes neither.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_set_max_entries(
    popover<_Content, _Source, _Features, _Policy>&                    _p,
    typename popover<_Content, _Source, _Features, _Policy>::size_type _n
)
{
    _p.max_visible_entries = _n;

    // forward to content if it carries its own cap
    if constexpr (detail::has_max_visible_rows_member<_Content>::value)
    {
        _p.content.max_visible_rows = _n;
    }
    else if constexpr (detail::has_max_visible_member<_Content>::value)
    {
        _p.content.max_visible = _n;
    }

    return;
}

// popover_set_min_entries
//   function: floor the visible entry count.  Prevents size jitter
// as content shrinks.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_set_min_entries(
    popover<_Content, _Source, _Features, _Policy>&                    _p,
    typename popover<_Content, _Source, _Features, _Policy>::size_type _n
)
{
    _p.min_visible_entries = _n;

    return;
}

// popover_set_size_hint
//   function: requests a specific rendered width x height.  Values
// of 0 mean "auto from content".
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_set_size_hint(
    popover<_Content, _Source, _Features, _Policy>&                    _p,
    typename popover<_Content, _Source, _Features, _Policy>::size_type _width,
    typename popover<_Content, _Source, _Features, _Policy>::size_type _height
)
{
    _p.desired_width  = _width;
    _p.desired_height = _height;

    return;
}


/*****************************************************************************/

// ---- 7.5  refresh -------------------------------------------------------

// popover_refresh
//   function: invokes the consumer-installed on_refresh hook against
// content.  Returns true if the hook fired, false if none is
// installed.  This is the generic "regenerate content" path,
// orthogonal to the source binding below.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
bool
popover_refresh(
    popover<_Content, _Source, _Features, _Policy>& _p
)
{
    if (!_p.on_refresh)
    {
        return false;
    }

    _p.on_refresh(_p.content);

    return true;
}

// popover_refresh_from_source
//   function: asks the bound suggest_adapter for suggestions against
// `_input`, then drains the results into `content` through the
// consumer-supplied sink function.  Only compiles when a concrete
// `_Source` was bound (i.e. _Source != void).
//
//   If no sink is installed, the popover attempts direct assignment
// — works when `_Content` is assignable from the adapter's
// `container_type&&` (e.g. `_Content == std::vector<suggestion>`).
// Otherwise, the sink is required and absence returns false.
//
//   Returns true if results flowed into content, false if the
// adapter pointer is null or no suitable drain path exists.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy,
          std::enable_if_t<
              !std::is_same<_Source, void>::value,
              int> = 0>
bool
popover_refresh_from_source(
    popover<_Content, _Source, _Features, _Policy>& _p,
    const typename _Source::input_type&             _input
)
{
    if (!_p.source)
    {
        return false;
    }

    auto results = _p.source->suggest(_input);

    // prefer the sink if installed
    if (_p.on_source_result)
    {
        _p.on_source_result(_p.content, std::move(results));

        return true;
    }

    // fall back to direct assignment when the types line up
    if constexpr (std::is_assignable_v<
                      _Content&,
                      typename _Source::container_type&&>)
    {
        _p.content = std::move(results);

        return true;
    }
    else
    {
        // no viable drain path — consumer must install a sink
        return false;
    }
}

// popover_bind_source
//   function: binds a suggest_adapter derivative to the popover.
// The popover does not take ownership — the adapter must outlive
// the popover or be rebound / cleared before destruction.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy,
          std::enable_if_t<
              !std::is_same<_Source, void>::value,
              int> = 0>
void
popover_bind_source(
    popover<_Content, _Source, _Features, _Policy>& _p,
    _Source*                                         _src
)
{
    _p.source = _src;

    return;
}


/*****************************************************************************/

// ---- 7.6  state toggles -------------------------------------------------

// popover_set_loading
//   function: marks the popover as awaiting async results.  The
// renderer is expected to surface loading_message while this
// flag is set.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
void
popover_set_loading(
    popover<_Content, _Source, _Features, _Policy>& _p,
    bool                                             _flag
)
{
    _p.is_loading = _flag;

    return;
}

// popover_notify_selected
//   function: called by the consumer when a selection has fired
// inside `content`.  If close_on_select is set, triggers
// popover_close (honoring any veto).  Returns true if the
// popover closed as a result.
template <typename      _Content,
          typename      _Source,
          std::uint32_t _Features,
          typename      _Policy>
bool
popover_notify_selected(
    popover<_Content, _Source, _Features, _Policy>& _p
)
{
    if (!_p.close_on_select)
    {
        return false;
    }

    return popover_close(_p);
}


/*****************************************************************************/

// ===============================================================================
//  8  TRAITS
// ===============================================================================

namespace popover_traits {
namespace detail {

    // -- popover-specific detectors -----------------------------------

    template <typename, typename = void>
    struct has_content_member : std::false_type
    {};

    template <typename _Type>
    struct has_content_member<_Type, std::void_t<
        decltype(std::declval<_Type>().content)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_direction_member : std::false_type
    {};

    template <typename _Type>
    struct has_direction_member<_Type, std::void_t<
        decltype(std::declval<_Type>().direction)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_alignment_member : std::false_type
    {};

    template <typename _Type>
    struct has_alignment_member<_Type, std::void_t<
        decltype(std::declval<_Type>().alignment)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_anchor_id_member : std::false_type
    {};

    template <typename _Type>
    struct has_anchor_id_member<_Type, std::void_t<
        decltype(std::declval<_Type>().anchor_id)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_source_member : std::false_type
    {};

    template <typename _Type>
    struct has_source_member<_Type, std::void_t<
        decltype(std::declval<_Type>().source)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_visible_member : std::false_type
    {};

    template <typename _Type>
    struct has_visible_member<_Type, std::void_t<
        decltype(std::declval<_Type>().visible)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_active_member : std::false_type
    {};

    template <typename _Type>
    struct has_active_member<_Type, std::void_t<
        decltype(std::declval<_Type>().active)
    >> : std::true_type
    {};

}   // namespace detail


// -- popover-specific value aliases ---------------------------------------

template <typename _Type>
inline constexpr bool has_content_v =
    detail::has_content_member<_Type>::value;

template <typename _Type>
inline constexpr bool has_direction_v =
    detail::has_direction_member<_Type>::value;

template <typename _Type>
inline constexpr bool has_alignment_v =
    detail::has_alignment_member<_Type>::value;

template <typename _Type>
inline constexpr bool has_anchor_id_v =
    detail::has_anchor_id_member<_Type>::value;

template <typename _Type>
inline constexpr bool has_source_v =
    detail::has_source_member<_Type>::value;


// -- composite traits -----------------------------------------------------

// is_popover
//   trait: structural detection for any popover-like type — has
// content + direction + visible + active.  Matches popover<>
// regardless of its template parameters and any future types
// that satisfy the same shape.
template <typename _Type>
struct is_popover : std::conjunction<
    detail::has_content_member<_Type>,
    detail::has_direction_member<_Type>,
    detail::has_visible_member<_Type>,
    detail::has_active_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_popover_v =
    is_popover<_Type>::value;

// is_source_bound_popover
//   trait: a popover that also carries a `source` pointer (i.e. was
// instantiated with a concrete _Source).
template <typename _Type>
struct is_source_bound_popover : std::conjunction<
    is_popover<_Type>,
    detail::has_source_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_source_bound_popover_v =
    is_source_bound_popover<_Type>::value;


}   // namespace popover_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_POPOVER_
