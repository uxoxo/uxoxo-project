/******************************************************************************
* uxoxo [core]                                                      element.hpp
*
* The open signature functor: one node type, descriptors owned by their modules.
*   The monograph's signature functor is a sum over element types,
*
*     F X  =  sum_{tau in Ttyp} ( Attr_tau  x  X^{prof(tau)} ).
*
* Realized *openly*: rather than enumerate the summands in one place (a closed
* variant, where every element type is co-located and adding one edits the core),
* an element node is type-erased -- it carries a pointer to a per-type descriptor
* that its own module owns and registers, the way an object carries a vtable. The
* sum becomes existential ("some element type, with its attributes and children")
* and the per-type code lives in per-type modules that know nothing of each other.
* This is the runtime-open counterpart of how std::swap, or djinterp's own
* functor_traits, distribute their specializations: visible only when the type's
* module is included, never gathered into a master list.
*
*   What makes this clean is that the *functorial* structure is uniform. The
* functor action F(f) only ever touches the X-positions (the children); the
* element type tau is identity carried alongside, and its arity / attribute schema
* / behavior are descriptor data, not part of the map. So the children are a plain
* vector<X>, the functor instance is written once over the node and names no
* element type, and free.hpp / cofree.hpp build on it directly. The price -- the
* honest cost of openness over the closed variant -- is that per-type child arity
* is a descriptor rule checked at build time rather than a static guarantee, and
* the cascadable data rides in the option_set rather than typed fields. Both were
* already true once attributes became an option set.
*
*   Rendering inherits the expression problem: a backend cannot match on an open
* set of types. The node's descriptor therefore declares a small, platform-
* agnostic render_role -- the element's render contribution -- and a backend
* interprets roles, not types. A new element reusing existing roles costs a
* backend nothing; a new backend interprets the role vocabulary without knowing
* any element. render_role is the one deliberate shared contract between elements
* and platforms.
*
*   This header knows no element type. Concrete types (label, button, row, ...)
* are separate modules; see element_label.hpp and its siblings for the pattern.
*
* path:      /inc/uxoxo/element.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/

/*
TABLE OF CONTENTS
=================
I.    RENDER CONTRIBUTION                           (render_role -- the contract)
II.   THE ELEMENT TYPE DESCRIPTOR                    (element_type)
III.  THE OPEN SIGNATURE FUNCTOR                     (element_node<X> + functor)
IV.   THE HOLE AND THE TEMPLATE                       (free<element_node, hole>)
V.    THE GENERIC BUILDER                             (make_element -- for modules)
*/


#ifndef UXOXO_ELEMENT_
#define UXOXO_ELEMENT_ 1

// std
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/functional/free.hpp>
#include <djinterp/core/functional/functor.hpp>
#include <djinterp/core/option/option_record.hpp>  // option_record, overlay
// uxoxo
#include "./uxoxo.hpp"


NS_UXOXO


// option_set
//   type: an element's attribute record -- djinterp's runtime option record,
// brought into uxoxo under the name the element vocabulary uses. djinterp's
// own option_set is the compile-time schema template, so this is an alias of
// option_record rather than a using-declaration of that name.
using option_set = ::djinterp::option_record;


///////////////////////////////////////////////////////////////////////////////
///             I.    RENDER CONTRIBUTION  (the archetype token)            ///
///////////////////////////////////////////////////////////////////////////////
//   The render contribution is an OPEN token: a short string naming the
// rendering archetype an element falls back to when a backend has no bespoke
// renderer registered for that exact element type. It is the fallback half of
// the hybrid dispatch in realize.hpp -- a backend tries its per-type override
// table first (keyed by descriptor identity), and only on a miss interprets this
// token. So the token is a baseline, not a ceiling: an element that wants a real
// native control gets a per-(type, platform) override; everything else degrades
// to its archetype.
//
//   Open means a new archetype is a new string -- declared wherever it is first
// needed, touching no core file. Backends interpret the archetypes they know and
// degrade gracefully on the rest (the cost of openness: no compile-time
// exhaustiveness; a backend silently degrades rather than failing to compile, so
// a backend that wants the old safety keeps its own closed switch internally).
//
//   The names below are conveniences for the common archetypes, gathered here
// only so modules and backends spell them identically -- NOT a closed set. Any
// string is a valid archetype; these are merely the ones the bundled backends
// understand out of the box.

// archetype_*
//   constants: conventional names for the common render archetypes. Optional;
// any string is a valid token.
inline constexpr const char* archetype_text_leaf   = "text_leaf";    // atomic text, no children
inline constexpr const char* archetype_container   = "container";    // lays children out together
inline constexpr const char* archetype_interactive = "interactive";  // a surface wrapping content


///////////////////////////////////////////////////////////////////////////////
///             II.   THE ELEMENT TYPE DESCRIPTOR  (element_type)           ///
///////////////////////////////////////////////////////////////////////////////

// event
//   class: an input delivered to an element at runtime -- a named kind and an
// optional payload (e.g. {"setText", {value: "..."}}). The I of a Mealy step.
struct event
{
    std::string kind;       // what happened: "click", "toggle", "increment", ...
    option_set  payload;    // optional accompanying data
};

// handler_fn
//   type: an element's Mealy handler -- given its current state and an event,
// produce its next state. Pure (no effects); emitting outputs / commands from
// the step is the next layer up. Empty for elements with no behavior.
using handler_fn = std::function<option_set(
    const option_set& /*state*/, const event& /*ev*/)>;

