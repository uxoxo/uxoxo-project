/*******************************************************************************
* uxoxo [component]                                             stacked_view.hpp
*
* Stacked view component:
*   A framework-agnostic, pure-data stacked-view template.  Holds an
* ordered collection of pages and renders exactly one of them at a
* time — selection is the entire visible state.  Unlike tab_control,
* a stacked_view has NO visible chrome of its own: no tabs, no
* breadcrumbs, no scroll arrows.  Navigation is driven externally,
* by whatever UI the application cares to attach (buttons, menus,
* keyboard shortcuts, state machines, etc.).
*
*   The component is split into two template levels:
*
*     stacked_page<_PageFeat, _Icon>
*       A single page.  Minimal by default (enabled + visible +
*       user_id), with zero-cost optional features via EBO mixins:
*       string id (for named navigation), title, icon, dirty flag,
*       lazy-load state, and visit-count statistics.
*
*     stacked_view<_PageFeat, _CtrlFeat, _Icon>
*       The page container.  Owns a vector of stacked_page, manages
*       the selected index, bounds policy (clamp vs wrap), and
*       enabled/visible state.  Opts in to history tracking,
*       id-indexed lookup, navigation locking, transition animation
*       state, and will-leave veto callbacks via _CtrlFeat.
*
*   The template prescribes NOTHING about rendering.  The selected
* page is whatever index the navigation driver has set; the
* integrating renderer decides what to draw based on that index.
* A renderer can query per-page data via the stacked_traits::
* detectors and compose behaviour with if constexpr.
*
*   Feature composition follows the same EBO-mixin bitfield pattern
* used by tab_control.  Disabled features cost zero bytes thanks
* to empty-base-optimization on empty `_Enable = false`
* specializations.
*
* Contents:
*   1.   Page feature flags (sp_feat)
*   2.   Control feature flags (sv_feat)
*   3.   Enums (stacked_transition, stacked_bounds_policy)
*   4.   Page EBO mixins (namespace page_mixin)
*   5.   stacked_page struct
*   6.   Control EBO mixins (namespace stacked_view_mixin)
*   7.   stacked_view struct
*   8.   Page free functions (sp_*)
*   9.   Control free functions (sv_*)
*   10.  Traits (namespace stacked_traits)
*
*
* path:      /inc/uxoxo/templates/component/stacked/stacked_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.17
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_STACKED_VIEW_
#define  UXOXO_COMPONENT_STACKED_VIEW_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
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
//  1.  PAGE FEATURE FLAGS
// ===============================================================================
//   Per-page features.  These control what optional data each
// stacked_page carries.  Combine with bitwise OR:
//
//   stacked_page<spf_id | spf_title | spf_dirty>

enum sp_feat : unsigned
{
    spf_none         = 0,
    spf_id           = 1u << 0,     // per-page string id for named lookup
    spf_title        = 1u << 1,     // per-page title (external display)
    spf_icon         = 1u << 2,     // per-page icon
    spf_dirty        = 1u << 3,     // per-page dirty flag (unsaved changes)
    spf_lazy         = 1u << 4,     // per-page lazy-load state
    spf_visit_stats  = 1u << 5,     // per-page visit count + timestamp
    spf_user_tag     = 1u << 6,     // per-page arbitrary user string tag

    spf_standard     = spf_id | spf_title,
    spf_all          = spf_id         | spf_title
                     | spf_icon       | spf_dirty
                     | spf_lazy       | spf_visit_stats
                     | spf_user_tag
};

constexpr unsigned
operator|(sp_feat _a,
          sp_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool
has_spf(unsigned _f,
        sp_feat  _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


// ===============================================================================
//  2.  CONTROL FEATURE FLAGS
// ===============================================================================
//   Control-level features.  These govern what optional data and
// behaviour the stacked_view itself carries.

enum sv_feat : unsigned
{
    svf_none         = 0,
    svf_history      = 1u << 0,     // back/forward navigation stack
    svf_indexed      = 1u << 1,     // id → index lookup map (requires spf_id)
    svf_lock         = 1u << 2,     // navigation lock / unlock state
    svf_animated     = 1u << 3,     // transition state + timing
    svf_veto_guard   = 1u << 4,     // on_will_leave callback for veto

    svf_standard     = svf_history | svf_indexed,
    svf_all          = svf_history    | svf_indexed
                     | svf_lock       | svf_animated
                     | svf_veto_guard
};

constexpr unsigned
operator|(sv_feat _a,
          sv_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool
has_svf(unsigned _f,
        sv_feat  _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


// ===============================================================================
//  3.  ENUMS
// ===============================================================================

// stacked_transition
//   enum: requested transition animation between pages.  The renderer
// interprets these as hints — `none` is always valid and any renderer
// may treat an unsupported transition as `none`.
enum class stacked_transition : std::uint8_t
{
    none,
    slide_horizontal,       // left/right slide
    slide_vertical,         // up/down slide
    fade,                   // cross-fade
    push,                   // new page pushes old off (nav-stack style)
    cover                   // new page covers old in place
};

// stacked_bounds_policy
//   enum: behaviour of next/prev navigation at the bounds of the page
// list.  `clamp` stops; `wrap` rolls around to the far end.
enum class stacked_bounds_policy : std::uint8_t
{
    clamp,
    wrap
};


// ===============================================================================
//  4.  PAGE EBO MIXINS
// ===============================================================================
//   Each mixin is a struct template parameterised on a bool.  The
// `false` specialisation is empty; the `true` specialisation holds
// per-page data.  EBO guarantees zero storage overhead when disabled.

namespace page_mixin {

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

    // -- icon -------------------------------------------------------------
    template <bool     _Enable,
              typename _Icon = int>
    struct icon_data
    {};

    template <typename _Icon>
    struct icon_data<true, _Icon>
    {
        _Icon icon {};
    };

    // -- dirty ------------------------------------------------------------
    template <bool _Enable>
    struct dirty_data
    {};

    template <>
    struct dirty_data<true>
    {
        bool dirty = false;
    };

    // -- lazy -------------------------------------------------------------
    //   Single boolean for "has this page been constructed yet".
    // Callers flip `loaded` to true on first navigation; renderers use
    // it to decide whether to build or reuse the page's content.
    template <bool _Enable>
    struct lazy_data
    {};

    template <>
    struct lazy_data<true>
    {
        bool loaded = false;
    };

    // -- visit stats ------------------------------------------------------
    //   Simple per-page counters.  Timestamps are std::int64_t so the
    // framework is clock-agnostic — callers supply milliseconds,
    // seconds, tick counts, whatever suits the integrating layer.
    template <bool _Enable>
    struct visit_stats_data
    {};

    template <>
    struct visit_stats_data<true>
    {
        std::size_t   visit_count     = 0;
        std::int64_t  last_visit_time = 0;
    };

    // -- user tag ---------------------------------------------------------
    //   Arbitrary free-form user string, distinct from spf_id (which
    // must be unique for svf_indexed to work) and from the core
    // user_id size_t.  Use for categorisation, routing keys, etc.
    template <bool _Enable>
    struct user_tag_data
    {};

    template <>
    struct user_tag_data<true>
    {
        std::string user_tag;
    };

}   // namespace page_mixin


// ===============================================================================
//  5.  STACKED PAGE
// ===============================================================================
//   A single page.  Pure data aggregate with zero-cost optional
// features.  Unlike tab_entry, a stacked_page has no `label` in its
// core state — a page is rarely displayed on its own, and when it is,
// the external navigation driver typically carries the displayed text.
// Page titles are available via the spf_title feature for the cases
// that do want them.

template <unsigned _Feat = spf_none,
          typename _Icon = int>
struct stacked_page
    : page_mixin::id_data          <has_spf(_Feat, spf_id)>
    , page_mixin::title_data       <has_spf(_Feat, spf_title)>
    , page_mixin::icon_data        <has_spf(_Feat, spf_icon), _Icon>
    , page_mixin::dirty_data       <has_spf(_Feat, spf_dirty)>
    , page_mixin::lazy_data        <has_spf(_Feat, spf_lazy)>
    , page_mixin::visit_stats_data <has_spf(_Feat, spf_visit_stats)>
    , page_mixin::user_tag_data    <has_spf(_Feat, spf_user_tag)>
{
    using self_type = stacked_page<_Feat, _Icon>;
    using icon_type = _Icon;

    static constexpr unsigned features = _Feat;

    // compile-time feature queries
    static constexpr bool has_id          = has_spf(_Feat, spf_id);
    static constexpr bool has_title       = has_spf(_Feat, spf_title);
    static constexpr bool has_icon        = has_spf(_Feat, spf_icon);
    static constexpr bool has_dirty       = has_spf(_Feat, spf_dirty);
    static constexpr bool has_lazy        = has_spf(_Feat, spf_lazy);
    static constexpr bool has_visit_stats = has_spf(_Feat, spf_visit_stats);
    static constexpr bool has_user_tag    = has_spf(_Feat, spf_user_tag);

    // -- core state -------------------------------------------------------
    bool        enabled = true;    // navigation to this page is permitted
    bool        visible = true;    // page appears in next/prev cycling

    // -- user data --------------------------------------------------------
    //   opaque integer identifier for associating pages with content in
    // the integrating layer.  The framework does not interpret this
    // value.
    std::size_t user_id = 0;

    // -- construction -----------------------------------------------------
    stacked_page() = default;

    explicit stacked_page(
            std::size_t _uid
        ) noexcept
            : user_id(_uid)
        {}
};


// ===============================================================================
//  6.  CONTROL EBO MIXINS
// ===============================================================================

namespace stacked_view_mixin {

    // -- history ----------------------------------------------------------
    //   Back/forward navigation stacks.  `max_history == 0` means
    // unbounded.
    template <bool _Enable>
    struct history_data
    {};

    template <>
    struct history_data<true>
    {
        std::vector<std::size_t> back_stack;
        std::vector<std::size_t> forward_stack;
        std::size_t              max_history = 0;
    };

    // -- indexed lookup ---------------------------------------------------
    //   id → index map for O(1) named navigation.  Kept in sync by
    // sv_add_page / sv_insert_page / sv_remove_page and rebuilt on
    // demand via sv_rebuild_index.
    template <bool _Enable>
    struct index_data
    {};

    template <>
    struct index_data<true>
    {
        std::unordered_map<std::string, std::size_t> id_index;
    };

    // -- lock -------------------------------------------------------------
    template <bool _Enable>
    struct lock_data
    {};

    template <>
    struct lock_data<true>
    {
        bool        locked = false;
        std::string lock_reason;
    };

    // -- animation --------------------------------------------------------
    //   Transition state carried for the renderer to animate over
    // frames.  `anim_progress` runs 0 → 1 across `anim_duration`
    // seconds; the renderer is responsible for advancing it.
    template <bool _Enable>
    struct anim_data
    {};

    template <>
    struct anim_data<true>
    {
        stacked_transition transition    = stacked_transition::none;
        float              anim_progress = 0.0f;
        float              anim_duration = 0.15f;
        std::size_t        anim_from     = 0;
        std::size_t        anim_to       = 0;
        bool               anim_active   = false;
    };

    // -- veto guard -------------------------------------------------------
    //   Optional callback that can veto a navigation.  Return true to
    // permit the transition, false to block it.  Typical use: prompt
    // the user to save unsaved changes on the outgoing page.
    template <bool _Enable>
    struct veto_data
    {};

    template <>
    struct veto_data<true>
    {
        using will_leave_fn =
            std::function<bool(std::size_t, std::size_t)>;

        will_leave_fn on_will_leave;
    };

}   // namespace stacked_view_mixin


// ===============================================================================
//  7.  STACKED VIEW
// ===============================================================================
//   _PageFeat   bitwise OR of sp_feat flags for per-page features.
//   _CtrlFeat   bitwise OR of sv_feat flags for control features.
//   _Icon       icon storage type.

template <unsigned _PageFeat = spf_none,
          unsigned _CtrlFeat = svf_none,
          typename _Icon     = int>
struct stacked_view
    : stacked_view_mixin::history_data <has_svf(_CtrlFeat, svf_history)>
    , stacked_view_mixin::index_data   <has_svf(_CtrlFeat, svf_indexed)>
    , stacked_view_mixin::lock_data    <has_svf(_CtrlFeat, svf_lock)>
    , stacked_view_mixin::anim_data    <has_svf(_CtrlFeat, svf_animated)>
    , stacked_view_mixin::veto_data    <has_svf(_CtrlFeat, svf_veto_guard)>
{
    using page_type = stacked_page<_PageFeat, _Icon>;
    using icon_type = _Icon;
    using size_type = std::size_t;

    static constexpr unsigned page_features = _PageFeat;
    static constexpr unsigned features      = _CtrlFeat;

    // compile-time feature queries (page level)
    static constexpr bool pages_have_id     = has_spf(_PageFeat, spf_id);
    static constexpr bool pages_have_title  = has_spf(_PageFeat, spf_title);
    static constexpr bool pages_have_icon   = has_spf(_PageFeat, spf_icon);
    static constexpr bool pages_have_dirty  = has_spf(_PageFeat, spf_dirty);
    static constexpr bool pages_have_lazy   = has_spf(_PageFeat, spf_lazy);
    static constexpr bool pages_have_stats  = has_spf(_PageFeat, spf_visit_stats);
    static constexpr bool pages_have_tag    = has_spf(_PageFeat, spf_user_tag);

    // compile-time feature queries (control level)
    static constexpr bool has_history       = has_svf(_CtrlFeat, svf_history);
    static constexpr bool is_indexed        = has_svf(_CtrlFeat, svf_indexed);
    static constexpr bool is_lockable       = has_svf(_CtrlFeat, svf_lock);
    static constexpr bool is_animated       = has_svf(_CtrlFeat, svf_animated);
    static constexpr bool has_veto          = has_svf(_CtrlFeat, svf_veto_guard);

    // sanity: indexed lookup requires per-page id
    static_assert(!is_indexed || pages_have_id,
                  "svf_indexed requires spf_id on pages");

    // component identity
    //   A stacked_view has no visible chrome of its own — navigation
    // is external — so the view itself is not focusable.  The page
    // content it selects may be focusable independently.
    static constexpr bool focusable = false;

    // -- pages ------------------------------------------------------------
    std::vector<page_type> pages;
    size_type              selected = 0;

    // -- policy -----------------------------------------------------------
    stacked_bounds_policy bounds = stacked_bounds_policy::clamp;

    // -- state ------------------------------------------------------------
    bool enabled = true;
    bool visible = true;

    // -- callbacks --------------------------------------------------------
    using select_fn = std::function<void(size_type)>;
    using change_fn = std::function<void(size_type, size_type)>;  // old, new

    select_fn on_select;        // fired on any successful selection
    change_fn on_change;        // fired only when selection actually changes

    // -- construction -----------------------------------------------------
    stacked_view() = default;

    // -- queries ----------------------------------------------------------
    [[nodiscard]] bool
    empty() const noexcept
    {
        return pages.empty();
    }

    [[nodiscard]] size_type
    count() const noexcept
    {
        return pages.size();
    }

    [[nodiscard]] bool
    valid_index(size_type _idx) const noexcept
    {
        return _idx < pages.size();
    }

    [[nodiscard]] page_type*
    selected_page() noexcept
    {
        if (selected < pages.size())
        {
            return &pages[selected];
        }

        return nullptr;
    }

    [[nodiscard]] const page_type*
    selected_page() const noexcept
    {
        if (selected < pages.size())
        {
            return &pages[selected];
        }

        return nullptr;
    }

    // -- compositional forwarding ----------------------------------------
    //   Enables integration with component_common.hpp's for_each_sub,
    // enable_all, and disable_all without writing per-op wrappers.
    template <typename _Fn>
    void
    visit_components(_Fn&& _fn)
    {
        for (auto& page : pages)
        {
            _fn(page);
        }

        return;
    }
};


// ===============================================================================
//  8.  PAGE FREE FUNCTIONS
// ===============================================================================
//   Domain-specific per-page operations.  Shared operations (enable,
// disable) work on stacked_page directly via the ADL functions in
// component_common.hpp since stacked_page has enabled and visible
// members.

// sp_set_id
//   function: sets the page's string id.
template <unsigned _F, typename _I>
void
sp_set_id(stacked_page<_F, _I>& _page,
          std::string           _id)
{
    static_assert(has_spf(_F, spf_id),
                  "requires spf_id");

    _page.id = std::move(_id);

    return;
}

// sp_set_title
//   function: sets the page's title.
template <unsigned _F, typename _I>
void
sp_set_title(stacked_page<_F, _I>& _page,
             std::string           _title)
{
    static_assert(has_spf(_F, spf_title),
                  "requires spf_title");

    _page.title = std::move(_title);

    return;
}

// sp_set_icon
//   function: sets the page's icon.
template <unsigned _F, typename _I>
void
sp_set_icon(stacked_page<_F, _I>&                        _page,
            typename stacked_page<_F, _I>::icon_type     _icon)
{
    static_assert(has_spf(_F, spf_icon),
                  "requires spf_icon");

    _page.icon = std::move(_icon);

    return;
}

// sp_mark_dirty
//   function: marks the page as having unsaved changes.
template <unsigned _F, typename _I>
void
sp_mark_dirty(stacked_page<_F, _I>& _page)
{
    static_assert(has_spf(_F, spf_dirty),
                  "requires spf_dirty");

    _page.dirty = true;

    return;
}

// sp_mark_clean
//   function: clears the page's dirty flag.
template <unsigned _F, typename _I>
void
sp_mark_clean(stacked_page<_F, _I>& _page)
{
    static_assert(has_spf(_F, spf_dirty),
                  "requires spf_dirty");

    _page.dirty = false;

    return;
}

// sp_is_dirty
template <unsigned _F, typename _I>
[[nodiscard]] bool
sp_is_dirty(const stacked_page<_F, _I>& _page) noexcept
{
    static_assert(has_spf(_F, spf_dirty),
                  "requires spf_dirty");

    return _page.dirty;
}

// sp_set_loaded
//   function: marks the page's lazy-load state.
template <unsigned _F, typename _I>
void
sp_set_loaded(stacked_page<_F, _I>& _page,
              bool                  _loaded)
{
    static_assert(has_spf(_F, spf_lazy),
                  "requires spf_lazy");

    _page.loaded = _loaded;

    return;
}

// sp_is_loaded
template <unsigned _F, typename _I>
[[nodiscard]] bool
sp_is_loaded(const stacked_page<_F, _I>& _page) noexcept
{
    static_assert(has_spf(_F, spf_lazy),
                  "requires spf_lazy");

    return _page.loaded;
}

// sp_record_visit
//   function: increments visit count and records the timestamp.
// The caller supplies a clock value — the framework is clock-agnostic.
template <unsigned _F, typename _I>
void
sp_record_visit(stacked_page<_F, _I>& _page,
                std::int64_t          _timestamp)
{
    static_assert(has_spf(_F, spf_visit_stats),
                  "requires spf_visit_stats");

    ++_page.visit_count;
    _page.last_visit_time = _timestamp;

    return;
}


// ===============================================================================
//  9.  CONTROL FREE FUNCTIONS
// ===============================================================================

// -- page management ------------------------------------------------------

/*
sv_add_page
  Appends a new page to the view and returns a reference to it.
When the view is indexed (svf_indexed) and pages carry ids (spf_id),
the id-index map is updated if the new page's id is non-empty.

Parameter(s):
  _sv: the stacked_view to append into.
Return:
  A reference to the newly-added page.
*/
template <unsigned _PF, unsigned _CF, typename _I>
stacked_page<_PF, _I>&
sv_add_page(stacked_view<_PF, _CF, _I>& _sv)
{
    // default-constructed page has an empty id, so the id-index
    // map (if present) requires no update here
    _sv.pages.emplace_back();

    return _sv.pages.back();
}

