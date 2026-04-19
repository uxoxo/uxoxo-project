/*******************************************************************************
* uxoxo [component]                                                    frame.hpp
*
* Frame component:
*   A framework-agnostic, pure-data bordered container template.  A
* frame carries a title that can sit on any of the four edges and align
* anywhere along that edge, and a border that may be drawn unbroken or
* notched (broken) around the title text.  Unlike `tab_control` or
* `stacked_view` a frame has no notion of "entries" — it is a single
* chrome element.  It owns only its own metadata (title, placement,
* alignment, border visibility, emphasis); the content it wraps is
* managed externally, exactly as a `stacked_view` owns page metadata
* but not page content.
*
*   The component is a single template level:
*
*     frame<_Feat>
*       The chrome.  Carries a title string, title position along one of
*       the four edges, alignment along that edge, border visibility,
*       and enabled/visible state.  Opts in to semantic emphasis,
*       hover tooltips, collapsibility, renderer-set hover state, and
*       an opaque user identifier via _Feat.
*
*   The template prescribes NOTHING about rendering.  A renderer decides
* whether to draw stacked vertical titles, rotated titles, or some
* rasterised alternative; whether to notch the border at all; and how
* to map emphasis to concrete colours.  Traits in frame_traits::
* advertise what a particular frame instance carries so renderers and
* generic code can compose behaviour with if constexpr.
*
*   Feature composition follows the same EBO-mixin bitfield pattern
* used throughout the uxoxo component layer.  Disabled features cost
* zero bytes thanks to empty-base-optimisation on empty `_Enable = false`
* specialisations.
*
* Contents:
*   1.   Feature flags (frame_feat)
*   2.   Enums (frame_title_position, frame_title_align, frame_emphasis)
*   3.   EBO mixins (namespace frame_mixin)
*   4.   frame struct
*   5.   Free functions (fr_*)
*   6.   Traits (namespace frame_traits)
*
*
* path:      /inc/uxoxo/templates/component/frame/frame.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_FRAME_
#define  UXOXO_COMPONENT_FRAME_ 1

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


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  FEATURE FLAGS
// ===============================================================================
//   Frame-level features.  These control what optional data each
// frame carries.  Combine with bitwise OR:
//
//   frame<frf_emphasis | frf_collapsible>

// frame_feat
//   enum: per-frame feature bitflags.  Each flag toggles an EBO mixin
// on or off.  Unset flags cost zero bytes.
enum frame_feat : unsigned
{
    frf_none         = 0,
    frf_emphasis     = 1u << 0,     // semantic emphasis / severity tag
    frf_tooltip      = 1u << 1,     // hover tooltip string
    frf_collapsible  = 1u << 2,     // collapsed state + toggle callback
    frf_hover_state  = 1u << 3,     // renderer-set hovered flag
    frf_user_id      = 1u << 4,     // opaque user identifier

    frf_standard     = frf_emphasis | frf_tooltip,
    frf_all          = frf_emphasis    | frf_tooltip
                     | frf_collapsible | frf_hover_state
                     | frf_user_id
};

constexpr unsigned
operator|(frame_feat _a,
          frame_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool
has_frf(unsigned   _f,
        frame_feat _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


// ===============================================================================
//  2.  ENUMS
// ===============================================================================

// frame_title_position
//   enum: which edge of a frame the title sits on.  The renderer is
// responsible for choosing an appropriate glyph orientation for the
// vertical edges (stacked characters, rotated text, etc.); the
// template prescribes only where the title lives.
enum class frame_title_position : std::uint8_t
{
    top,
    bottom,
    left,
    right
};

// frame_title_align
//   enum: alignment of the title along the edge it sits on.  Uses the
// axis-agnostic start / center / end triad so the same enum reads
// correctly on any edge:
//   top / bottom:  start = flush-left,  end = flush-right.
//   left / right:  start = flush-top,   end = flush-bottom.
enum class frame_title_align : std::uint8_t
{
    start,
    center,
    end
};

// frame_emphasis
//   enum: semantic severity / emphasis tag for the frame's border.
// The renderer maps these to concrete colours.  `normal` should
// resolve to the ambient theme border colour so unadorned frames
// blend into surrounding chrome.
enum class frame_emphasis : std::uint8_t
{
    normal,
    primary,
    secondary,
    success,
    warning,
    danger,
    info,
    muted
};


// ===============================================================================
//  3.  EBO MIXINS
// ===============================================================================
//   Each mixin is a struct template parameterised on a bool.  The
// `false` specialisation is empty; the `true` specialisation holds
// per-feature data.  EBO guarantees zero storage overhead when the
// feature is disabled.

namespace frame_mixin {

    // -- emphasis ---------------------------------------------------------
    template <bool _Enable>
    struct emphasis_data
    {};

    template <>
    struct emphasis_data<true>
    {
        frame_emphasis emph = frame_emphasis::normal;
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

    // -- collapsible ------------------------------------------------------
    //   `collapsed` is the current visual state.  `user_collapsible`
    // gates whether the user can toggle it via chrome interaction —
    // programmatic collapse / expand via fr_collapse / fr_expand still
    // works regardless.
    template <bool _Enable>
    struct collapse_data
    {};

    template <>
    struct collapse_data<true>
    {
        using toggle_fn = std::function<void(bool)>;

        bool      collapsed        = false;
        bool      user_collapsible = true;
        toggle_fn on_toggle;             // invoked with the new collapsed state
    };

    // -- hover state ------------------------------------------------------
    //   Renderer-set.  The frame component never mutates this itself;
    // the integrating ImGui / other renderer writes the flag each
    // frame based on cursor position.
    template <bool _Enable>
    struct hover_data
    {};

    template <>
    struct hover_data<true>
    {
        bool hovered = false;
    };

    // -- user id ----------------------------------------------------------
    //   Opaque integer identifier for associating a frame with content
    // in the integrating layer.  The framework does not interpret this
    // value.
    template <bool _Enable>
    struct user_id_data
    {};

    template <>
    struct user_id_data<true>
    {
        std::size_t user_id = 0;
    };

}   // namespace frame_mixin


// ===============================================================================
//  4.  FRAME
// ===============================================================================
//   _Feat  bitwise OR of frame_feat flags for optional features.
//
//   Title alignment semantics per edge:
//     top    / bottom: start = flush-left, center, end = flush-right.
//     left   / right:  start = flush-top,  center, end = flush-bottom.
//
//   Edge cases:
//     - title empty         → plain bordered box, unbroken border line.
//     - show_border false   → bare title, no surrounding line.
//     - both                → invisible layout grouping (semantic only).

// frame
//   struct: bordered container with a title that can sit on any of the
// four edges and align anywhere along that edge.  Owns chrome
// metadata only — content is managed externally by the integrating
// layer.
template <unsigned _Feat = frf_none>
struct frame
    : frame_mixin::emphasis_data <has_frf(_Feat, frf_emphasis)>
    , frame_mixin::tooltip_data  <has_frf(_Feat, frf_tooltip)>
    , frame_mixin::collapse_data <has_frf(_Feat, frf_collapsible)>
    , frame_mixin::hover_data    <has_frf(_Feat, frf_hover_state)>
    , frame_mixin::user_id_data  <has_frf(_Feat, frf_user_id)>
{
    using self_type = frame<_Feat>;
    using size_type = std::size_t;

    static constexpr unsigned features = _Feat;

    // compile-time feature queries
    static constexpr bool has_emphasis    = has_frf(_Feat, frf_emphasis);
    static constexpr bool has_tooltip     = has_frf(_Feat, frf_tooltip);
    static constexpr bool is_collapsible  = has_frf(_Feat, frf_collapsible);
    static constexpr bool has_hover_state = has_frf(_Feat, frf_hover_state);
    static constexpr bool has_user_id     = has_frf(_Feat, frf_user_id);

    // component identity
    //   A frame has no input surface of its own — it is pure chrome
    // around externally-managed content.  Collapsibility adds a
    // single toggle affordance but the frame as a whole still does
    // not own keyboard focus; the toggle, if present, is a sub-
    // element owned by the renderer.
    static constexpr bool focusable = false;

    // -- core chrome ------------------------------------------------------
    std::string          title;
    frame_title_position title_pos   = frame_title_position::top;
    frame_title_align    title_align = frame_title_align::start;
    bool                 show_border = true;

    // -- core state -------------------------------------------------------
    bool enabled = true;
    bool visible = true;

    // -- construction -----------------------------------------------------
    frame() = default;

    explicit frame(
            std::string _title
        )
            : title(std::move(_title))
        {}

    frame(
            std::string          _title,
            frame_title_position _pos
        )
            : title(std::move(_title)),
              title_pos(_pos)
        {}

    frame(
            std::string          _title,
            frame_title_position _pos,
            frame_title_align    _align
        )
            : title(std::move(_title)),
              title_pos(_pos),
              title_align(_align)
        {}

    // -- queries ----------------------------------------------------------
    [[nodiscard]] bool
    has_title() const noexcept
    {
        return !title.empty();
    }

    [[nodiscard]] bool
    is_horizontal_title() const noexcept
    {
        return ( (title_pos == frame_title_position::top) ||
                 (title_pos == frame_title_position::bottom) );
    }

    [[nodiscard]] bool
    is_vertical_title() const noexcept
    {
        return ( (title_pos == frame_title_position::left) ||
                 (title_pos == frame_title_position::right) );
    }

    // -- compositional forwarding -----------------------------------------
    //   A frame has no sub-components to visit.  The no-op overload
    // exists so generic component-tree algorithms can recurse uniformly.
    template <typename _Fn>
    void
    visit_components(_Fn&& /*_fn*/)
    {
        return;
    }
};


