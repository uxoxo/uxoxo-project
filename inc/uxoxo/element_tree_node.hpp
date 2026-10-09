/*******************************************************************************
* uxoxo [core]                                             element_tree_node.hpp
*
* The tree_node element: one collapsible node of a tree, over its children.
*   The node is controlled: "open" is whether it is expanded, and the
* application owns it. Opening or closing it reports "toggle_action" with
* the node's "value" and the new state under "open"; clicking it reports
* "action" with "value". "leaf" draws it without an arrow, "selected"
* highlights it, and "r", "g", "b", "a" (alpha above zero) draw a
* swatch before the text. With "drag_type" set, a node can be dragged onto
* another of the same type: the drop reports "drop_action" with the dragged
* node's value under "value" and the receiving node's under "target".
*
* path:      /inc/uxoxo/element_tree_node.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
*                                                            revised: 2026.10.04
*******************************************************************************/

#ifndef UXOXO_ELEMENT_TREE_NODE_HPP
#define UXOXO_ELEMENT_TREE_NODE_HPP 1

// std
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_tree_node
//   constant: the render archetype for a collapsible tree node.
inline constexpr const char* archetype_tree_node = "tree_node";


NS_COMPONENT

    // tree_node_type
    //   the descriptor for the tree_node element, owned by this module.
    // Unbounded arity: the children are the node's subtree.
    D_NODISCARD
    inline const element_type* tree_node_type()
    {
        static const element_type descriptor =
            element_type{
                "tree_node",
                archetype_tree_node,
                option_set{ { { "text",          ::djinterp::option_value(
                                                     std::string()) },
                              { "value",         ::djinterp::option_value(
                                                     0L) },
                              { "open",          ::djinterp::option_value(
                                                     false) },
                              { "leaf",          ::djinterp::option_value(
                                                     false) },
                              { "selected",      ::djinterp::option_value(
                                                     false) },
                              { "action",        ::djinterp::option_value(
                                                     std::string()) },
                              { "toggle_action", ::djinterp::option_value(
                                                     std::string()) },
                              { "drag_type",     ::djinterp::option_value(
                                                     std::string()) },
                              { "drop_action",   ::djinterp::option_value(
                                                     std::string()) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    tree_node
      Builds a `tree_node` template over its children.

    Parameter(s):
      _text:     the node's label ("text").
      _value:    the node's identity, reported with its events ("value").
      _children: the node's subtree, drawn while it is open.
      _attrs:    optional extra attributes (open, leaf, selected, action,
                 toggle_action, drag_type, drop_action, r, g, b, a),
                 overlaid on top (caller wins).
    Return:
      An element_template wrapping a tree_node node over _children.
    */
    D_NODISCARD
    inline element_template tree_node(
        const std::string&            _text,
        long                          _value,
        std::vector<element_template> _children,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = tree_node_type()->defaults;
        resolved.set("text", _text);
        resolved.set("value", _value);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(tree_node_type(), resolved, std::move(_children));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_TREE_NODE_HPP
