/*******************************************************************************
* uxoxo [layout]                                      layout_template_traits.hpp
*
* Layout template SFINAE traits:
*   Compile-time detection of layout structural members, lifecycle hooks, and
* region presence.  Mirrors the split used by component_traits.hpp and
* database_traits.hpp: public `has_X` / `is_X` traits in struct form, `_v`
* variable-template aliases, and tagless `can_` / `does_` / `is_` variable
* templates for use in if-constexpr without `::value` noise.
*
*   Layouts are structurally detected - there is no is-a relationship with
* layout_template required.  A type satisfies `is_layout_v<T>` if it carries
* the minimum shared surface (metadata, visible, enabled, kind, build()).
* This lets WYSIWYG tooling, editors, and free operations in layout_common
* treat third-party layouts uniformly with first-party ones.
*
*   Region detection is entirely structural: a layout is classified as
* `has_master_region_v` if it exposes a `master` member, `has_detail_region_v`
* if it exposes a `detail` member, and so on.  This pairs with the
* DRegionRole enum in layout_template.hpp without forcing any particular
* region-container type.
*
* Contents:
*   1   Member detector aliases    - primitive members
*   2   Region detector aliases    - master / detail / header / footer / ...
*   3   Method detector aliases    - build / apply / cancel / teardown
*   4   Callback detector aliases  - on_open / on_close / on_apply / on_cancel
*   5   has_X SFINAE traits        - struct form + _v alias
*   6   is_X aggregate traits      - layout / form / master-detail / wizard
*   7   Tagless variable templates - can_X / does_X / is_X (if-constexpr use)
*   8   Kind-based traits          - identity checks on DLayoutKind
*   9   SFINAE enable_if helpers   - for constrained overload sets
*
*
* path:      /inc/uxoxo/layout/layout_template_traits.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.24
*******************************************************************************/

#ifndef  UXOXO_LAYOUT_TEMPLATE_TRAITS_
#define  UXOXO_LAYOUT_TEMPLATE_TRAITS_ 1

// std
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/meta/type_traits.hpp>
// uxoxo
#include "../uxoxo.hpp"
#include "./layout_template.hpp"


#ifndef NS_LAYOUT
    #define NS_LAYOUT   D_NAMESPACE(layout)
#endif


NS_UXOXO
NS_LAYOUT


using djinterp::is_detected;


// ===============================================================================
//  1  MEMBER DETECTOR ALIASES
// ===============================================================================
//   Primitive members carried by layout_template, plus any that concrete
// layouts are expected to expose by convention.  The detectors follow the
// djinterp is_detected idiom: each alias is the type of an expression that
// is well-formed only when the member exists.

// metadata_t
//   detector: layout metadata member (title / description / ...).
template <typename _Type>
using metadata_t = decltype(std::declval<_Type&>().metadata);

// dimensions_t
//   detector: layout dimensions member (width / height hints).
template <typename _Type>
using dimensions_t = decltype(std::declval<_Type&>().dimensions);

// visible_t
//   detector: visible bool member.
template <typename _Type>
using visible_t = decltype(std::declval<_Type&>().visible);

// enabled_t
//   detector: enabled bool member.
template <typename _Type>
using enabled_t = decltype(std::declval<_Type&>().enabled);

// modal_t
//   detector: modal bool member.
template <typename _Type>
using modal_t = decltype(std::declval<_Type&>().modal);

// resizable_t
//   detector: resizable bool member.
template <typename _Type>
using resizable_t = decltype(std::declval<_Type&>().resizable);

// closable_t
//   detector: closable bool member.
template <typename _Type>
using closable_t = decltype(std::declval<_Type&>().closable);

// dirty_t
//   detector: dirty bool member.
template <typename _Type>
using dirty_t = decltype(std::declval<_Type&>().dirty);

// kind_t
//   detector: kind static constexpr member (DLayoutKind).
template <typename _Type>
using kind_t = decltype(_Type::kind);