// ===============================================================================
//  5.  FREE FUNCTIONS
// ===============================================================================
//   Domain-specific frame operations.  Shared operations (enable,
// disable, show, hide) work on a frame directly via the ADL functions
// in component_common.hpp since frame has enabled and visible members.

// -- title ----------------------------------------------------------------

/*
fr_set_title
  Sets the frame's title string.  An empty string is meaningful — it
selects the "unbroken border" rendering path in the renderer.

Parameter(s):
  _fr:    the frame to mutate.
  _title: the new title.
Return:
  none.
*/
template <unsigned _F>
void
fr_set_title(frame<_F>&  _fr,
             std::string _title)
{
    _fr.title = std::move(_title);

    return;
}

/*
fr_set_title_position
  Sets which edge of the frame the title sits on.

Parameter(s):
  _fr:  the frame to mutate.
  _pos: the new edge.
Return:
  none.
*/
template <unsigned _F>
void
fr_set_title_position(frame<_F>&           _fr,
                      frame_title_position _pos) noexcept
{
    _fr.title_pos = _pos;

    return;
}

/*
fr_set_title_align
  Sets the title's alignment along its edge.

Parameter(s):
  _fr:    the frame to mutate.
  _align: the new alignment.
Return:
  none.
*/
template <unsigned _F>
void
fr_set_title_align(frame<_F>&        _fr,
                   frame_title_align _align) noexcept
{
    _fr.title_align = _align;

    return;
}


