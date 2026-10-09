/******************************************************************************
* uxoxo [core]                                                        live.hpp
*
* The running UI: the template's mirror image, state where the holes were.
*   A template is the free monad over the signature -- free<element_node, hole>,
* Pure-or-Roll, holes at the leaves. The *live* UI is its exact dual, the cofree
* comonad over the same signature:
*
*     live_node = cofree<element_node, option_set>   =   state :< element_node(children)
*
* Every node carries a head -- its runtime state -- and a layer of element_node
* (type, attributes, children) around it. Where free was sometimes a bare hole,
* cofree is always a value paired with a layer: a running node always has state
* and can always be read. Closing fills a template's holes; instantiate then
* turns that closed template into a live tree, seeding each node from its
* descriptor's initial_state. This is the bridge from the authoring side (free)
* to the running side (cofree).
*
*   An event is delivered to a node; the node's Mealy handler (its descriptor's
* handler) maps its current state and the event to a next state; dispatch returns
* a new tree with just that node's state replaced. State transition is pure, so
* the update is a value -- the previous tree is untouched, the new one shares
* everything unchanged. Emitting outputs / commands from a handler (the full
* Mealy step, returning a free<Omega, A> program to run) is the next layer; here
* a handler only moves state.
*
*   Reading the running UI is comonadic: extract is the head (a node's state),
* and extend re-decorates every node from its whole sub-tree -- which is the
* operation a per-frame platform render is (lower each node using its state and
* its descendants'). That render -- folding the live tree to a backend blueprint,
* the cofree analogue of realize -- is the next piece; show_live here is a
* minimal text view for observing state.
*
* USAGE:
*   using namespace uxoxo;
*   element_template ready = close(card, env);     // hole-free
*   live_node ui = instantiate(ready);             // state-annotated, dual tree
*   ui = dispatch(ui, { 0, 1 }, event{ "increment", {} });   // run a handler
*   std::string view = show_live(ui);              // observe state
*
*
* path:      /inc/uxoxo/live.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/

/*
TABLE OF CONTENTS
=================
I.    THE LIVE TREE                                 (live_node)
II.   INSTANTIATE                                    (closed template -> live tree)
III.  DISPATCH                                        (deliver an event, run handler)
IV.   OBSERVE                                         (show_live -- a text view)
*/


#ifndef UXOXO_LIVE_
#define UXOXO_LIVE_ 1

// std
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
// djinterp
#include <djinterp/core/functional/cofree.hpp>
#include <djinterp/core/functional/free.hpp>
// uxoxo
#include "./element.hpp"
#include "./uxoxo.hpp"


NS_UXOXO


///////////////////////////////////////////////////////////////////////////////
///             I.    THE LIVE TREE  (live_node)                            ///
///////////////////////////////////////////////////////////////////////////////

// live_node
//   type: a node of the running UI -- the cofree comonad over the signature
// functor, annotated with per-node state. The dual of element_template: where
// the template has holes at its leaves, the live tree has state at every node.
// Fully qualified -- a djinterp carrier over the uxoxo signature.
using live_node = ::djinterp::cofree<element_node, option_set>;

// live_layer
//   type: the element layer of a live node -- its type, attributes, and child
// live nodes (behind the shared_ptr the cofree carries its recursion through).
using live_layer = element_node<std::shared_ptr<live_node> >;


///////////////////////////////////////////////////////////////////////////////
///             II.   INSTANTIATE  (closed template -> live tree)          ///
///////////////////////////////////////////////////////////////////////////////
//   Folding a closed template into a live tree: each committed node becomes a
// cofree node whose head is its descriptor's initial_state. A fold_free whose
// algebra builds cofree nodes -- the authoring structure consumed, the running
// structure produced. A hole means the template was not closed, so the Pure leg
// is an error.

NS_INTERNAL

    // instantiate_pure
    //   helper: the leaf step -- a hole reaching instantiate means the template
    // was not closed first.
    struct instantiate_pure
    {
        live_node operator()(
            const hole& _hole
        ) const
        {
            throw std::logic_error(
                "uxoxo::instantiate: template has an unfilled hole '"
                + _hole.slot + "'");
        }
    };

    // instantiate_impure
    //   helper: the node step -- build a live node, head seeded from the
    // descriptor's initial_state, over its already-instantiated children.
    struct instantiate_impure
    {
        live_node operator()(
            const element_node<live_node>& _node
        ) const
        {
            std::vector<std::shared_ptr<live_node> > children;
            children.reserve(_node.children.size());

            for (const live_node& child : _node.children)
            {
                children.push_back(std::make_shared<live_node>(child));
            }

            live_layer layer;
            layer.type     = _node.type;
            layer.attrs    = _node.attrs;
            layer.children = std::move(children);

            option_set state =
                (_node.type != nullptr) ? _node.type->initial_state : option_set{};

            return live_node::make(state, layer);
        }
    };