// sv_add_page (with user_id)
template <unsigned _PF, unsigned _CF, typename _I>
stacked_page<_PF, _I>&
sv_add_page(stacked_view<_PF, _CF, _I>& _sv,
            std::size_t                 _user_id)
{
    _sv.pages.emplace_back(_user_id);

    return _sv.pages.back();
}

/*
sv_insert_page
  Inserts a new page at the given index.  Adjusts `selected` if the
insertion occurs at or before it.  Clamps out-of-range indices to the
end of the vector.  Does not touch the id-index map (pages are
default-constructed with an empty id).

Parameter(s):
  _sv:    the stacked_view to insert into.
  _index: the target index.  Values > count are clamped.
Return:
  A reference to the newly-inserted page.
*/
template <unsigned _PF, unsigned _CF, typename _I>
stacked_page<_PF, _I>&
sv_insert_page(stacked_view<_PF, _CF, _I>& _sv,
               std::size_t                 _index)
{
    if (_index > _sv.pages.size())
    {
        _index = _sv.pages.size();
    }

    auto it = _sv.pages.emplace(
        _sv.pages.begin() + static_cast<std::ptrdiff_t>(_index));

    // shift selected to keep it on the same page
    if ( (_index <= _sv.selected) &&
         (!_sv.pages.empty()) )
    {
        ++_sv.selected;
    }

    // indexed lookup: rebuild because all trailing indices shifted
    if constexpr (has_svf(_CF, svf_indexed))
    {
        for (auto& kv : _sv.id_index)
        {
            if (kv.second >= _index)
            {
                ++kv.second;
            }
        }
    }

    return *it;
}

