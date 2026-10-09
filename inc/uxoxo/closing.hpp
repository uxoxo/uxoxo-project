/******************************************************************************
* uxoxo [core]                                                     closing.hpp
*
* Closing a template: fill its holes, overlay its attributes.
*   A template is the free monad over the signature -- free<element_node, hole> --
* with holes (unfilled child slots) at the Pure leaves. Closing turns an open
* template into a runnable one by substituting a filler for each hole, and,
* optionally, overlaying instance attributes. Both halves are operations the free
* monad and the option companion already provide:
*
*     fill     substitute a filler template for each hole. This is exactly
*              monadic bind: free_bind grafts a continuation onto every Pure leaf,
*              and "the filler for this slot" is that continuation. A hole with no
*              filler returns itself (pure), so an unmatched slot stays open --
*              partial closing falls straight out, and fillers may themselves
*              carry holes, so closing composes and nests.
*
*     restyle  overlay an option_set onto a node's attributes via the cascade (+).
*              Onto the root, this is instance styling -- "this card, tighter";
*              onto every node, a blanket theme.
*
*   close is the two together: fill, then overlay onto the root. Each is a pure
* function of the template -- no rendering, no platform. The result is another
* template (closed, or still open if some holes went unfilled), so it feeds
* realize, or another round of closing, unchanged.
*
*   Filling is bind, so the monad laws hold here as facts about closing: closing
* with an empty filler set is the identity (bind with pure), and closing in two
* stages equals closing once with the composed environment (bind associativity).
*
* USAGE:
*   using namespace uxoxo;
*   element_template card = component::row({ component::label("Card:"),
*                                            hole_at("title"), hole_at("body") });
*   closing_env env;
*   env.set("title", component::label("Report"));
*   env.set("body",  component::label("..."));
*   env.overrides.set("spacing", 2L);          // instance styling, optional
*   element_template ready = close(card, env); // hole-free, restyled
*
*
* path:      /inc/uxoxo/closing.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.06.29
******************************************************************************/

/*
TABLE OF CONTENTS
=================
I.    THE CLOSING ENVIRONMENT                       (closing_env)
II.   FILLING HOLES                                  (fill -- via free_bind)
III.  ATTRIBUTE OVERLAY                              (restyle / restyle_tree)
IV.   CLOSE                                           (fill, then restyle root)
*/


#ifndef UXOXO_CLOSING_
#define UXOXO_CLOSING_ 1

// std
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/functional/free.hpp>
#include <djinterp/core/option/option_record.hpp>  // option_record, overlay
// uxoxo
#include "./element.hpp"
#include "./uxoxo.hpp"


NS_UXOXO


///////////////////////////////////////////////////////////////////////////////
///             I.    THE CLOSING ENVIRONMENT  (closing_env)               ///
///////////////////////////////////////////////////////////////////////////////

// closing_env
//   class: what closing draws from -- a filler template per hole slot, and an
// optional attribute overlay applied to the root at close. A slot with no filler
// is left open. (A computed environment -- slot -> maybe<template> -- would
// generalize the map; the map is the common concrete case.)
class closing_env
{
public:
    option_set overrides;   // root attribute overlay (plain data; set directly)

    // set
    //   register (or replace) the filler for a slot.
    void set(
        const std::string&      _slot,
        const element_template& _filler
    )
    {
        m_fillers.insert_or_assign(_slot, _filler);
    }

    // find
    //   the filler for a slot, or null when the slot is unfilled.
    D_NODISCARD
    const element_template* find(
        const std::string& _slot
    ) const
    {
        std::map<std::string, element_template>::const_iterator found =
            m_fillers.find(_slot);

        return (found == m_fillers.end()) ? nullptr : &found->second;
    }

private:
    // Kept private and reached through set()/find(): element_template is a free
    // monad with a private default constructor by design, so std::map::operator[]
    // -- which default-constructs the mapped value -- is unavailable. set() uses
    // insert_or_assign, which constructs in place and needs no default.
    std::map<std::string, element_template> m_fillers;
};


///////////////////////////////////////////////////////////////////////////////
///             II.   FILLING HOLES  (fill -- via free_bind)               ///
///////////////////////////////////////////////////////////////////////////////
//   Substituting a filler for each hole is monadic bind on the template. The
// continuation maps a hole's slot to its filler, or returns the hole unchanged
// when the slot is unfilled -- so unmatched holes survive and closing is partial
// by default.

NS_INTERNAL

    // hole_filler
    //   helper: the bind continuation -- look a hole's slot up in the closing
    // environment; substitute the filler if present, else leave the hole (pure)
    // in place.
    struct hole_filler
    {
        const closing_env* env;

        element_template operator()(
            const hole& _hole
        ) const
        {
            const element_template* filler = env->find(_hole.slot);

            if (filler)
            {
                return *filler;
            }

            return element_template::pure(_hole);   // unmatched slot stays open
        }
    };

