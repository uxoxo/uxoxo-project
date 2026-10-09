/******************************************************************************
* uxoxo [core]                                                     realize.hpp
*
* Lowering a template to a platform: a hybrid fold against an F-algebra.
*   A platform is an F-algebra out of the signature functor whose impure leg
* builds a node from its already-built children. Because the element set is OPEN,
* that leg cannot match on element type. It dispatches in two stages instead:
*
*     1. override   per-(element type, platform) renderer, keyed by descriptor
*                   identity -- a bespoke native control for one element on one
*                   platform. Registered by a pairing module that depends on both.
*     2. archetype  the element's render-contribution token (its descriptor's
*                   `archetype`), interpreted by the backend as a baseline. The
*                   fallback when no override is registered.
*
*   The framework composes these into the impure algebra (override first,
* archetype on a miss), so a backend supplies only its blueprint type, on_pure,
* and on_archetype, and a pairing registers overrides with set_renderer. Each
* backend then chooses where it sits on the spectrum: an immediate-mode backend
* registers no overrides and is pure archetype; a native-widget backend overrides
* the controls that must be real and lets archetypes cover the rest. One template
* serves both ends, since on_archetype : element_node<R> -> R is generic over the
* blueprint R -- a draw-thunk for immediate mode, a widget or description for
* retained mode.
*
*   realize is the catamorphism (fold_free with this algebra). It is pure -- the
* algebra allocates no platform objects -- so it is testable with no GUI and its
* result, a blueprint value, can be diffed, serialized, cached, or inspected.
* Instantiating real widgets and the per-frame redraw (the comonadic extend over
* Cofree) are separate passes downstream.
*
*   The override table self-registers from pairing modules; in a static library,
* an unreferenced pairing TU can be dropped (its renderer silently vanishing into
* a runtime fallback), so register from a TU that is linked, or call an explicit
* install_*() at startup.
*
* DEFINING A PLATFORM (archetype side):
*   struct my_platform
*   {
*       using blueprint = ...;                                      // R
*       struct on_pure      { blueprint operator()(const hole&)                 const; };
*       struct on_archetype { blueprint operator()(const element_node<blueprint>&) const; };
*   };
*
* OVERRIDING ONE ELEMENT ON ONE PLATFORM (pairing module):
*   set_renderer<my_platform>(some_element_type(), [](const element_node<...>& n){ ... });
*
* USAGE:
*   using namespace uxoxo;
*   element_template t = component::button(component::label("OK"));
*   std::string s = realize<platform::ascii_platform>(t);   // "[ OK ]"
*
*
* path:      /inc/uxoxo/realize.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/

/*
TABLE OF CONTENTS
=================
I.    DISPATCH MACHINERY                             (override table, realize)
II.   A WORKED BACKEND                                (platform::ascii_platform)
*/


#ifndef UXOXO_REALIZE_
#define UXOXO_REALIZE_ 1

// std
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
// djinterp
#include <djinterp/core/functional/free.hpp>
// uxoxo
#include "./element.hpp"
#include "./uxoxo.hpp"


NS_UXOXO


///////////////////////////////////////////////////////////////////////////////
///             I.    DISPATCH MACHINERY  (override table, realize)        ///
///////////////////////////////////////////////////////////////////////////////

// renderer
//   type: a per-(element type, platform) renderer -- builds a backend's blueprint
// from a node whose children are already built. The value stored in the override
// table and registered by pairing modules.
template<typename _Backend>
using renderer = std::function<
    typename _Backend::blueprint(
        const element_node<typename _Backend::blueprint>&)>;

// override_table
//   the per-backend table of bespoke renderers, keyed by descriptor identity.
// One table per backend type (a function-template static). Empty unless pairing
// modules register into it.
template<typename _Backend>
D_NODISCARD
std::map<const element_type*, renderer<_Backend> >& override_table()
{
    static std::map<const element_type*, renderer<_Backend> > table;
    return table;
}

/*
set_renderer
  Registers a bespoke renderer for one element type on one platform -- the
override that beats the archetype fallback. Called by a pairing module that
depends on both the element and the backend.

Parameter(s):
  _type:     the element type's descriptor (from its module).
  _renderer: how to build this platform's blueprint for that element.
*/
template<typename _Backend>
void set_renderer(
    const element_type* _type,
    renderer<_Backend>  _renderer
)
{
    override_table<_Backend>()[_type] = std::move(_renderer);
}