// ===============================================================================
//  2  REGION DETECTOR ALIASES
// ===============================================================================
//   Region members are detected by name.  A concrete layout that exposes a
// `master` member - whatever its type - is classified as having a master
// region.  This keeps the traits layer agnostic of the particular
// container, wrapper, or component type the region holds.

// header_region_t
//   detector: header region member.
template <typename _Type>
using header_region_t = decltype(std::declval<_Type&>().header);

// master_region_t
//   detector: master region member.
template <typename _Type>
using master_region_t = decltype(std::declval<_Type&>().master);

// detail_region_t
//   detector: detail region member.
template <typename _Type>
using detail_region_t = decltype(std::declval<_Type&>().detail);

// aside_region_t
//   detector: aside region member.
template <typename _Type>
using aside_region_t = decltype(std::declval<_Type&>().aside);

// footer_region_t
//   detector: footer region member.
template <typename _Type>
using footer_region_t = decltype(std::declval<_Type&>().footer);

// toolbar_region_t
//   detector: toolbar region member.
template <typename _Type>
using toolbar_region_t = decltype(std::declval<_Type&>().toolbar);

// primary_actions_region_t
//   detector: primary_actions region member.
template <typename _Type>
using primary_actions_region_t =
    decltype(std::declval<_Type&>().primary_actions);

// master_actions_region_t
//   detector: master_actions region member.
template <typename _Type>
using master_actions_region_t =
    decltype(std::declval<_Type&>().master_actions);

// content_region_t
//   detector: content region member.
template <typename _Type>
using content_region_t = decltype(std::declval<_Type&>().content);




// ===============================================================================
//  3  METHOD DETECTOR ALIASES
// ===============================================================================
//   Lifecycle methods on the public (forwarding) surface of layout_template
// and its concrete derivatives.

// build_t
//   detector: build() method.
template <typename _Type>
using build_t = decltype(std::declval<_Type&>().build());

// teardown_t
//   detector: teardown() method.
template <typename _Type>
using teardown_t = decltype(std::declval<_Type&>().teardown());

// apply_t
//   detector: apply() method.
template <typename _Type>
using apply_t = decltype(std::declval<_Type&>().apply());

// cancel_t
//   detector: cancel() method.
template <typename _Type>
using cancel_t = decltype(std::declval<_Type&>().cancel());

// has_pending_t
//   detector: has_pending() const method.
template <typename _Type>
using has_pending_t =
    decltype(std::declval<const _Type&>().has_pending());

// get_kind_t
//   detector: get_kind() static method returning DLayoutKind.
template <typename _Type>
using get_kind_t = decltype(_Type::get_kind());




// ===============================================================================
//  4  CALLBACK DETECTOR ALIASES
// ===============================================================================

// on_open_t
//   detector: on_open callable member.
template <typename _Type>
using on_open_t = decltype(std::declval<_Type&>().on_open);

// on_close_t
//   detector: on_close callable member.
template <typename _Type>
using on_close_t = decltype(std::declval<_Type&>().on_close);

// on_apply_t
//   detector: on_apply callable member.
template <typename _Type>
using on_apply_t = decltype(std::declval<_Type&>().on_apply);

// on_cancel_t
//   detector: on_cancel callable member.
template <typename _Type>
using on_cancel_t = decltype(std::declval<_Type&>().on_cancel);




// ===============================================================================
//  5  has_X SFINAE TRAITS
// ===============================================================================
//   Struct-form traits that expose `::value`.  Each ships with a `_v`
// variable template alias when variable templates are available.

// has_metadata
//   trait: true if _Type exposes a metadata member.
template <typename _Type>
struct has_metadata : is_detected<metadata_t, _Type>
{};

// has_dimensions
//   trait: true if _Type exposes a dimensions member.
template <typename _Type>
struct has_dimensions : is_detected<dimensions_t, _Type>
{};