// -- border ---------------------------------------------------------------

/*
fr_set_border
  Toggles whether the frame's border is drawn.  When disabled the
frame degrades to a bare title; when both border and title are
disabled the frame is an invisible layout grouping.

Parameter(s):
  _fr:   the frame to mutate.
  _show: true to draw the border, false to suppress.
Return:
  none.
*/
template <unsigned _F>
void
fr_set_border(frame<_F>& _fr,
              bool       _show) noexcept
{
    _fr.show_border = _show;

    return;
}


// -- emphasis (frf_emphasis) ---------------------------------------------

/*
fr_set_emphasis
  Sets the frame's semantic emphasis.  The renderer maps this to a
concrete border colour.

Parameter(s):
  _fr:   the frame to mutate.
  _emph: the new emphasis.
Return:
  none.
*/
template <unsigned _F>
void
fr_set_emphasis(frame<_F>&     _fr,
                frame_emphasis _emph) noexcept
{
    static_assert(has_frf(_F, frf_emphasis),
                  "requires frf_emphasis");

    _fr.emph = _emph;

    return;
}


// -- tooltip (frf_tooltip) -----------------------------------------------

/*
fr_set_tooltip
  Sets the hover tooltip shown over the frame's chrome.

Parameter(s):
  _fr:      the frame to mutate.
  _tooltip: the tooltip text; empty to clear.
Return:
  none.
*/
template <unsigned _F>
void
fr_set_tooltip(frame<_F>&  _fr,
               std::string _tooltip)
{
    static_assert(has_frf(_F, frf_tooltip),
                  "requires frf_tooltip");

    _fr.tooltip = std::move(_tooltip);

    return;
}


