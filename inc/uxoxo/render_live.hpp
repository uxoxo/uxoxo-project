/******************************************************************************
* uxoxo [core]                                                 render_live.hpp
*
* Rendering the running UI: the cofree analogue of realize.
*   realize lowers a template -- free<element_node, hole> -- to a backend's
* blueprint by folding the free monad. render_live does the same for the live
* tree -- cofree<element_node, option_set> -- folding the cofree comonad. It is
* the operational form of the comonadic render the monograph names: a node is
* rendered from its already-rendered children (a catamorphism, bottom-up), which
* is what makes it O(n); the denotational "render = extend" re-decorates each
* node from its whole sub-tree and would recompute, so the fold is the
* implementation and extend the characterization. (cofree_extend with render_live
* would decorate every node with its own render -- the form a partial redraw
* wants later.)
*
*   State reaches the render as attributes. Each node's effective attributes are
* its declared attributes with its runtime state overlaid (state wins, via the
* same cascade (+) that styles templates) -- so a counter's count or a checkbox's
* checked, which live in state, are simply present in the attributes the backend
* reads. Crucially this means render_live and realize share one backend and one
* dispatch: the per-node step is render_node (override table first, archetype
* fallback), identical to the template render. A backend, and any per-type
* override, serves a static template and a running tree alike; the only
* difference is that the running tree's nodes carry state folded in.
*
*   The loop this closes: author a template, close it, instantiate it into a live
* tree, then each frame render_live the tree to a blueprint and dispatch events
* back into it -- the running UI drawn with its state, through the same platform
* backends as everything else.
*
*   (An element whose state should show through an archetype -- rather than a
* per-type override -- projects it into the keys that archetype reads; e.g. a
* counter setting "text". A descriptor-level state-to-render projection is the
* natural place for that, and a later refinement.)
*
* USAGE:
*   using namespace uxoxo;
*   live_node ui = instantiate(close(card, env));
*   std::string frame = render_live<platform::ascii_platform>(ui);
*   // ... dispatch events, render_live again ...
*
*
* path:      /inc/uxoxo/render_live.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/


#ifndef UXOXO_RENDER_LIVE_
#define UXOXO_RENDER_LIVE_ 1

// std
#include <memory>
#include <utility>
#include <vector>
// uxoxo
#include "./element.hpp"
#include "./live.hpp"
#include "./realize.hpp"
#include "./uxoxo.hpp"


NS_UXOXO


///////////////////////////////////////////////////////////////////////////////
///             THE LIVE RENDER  (render_live<Backend>)                    ///
///////////////////////////////////////////////////////////////////////////////

/*
render_live
  Renders the running UI to the given backend's blueprint by folding the live
tree bottom-up: each node's children are rendered first, its state is overlaid
onto its attributes (state wins), and the result runs through the same per-node
dispatch as realize (override table first, archetype fallback).

Parameter(s):
  _node: the live tree (or a sub-tree) to render.
Return:
  A value of _Backend::blueprint -- the platform's description of the running UI.
*/
template<typename _Backend>
D_NODISCARD
typename _Backend::blueprint
render_live
(
    const live_node& _node
)
{
    const live_layer& layer = _node.unwrap();

    // render children first (bottom-up)
    std::vector<typename _Backend::blueprint> rendered;
    rendered.reserve(layer.children.size());

    for (const std::shared_ptr<live_node>& child : layer.children)
    {
        rendered.push_back(render_live<_Backend>(*child));
    }

    // the effective node: state folded onto attributes, children already rendered
    element_node<typename _Backend::blueprint> effective;
    effective.type     = layer.type;
    effective.attrs    = ::djinterp::overlay(layer.attrs, _node.head());   // state wins
    effective.children = std::move(rendered);

    // identical dispatch to realize -- one backend, one override table, both renders
    return render_node<_Backend>{}(effective);
}


NS_END  // uxoxo


#endif  // UXOXO_RENDER_LIVE_