// has_visible
//   trait: true if _Type exposes a visible member.
template <typename _Type>
struct has_visible : is_detected<visible_t, _Type>
{};

// has_enabled
//   trait: true if _Type exposes an enabled member.
template <typename _Type>
struct has_enabled : is_detected<enabled_t, _Type>
{};

// has_modal
//   trait: true if _Type exposes a modal member.
template <typename _Type>
struct has_modal : is_detected<modal_t, _Type>
{};

// has_resizable
//   trait: true if _Type exposes a resizable member.
template <typename _Type>
struct has_resizable : is_detected<resizable_t, _Type>
{};

// has_closable
//   trait: true if _Type exposes a closable member.
template <typename _Type>
struct has_closable : is_detected<closable_t, _Type>
{};

// has_dirty
//   trait: true if _Type exposes a dirty member.
template <typename _Type>
struct has_dirty : is_detected<dirty_t, _Type>
{};

// has_kind
//   trait: true if _Type exposes a static kind constant.
template <typename _Type>
struct has_kind : is_detected<kind_t, _Type>
{};

// -- region presence ----------------------------------------------------------

// has_header_region
//   trait: true if _Type exposes a header region member.
template <typename _Type>
struct has_header_region : is_detected<header_region_t, _Type>
{};

// has_master_region
//   trait: true if _Type exposes a master region member.
template <typename _Type>
struct has_master_region : is_detected<master_region_t, _Type>
{};

// has_detail_region
//   trait: true if _Type exposes a detail region member.
template <typename _Type>
struct has_detail_region : is_detected<detail_region_t, _Type>
{};

// has_aside_region
//   trait: true if _Type exposes an aside region member.
template <typename _Type>
struct has_aside_region : is_detected<aside_region_t, _Type>
{};

// has_footer_region
//   trait: true if _Type exposes a footer region member.
template <typename _Type>
struct has_footer_region : is_detected<footer_region_t, _Type>
{};

// has_toolbar_region
//   trait: true if _Type exposes a toolbar region member.
template <typename _Type>
struct has_toolbar_region : is_detected<toolbar_region_t, _Type>
{};

// has_primary_actions_region
//   trait: true if _Type exposes a primary_actions region member.
template <typename _Type>
struct has_primary_actions_region
    : is_detected<primary_actions_region_t, _Type>
{};

// has_master_actions_region
//   trait: true if _Type exposes a master_actions region member.
template <typename _Type>
struct has_master_actions_region
    : is_detected<master_actions_region_t, _Type>
{};

// has_content_region
//   trait: true if _Type exposes a content region member.
template <typename _Type>
struct has_content_region : is_detected<content_region_t, _Type>
{};

// -- lifecycle methods --------------------------------------------------------

// has_build
//   trait: true if _Type exposes build().
template <typename _Type>
struct has_build : is_detected<build_t, _Type>
{};

// has_teardown
//   trait: true if _Type exposes teardown().
template <typename _Type>
struct has_teardown : is_detected<teardown_t, _Type>
{};

// has_apply
//   trait: true if _Type exposes apply().
template <typename _Type>
struct has_apply : is_detected<apply_t, _Type>
{};

// has_cancel
//   trait: true if _Type exposes cancel().
template <typename _Type>
struct has_cancel : is_detected<cancel_t, _Type>
{};

// has_pending_query
//   trait: true if _Type exposes has_pending() const.
template <typename _Type>
struct has_pending_query : is_detected<has_pending_t, _Type>
{};

// -- callbacks ----------------------------------------------------------------

// has_on_open
//   trait: true if _Type exposes an on_open member.
template <typename _Type>
struct has_on_open : is_detected<on_open_t, _Type>
{};

// has_on_close
//   trait: true if _Type exposes an on_close member.
template <typename _Type>
struct has_on_close : is_detected<on_close_t, _Type>
{};