/*
sv_remove_page
  Removes the page at the given index.  Adjusts `selected` to stay
in bounds and updates the id-index map when present.  Out-of-range
indices are a silent no-op returning false.

Parameter(s):
  _sv:    the stacked_view to remove from.
  _index: the index of the page to remove.
Return:
  true if a page was removed, false if the index was out of range.
*/
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_remove_page(stacked_view<_PF, _CF, _I>& _sv,
               std::size_t                 _index)
{
    // parameter validation
    if (_index >= _sv.pages.size())
    {
        return false;
    }

    // indexed lookup: drop this page's id, then shift trailing indices
    if constexpr ( (has_svf(_CF, svf_indexed)) &&
                   (has_spf(_PF, spf_id)) )
    {
        if (!_sv.pages[_index].id.empty())
        {
            _sv.id_index.erase(_sv.pages[_index].id);
        }

        for (auto& kv : _sv.id_index)
        {
            if (kv.second > _index)
            {
                --kv.second;
            }
        }
    }

    _sv.pages.erase(
        _sv.pages.begin() + static_cast<std::ptrdiff_t>(_index));

    // adjust selected
    if (_sv.pages.empty())
    {
        _sv.selected = 0;
    }
    else if (_sv.selected >= _sv.pages.size())
    {
        _sv.selected = _sv.pages.size() - 1;
    }
    else if ( (_sv.selected > _index) &&
              (_sv.selected > 0) )
    {
        --_sv.selected;
    }

    return true;
}