// -- collapsible (frf_collapsible) ---------------------------------------

/*
fr_collapse
  Programmatically collapses the frame.  Invokes on_toggle if set.
Bypasses user_collapsible — the gating only applies to user
interaction, not programmatic control.

Parameter(s):
  _fr: the frame to collapse.
Return:
  none.
*/
template <unsigned _F>
void
fr_collapse(frame<_F>& _fr)
{
    static_assert(has_frf(_F, frf_collapsible),
                  "requires frf_collapsible");

    if (_fr.collapsed)
    {
        return;
    }

    _fr.collapsed = true;

    if (_fr.on_toggle)
    {
        _fr.on_toggle(true);
    }

    return;
}

/*
fr_expand
  Programmatically expands the frame.  Invokes on_toggle if set.

Parameter(s):
  _fr: the frame to expand.
Return:
  none.
*/
template <unsigned _F>
void
fr_expand(frame<_F>& _fr)
{
    static_assert(has_frf(_F, frf_collapsible),
                  "requires frf_collapsible");

    if (!_fr.collapsed)
    {
        return;
    }

    _fr.collapsed = false;

    if (_fr.on_toggle)
    {
        _fr.on_toggle(false);
    }

    return;
}

/*
fr_toggle_collapsed
  Toggles the frame's collapsed state.  Respects user_collapsible —
if the caller is a user-interaction path the frame's own gating is
honoured; pass _force=true to bypass it (e.g. for keyboard shortcuts
that should always work).

Parameter(s):
  _fr:    the frame to toggle.
  _force: true to ignore user_collapsible; false to honour it.
Return:
  true if the state changed; false if the toggle was gated out.
*/
template <unsigned _F>
bool
fr_toggle_collapsed(frame<_F>& _fr,
                    bool       _force = false)
{
    static_assert(has_frf(_F, frf_collapsible),
                  "requires frf_collapsible");

    // respect the user-collapsible gate unless forced
    if ( (!_force) &&
         (!_fr.user_collapsible) )
    {
        return false;
    }

    _fr.collapsed = !_fr.collapsed;

    if (_fr.on_toggle)
    {
        _fr.on_toggle(_fr.collapsed);
    }

    return true;
}

/*
fr_is_collapsed
  Queries the frame's collapsed state.

Parameter(s):
  _fr: the frame to query.
Return:
  true if the frame is collapsed, false otherwise.
*/
template <unsigned _F>
[[nodiscard]] bool
fr_is_collapsed(const frame<_F>& _fr) noexcept
{
    static_assert(has_frf(_F, frf_collapsible),
                  "requires frf_collapsible");

    return _fr.collapsed;
}


// -- hover state (frf_hover_state) ---------------------------------------

/*
fr_set_hovered
  Sets the frame's renderer-tracked hovered flag.  Intended for the
integrating renderer to call each frame; application code should
prefer reading the flag over writing it.

Parameter(s):
  _fr:      the frame to mutate.
  _hovered: the new hovered state.
Return:
  none.
*/
template <unsigned _F>
void
fr_set_hovered(frame<_F>& _fr,
               bool       _hovered) noexcept
{
    static_assert(has_frf(_F, frf_hover_state),
                  "requires frf_hover_state");

    _fr.hovered = _hovered;

    return;
}

/*
fr_is_hovered
  Queries the frame's hovered state.

Parameter(s):
  _fr: the frame to query.
Return:
  true if the renderer reported the frame as hovered on the last
  frame it was drawn.
*/
template <unsigned _F>
[[nodiscard]] bool
fr_is_hovered(const frame<_F>& _fr) noexcept
{
    static_assert(has_frf(_F, frf_hover_state),
                  "requires frf_hover_state");

    return _fr.hovered;
}


// ===============================================================================
//  6.  TRAITS
// ===============================================================================
//   SFINAE detectors following the stacked_view / tab_control traits
// pattern.  Renderers and generic code query these to discover what
// a frame instance carries, without hard-coding specific feature
// combinations.

namespace frame_traits {

NS_INTERNAL

    // -- member detectors -------------------------------------------------