// has_on_apply
//   trait: true if _Type exposes an on_apply member.
template <typename _Type>
struct has_on_apply : is_detected<on_apply_t, _Type>
{};

// has_on_cancel
//   trait: true if _Type exposes an on_cancel member.
template <typename _Type>
struct has_on_cancel : is_detected<on_cancel_t, _Type>
{};


// -- variable template aliases ------------------------------------------------

#if D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES

    // primitive members
    template <typename _Type>
    constexpr bool has_metadata_v   = has_metadata<_Type>::value;

    template <typename _Type>
    constexpr bool has_dimensions_v = has_dimensions<_Type>::value;

    template <typename _Type>
    constexpr bool has_visible_v    = has_visible<_Type>::value;

    template <typename _Type>
    constexpr bool has_enabled_v    = has_enabled<_Type>::value;

    template <typename _Type>
    constexpr bool has_modal_v      = has_modal<_Type>::value;

    template <typename _Type>
    constexpr bool has_resizable_v  = has_resizable<_Type>::value;

    template <typename _Type>
    constexpr bool has_closable_v   = has_closable<_Type>::value;

    template <typename _Type>
    constexpr bool has_dirty_v      = has_dirty<_Type>::value;

    template <typename _Type>
    constexpr bool has_kind_v       = has_kind<_Type>::value;

    // regions
    template <typename _Type>
    constexpr bool has_header_region_v  = has_header_region<_Type>::value;

    template <typename _Type>
    constexpr bool has_master_region_v  = has_master_region<_Type>::value;

    template <typename _Type>
    constexpr bool has_detail_region_v  = has_detail_region<_Type>::value;

    template <typename _Type>
    constexpr bool has_aside_region_v   = has_aside_region<_Type>::value;

    template <typename _Type>
    constexpr bool has_footer_region_v  = has_footer_region<_Type>::value;

    template <typename _Type>
    constexpr bool has_toolbar_region_v = has_toolbar_region<_Type>::value;

    template <typename _Type>
    constexpr bool has_primary_actions_region_v =
        has_primary_actions_region<_Type>::value;

    template <typename _Type>
    constexpr bool has_master_actions_region_v =
        has_master_actions_region<_Type>::value;

    template <typename _Type>
    constexpr bool has_content_region_v =
        has_content_region<_Type>::value;

    // lifecycle
    template <typename _Type>
    constexpr bool has_build_v         = has_build<_Type>::value;

    template <typename _Type>
    constexpr bool has_teardown_v      = has_teardown<_Type>::value;

    template <typename _Type>
    constexpr bool has_apply_v         = has_apply<_Type>::value;

    template <typename _Type>
    constexpr bool has_cancel_v        = has_cancel<_Type>::value;

    template <typename _Type>
    constexpr bool has_pending_query_v = has_pending_query<_Type>::value;

    // callbacks
    template <typename _Type>
    constexpr bool has_on_open_v    = has_on_open<_Type>::value;

    template <typename _Type>
    constexpr bool has_on_close_v   = has_on_close<_Type>::value;

    template <typename _Type>
    constexpr bool has_on_apply_v   = has_on_apply<_Type>::value;

    template <typename _Type>
    constexpr bool has_on_cancel_v  = has_on_cancel<_Type>::value;

#endif  // D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES




// ===============================================================================
//  6  is_X AGGREGATE TRAITS
// ===============================================================================
//   Composite structural classifications.  These combine the primitive
// has_X traits into layout archetypes that can be matched without
// reading a DLayoutKind tag - useful for operations that care only about
// the shape of the layout, not its labeled kind.

// is_layout
//   trait: verifies _Type satisfies the minimum layout protocol
// (metadata + visible + enabled + kind + build).
template <typename _Type>
struct is_layout : djinterp::conjunction<
    has_metadata<_Type>,
    has_visible<_Type>,
    has_enabled<_Type>,
    has_kind<_Type>,
    has_build<_Type>>
{};