// sv_remove_all
//   function: clears all pages.  Resets selected to 0, clears the
// id-index and history if present.
template <unsigned _PF, unsigned _CF, typename _I>
void
sv_remove_all(stacked_view<_PF, _CF, _I>& _sv)
{
    _sv.pages.clear();
    _sv.selected = 0;

    if constexpr (has_svf(_CF, svf_indexed))
    {
        _sv.id_index.clear();
    }

    if constexpr (has_svf(_CF, svf_history))
    {
        _sv.back_stack.clear();
        _sv.forward_stack.clear();
    }

    return;
}


// -- indexed lookup -------------------------------------------------------

/*
sv_rebuild_index
  Rebuilds the id → index lookup map from scratch.  Call after bulk
mutations that bypassed sv_add_page / sv_insert_page / sv_remove_page.

Parameter(s):
  _sv: the stacked_view whose index to rebuild.
Return:
  none.
*/
template <unsigned _PF, unsigned _CF, typename _I>
void
sv_rebuild_index(stacked_view<_PF, _CF, _I>& _sv)
{
    static_assert(has_svf(_CF, svf_indexed),
                  "requires svf_indexed");
    static_assert(has_spf(_PF, spf_id),
                  "indexed lookup requires spf_id");

    std::size_t i;

    _sv.id_index.clear();

    for (i = 0; i < _sv.pages.size(); ++i)
    {
        if (!_sv.pages[i].id.empty())
        {
            _sv.id_index[_sv.pages[i].id] = i;
        }
    }

    return;
}

