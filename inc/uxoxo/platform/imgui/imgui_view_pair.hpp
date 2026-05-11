/*******************************************************************************
* uxoxo [imgui]                                             imgui_view_pair.hpp
*
* Generic registry-dispatch bundle for view + state (+ ui buffers):
*   The component registry dispatches through a single `void*`, so any
* renderer that needs more than one piece of mutable state per draw
* call has historically defined a small "pair" struct bundling
* references.  Three nearly-identical such structs already exist:
*
*     - database_table_view_pair (table_view + database_table_state)
*     - mysql_table_view_pair    (table_view + mysql_table_state +
*                                  imgui_mysql_table_ui)
*     - mariadb_table_view_pair  (table_view + mariadb_table_state +
*                                  imgui_mariadb_table_ui)
*
*   This header replaces all three with a single template:
*
*     template<typename _View,
*              typename _State,
*              typename _Ui = void>
*     struct view_state_pair { _View& view; _State& state; _Ui& ui; };
*
*   plus a partial specialization for the no-ui case.  Existing pair
* names become type aliases:
*
*     using database_table_view_pair =
*         view_state_pair<table_view, database_table_state>;
*     using mysql_table_view_pair    =
*         view_state_pair<table_view, mysql_table_state,
*                         imgui_mysql_table_ui>;
*
*   Construction uses brace initialization, identical to the existing
* struct usage at every call site.  No code in any draw handler needs
* to change beyond the type alias.
*
* Contents:
*   1.  view_state_pair<_View, _State, _Ui>
*   2.  view_state_pair<_View, _State, void>  (specialization)
*   3.  is_view_state_pair_v                  (trait)
*
*
* path:      /inc/uxoxo/platform/imgui/core/imgui_view_pair.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.08
*******************************************************************************/

#ifndef  UXOXO_IMGUI_VIEW_PAIR_
#define  UXOXO_IMGUI_VIEW_PAIR_ 1

// std
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  1.  VIEW STATE PAIR
// ===========================================================================
//   Bundles references to a view, its companion state, and an optional
// per-renderer UI buffer object.  Holds references rather than owning
// instances so callers retain control over lifetime and storage; the
// registry dispatches a `void*` pointing at this lightweight aggregate.

// view_state_pair
//   struct: registry dispatch bundle for view + state + ui.  The third
// type parameter defaults to void for the common two-member case;
// see the specialization below.
template<typename _View,
          typename _State,
          typename _Ui = void>
struct view_state_pair
{
    _View&  view;
    _State& state;
    _Ui&    ui;
};


// ===========================================================================
//  2.  VIEW STATE PAIR (no-ui specialization)
// ===========================================================================

// view_state_pair<_View, _State, void>
//   struct: specialization for the (view, state) two-member case.
// Used by database_table_view_pair, where the database state carries
// no per-renderer UI buffer.
template<typename _View,
          typename _State>
struct view_state_pair<_View, _State, void>
{
    _View&  view;
    _State& state;
};


// ===========================================================================
//  3.  TRAITS
// ===========================================================================

NS_INTERNAL

    // is_view_state_pair_helper
    //   trait: primary template, false for non-pair types.
    template<typename>
    struct is_view_state_pair_helper : std::false_type
    {};

    // is_view_state_pair_helper specializations
    //   trait: positive specializations for the two-member and
    // three-member view_state_pair forms.
    template<typename _View,
              typename _State>
    struct is_view_state_pair_helper<view_state_pair<_View, _State, void>>
        : std::true_type
    {};

    template<typename _View,
              typename _State,
              typename _Ui>
    struct is_view_state_pair_helper<view_state_pair<_View, _State, _Ui>>
        : std::true_type
    {};

NS_END  // internal

// is_view_state_pair_v
//   trait: true iff `_Type` is an instantiation of view_state_pair.
template<typename _Type>
inline constexpr bool is_view_state_pair_v =
    internal::is_view_state_pair_helper<_Type>::value;


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_VIEW_PAIR_