// is_dismissable_layout
//   trait: verifies _Type has apply() and cancel() - an actionable
// dialog-style layout.
template <typename _Type>
struct is_dismissable_layout : djinterp::conjunction<
    is_layout<_Type>,
    has_apply<_Type>,
    has_cancel<_Type>>
{};

// is_modal_layout
//   trait: verifies _Type supports a modal flag.
template <typename _Type>
struct is_modal_layout : djinterp::conjunction<
    is_layout<_Type>,
    has_modal<_Type>>
{};

// is_resizable_layout
//   trait: verifies _Type supports a resizable flag.
template <typename _Type>
struct is_resizable_layout : djinterp::conjunction<
    is_layout<_Type>,
    has_resizable<_Type>>
{};

// is_master_detail_layout
//   trait: verifies _Type has both a master and a detail region - the
// defining shape of HeidiSQL / WinSCP session managers, file browsers,
// and preferences panes.
template <typename _Type>
struct is_master_detail_layout : djinterp::conjunction<
    is_layout<_Type>,
    has_master_region<_Type>,
    has_detail_region<_Type>>
{};

// is_form_dialog_layout
//   trait: verifies _Type has a body (content or detail region) plus
// primary_actions - a classic modal form dialog.
template <typename _Type>
struct is_form_dialog_layout : djinterp::conjunction<
    is_layout<_Type>,
    djinterp::disjunction<
        has_content_region<_Type>,
        has_detail_region<_Type>>,
    has_primary_actions_region<_Type>,
    has_apply<_Type>,
    has_cancel<_Type>>
{};

// is_session_manager_layout
//   trait: verifies _Type has the full session-manager shape - master
// list, detail pane, master-level actions, and primary actions.  This
// matches the HeidiSQL session manager and WinSCP login dialog.
template <typename _Type>
struct is_session_manager_layout : djinterp::conjunction<
    is_master_detail_layout<_Type>,
    has_master_actions_region<_Type>,
    has_primary_actions_region<_Type>>
{};

// is_wizard_layout
//   trait: verifies _Type exposes a content region together with apply
// and cancel - the minimum wizard shape.  More specific detection
// (step traversal, back / next) belongs to a dedicated wizard_traits
// header.
template <typename _Type>
struct is_wizard_layout : djinterp::conjunction<
    is_layout<_Type>,
    has_content_region<_Type>,
    has_apply<_Type>,
    has_cancel<_Type>>
{};

// is_tracked_layout
//   trait: verifies _Type has dirty-state tracking and a pending-work
// query.  Useful for wiring save reminders and unsaved-changes dialogs
// without inspecting the concrete layout type.
template <typename _Type>
struct is_tracked_layout : djinterp::conjunction<
    is_layout<_Type>,
    has_dirty<_Type>,
    has_pending_query<_Type>>
{};


#if D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES

    template <typename _Type>
    constexpr bool is_layout_v = is_layout<_Type>::value;

    template <typename _Type>
    constexpr bool is_dismissable_layout_v =
        is_dismissable_layout<_Type>::value;

    template <typename _Type>
    constexpr bool is_modal_layout_v = is_modal_layout<_Type>::value;

    template <typename _Type>
    constexpr bool is_resizable_layout_v =
        is_resizable_layout<_Type>::value;

    template <typename _Type>
    constexpr bool is_master_detail_layout_v =
        is_master_detail_layout<_Type>::value;

    template <typename _Type>
    constexpr bool is_form_dialog_layout_v =
        is_form_dialog_layout<_Type>::value;

    template <typename _Type>
    constexpr bool is_session_manager_layout_v =
        is_session_manager_layout<_Type>::value;

    template <typename _Type>
    constexpr bool is_wizard_layout_v = is_wizard_layout<_Type>::value;

    template <typename _Type>
    constexpr bool is_tracked_layout_v =
        is_tracked_layout<_Type>::value;

#endif  // D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES




