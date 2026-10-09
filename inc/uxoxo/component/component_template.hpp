/******************************************************************************
* uxoxo [core]                                          component_template.hpp
*
* The component foundation: one open node type, descriptors owned by their
* modules.
*   The monograph's signature functor is a sum over component types,
*
*     F X  =  sum_{tau in Ttyp} ( Attr_tau  x  X^{prof(tau)} ).
*
* Realized *openly*: rather than enumerate the summands in one place (a closed
* variant, where every component type is co-located and adding one edits the
* core), a component node is type-erased -- it carries a pointer to a per-type
* descriptor that its own module owns and registers, the way an object carries
* a vtable. The sum becomes existential ("some component type, with its
* attributes and children"), and the per-type code lives in per-type modules
* that know nothing of each other. Adding a component is adding a module, never
* editing this file.
*
*   What makes this clean is that the *functorial* structure is uniform. The
* functor action F(f) only ever touches the X-positions (the children); the
* component type tau is identity carried alongside, and its arity / attribute
* schema are descriptor data, not part of the map. So the children are a plain
* vector<X>, the functor instance is written once over the node and names no
* component type, and djinterp's free.hpp / cofree.hpp build on it directly. The
* honest price of openness over a closed variant: per-type child arity is a
* descriptor rule checked downstream rather than a static guarantee, and the
* cascadable data rides in the option_set rather than typed fields.
*
*   Rendering inherits the expression problem: a backend cannot match on an
* open set of types. The node's descriptor therefore declares a small,
* platform-agnostic render archetype -- the component's render contribution --
* and a backend interprets archetypes, not types. A new component reusing
* existing archetypes costs a backend nothing; a new backend interprets the
* archetype vocabulary without knowing any component. The archetype is the one
* deliberate shared contract between components and platforms.
*
*   This header knows no component type, and no runtime behaviour. Concrete
* components (label, button, row, ...) are separate modules; state and the
* Mealy handler ride a later behaviour layer that extends the descriptor by
* this same per-module pattern. What lives here is exactly what it takes to
* *describe* a component and *render* it: the open node, its descriptor, the
* template (the free monad with holes over the node), and the one generic
* builder every component module defers to.
*
* USAGE (from a concrete component module -- e.g. component_button.hpp):
*   using namespace uxoxo;
*   // the module owns one descriptor (a static) ...
*   const component_type* button_type();
*   // ... and a typed builder that resolves attributes and defers here:
*   component_template t = make_component(button_type(), resolved_attrs,
*                                         { some_child });
*   // an unfilled slot, to be filled by a closing pass:
*   component_template slot = hole_at("title");
*
* path:      /inc/uxoxo/component_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

/*
TABLE OF CONTENTS
=================
I.    RENDER CONTRIBUTION                           (archetype -- the contract)
II.   THE COMPONENT DESCRIPTOR                        (component_type)
III.  THE OPEN SIGNATURE FUNCTOR                      (component_node<X> + functor)
IV.   THE HOLE AND THE TEMPLATE                        (free<component_node, hole>)
V.    THE GENERIC BUILDER                              (make_component -- for modules)
*/


#ifndef UXOXO_COMPONENT_TEMPLATE_
#define UXOXO_COMPONENT_TEMPLATE_ 1

// std
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/functional/free.hpp>
#include <djinterp/core/functional/functor.hpp>
#include <djinterp/core/option/option_record.hpp>
// uxoxo
#include "./uxoxo.hpp"


NS_UXOXO


// option_set
//   re-export: a component's attribute record is an option set (djinterp's
// options companion), brought into uxoxo so the component vocabulary names it
// without qualification. It is UI-agnostic and defined in djinterp; the cascade
// (overlay) and the reconciler diff apply to it there. djinterp calls the type
// option_record -- its option_set is the compile-time schema -- so uxoxo's
// name is an alias.
using option_set = ::djinterp::option_record;


///////////////////////////////////////////////////////////////////////////////
///             I.    RENDER CONTRIBUTION  (the archetype token)            ///
///////////////////////////////////////////////////////////////////////////////
//   The render contribution is an OPEN token: a short string naming the
// rendering archetype a component falls back to when a backend has no bespoke
// renderer registered for that exact component type. It is the fallback half of
// a backend's dispatch -- a backend tries its per-type overrides first (keyed by
// descriptor identity) and only on a miss interprets this token. So the token is
// a baseline, not a ceiling: a component that wants a real native control gets a
// per-(type, platform) override; everything else degrades to its archetype.
//
//   Open means a new archetype is a new string -- declared wherever it is first
// needed, touching no core file. Backends interpret the archetypes they know and
// degrade gracefully on the rest (the cost of openness: no compile-time
// exhaustiveness). The names below are conveniences for the common archetypes,
// gathered here only so modules and backends spell them identically -- NOT a
// closed set. Any string is a valid archetype; these are merely the ones the
// bundled backends understand out of the box.

// archetype_*
//   constants: conventional names for the common render archetypes. Optional;
// any string is a valid token.
inline constexpr const char* archetype_text_leaf   = "text_leaf";    // atomic text, no children
inline constexpr const char* archetype_container   = "container";    // lays children out together
inline constexpr const char* archetype_interactive = "interactive";  // a surface wrapping content


///////////////////////////////////////////////////////////////////////////////
///             II.   THE COMPONENT DESCRIPTOR  (component_type)             ///
///////////////////////////////////////////////////////////////////////////////

