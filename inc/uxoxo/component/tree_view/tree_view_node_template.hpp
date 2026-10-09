/******************************************************************************
* uxoxo [component]                                   tree_view_node_template.hpp
*
* The `tree_view_node` component: a labelled node that holds child nodes.
*   A self-contained module owning one component type -- ignorant of every other
* -- and the framework's first *recursive* component: a node's children are more
* nodes, so a whole tree is one tree_view_node whose descendants are
* tree_view_nodes, exactly the nesting the free monad already gives us. No
* vector<self_type>, no per-node feature mixins: the children are ordinary
* template children, and expansion / check state ride in attributes.
*
*   Render archetype: tree_view_node (declared here). A backend draws the row --
* label, disclosure control from "expanded", optional check from "checked", and
* indents by depth (which it knows from the tree walk, not from the node). A
* backend that does not know it degrades.
*
*   Attribute keys: "text" (string, the node label), "expanded" (bool -- are
* children shown; default true), "checked" (bool), "enabled" (bool). Arity: 0..*
* (the child nodes). A leaf is simply a node with no children.
*
*   Declarative form: the old node's icon / expanded-icon / rename / context
* features and its checked-as-tri-state are gone from the core; "checked" is a
* plain bool here, and a tri-state parent-reflects-children value can return as
* a string key the way check_box models "state" if a tree needs it. Mutating
* the tree (expand, rename, re-check) is the behaviour layer's job over the live
* tree; this is the shape.
*
* USAGE:
*   using namespace uxoxo;
*   component_template tree = component::tree_view_node("src",
*       { component::tree_view_node("main.cpp"),
*         component::tree_view_node("include",
*             { component::tree_view_node("app.hpp") }) });
*
* path:      /inc/uxoxo/tree_view_node_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_TREE_VIEW_NODE_TEMPLATE_
#define UXOXO_TREE_VIEW_NODE_TEMPLATE_ 1

// std
#include <string>
#include <utility>
#include <vector>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_tree_view_node
//   constant: the render archetype for a single tree row that may hold child
// rows. Declared here (open-archetype pattern), at uxoxo:: root.
inline constexpr const char* archetype_tree_view_node = "tree_view_node";


NS_COMPONENT

    // tree_view_node_type
    //   the descriptor for a tree node, owned by this module.
    D_NODISCARD
    inline const component_type* tree_view_node_type()
    {
        static const component_type descriptor =
            component_type{
                "tree_view_node",
                archetype_tree_view_node,
                option_set{ { { "text",     ::djinterp::option_value(std::string()) },
                              { "expanded", ::djinterp::option_value(true) },
                              { "checked",  ::djinterp::option_value(false) },
                              { "enabled",  ::djinterp::option_value(true) } } },
                0,
                -1
            };

        return &descriptor;
    }

    /*
    tree_view_node
      Builds a `tree_view_node` template -- a labelled node over its child nodes.

    Parameter(s):
      _label:    the node's display text (stored under "text").
      _children: the child node templates, in order (typically more
                 tree_view_node nodes); empty for a leaf.
      _attrs:    optional extra attributes -- "expanded", "checked", "enabled" --
                 overlaid on top (caller wins).
    Return:
      A component_template wrapping a tree_view_node node over _children. Closed
      iff every child is closed.
    */
    D_NODISCARD
    inline component_template tree_view_node(
        const std::string&              _label,
        std::vector<component_template> _children = std::vector<component_template>{},
        option_set                      _attrs    = option_set{}
    )
    {
        option_set resolved = tree_view_node_type()->defaults;
        resolved.set("text", _label);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(tree_view_node_type(), resolved, std::move(_children));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_TREE_VIEW_NODE_TEMPLATE_