// ===============================================================================
//  7  TAGLESS VARIABLE TEMPLATES
// ===============================================================================
//   Tagless traits resolve directly to constexpr bool via partial
// specialization over std::void_t.  Unlike the struct-based traits in
// section 5, these have no intermediate struct, no ::value accessor,
// and no _v alias - they are first-class bool values suitable for
// direct use in if-constexpr, static_assert, and enable_if.
//
//   Naming: `can_` indicates a callable capability (a method exists),
// `does_` indicates a combined capability, and `is_` indicates an
// identity / shape check.  The prefix mirrors the database_traits
// tagless convention.

// can_build
//   tagless trait: true if _Type has a build() method.
template <typename _Type,
          typename = void>
constexpr bool can_build = false;

template <typename _Type>
constexpr bool can_build<_Type, std::void_t<build_t<_Type>>> = true;

// can_teardown
//   tagless trait: true if _Type has a teardown() method.
template <typename _Type,
          typename = void>
constexpr bool can_teardown = false;

template <typename _Type>
constexpr bool can_teardown<_Type, std::void_t<teardown_t<_Type>>> = true;

// can_apply
//   tagless trait: true if _Type has an apply() method.
template <typename _Type,
          typename = void>
constexpr bool can_apply = false;

template <typename _Type>
constexpr bool can_apply<_Type, std::void_t<apply_t<_Type>>> = true;

// can_cancel
//   tagless trait: true if _Type has a cancel() method.
template <typename _Type,
          typename = void>
constexpr bool can_cancel = false;

template <typename _Type>
constexpr bool can_cancel<_Type, std::void_t<cancel_t<_Type>>> = true;

// can_query_pending
//   tagless trait: true if _Type has a has_pending() const method.
template <typename _Type,
          typename = void>
constexpr bool can_query_pending = false;

template <typename _Type>
constexpr bool can_query_pending<_Type, std::void_t<has_pending_t<_Type>>>
    = true;

// does_dismiss
//   tagless trait: true if _Type supports both apply and cancel.
template <typename _Type>
constexpr bool does_dismiss =
    ( can_apply<_Type> &&
      can_cancel<_Type> );

// does_lifecycle
//   tagless trait: true if _Type supports the full lifecycle
// (build + teardown + apply + cancel).
template <typename _Type>
constexpr bool does_lifecycle =
    ( can_build<_Type>    &&
      can_teardown<_Type> &&
      can_apply<_Type>    &&
      can_cancel<_Type> );

// is_layoutable
//   tagless trait: true if _Type satisfies the minimum layout protocol
// (metadata + visible + enabled + kind + build).
template <typename _Type,
          typename = void>
constexpr bool is_layoutable = false;

template <typename _Type>
constexpr bool is_layoutable<_Type, std::void_t<
    metadata_t<_Type>,
    visible_t<_Type>,
    enabled_t<_Type>,
    kind_t<_Type>,
    build_t<_Type>>> = true;

// is_master_detail_shaped
//   tagless trait: true if _Type exposes both master and detail regions.
template <typename _Type,
          typename = void>
constexpr bool is_master_detail_shaped = false;

template <typename _Type>
constexpr bool is_master_detail_shaped<_Type, std::void_t<
    master_region_t<_Type>,
    detail_region_t<_Type>>> = true;

// is_session_manager_shaped
//   tagless trait: true if _Type has the full session manager region
// set (master + detail + master_actions + primary_actions).
template <typename _Type,
          typename = void>
constexpr bool is_session_manager_shaped = false;

template <typename _Type>
constexpr bool is_session_manager_shaped<_Type, std::void_t<
    master_region_t<_Type>,
    detail_region_t<_Type>,
    master_actions_region_t<_Type>,
    primary_actions_region_t<_Type>>> = true;

