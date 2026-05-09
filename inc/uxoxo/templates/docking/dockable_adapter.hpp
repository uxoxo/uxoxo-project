/*******************************************************************************
* uxoxo [component]                                         dockable_adapter.hpp
*
* Adapter for lifting non-dockable components into the dockable subsystem:
*   `dockable_adapter<_Inner, _WithPopup>` is an inheritance-based wrapper
* that takes any component template _Inner whose declaration does not
* already include the dockable members and produces a derived type that
* satisfies the full dockable contract.  Because the wrapper inherits
* publicly from _Inner, the inner's complete structural surface is
* preserved: every free function in component_common.hpp that worked on
* _Inner continues to work on the adapter, and the dockable verbs from
* dockable_common.hpp work on it too.
*
*   The popup mixin is opted into via the second template parameter.
* Defaults to true; pass false to skip the popup_open byte for
* components that will only ever use behavior-only dockability.
*
*   Inheritance was chosen over composition because composition would
* hide the inner's members behind an `inner.` accessor, breaking the
* framework's structural-conformance contract: `enable(adapter)`,
* `set_value(adapter, v)`, and every other shared verb would refuse to
* match the adapter unless we wrote per-member forwarding for each
* one.  Inheritance gets all of that for free.
*
*   Construction:
*     // direct, with default-constructed _Inner
*     dockable_adapter<my_panel> p;
*
*     // forward arguments to _Inner's constructor
*     auto p = make_dockable<my_panel>(arg1, arg2);
*
*     // skip popup_open for a leaner footprint
*     auto p = make_behavior_dockable<my_panel>(arg1, arg2);
*
*   The adapter is a zero-overhead transformation: every disabled mixin
* EBO-collapses to nothing, and the perfect-forwarding factory inlines
* into a direct construction of the adapter at the call site.
*
*   Pre-existing dockable members on _Inner are rejected at compile
* time by a single combined static_assert.  Users adapting a partially-
* dockable type (e.g. one already carrying popup_open) should remove
* the conflicting fields from _Inner first, since multi-inheriting
* both _Inner and the dockable mixins would yield an ambiguous member
* lookup.
*
* Contents:
*   1.  dockable_adapter           - the wrapper class template
*   2.  is_dockable_adapter        - structural detector for the wrapper
*   3.  make_dockable              - perfect-forwarding factory
*   4.  make_behavior_dockable     - factory for behavior-only adapters
*
*
* path:      /inc/uxoxo/templates/component/dockable/dockable_adapter.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCKABLE_ADAPTER_
#define  UXOXO_DOCKABLE_ADAPTER_ 1

// std
#include <type_traits>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./dockable_mixin.hpp"
#include "./dockable_traits.hpp"
#include "./dockable_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  1.  DOCKABLE ADAPTER
// ===========================================================================

// dockable_adapter
//   class: inheritance-based wrapper that promotes a non-dockable
// component template _Inner into the dockable subsystem by multi-
// inheriting from _Inner plus the dockable EBO mixins.  The
// _WithPopup non-type parameter selects whether the popup-open byte
// is included.
template<typename _Inner,
         bool     _WithPopup = true>
struct dockable_adapter
    : _Inner
    , component_mixin::dockable_data       <true>
    , component_mixin::dockable_popup_data <_WithPopup>
{
    using inner_type                 = _Inner;
    static constexpr bool with_popup = _WithPopup;

    static_assert(( !has_current_zone_v<_Inner>       &&
                    !has_target_zone_v<_Inner>        &&
                    !has_dock_requested_v<_Inner>     &&
                    !has_dock_policy_v<_Inner>        &&
                    !has_dock_presentation_v<_Inner>  &&
                    !has_popup_open_v<_Inner> ),
                  "dockable_adapter<_Inner>: _Inner already declares "
                  "one or more dockable members; use _Inner directly "
                  "or remove the conflicting fields before adapting.");

    dockable_adapter() = default;

    // dockable_adapter (forwarding constructor)
    //   function: forwards every argument to _Inner's constructor.
    // Constrained to non-empty argument packs that _Inner can actually
    // be constructed from, so the implicitly-generated copy and move
    // constructors remain the better match for adapter-to-adapter
    // copies.
    template<typename... _Args,
             std::enable_if_t<
                 ( sizeof...(_Args) > 0 &&
                   std::is_constructible_v<_Inner, _Args...> ),
                 int> = 0>
    explicit dockable_adapter(
            _Args&&... _args
        )
            : _Inner(std::forward<_Args>(_args)...)
        {}
};




// ===========================================================================
//  2.  IS_DOCKABLE_ADAPTER
// ===========================================================================
//   Structural detector reporting whether a type is (exactly) an
// instantiation of dockable_adapter.  Useful for generic code that
// wants to special-case adapted components - e.g. logging or
// debugging that displays the wrapped inner type.

NS_INTERNAL

    // is_dockable_adapter_helper
    //   trait: primary template, negative case.
    template<typename _T>
    struct is_dockable_adapter_helper : std::false_type
    {};

    // is_dockable_adapter_helper<dockable_adapter<...>>
    //   trait: positive specialization matched on adapter
    // instantiations.
    template<typename _Inner,
             bool     _WithPopup>
    struct is_dockable_adapter_helper<dockable_adapter<_Inner, _WithPopup>>
        : std::true_type
    {};

NS_END  // internal

// is_dockable_adapter_v
//   trait: true iff _T is an instantiation of dockable_adapter.
template<typename _T>
inline constexpr bool is_dockable_adapter_v =
    internal::is_dockable_adapter_helper<_T>::value;




// ===========================================================================
//  3.  MAKE_DOCKABLE
// ===========================================================================

/*
make_dockable
  Perfect-forwarding factory that constructs a dockable_adapter wrapping
_Inner, with popup support enabled.  The returned object is a value -
callers that need a long-lived dockable typically store it in a member
or container.

Parameter(s):
  _args: arguments forwarded to _Inner's constructor.  May be empty,
         in which case _Inner is default-constructed.
Return:
  A dockable_adapter<_Inner, true> initialized from _args.
*/
template<typename    _Inner,
         typename... _Args>
[[nodiscard]] constexpr auto
make_dockable(_Args&&... _args)
{
    return dockable_adapter<_Inner, true>(
               std::forward<_Args>(_args)...);
}




// ===========================================================================
//  4.  MAKE_BEHAVIOR_DOCKABLE
// ===========================================================================

/*
make_behavior_dockable
  Sibling of make_dockable that disables the popup mixin.  Use when the
component will never need to expose the popup-driven dock picker - the
adapter saves one byte and signals the intent at the type level.

Parameter(s):
  _args: arguments forwarded to _Inner's constructor.  May be empty.
Return:
  A dockable_adapter<_Inner, false> initialized from _args.
*/
template<typename    _Inner,
         typename... _Args>
[[nodiscard]] constexpr auto
make_behavior_dockable(_Args&&... _args)
{
    return dockable_adapter<_Inner, false>(
               std::forward<_Args>(_args)...);
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCKABLE_ADAPTER_