    template <typename, typename = void>
    struct has_title_member : std::false_type
    {};

    template <typename _Type>
    struct has_title_member<_Type, std::void_t<
        decltype(std::declval<_Type>().title)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_title_pos_member : std::false_type
    {};

    template <typename _Type>
    struct has_title_pos_member<_Type, std::void_t<
        decltype(std::declval<_Type>().title_pos)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_title_align_member : std::false_type
    {};

    template <typename _Type>
    struct has_title_align_member<_Type, std::void_t<
        decltype(std::declval<_Type>().title_align)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_show_border_member : std::false_type
    {};

    template <typename _Type>
    struct has_show_border_member<_Type, std::void_t<
        decltype(std::declval<_Type>().show_border)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_emph_member : std::false_type
    {};

    template <typename _Type>
    struct has_emph_member<_Type, std::void_t<
        decltype(std::declval<_Type>().emph)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_tooltip_member : std::false_type
    {};

    template <typename _Type>
    struct has_tooltip_member<_Type, std::void_t<
        decltype(std::declval<_Type>().tooltip)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_collapsed_member : std::false_type
    {};

    template <typename _Type>
    struct has_collapsed_member<_Type, std::void_t<
        decltype(std::declval<_Type>().collapsed)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_on_toggle_member : std::false_type
    {};

    template <typename _Type>
    struct has_on_toggle_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_toggle)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_hovered_member : std::false_type
    {};

    template <typename _Type>
    struct has_hovered_member<_Type, std::void_t<
        decltype(std::declval<_Type>().hovered)
    >> : std::true_type
    {};

    template <typename, typename = void>
    struct has_user_id_member : std::false_type
    {};

    template <typename _Type>
    struct has_user_id_member<_Type, std::void_t<
        decltype(std::declval<_Type>().user_id)
    >> : std::true_type
    {};

NS_END  // internal


// -- value aliases --------------------------------------------------------
template <typename _Type>
inline constexpr bool has_title_v =
    internal::has_title_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_title_pos_v =
    internal::has_title_pos_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_title_align_v =
    internal::has_title_align_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_show_border_v =
    internal::has_show_border_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_emph_v =
    internal::has_emph_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_tooltip_v =
    internal::has_tooltip_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_collapsed_v =
    internal::has_collapsed_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_on_toggle_v =
    internal::has_on_toggle_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_hovered_v =
    internal::has_hovered_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_user_id_v =
    internal::has_user_id_member<_Type>::value;


// -- composite traits -----------------------------------------------------

// is_frame
//   trait: structurally identifies a frame — has title, title_pos,
// title_align, show_border, enabled, visible, and the focusable flag.
template <typename _Type>
struct is_frame : std::conjunction<
    internal::has_title_member<_Type>,
    internal::has_title_pos_member<_Type>,
    internal::has_title_align_member<_Type>,
    internal::has_show_border_member<_Type>,
    component::internal::has_enabled_member<_Type>,
    component::internal::has_visible_member<_Type>,
    component::internal::has_focusable_flag<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_frame_v = is_frame<_Type>::value;

// is_emphasized_frame
//   trait: frame carrying an emphasis member.
template <typename _Type>
struct is_emphasized_frame : std::conjunction<
    is_frame<_Type>,
    internal::has_emph_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_emphasized_frame_v =
    is_emphasized_frame<_Type>::value;

// is_tooltip_frame
//   trait: frame carrying a tooltip member.
template <typename _Type>
struct is_tooltip_frame : std::conjunction<
    is_frame<_Type>,
    internal::has_tooltip_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_tooltip_frame_v =
    is_tooltip_frame<_Type>::value;

// is_collapsible_frame
//   trait: frame carrying a collapsed member.
template <typename _Type>
struct is_collapsible_frame : std::conjunction<
    is_frame<_Type>,
    internal::has_collapsed_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_collapsible_frame_v =
    is_collapsible_frame<_Type>::value;

// is_hoverable_frame
//   trait: frame carrying a renderer-set hovered member.
template <typename _Type>
struct is_hoverable_frame : std::conjunction<
    is_frame<_Type>,
    internal::has_hovered_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_hoverable_frame_v =
    is_hoverable_frame<_Type>::value;


}   // namespace frame_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_FRAME_