// is_form_dialog_shaped
//   tagless trait: true if _Type has a body region (content or detail)
// plus a primary_actions region, with apply and cancel hooks.
template <typename _Type>
constexpr bool is_form_dialog_shaped =
    ( is_layoutable<_Type>                                    &&
      ( is_detected<content_region_t, _Type>::value ||
        is_detected<detail_region_t,  _Type>::value )         &&
      is_detected<primary_actions_region_t, _Type>::value     &&
      does_dismiss<_Type> );




// ===============================================================================
//  8  KIND-BASED TRAITS
// ===============================================================================
//   Identity checks that key off the compile-time kind tag rather than
// region shape.  These are separate from the structural traits above so
// that consumers can pick either convention.  A kind check answers
// "what did the author declare?"; a shape check answers "what does the
// layout actually look like?".

// is_kind
//   trait: true if _Type::kind equals the requested DLayoutKind.
template <typename _Type,
          DLayoutKind _Kind,
          typename    = void>
struct is_kind : std::false_type
{};

template <typename _Type, DLayoutKind _Kind>
struct is_kind<_Type, _Kind, std::void_t<kind_t<_Type>>>
    : std::integral_constant<bool, (_Type::kind == _Kind)>
{};

#if D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES

    template <typename _Type, DLayoutKind _Kind>
    constexpr bool is_kind_v = is_kind<_Type, _Kind>::value;

    template <typename _Type>
    constexpr bool is_form_dialog_kind_v =
        is_kind<_Type, DLayoutKind::form_dialog>::value;

    template <typename _Type>
    constexpr bool is_master_detail_kind_v =
        is_kind<_Type, DLayoutKind::master_detail>::value;

    template <typename _Type>
    constexpr bool is_session_manager_kind_v =
        is_kind<_Type, DLayoutKind::session_manager>::value;

    template <typename _Type>
    constexpr bool is_preferences_pane_kind_v =
        is_kind<_Type, DLayoutKind::preferences_pane>::value;

    template <typename _Type>
    constexpr bool is_tabbed_dialog_kind_v =
        is_kind<_Type, DLayoutKind::tabbed_dialog>::value;

    template <typename _Type>
    constexpr bool is_wizard_kind_v =
        is_kind<_Type, DLayoutKind::wizard>::value;

    template <typename _Type>
    constexpr bool is_split_view_kind_v =
        is_kind<_Type, DLayoutKind::split_view>::value;

    template <typename _Type>
    constexpr bool is_dashboard_kind_v =
        is_kind<_Type, DLayoutKind::dashboard>::value;

#endif  // D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES




// ===============================================================================
//  9  SFINAE ENABLE-IF HELPERS
// ===============================================================================
//   Parallels the enable_if_* helpers in database_traits.hpp.  Useful
// for constraining overload sets without requiring C++20 concepts.

// enable_if_layout
//   type: SFINAE helper for layout-protocol constraints.
template <typename _Type>
using enable_if_layout =
    typename std::enable_if<is_layout<_Type>::value>::type;

// enable_if_dismissable_layout
//   type: SFINAE helper for apply / cancel constraints.
template <typename _Type>
using enable_if_dismissable_layout =
    typename std::enable_if<is_dismissable_layout<_Type>::value>::type;

// enable_if_master_detail_layout
//   type: SFINAE helper for master-detail shape constraints.
template <typename _Type>
using enable_if_master_detail_layout =
    typename std::enable_if<is_master_detail_layout<_Type>::value>::type;

// enable_if_session_manager_layout
//   type: SFINAE helper for session-manager shape constraints.
template <typename _Type>
using enable_if_session_manager_layout =
    typename std::enable_if<is_session_manager_layout<_Type>::value>::type;

// enable_if_tracked_layout
//   type: SFINAE helper for dirty-tracking constraints.
template <typename _Type>
using enable_if_tracked_layout =
    typename std::enable_if<is_tracked_layout<_Type>::value>::type;


NS_END  // layout
NS_END  // uxoxo


#endif  // UXOXO_LAYOUT_TEMPLATE_TRAITS_