NS_END  // internal


/*
instantiate
  Folds a closed template into a live tree, seeding each node's state from its
descriptor's initial_state.

Parameter(s):
  _closed: a hole-free template (close it first).
Return:
  The live tree -- the same structure, state at every node.
Throws:
  std::logic_error if the template still contains a hole.
*/
D_NODISCARD
inline live_node instantiate(
    const element_template& _closed
)
{
    return ::djinterp::fold_free(
        _closed,
        internal::instantiate_pure{},
        internal::instantiate_impure{});
}


///////////////////////////////////////////////////////////////////////////////
///             III.  DISPATCH  (deliver an event, run handler)            ///
///////////////////////////////////////////////////////////////////////////////
//   Delivering an event to the node at a path (child indices from the root) and
// running its Mealy handler. The handler maps (state, event) -> state; dispatch
// returns a new tree with that node's head replaced and everything else shared.
// A node with no handler is inert -- its state is unchanged.

/*
dispatch
  Delivers an event to the node reached by following _path (child indices from
the root), running that node's handler, and returns the updated tree.

Parameter(s):
  _tree:  the live tree.
  _path:  child indices from the root to the target node (empty = the root).
  _event: the event to deliver.
Return:
  A new tree with the target node's state advanced by its handler (or the tree
  unchanged if the target has no handler, or the path runs off the tree).
*/
D_NODISCARD
inline live_node dispatch(
    const live_node&                _tree,
    const std::vector<std::size_t>& _path,
    const event&                    _event
)
{
    const live_layer& layer = _tree.unwrap();

    if (_path.empty())
    {
        option_set next = _tree.head();

        // run the target's Mealy handler, if it has one
        if ((layer.type != nullptr) && (layer.type->handler))
        {
            next = layer.type->handler(_tree.head(), _event);
        }

        return live_node::make(next, layer);
    }

    std::size_t index = _path.front();

    if (index >= layer.children.size())
    {
        return _tree;   // path runs off the tree: unchanged
    }

    live_layer               rebuilt = layer;   // copy; replace one child
    std::vector<std::size_t> rest(_path.begin() + 1, _path.end());

    rebuilt.children[index] = std::make_shared<live_node>(
        dispatch(*layer.children[index], rest, _event));

    return live_node::make(_tree.head(), rebuilt);
}


///////////////////////////////////////////////////////////////////////////////
///             IV.   OBSERVE  (show_live -- a text view)                  ///
///////////////////////////////////////////////////////////////////////////////
//   A minimal text view of the running tree, name{state}(children), for seeing
// state. Not the platform render (that folds the live tree to a backend
// blueprint, the cofree analogue of realize) -- just a debug observer.

NS_INTERNAL

    // show_value_visitor
    //   helper: render one option_value to text.
    struct show_value_visitor
    {
        std::string operator()(bool _b)               const { return _b ? "true" : "false"; }
        std::string operator()(long _n)               const { return std::to_string(_n); }
        std::string operator()(double _d)             const { return std::to_string(_d); }
        std::string operator()(const std::string& _s) const { return _s; }
    };

NS_END  // internal


/*
show_live
  Renders the running tree to text -- name{state}(children) -- for observation.

Parameter(s):
  _tree: the live tree.
Return:
  A text view showing each node's name, state, and children.
*/
D_NODISCARD
inline std::string show_live(
    const live_node& _tree
)
{
    const live_layer& layer = _tree.unwrap();

    std::string out = (layer.type != nullptr) ? layer.type->name : std::string("?");

    // state, if any: name{k=v,...}
    if (!_tree.head().entries.empty())
    {
        out += "{";
        bool first = true;

        for (const std::pair<const std::string, ::djinterp::option_value>& entry :
             _tree.head().entries)
        {
            if (!first)
            {
                out += ",";
            }
            first = false;

            out += entry.first + "="
                 + std::visit(internal::show_value_visitor{}, entry.second);
        }

        out += "}";
    }

    // children, if any: ...(child child ...)
    if (!layer.children.empty())
    {
        out += "(";

        for (std::size_t i = 0; i < layer.children.size(); ++i)
        {
            if (i != 0)
            {
                out += " ";
            }

            out += show_live(*layer.children[i]);
        }

        out += ")";
    }

    return out;
}


NS_END  // uxoxo


#endif  // UXOXO_LIVE_