NS_END  // internal


/*
fill
  Substitutes a filler template for each hole whose slot the environment fills,
leaving unmatched holes open. This is the structural half of closing -- monadic
bind, with slot lookup as the continuation. (Ignores the environment's attribute
overlay; that is close's job.)

Parameter(s):
  _template: the template to fill (may contain holes).
  _env:      the closing environment (its fillers are consulted).
Return:
  The template with matched holes replaced. Closed iff every remaining hole was
  filled (and no filler introduced a new one).
*/
D_NODISCARD
inline element_template fill(
    const element_template& _template,
    const closing_env&      _env
)
{
    return ::djinterp::free_bind(
        _template,
        internal::hole_filler{ &_env });
}


///////////////////////////////////////////////////////////////////////////////
///             III.  ATTRIBUTE OVERLAY  (restyle / restyle_tree)          ///
///////////////////////////////////////////////////////////////////////////////
//   Overlaying an option_set onto node attributes via the cascade (+), the
// overlay winning. restyle touches the root (instance styling); restyle_tree
// touches every node (a blanket theme), rebuilt through fold_free. A selective,
// inherited cascade -- where children inherit unless they override -- is a
// further top-down traversal, deferred.

/*
restyle
  Overlays attributes onto the template's root node (the cascade, overlay wins).
Instance styling: customize this component without touching its interior.

Parameter(s):
  _template: the template whose root to restyle.
  _overlay:  attributes to overlay onto the root.
Return:
  The template with the root's attributes cascaded. A bare hole is returned
  unchanged (it has no attributes yet).
*/
D_NODISCARD
inline element_template restyle(
    const element_template& _template,
    const option_set&       _overlay
)
{
    if (_template.is_pure())
    {
        return _template;   // a bare hole; nothing to restyle
    }

    element_node<child_ptr> root = _template.layer();   // copy the root layer
    root.attrs = ::djinterp::overlay(root.attrs, _overlay);

    return element_template::roll(root);
}


NS_INTERNAL

    // restyle_pure
    //   helper: the leaf step of restyle_tree -- a hole passes through unchanged.
    struct restyle_pure
    {
        element_template operator()(
            const hole& _hole
        ) const
        {
            return element_template::pure(_hole);
        }
    };

    // restyle_impure
    //   helper: the node step of restyle_tree -- overlay onto this node's attrs
    // and re-roll over its already-rebuilt children.
    struct restyle_impure
    {
        const option_set* overlay;

        element_template operator()(
            const element_node<element_template>& _node
        ) const
        {
            std::vector<child_ptr> children;
            children.reserve(_node.children.size());

            for (const element_template& child : _node.children)
            {
                children.push_back(std::make_shared<element_template>(child));
            }

            element_node<child_ptr> rebuilt;
            rebuilt.type     = _node.type;
            rebuilt.attrs    = ::djinterp::overlay(_node.attrs, *overlay);
            rebuilt.children = std::move(children);

            return element_template::roll(rebuilt);
        }
    };

NS_END  // internal


/*
restyle_tree
  Overlays attributes onto every node of the template (a blanket cascade), holes
left in place. Rebuilds the tree through fold_free.

Parameter(s):
  _template: the template to restyle throughout.
  _overlay:  attributes to overlay onto each node.
Return:
  The template with every node's attributes cascaded.
*/
D_NODISCARD
inline element_template restyle_tree(
    const element_template& _template,
    const option_set&       _overlay
)
{
    return ::djinterp::fold_free(
        _template,
        internal::restyle_pure{},
        internal::restyle_impure{ &_overlay });
}


///////////////////////////////////////////////////////////////////////////////
///             IV.   CLOSE  (fill, then restyle root)                     ///
///////////////////////////////////////////////////////////////////////////////

/*
close
  Closes a template against an environment: fills its holes, then overlays the
environment's root attributes. The composition of fill and restyle.

Parameter(s):
  _template: the template to close (may contain holes).
  _env:      fillers per slot, and an optional root attribute overlay.
Return:
  The filled, restyled template -- ready for realize, or for further closing if
  any holes were left open.
*/
D_NODISCARD
inline element_template close(
    const element_template& _template,
    const closing_env&      _env
)
{
    element_template filled = fill(_template, _env);

    if (_env.overrides.entries.empty())
    {
        return filled;
    }

    return restyle(filled, _env.overrides);
}


NS_END  // uxoxo


#endif  // UXOXO_CLOSING_