// sv_assign_id
//   function: sets a page's id and updates the id-index map.
template <unsigned _PF, unsigned _CF, typename _I>
void
sv_assign_id(stacked_view<_PF, _CF, _I>& _sv,
             std::size_t                 _index,
             std::string                 _id)
{
    static_assert(has_spf(_PF, spf_id),
                  "requires spf_id");

    if (_index >= _sv.pages.size())
    {
        return;
    }

    // update the index map if present
    if constexpr (has_svf(_CF, svf_indexed))
    {
        if (!_sv.pages[_index].id.empty())
        {
            _sv.id_index.erase(_sv.pages[_index].id);
        }

        if (!_id.empty())
        {
            _sv.id_index[_id] = _index;
        }
    }

    _sv.pages[_index].id = std::move(_id);

    return;
}

// sv_find_by_id
//   function: returns the index of the page with the given id, or
// SIZE_MAX if not found.  O(1) when svf_indexed is enabled; O(n)
// linear scan otherwise.
template <unsigned _PF, unsigned _CF, typename _I>
[[nodiscard]] std::size_t
sv_find_by_id(const stacked_view<_PF, _CF, _I>& _sv,
              const std::string&                _id)
{
    static_assert(has_spf(_PF, spf_id),
                  "requires spf_id");

    std::size_t i;

    if constexpr (has_svf(_CF, svf_indexed))
    {
        auto it = _sv.id_index.find(_id);
        if (it != _sv.id_index.end())
        {
            return it->second;
        }

        return static_cast<std::size_t>(-1);
    }
    else
    {
        for (i = 0; i < _sv.pages.size(); ++i)
        {
            if (_sv.pages[i].id == _id)
            {
                return i;
            }
        }

        return static_cast<std::size_t>(-1);
    }
}


// -- navigation -----------------------------------------------------------

/*
sv_can_navigate
  Reports whether a navigation to the target index would pass the
basic checks: bounds, per-page enabled and visible, and control lock
state.  Does NOT consult the veto callback (on_will_leave), because
that callback is permitted to have side effects — typically a "save
your changes?" dialog — and this function is intended as a cheap
side-effect-free query for UI enable/disable decisions.  To perform
a full navigation with veto enforcement, call sv_select.

Parameter(s):
  _sv:    the stacked_view.
  _index: the target page index.
Return:
  true if the target is in bounds, the page is enabled and visible,
  and the view is not locked.  Note that sv_select may still fail
  (because of the veto callback) when this returns true.
*/
template <unsigned _PF, unsigned _CF, typename _I>
[[nodiscard]] bool
sv_can_navigate(const stacked_view<_PF, _CF, _I>& _sv,
                std::size_t                       _index)
{
    // bounds + per-page enabled/visible
    if (_index >= _sv.pages.size())
    {
        return false;
    }
    if ( (!_sv.pages[_index].enabled) ||
         (!_sv.pages[_index].visible) )
    {
        return false;
    }

    // lock
    if constexpr (has_svf(_CF, svf_lock))
    {
        if (_sv.locked)
        {
            return false;
        }
    }

    return true;
}