// component_type
//   class: the descriptor for one component type tau -- the metadata generic
// machinery needs to treat a node of that type without knowing the type
// statically. Each component module owns exactly one of these (a static), and
// nodes reference it by pointer; pointer identity distinguishes types, so no
// central enum or tag registry is needed. The descriptor is the open analog of
// a constructor declaration: meaning entered once, in the type's own module.
//
//   It carries the structural and render facts: identity, the render archetype,
// the default attribute record A_tau, and the child arity (prof). Runtime
// behaviour -- the initial state and the Mealy handler -- is a separate concern
// and extends this record from the behaviour layer, by the same per-module
// pattern; a purely structural component (a label, a row) needs nothing beyond
// what is here. When the behaviour fields are appended they carry default
// member initializers, so these structural descriptors stay unchanged.
struct component_type
{
    std::string  name;           // identity for debugging / serialization
    std::string  archetype;      // the render-contribution token (open; section I)
    option_set   defaults;       // the default attribute record A_tau
    int          min_children;   // arity floor   (prof(tau) lower bound)
    int          max_children;   // arity ceiling (-1 for unbounded)
};


///////////////////////////////////////////////////////////////////////////////
///             III.  THE OPEN SIGNATURE FUNCTOR  (component_node<X>)        ///
///////////////////////////////////////////////////////////////////////////////

// component_node
//   functor: the signature functor F, realized openly -- one layer of the
// component tree. It names which component type it is (by descriptor pointer),
// its attribute record, and its X-children in order. _X is the child position.
// The node is type-erased over the component type: it can hold any type whose
// module is linked, with no compile-time enumeration.
template<typename _X>
struct component_node
{
    const component_type* type;       // which constructor (a module's descriptor)
    option_set            attrs;      // A_tau
    std::vector<_X>       children;   // the X-positions
};


///////////////////////////////////////////////////////////////////////////////
//   component_node<X> is a Functor: map applies f : X -> Y to every child and
// leaves the descriptor pointer and the attribute record alone. Because the
// children are uniform, this is written once and names no component type -- the
// whole point of the open encoding. free.hpp / cofree.hpp reach the children
// only through this map. The specialization is opened in djinterp (the namespace
// of the primary functor_traits) and names the uxoxo node by qualification.

NS_END  // uxoxo


NS_DJINTERP

    // functor_traits<uxoxo::component_node<_X>>
    template<typename _X>
    struct functor_traits< ::uxoxo::component_node<_X>, void>
    {
        using is_specialized = std::true_type;
        using value_type     = _X;

        template<typename _To>
        using rebind = ::uxoxo::component_node<_To>;

        template<typename _Shape,
                 typename _Function>
        static
        auto map(
            _Shape&&  _node,
            _Function _function
        )
        -> ::uxoxo::component_node<typename std::decay<decltype(
               _function(std::declval<const _X&>()))>::type>
        {
            using mapped_t = typename std::decay<decltype(
                _function(std::declval<const _X&>()))>::type;

            ::uxoxo::component_node<mapped_t> result;
            result.type  = _node.type;
            result.attrs = _node.attrs;
            result.children.reserve(_node.children.size());

            // map each child position under f, preserving order
            for (const _X& child : _node.children)
            {
                result.children.push_back(_function(child));
            }

            return result;
        }
    };

NS_END  // djinterp


NS_UXOXO


///////////////////////////////////////////////////////////////////////////////
///             IV.   THE HOLE AND THE TEMPLATE                            ///
///////////////////////////////////////////////////////////////////////////////
//   A template is the free monad over the signature: free<component_node, hole>,
// with holes (unfilled child slots) at the Pure leaves and committed nodes at
// the Roll layers. Closed when it has no holes; a template with holes is filled
// by a closing pass before it is lowered. The structural half of that closing is
// just free_bind -- grafting a subtree onto each hole.

// hole
//   type: an unfilled child slot -- the Pure leaf of free<F, A>. Carries a slot
// name so a closing pass and a backend can identify it.
struct hole
{
    std::string slot;
};

// component_template
//   type: a UI template -- the free monad over the open signature functor, with
// holes at the leaves. The monograph's "component with unfilled children."
// Closed when it contains no holes. Fully qualified: a djinterp carrier over the
// uxoxo signature.
using component_template = ::djinterp::free<component_node, hole>;

// child_ptr
//   type: a template child behind the shared_ptr free carries its recursion
// through.
using child_ptr = std::shared_ptr<component_template>;


///////////////////////////////////////////////////////////////////////////////
///             V.    THE GENERIC BUILDER  (make_component)                ///
///////////////////////////////////////////////////////////////////////////////
//   The one constructor component modules call. It hides the Roll / shared_ptr
// wrapping and is generic over the component type (it takes the descriptor as a
// parameter), so it names no component type either. A module's typed builder
// (component::label, ...) resolves attributes and children, then defers here.

/*
make_component
  Builds a committed component node of the given type.

Parameter(s):
  _type:     the component type's descriptor (from its module).
  _attrs:    the resolved attribute record for this node.
  _children: the child templates, in order.
Return:
  A component_template wrapping a single node of _type over _children.
*/
D_NODISCARD
inline component_template make_component(
    const component_type*           _type,
    option_set                      _attrs,
    std::vector<component_template> _children
)
{
    std::vector<child_ptr> wrapped;
    wrapped.reserve(_children.size());

    // wrap each child in the shared_ptr free carries its recursion through
    for (const component_template& child : _children)
    {
        wrapped.push_back(std::make_shared<component_template>(child));
    }

    component_node<child_ptr> node;
    node.type     = _type;
    node.attrs    = std::move(_attrs);
    node.children = std::move(wrapped);

    return component_template::roll(node);
}

/*
hole_at
  Builds an unfilled slot -- a Pure leaf -- to be filled by a closing pass.

Parameter(s):
  _slot: the slot name, matched against a closing environment.
Return:
  An open component_template consisting of a single hole.
*/
D_NODISCARD
inline component_template hole_at(
    const std::string& _slot
)
{
    return component_template::pure(hole{ _slot });
}


NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_TEMPLATE_