// element_type
//   class: the descriptor for one element type tau -- everything generic
// machinery needs to treat a node of that type without knowing the type
// statically. Each element module owns exactly one of these (a static), and
// nodes reference it by pointer; pointer identity distinguishes types, so no
// central enum or tag registry is needed. The descriptor is the open analog of
// a constructor declaration: meaning entered once, here, in the type's module.
//
//   delta = (tau, A, S, E, rho): identity and arity; the attribute record A
// (defaults); the state space S (initial_state); and the behavior rho -- here
// the render archetype and the Mealy handler. The capability set and richer
// event typing join this record next, by the same pattern. The behavioral
// fields default to empty, so a purely structural element simply omits them and
// existing descriptors are unchanged.
struct element_type
{
    std::string name;           // identity for debugging / serialization
    std::string archetype;      // the render-contribution token (open; section I)
    option_set  defaults;       // the default attribute record A_tau
    int         min_children;   // arity floor   (prof(tau) lower bound)
    int         max_children;   // arity ceiling (-1 for unbounded)
    option_set  initial_state;  // the initial state S_tau (empty = stateless)
    handler_fn  handler;        // the Mealy step (state, event) -> state (empty = inert)
};


///////////////////////////////////////////////////////////////////////////////
///             III.  THE OPEN SIGNATURE FUNCTOR  (element_node<X>)         ///
///////////////////////////////////////////////////////////////////////////////

// element_node
//   functor: the signature functor F, realized openly -- one layer of the
// element tree. It names which element type it is (by descriptor pointer), its
// attribute record, and its X-children in order. _X is the child position. The
// node is type-erased over the element type: it can hold any type whose module
// is linked, with no compile-time enumeration.
template<typename _X>
struct element_node
{
    const element_type* type;       // which constructor (a module's descriptor)
    option_set          attrs;      // A_tau
    std::vector<_X>     children;   // the X-positions
};


///////////////////////////////////////////////////////////////////////////////
//   element_node<X> is a Functor: map applies f : X -> Y to every child and
// leaves the descriptor pointer and the attribute record alone. Because the
// children are uniform, this is written once and names no element type -- the
// whole point of the open encoding. free.hpp / cofree.hpp reach the children
// only through this map. The specialization is opened in djinterp (the namespace
// of the primary functor_traits) and names the uxoxo node by qualification.

NS_END  // uxoxo


NS_DJINTERP

    // functor_traits<uxoxo::element_node<_X>>
    template<typename _X>
    struct functor_traits< ::uxoxo::element_node<_X>, void>
    {
        using is_specialized = std::true_type;
        using value_type     = _X;

        template<typename _To>
        using rebind = ::uxoxo::element_node<_To>;

        template<typename _Shape,
                 typename _Function>
        static
        auto map(
            _Shape&&  _node,
            _Function _function
        )
        -> ::uxoxo::element_node<typename std::decay<decltype(
               _function(std::declval<const _X&>()))>::type>
        {
            using mapped_t = typename std::decay<decltype(
                _function(std::declval<const _X&>()))>::type;

            ::uxoxo::element_node<mapped_t> result;
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
//   A template is the free monad over the signature: free<element_node, hole>,
// with holes (unfilled child slots) at the Pure leaves and committed nodes at the
// Roll layers. Closed when it has no holes; a template with holes is filled by
// the closing pass before it is lowered. The structural half of that closing is
// just free_bind -- grafting a subtree onto each hole.

// hole
//   type: an unfilled child slot -- the Pure leaf of free<F, A>. Carries a slot
// name so the closing pass and a backend can identify it.
struct hole
{
    std::string slot;
};

// element_template
//   type: a UI template -- the free monad over the open signature functor, with
// holes at the leaves. The monograph's "component with unfilled children." Fully
// qualified: a djinterp carrier over the uxoxo signature.
using element_template = ::djinterp::free<element_node, hole>;

// child_ptr
//   type: a template child behind the shared_ptr free carries its recursion
// through.
using child_ptr = std::shared_ptr<element_template>;


///////////////////////////////////////////////////////////////////////////////
///             V.    THE GENERIC BUILDER  (make_element)                  ///
///////////////////////////////////////////////////////////////////////////////
//   The one constructor element modules call. It hides the Roll / shared_ptr
// wrapping and is generic over the element type (it takes the descriptor as a
// parameter), so it names no element type either. A module's typed builder
// (component::label, ...) resolves attributes and children, then defers here.

/*
make_element
  Builds a committed element node of the given type.

Parameter(s):
  _type:     the element type's descriptor (from its module).
  _attrs:    the resolved attribute record for this node.
  _children: the child templates, in order.
Return:
  An element_template wrapping a single node of _type over _children.
*/
D_NODISCARD
inline element_template make_element(
    const element_type*           _type,
    option_set                    _attrs,
    std::vector<element_template> _children
)
{
    std::vector<child_ptr> wrapped;
    wrapped.reserve(_children.size());

    for (const element_template& child : _children)
    {
        wrapped.push_back(std::make_shared<element_template>(child));
    }

    element_node<child_ptr> node;
    node.type     = _type;
    node.attrs    = std::move(_attrs);
    node.children = std::move(wrapped);

    return element_template::roll(node);
}

/*
hole_at
  Builds an unfilled slot -- a Pure leaf -- to be filled by the closing pass.

Parameter(s):
  _slot: the slot name, matched against a closing environment.
Return:
  An open element_template consisting of a single hole.
*/
D_NODISCARD
inline element_template hole_at(
    const std::string& _slot
)
{
    return element_template::pure(hole{ _slot });
}


NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_
