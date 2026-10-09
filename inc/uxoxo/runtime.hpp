/******************************************************************************
* uxoxo [core]                                                     runtime.hpp
*
* The loop made concrete: render, read input, route it, dispatch, repeat.
*   instantiate gives a live tree; dispatch advances one node's state; render_live
* draws the tree. The runtime ties them into an actual cycle and adds the piece a
* real loop needs that manual dispatch did not: routing. It holds the live tree
* and a focus -- a path to the node currently receiving input -- and turns raw
* input into events:
*
*     "quit"  -> stop the loop
*     "tab"   -> move focus to the next focusable node (one with a handler)
*     else    -> deliver event{"key", {key: ...}} to the focused node
*
* The loop never interprets a key's *meaning*; it routes a generic key event and
* each element's Mealy handler decides what that key does (a counter reads "+",
* a checkbox reads " "). So the runtime stays element-agnostic, and an element
* owns its own input semantics, exactly as it owns its render archetype and state.
*
*   It is also backend- and input-agnostic. frame() renders the current tree
* (with the focused node marked, via an overlaid attribute the backend may read)
* to any backend's blueprint; run() drives the cycle from an input source and a
* present sink supplied by the caller. The same runtime therefore drives a real
* terminal (a getch reader, a screen-clearing printer) or a scripted test
* sequence, unchanged -- the loop is the loop regardless of where input comes
* from or where frames go.
*
* USAGE:
*   using namespace uxoxo;
*   runtime<platform::ascii_platform> app(instantiate(close(panel, env)));
*   app.run(read_key, print_frame);   // read_key yields commands, print_frame shows frames
*
*
* path:      /inc/uxoxo/runtime.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/

/*
TABLE OF CONTENTS
=================
I.    OVERLAY AT A PATH                              (overlay_at -- focus marking)
II.   THE RUNTIME                                    (runtime<Backend>)
*/


#ifndef UXOXO_RUNTIME_
#define UXOXO_RUNTIME_ 1

// std
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>
// uxoxo
#include "./element.hpp"
#include "./live.hpp"
#include "./render_live.hpp"
#include "./uxoxo.hpp"


NS_UXOXO


///////////////////////////////////////////////////////////////////////////////
///             I.    OVERLAY AT A PATH  (overlay_at)                      ///
///////////////////////////////////////////////////////////////////////////////
//   Overlay attributes onto the node at a path in a live tree, everything else
// shared. Used to mark the focused node for rendering (a transient "focused"
// attribute the backend may read), but it is a general targeted live transform,
// the attribute-side companion of dispatch.

/*
overlay_at
  Overlays attributes onto the node reached by following _path (child indices
from the root), via the cascade (overlay wins), and returns the updated tree.

Parameter(s):
  _tree:    the live tree.
  _path:    child indices from the root to the target node (empty = the root).
  _overlay: attributes to overlay onto the target node.
Return:
  A new tree with the target node's attributes cascaded (or unchanged if the path
  runs off the tree).
*/
D_NODISCARD
inline live_node overlay_at(
    const live_node&                _tree,
    const std::vector<std::size_t>& _path,
    const option_set&               _overlay
)
{
    const live_layer& layer = _tree.unwrap();

    if (_path.empty())
    {
        live_layer rebuilt = layer;
        rebuilt.attrs = ::djinterp::overlay(layer.attrs, _overlay);

        return live_node::make(_tree.head(), rebuilt);
    }

    std::size_t index = _path.front();

    if (index >= layer.children.size())
    {
        return _tree;
    }

    live_layer               rebuilt = layer;
    std::vector<std::size_t> rest(_path.begin() + 1, _path.end());

    rebuilt.children[index] = std::make_shared<live_node>(
        overlay_at(*layer.children[index], rest, _overlay));

    return live_node::make(_tree.head(), rebuilt);
}


///////////////////////////////////////////////////////////////////////////////
///             II.   THE RUNTIME  (runtime<Backend>)                      ///
///////////////////////////////////////////////////////////////////////////////

// runtime
//   class: a running UI. Holds the live tree and a focus, renders frames to a
// backend, and turns input commands into events (focus moves and key events
// routed to the focused node). Backend- and input-agnostic: frame() renders to
// _Backend::blueprint, run() drives the cycle from caller-supplied input and
// present callables.
template<typename _Backend>
class runtime
{
public:
    // runtime
    //   construct over an instantiated live tree; focus starts at the first
    // focusable node.
    explicit runtime(
        const live_node& _tree
    )
        : m_tree(_tree)
        , m_focus(0)
    {
        collect_focusables();
    }

    // tree
    //   the current live tree.
    D_NODISCARD
    const live_node& tree() const
    {
        return m_tree;
    }

    // frame
    //   render the current tree, the focused node marked with a "focused"
    // attribute the backend may read, to the backend's blueprint.
    D_NODISCARD
    typename _Backend::blueprint frame() const
    {
        if (m_focusables.empty())
        {
            return render_live<_Backend>(m_tree);
        }

        option_set mark;
        mark.set("focused", true);

        return render_live<_Backend>(
            overlay_at(m_tree, m_focusables[m_focus], mark));
    }

    // feed
    //   process one input command. Returns false on "quit" (stop the loop).
    // "tab" moves focus; any other command is delivered as a key event to the
    // focused node, whose handler interprets it.
    bool feed(
        const std::string& _command
    )
    {
        if (_command == "quit")
        {
            return false;
        }

        if (_command == "tab")
        {
            if (!m_focusables.empty())
            {
                m_focus = (m_focus + 1) % m_focusables.size();
            }

            return true;
        }

        if (!m_focusables.empty())
        {
            event key_event;
            key_event.kind = "key";
            key_event.payload.set("key", _command);

            m_tree = dispatch(m_tree, m_focusables[m_focus], key_event);
        }

        return true;
    }

    // run
    //   the loop: present the current frame, read a command, feed it, and repeat
    // until "quit". _input yields commands (std::string); _present consumes
    // frames (_Backend::blueprint).
    template<typename _Input,
             typename _Present>
    void run(
        _Input   _input,
        _Present _present
    )
    {
        _present(frame());

        while (true)
        {
            std::string command = _input();

            if (!feed(command))
            {
                break;
            }

            _present(frame());
        }
    }

private:
    live_node                              m_tree;
    std::vector<std::vector<std::size_t> > m_focusables;   // paths, tree order
    std::size_t                            m_focus;        // index into m_focusables

    // collect_focusables
    //   gather the paths of all focusable nodes (those with a handler), in tree
    // order.
    void collect_focusables()
    {
        m_focusables.clear();

        std::vector<std::size_t> path;
        collect(m_tree, path);

        if (m_focus >= m_focusables.size())
        {
            m_focus = 0;
        }
    }

    void collect(
        const live_node&          _node,
        std::vector<std::size_t>& _path
    )
    {
        const live_layer& layer = _node.unwrap();

        if ((layer.type != nullptr) && (layer.type->handler))
        {
            m_focusables.push_back(_path);
        }

        for (std::size_t i = 0; i < layer.children.size(); ++i)
        {
            _path.push_back(i);
            collect(*layer.children[i], _path);
            _path.pop_back();
        }
    }
};


NS_END  // uxoxo


#endif  // UXOXO_RUNTIME_