/*
sv_select
  Navigates to the page at the given index.  Performs all permission
checks (bounds, enabled, locked, veto), pushes the outgoing index
onto the back stack when history is enabled, updates lazy/visit
state on the incoming page, and fires on_select and on_change
callbacks.  A navigation to the currently-selected index still
fires on_select (but not on_change) and does not disturb history.

Parameter(s):
  _sv:    the stacked_view to navigate.
  _index: the target page index.
Return:
  true if the navigation completed, false if any check blocked it.
*/
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_select(stacked_view<_PF, _CF, _I>& _sv,
          std::size_t                 _index)
{
    std::size_t old;

    // cheap side-effect-free permission checks
    if (!sv_can_navigate(_sv, _index))
    {
        return false;
    }

    // initialize
    old = _sv.selected;

    // veto guard — only consulted for actual transitions, because
    // on_will_leave may have user-visible side effects (e.g. a
    // "discard unsaved changes?" prompt) that should not run on
    // same-page reselection
    if constexpr (has_svf(_CF, svf_veto_guard))
    {
        if ( (old != _index)       &&
             (_sv.on_will_leave)   &&
             (!_sv.on_will_leave(old, _index)) )
        {
            return false;
        }
    }

    // history: push outgoing index and clear forward stack on new nav
    if constexpr (has_svf(_CF, svf_history))
    {
        if (old != _index)
        {
            _sv.back_stack.push_back(old);
            _sv.forward_stack.clear();

            // enforce max_history bound
            if ( (_sv.max_history > 0) &&
                 (_sv.back_stack.size() > _sv.max_history) )
            {
                _sv.back_stack.erase(_sv.back_stack.begin());
            }
        }
    }

    // lazy: mark incoming page loaded
    if constexpr (has_spf(_PF, spf_lazy))
    {
        _sv.pages[_index].loaded = true;
    }

    // visit stats: increment count (timestamp is caller's problem)
    if constexpr (has_spf(_PF, spf_visit_stats))
    {
        ++_sv.pages[_index].visit_count;
    }

    _sv.selected = _index;

    // callbacks
    if (_sv.on_select)
    {
        _sv.on_select(_index);
    }

    if ( (_sv.on_change) &&
         (old != _index) )
    {
        _sv.on_change(old, _index);
    }

    return true;
}

// sv_select_by_id
//   function: looks up a page by id and navigates to it.
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_select_by_id(stacked_view<_PF, _CF, _I>& _sv,
                const std::string&          _id)
{
    std::size_t idx;

    idx = sv_find_by_id(_sv, _id);
    if (idx == static_cast<std::size_t>(-1))
    {
        return false;
    }

    return sv_select(_sv, idx);
}

/*
sv_select_next
  Navigates to the next enabled and visible page.  Honours the bounds
policy: `clamp` stops at the last page (returning false); `wrap`
rolls around to the first eligible page.  Disabled or invisible
pages are skipped in either mode.

Parameter(s):
  _sv: the stacked_view.
Return:
  true if a navigation occurred, false if no eligible next page.
*/
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_select_next(stacked_view<_PF, _CF, _I>& _sv)
{
    std::size_t count;
    std::size_t i;
    std::size_t start;
    bool        wrap;

    if (_sv.pages.empty())
    {
        return false;
    }

    // initialize
    count = _sv.pages.size();
    start = _sv.selected;
    wrap  = (_sv.bounds == stacked_bounds_policy::wrap);

    for (i = 1; i <= count; ++i)
    {
        std::size_t idx = start + i;

        if (idx >= count)
        {
            if (!wrap)
            {
                return false;
            }
            idx = idx - count;
        }

        if ( (_sv.pages[idx].enabled) &&
             (_sv.pages[idx].visible) )
        {
            return sv_select(_sv, idx);
        }

        // if we've walked all the way around without finding one, stop
        if (idx == start)
        {
            return false;
        }
    }

    return false;
}

// sv_select_prev
//   function: symmetric to sv_select_next in the reverse direction.
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_select_prev(stacked_view<_PF, _CF, _I>& _sv)
{
    std::size_t count;
    std::size_t i;
    std::size_t start;
    bool        wrap;

    if (_sv.pages.empty())
    {
        return false;
    }

    // initialize
    count = _sv.pages.size();
    start = _sv.selected;
    wrap  = (_sv.bounds == stacked_bounds_policy::wrap);

    for (i = 1; i <= count; ++i)
    {
        std::size_t idx;

        if (i > start)
        {
            if (!wrap)
            {
                return false;
            }
            idx = count - (i - start);
        }
        else
        {
            idx = start - i;
        }

        if ( (_sv.pages[idx].enabled) &&
             (_sv.pages[idx].visible) )
        {
            return sv_select(_sv, idx);
        }

        if (idx == start)
        {
            return false;
        }
    }

    return false;
}

// sv_select_first
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_select_first(stacked_view<_PF, _CF, _I>& _sv)
{
    std::size_t i;

    for (i = 0; i < _sv.pages.size(); ++i)
    {
        if ( (_sv.pages[i].enabled) &&
             (_sv.pages[i].visible) )
        {
            return sv_select(_sv, i);
        }
    }

    return false;
}

// sv_select_last
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_select_last(stacked_view<_PF, _CF, _I>& _sv)
{
    std::size_t i;

    for (i = _sv.pages.size(); i > 0; --i)
    {
        std::size_t idx = i - 1;
        if ( (_sv.pages[idx].enabled) &&
             (_sv.pages[idx].visible) )
        {
            return sv_select(_sv, idx);
        }
    }

    return false;
}


// -- history --------------------------------------------------------------