// render_node
//   the per-node hybrid dispatch, given a node whose children are already
// rendered: try the per-type override table first, fall back to the backend's
// archetype interpreter on a miss. Owned by the framework so backends never
// write the plumbing -- and shared by both renders: realize uses it as the
// free fold's impure leg, render_live as the cofree fold's node step.
template<typename _Backend>
struct render_node
{
    typename _Backend::blueprint operator()(
        const element_node<typename _Backend::blueprint>& _node
    ) const
    {
        std::map<const element_type*, renderer<_Backend> >& table =
            override_table<_Backend>();

        typename std::map<const element_type*, renderer<_Backend> >::const_iterator
            found = table.find(_node.type);

        if (found != table.end())
        {
            return found->second(_node);          // bespoke (type, platform) renderer
        }

        return typename _Backend::on_archetype{}(_node);   // archetype baseline
    }
};

/*
realize
  Lowers a UI template to the given backend's blueprint by folding the free monad
against the composed hybrid algebra. Run once, at build time; pure.

Parameter(s):
  _template: the template to lower -- a free<element_node, hole>. Holes go to the
             backend's on_pure; commit nodes through the hybrid impure algebra.
Return:
  A value of _Backend::blueprint -- the platform's description of the template.
*/
template<typename _Backend>
D_NODISCARD
typename _Backend::blueprint
realize
(
    const element_template& _template
)
{
    return ::djinterp::fold_free(
        _template,
        typename _Backend::on_pure{},
        render_node<_Backend>{});
}


///////////////////////////////////////////////////////////////////////////////
///             II.   A WORKED BACKEND  (platform::ascii_platform)         ///
///////////////////////////////////////////////////////////////////////////////
//   A GUI-free, immediate-mode-style backend: blueprint is std::string, it
// registers no overrides, and it renders purely by interpreting archetype tokens.
// It reads only the token, attribute keys, and already-built children -- never an
// element type -- and degrades gracefully on an archetype it does not know.

NS_PLATFORM

    // ascii_platform
    //   platform: a text backend driven entirely by archetype tokens.
    struct ascii_platform
    {
        using blueprint = std::string;

        // on_pure
        //   algebra: realize an unfilled slot as a visible placeholder.
        struct on_pure
        {
            std::string operator()(
                const hole& _hole
            ) const
            {
                return "<" + _hole.slot + ">";
            }
        };

        // on_archetype
        //   algebra: interpret the render-contribution token; degrade on unknown.
        struct on_archetype
        {
            std::string operator()(
                const element_node<std::string>& _node
            ) const
            {
                const std::string& archetype = _node.type->archetype;

                if (archetype == archetype_text_leaf)
                {
                    return _node.attrs.as_string("text", "");
                }

                if (archetype == archetype_interactive)
                {
                    std::string inner =
                        _node.children.empty()
                            ? std::string()
                            : _node.children.front();

                    // enabled surfaces in [ ], disabled in ( )
                    if (_node.attrs.as_bool("enabled", true))
                    {
                        return "[ " + inner + " ]";
                    }

                    return "( " + inner + " )";
                }

                if (archetype == archetype_container)
                {
                    return join(_node, _node.attrs.as_long("spacing", 1));
                }

                // unknown archetype: degrade -- show children if any, else text
                if (!_node.children.empty())
                {
                    return join(_node, 1);
                }

                return _node.attrs.as_string("text", "");
            }

        private:
            // join
            //   concatenate already-built children with a run of spaces between.
            static std::string join(
                const element_node<std::string>& _node,
                long                              _gap
            )
            {
                std::string separator(
                    static_cast<std::size_t>(_gap > 0 ? _gap : 0), ' ');
                std::string result;

                for (std::size_t i = 0; i < _node.children.size(); ++i)
                {
                    if (i != 0)
                    {
                        result += separator;
                    }

                    result += _node.children[i];
                }

                return result;
            }
        };
    };

NS_END  // platform


NS_END  // uxoxo


#endif  // UXOXO_REALIZE_
