/******************************************************************************
* uxoxo [component]                                        tree_view_template.hpp
*
* The `tree_view` component: a scrollable view over one or more node trees.
*   A self-contained module owning the view container -- ignorant of every other
* component. A tree_view is a list of root children, each a tree_view_node
* (tree_view_node_template.hpp) whose own descendants carry the rest of the
* tree; the view supplies view-level policy (how selection behaves) and lays the
* roots out. It stays decoupled from the node type -- it just takes children --
* even though it is, in use, built from nodes.
*
*   Render archetype: tree_view (declared here). A backend flattens the visible
* nodes and draws rows with a scroll region; a backend that does not know it
* degrades.
*
*   Attribute keys: "selection_mode" (string: "single"/"multi"/"none"). Arity:
* 0..* (the root nodes).
*
*   Declarative form: the old view carried a great deal of *runtime* state -- a
* flat cursor, scroll offset, page size, the selected-index set, the search
* query, and the visible-entries cache -- plus all the navigation (cursor up /
* down / left / right, page up / down, expand-collapse). None of that is here:
* which rows are visible, where the cursor is, and what is selected are computed
* over the live tree by the behaviour and render layers. What remains is the
* structure and the one durable policy, "selection_mode".
*
* USAGE:
*   using namespace uxoxo;
*   component_template tv = component::tree_view(
*       { component::tree_view_node("Project",
*             { component::tree_view_node("README.md"),
*               component::tree_view_node("src") }) },
*       "single");
*
* path:      /inc/uxoxo/tree_view_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_TREE_VIEW_TEMPLATE_
#define UXOXO_TREE_VIEW_TEMPLATE_ 1

// std
#include <string>
#include <utility>
#include <vector>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_tree_view
//   constant: the render archetype for a scrollable tree view over root nodes.
// Declared here (open-archetype pattern), at uxoxo:: root.
inline constexpr const char* archetype_tree_view = "tree_view";


NS_COMPONENT

    // tree_view_type
    //   the descriptor for the tree view container, owned by this module.
    D_NODISCARD
    inline const component_type* tree_view_type()
    {
        static const component_type descriptor =
            component_type{
                "tree_view",
                archetype_tree_view,
                option_set{ { { "selection_mode", ::djinterp::option_value(std::string("single")) } } },
                0,
                -1
            };

        return &descriptor;
    }

    /*
    tree_view
      Builds a `tree_view` template laying out its root node children in order.

    Parameter(s):
      _roots:          the root node templates, in order (typically
                       tree_view_node nodes).
      _selection_mode: how selection behaves -- "single" / "multi" / "none"
                       ("selection_mode"); defaults to "single".
      _attrs:          optional extra attributes, overlaid on top (caller wins).
    Return:
      A component_template wrapping a tree_view node over _roots. Closed iff
      every root is closed.
    */
    D_NODISCARD
    inline component_template tree_view(
        std::vector<component_template> _roots,
        const std::string&              _selection_mode = std::string("single"),
        option_set                      _attrs          = option_set{}
    )
    {
        option_set resolved = tree_view_type()->defaults;
        resolved.set("selection_mode", _selection_mode);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(tree_view_type(), resolved, std::move(_roots));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_TREE_VIEW_TEMPLATE_