/*
sv_back
  Pops the back stack and navigates to that page.  Pushes the current
page onto the forward stack.  The veto guard is NOT consulted for
back navigation — convention is that history navigation is user-
initiated and already past any save prompts.

Parameter(s):
  _sv: the stacked_view.
Return:
  true if a back navigation occurred, false if the back stack was
  empty or the target page is no longer navigable.
*/
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_back(stacked_view<_PF, _CF, _I>& _sv)
{
    static_assert(has_svf(_CF, svf_history),
                  "requires svf_history");

    std::size_t target;
    std::size_t old;

    if (_sv.back_stack.empty())
    {
        return false;
    }

    // initialize
    target = _sv.back_stack.back();
    old    = _sv.selected;

    _sv.back_stack.pop_back();

    // bounds check — pages may have been removed since the history entry
    if (target >= _sv.pages.size())
    {
        return false;
    }

    // push current onto forward stack
    _sv.forward_stack.push_back(old);

    // bypass sv_select's back-stack push by writing directly
    _sv.selected = target;

    // lazy / stats / callbacks — same policy as sv_select
    if constexpr (has_spf(_PF, spf_lazy))
    {
        _sv.pages[target].loaded = true;
    }
    if constexpr (has_spf(_PF, spf_visit_stats))
    {
        ++_sv.pages[target].visit_count;
    }

    if (_sv.on_select)
    {
        _sv.on_select(target);
    }
    if ( (_sv.on_change) &&
         (old != target) )
    {
        _sv.on_change(old, target);
    }

    return true;
}

// sv_forward
//   function: symmetric to sv_back using the forward stack.
template <unsigned _PF, unsigned _CF, typename _I>
bool
sv_forward(stacked_view<_PF, _CF, _I>& _sv)
{
    static_assert(has_svf(_CF, svf_history),
                  "requires svf_history");

    std::size_t target;
    std::size_t old;

    if (_sv.forward_stack.empty())
    {
        return false;
    }

    // initialize
    target = _sv.forward_stack.back();
    old    = _sv.selected;

    _sv.forward_stack.pop_back();

    if (target >= _sv.pages.size())
    {
        return false;
    }

    _sv.back_stack.push_back(old);
    _sv.selected = target;

    if constexpr (has_spf(_PF, spf_lazy))
    {
        _sv.pages[target].loaded = true;
    }
    if constexpr (has_spf(_PF, spf_visit_stats))
    {
        ++_sv.pages[target].visit_count;
    }

    if (_sv.on_select)
    {
        _sv.on_select(target);
    }
    if ( (_sv.on_change) &&
         (old != target) )
    {
        _sv.on_change(old, target);
    }

    return true;
}

// sv_can_back
template <unsigned _PF, unsigned _CF, typename _I>
[[nodiscard]] bool
sv_can_back(const stacked_view<_PF, _CF, _I>& _sv) noexcept
{
    static_assert(has_svf(_CF, svf_history),
                  "requires svf_history");

    return !_sv.back_stack.empty();
}

// sv_can_forward
template <unsigned _PF, unsigned _CF, typename _I>
[[nodiscard]] bool
sv_can_forward(const stacked_view<_PF, _CF, _I>& _sv) noexcept
{
    static_assert(has_svf(_CF, svf_history),
                  "requires svf_history");

    return !_sv.forward_stack.empty();
}

// sv_clear_history
template <unsigned _PF, unsigned _CF, typename _I>
void
sv_clear_history(stacked_view<_PF, _CF, _I>& _sv)
{
    static_assert(has_svf(_CF, svf_history),
                  "requires svf_history");

    _sv.back_stack.clear();
    _sv.forward_stack.clear();

    return;
}


// -- locking --------------------------------------------------------------

// sv_lock
template <unsigned _PF, unsigned _CF, typename _I>
void
sv_lock(stacked_view<_PF, _CF, _I>& _sv,
        std::string                 _reason = {})
{
    static_assert(has_svf(_CF, svf_lock),
                  "requires svf_lock");

    _sv.locked      = true;
    _sv.lock_reason = std::move(_reason);

    return;
}

// sv_unlock
template <unsigned _PF, unsigned _CF, typename _I>
void
sv_unlock(stacked_view<_PF, _CF, _I>& _sv)
{
    static_assert(has_svf(_CF, svf_lock),
                  "requires svf_lock");

    _sv.locked = false;
    _sv.lock_reason.clear();

    return;
}

// sv_is_locked
template <unsigned _PF, unsigned _CF, typename _I>
[[nodiscard]] bool
sv_is_locked(const stacked_view<_PF, _CF, _I>& _sv) noexcept
{
    static_assert(has_svf(_CF, svf_lock),
                  "requires svf_lock");

    return _sv.locked;
}


// -- dirty queries --------------------------------------------------------

// sv_any_dirty
//   function: returns true if any page has its dirty flag set.
template <unsigned _PF, unsigned _CF, typename _I>
[[nodiscard]] bool
sv_any_dirty(const stacked_view<_PF, _CF, _I>& _sv) noexcept
{
    static_assert(has_spf(_PF, spf_dirty),
                  "requires spf_dirty");

    for (const auto& page : _sv.pages)
    {
        if (page.dirty)
        {
            return true;
        }
    }

    return false;
}

// sv_clear_dirty_all
//   function: clears the dirty flag on every page.
template <unsigned _PF, unsigned _CF, typename _I>
void
sv_clear_dirty_all(stacked_view<_PF, _CF, _I>& _sv)
{
    static_assert(has_spf(_PF, spf_dirty),
                  "requires spf_dirty");

    for (auto& page : _sv.pages)
    {
        page.dirty = false;
    }

    return;
}


// ===============================================================================
//  10.  TRAITS
// ===============================================================================
//   SFINAE detectors following the tab_control traits pattern.
// Renderers and generic code query these to discover what a
// stacked_page / stacked_view instance carries, without hard-coding
// specific feature combinations.

namespace stacked_traits {

NS_INTERNAL

    // -- page-level detectors ---------------------------------------------

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
    struct has_icon_member : std::false_type {};

    template <typename _Type>
    struct has_icon_member<_Type, std::void_t<
        decltype(std::declval<_Type>().icon)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_dirty_member : std::false_type {};

    template <typename _Type>
    struct has_dirty_member<_Type, std::void_t<
        decltype(std::declval<_Type>().dirty)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_loaded_member : std::false_type {};

    template <typename _Type>
    struct has_loaded_member<_Type, std::void_t<
        decltype(std::declval<_Type>().loaded)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_visit_count_member : std::false_type {};

    template <typename _Type>
    struct has_visit_count_member<_Type, std::void_t<
        decltype(std::declval<_Type>().visit_count)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_user_tag_member : std::false_type {};

    template <typename _Type>
    struct has_user_tag_member<_Type, std::void_t<
        decltype(std::declval<_Type>().user_tag)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_user_id_member : std::false_type {};

    template <typename _Type>
    struct has_user_id_member<_Type, std::void_t<
        decltype(std::declval<_Type>().user_id)
    >> : std::true_type {};

    // -- control-level detectors ------------------------------------------

    template <typename, typename = void>
    struct has_pages_member : std::false_type {};

    template <typename _Type>
    struct has_pages_member<_Type, std::void_t<
        decltype(std::declval<_Type>().pages)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_selected_member : std::false_type {};

    template <typename _Type>
    struct has_selected_member<_Type, std::void_t<
        decltype(std::declval<_Type>().selected)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_bounds_member : std::false_type {};

    template <typename _Type>
    struct has_bounds_member<_Type, std::void_t<
        decltype(std::declval<_Type>().bounds)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_back_stack_member : std::false_type {};

    template <typename _Type>
    struct has_back_stack_member<_Type, std::void_t<
        decltype(std::declval<_Type>().back_stack)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_id_index_member : std::false_type {};

    template <typename _Type>
    struct has_id_index_member<_Type, std::void_t<
        decltype(std::declval<_Type>().id_index)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_locked_member : std::false_type {};

    template <typename _Type>
    struct has_locked_member<_Type, std::void_t<
        decltype(std::declval<_Type>().locked)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_transition_member : std::false_type {};

    template <typename _Type>
    struct has_transition_member<_Type, std::void_t<
        decltype(std::declval<_Type>().transition)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_will_leave_member : std::false_type {};

    template <typename _Type>
    struct has_will_leave_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_will_leave)
    >> : std::true_type {};

NS_END  // internal


// -- page value aliases ---------------------------------------------------
template <typename _Type>
inline constexpr bool has_id_v =
    detail::has_id_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_title_v =
    detail::has_title_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_icon_v =
    detail::has_icon_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_dirty_v =
    detail::has_dirty_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_loaded_v =
    detail::has_loaded_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_visit_count_v =
    detail::has_visit_count_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_user_tag_v =
    detail::has_user_tag_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_user_id_v =
    detail::has_user_id_member<_Type>::value;

// -- control value aliases ------------------------------------------------
template <typename _Type>
inline constexpr bool has_pages_v =
    detail::has_pages_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_selected_v =
    detail::has_selected_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_bounds_v =
    detail::has_bounds_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_back_stack_v =
    detail::has_back_stack_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_id_index_v =
    detail::has_id_index_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_locked_v =
    detail::has_locked_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_transition_v =
    detail::has_transition_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_will_leave_v =
    detail::has_will_leave_member<_Type>::value;

// -- shared aliases (delegate to component_traits) ------------------------
template <typename _Type>
inline constexpr bool has_enabled_v =
    component_traits::has_enabled_v<_Type>;
template <typename _Type>
inline constexpr bool has_visible_v =
    component_traits::has_visible_v<_Type>;


// -- composite traits -----------------------------------------------------

// is_stacked_page
//   trait: structurally identifies a stacked_page — has enabled,
// visible, and user_id.  The label/title/icon features are optional.
template <typename _Type>
struct is_stacked_page : std::conjunction<
    component_traits::detail::has_enabled_member<_Type>,
    component_traits::detail::has_visible_member<_Type>,
    detail::has_user_id_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_stacked_page_v =
    is_stacked_page<_Type>::value;

// is_stacked_view
//   trait: structurally identifies a stacked_view — has pages,
// selected, bounds, enabled, and visible.
template <typename _Type>
struct is_stacked_view : std::conjunction<
    detail::has_pages_member<_Type>,
    detail::has_selected_member<_Type>,
    detail::has_bounds_member<_Type>,
    component_traits::detail::has_enabled_member<_Type>,
    component_traits::detail::has_visible_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_stacked_view_v =
    is_stacked_view<_Type>::value;

// is_history_stacked_view
template <typename _Type>
struct is_history_stacked_view : std::conjunction<
    is_stacked_view<_Type>,
    detail::has_back_stack_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_history_stacked_view_v =
    is_history_stacked_view<_Type>::value;

// is_indexed_stacked_view
template <typename _Type>
struct is_indexed_stacked_view : std::conjunction<
    is_stacked_view<_Type>,
    detail::has_id_index_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_indexed_stacked_view_v =
    is_indexed_stacked_view<_Type>::value;

// is_lockable_stacked_view
template <typename _Type>
struct is_lockable_stacked_view : std::conjunction<
    is_stacked_view<_Type>,
    detail::has_locked_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_lockable_stacked_view_v =
    is_lockable_stacked_view<_Type>::value;

// is_animated_stacked_view
template <typename _Type>
struct is_animated_stacked_view : std::conjunction<
    is_stacked_view<_Type>,
    detail::has_transition_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_animated_stacked_view_v =
    is_animated_stacked_view<_Type>::value;

// is_veto_stacked_view
template <typename _Type>
struct is_veto_stacked_view : std::conjunction<
    is_stacked_view<_Type>,
    detail::has_will_leave_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_veto_stacked_view_v =
    is_veto_stacked_view<_Type>::value;


}   // namespace stacked_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_STACKED_VIEW_
