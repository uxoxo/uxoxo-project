/*******************************************************************************
* uxoxo [component]                                               tree_node.hpp
*
*   A generic, hierarchical tree node template with zero-cost optional features
* controlled by a compile-time bitfield.
*
*   The user pays for exactly what they enable:
*
*     tree_node<string>                                56 bytes (data+children)
*     tree_node<string, tf_collapsible>                57 bytes (+1 bool)
*     tree_node<string, tf_collapsible | tf_checkable> 58 bytes (+1 enum)
*     tree_node<string, tf_all>                        ~80 bytes (everything)
*
*   Disabled features compile away to 0 bytes via Empty Base Optimisation.
* Each feature is a mixin base class that is either empty (disabled) or
* contains the feature's per-node data (enabled).
*
*   All mutating operations are free functions, not methods.  This keeps the
* node a pure data aggregate and matches the uxoxo trait-detection idiom:
* generic code discovers capabilities via tree_traits (in tree_view.hpp) and
* dispatches with `if constexpr`.
*
* Template parameters:
*   _Data:   user payload type  (e.g. std::string, file_entry, scene_object)
*   _Feat:   bitwise OR of tree_feat flags  (default: tf_none)
*   _Icon:   icon storage type when tf_icons is set  (default: int)
*
*
* path:      /inc/uxoxo/component/tree_node.hpp 
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.26
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_TREE_NODE_
#define  UXOXO_COMPONENT_TREE_NODE_ 1

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// imgui
#include "imgui.h"
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../view_common.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  FEATURE FLAGS
// ===============================================================================
//   tree_node reuses type_identity, check_state, check_policy, and
// context_action from view_common.hpp.  tree_feat extends view_feat
// with the tree-specific feature bits (tf_*).
//
//   Combine with bitwise OR:
//     tree_node<std::string, tf_collapsible | tf_checkable | tf_icons>

enum tree_feat : unsigned
{
    tf_none        = 0,
    tf_checkable   = 1u << 0,     // tri-state checkbox per node
    tf_icons       = 1u << 1,     // icon + optional expanded_icon per node
    tf_collapsible = 1u << 2,     // expanded/collapsed state per node
    tf_renamable   = 1u << 3,     // per-node renamable flag
    tf_context     = 1u << 4,     // per-node context-action descriptor

    tf_all         = tf_checkable   | 
                     tf_icons       | 
                     tf_collapsible | 
                     tf_renamable   | 
                     tf_context
};

constexpr tree_feat operator|(tree_feat a, tree_feat b) noexcept
{
    return static_cast<tree_feat>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr tree_feat operator&(tree_feat a, tree_feat b) noexcept
{
    return static_cast<tree_feat>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}

// has_feat is defined in view_common.hpp and works on view_feat.
// Overload it here for tree_feat so both feature sets can be tested
// with the same name.
constexpr bool has_feat(unsigned f, tree_feat bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


/*****************************************************************************/

// ===============================================================================
//  2  FEATURE ENUMERATIONS
// ===============================================================================
//   check_state, check_policy, and context_action are defined in
// view_common.hpp and reused here.


/*****************************************************************************/

// ===============================================================================
//  3  FEATURE MIXIN BASES  (EBO — disabled = 0 bytes)
// ===============================================================================
//   Each mixin is a struct template parameterised on a bool.  The `false`
// specialisation is empty; the `true` specialisation holds the feature data.
// When tree_node inherits from a disabled mixin, the Empty Base Optimisation
// guarantees zero storage overhead.

namespace mixin {

    // -- checkable --------------------------------------------------------

    template <bool _Enable>
    struct checkable_data {};

    template <>
    struct checkable_data<true>
    {
        check_state checked = check_state::unchecked;
    };

    // -- icons ------------------------------------------------------------

    template <bool _Enable,
              typename _Icon = int>
    struct icon_data {};

    template <typename _Icon>
    struct icon_data<true, _Icon>
    {
        _Icon icon{};
        _Icon expanded_icon{};      // shown when node is expanded (e.g. open folder)
        bool  use_expanded = false; // true -> renderer shows expanded_icon when open
    };

    // -- collapsible ------------------------------------------------------

    template <bool _Enable>
    struct collapse_data {};

    template <>
    struct collapse_data<true>
    {
        bool expanded = true;       // true -> children visible
    };

    // -- renamable --------------------------------------------------------

    template <bool _Enable>
    struct rename_data {};

    template <>
    struct rename_data<true>
    {
        bool renamable = true;      // per-node override (e.g. root not renamable)
    };

    // -- context menu -----------------------------------------------------

    template <bool _Enable>
    struct context_data {};

    template <>
    struct context_data<true>
    {
        unsigned context_actions = ctx_all;   // bitfield of context_action
    };

}   // namespace mixin


/*****************************************************************************/

// ===============================================================================
//  4  TREE NODE
// ===============================================================================

template <typename _Data,
          unsigned _Feat = tf_none,
          typename _Icon = int>
struct tree_node
    : mixin::checkable_data <has_feat(_Feat, tf_checkable)>
    , mixin::icon_data      <has_feat(_Feat, tf_icons), _Icon>
    , mixin::collapse_data  <has_feat(_Feat, tf_collapsible)>
    , mixin::rename_data    <has_feat(_Feat, tf_renamable)>
    , mixin::context_data   <has_feat(_Feat, tf_context)>
{
    // -- type aliases -----------------------------------------------------
    using data_type  = _Data;
    using icon_type  = _Icon;
    using self_type  = tree_node<_Data, _Feat, _Icon>;
    using child_list = std::vector<self_type>;

    static constexpr unsigned features = _Feat;

    // -- compile-time feature queries -------------------------------------
    static constexpr bool is_checkable   = has_feat(_Feat, tf_checkable);
    static constexpr bool has_icons      = has_feat(_Feat, tf_icons);
    static constexpr bool is_collapsible = has_feat(_Feat, tf_collapsible);
    static constexpr bool is_renamable   = has_feat(_Feat, tf_renamable);
    static constexpr bool has_context    = has_feat(_Feat, tf_context);

    // -- data -------------------------------------------------------------
    _Data       data;
    child_list  children;

    // -- construction -----------------------------------------------------
    tree_node() = default;

    explicit tree_node(_Data d) 
        : data(std::move(d)) 
    {}

    tree_node(_Data d, child_list kids) 
        : data(std::move(d))
        , children(std::move(kids)) 
    {}

    // -- basic queries ----------------------------------------------------
    [[nodiscard]] bool        is_leaf()     const noexcept { return children.empty(); }
    [[nodiscard]] std::size_t child_count() const noexcept { return children.size(); }

    // -- is_visible (for flatten_visible) ---------------------------------
    //   A node's children are visible if the node is expanded.
    //   If collapsible is disabled, children are always visible.
    [[nodiscard]] bool children_visible() const noexcept
    {
        if constexpr (is_collapsible)
            return this->expanded;
        else
        {
            return true;
        }
    }
};


/*****************************************************************************/

// ===============================================================================
//  5  NODE MUTATION
// ===============================================================================
//   Free functions.  Keep the node a pure aggregate.

// add_child
//   Appends a new child.  Returns a reference to the added node.
template <typename _Data,
          unsigned _F,
          typename _I>
tree_node<_Data, _F, _I>& add_child(
    tree_node<_Data, _F, _I>& parent,
    tree_node<_Data, _F, _I>  child)
{
    parent.children.push_back(std::move(child));
    return parent.children.back();
}

// emplace_child
//   Constructs a child in-place from data.  Returns a reference.
template <typename _Data,
          unsigned _F,
          typename _I>
tree_node<_Data, _F, _I>& emplace_child(
    tree_node<_Data, _F, _I>& parent,
    non_deduced<_Data> data)
{
    parent.children.emplace_back(std::move(data));
    return parent.children.back();
}

// remove_child
//   Removes the child at index.  Returns true if index was valid.
template <typename _Data,
          unsigned _F,
          typename _I>
bool remove_child(
    tree_node<_Data, _F, _I>& parent,
    std::size_t index)
{
    if (index >= parent.children.size())
    {
        return false;
    }
    parent.children.erase(parent.children.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

// remove_child_if
//   Removes all children matching a predicate.  Returns count removed.
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Pred>
std::size_t remove_child_if(
    tree_node<_Data, _F, _I>& parent,
    _Pred pred)
{
    auto& c = parent.children;
    auto it = std::remove_if(c.begin(), c.end(), pred);
    std::size_t n = static_cast<std::size_t>(std::distance(it, c.end()));
    c.erase(it, c.end());
    return n;
}


/*****************************************************************************/

// ===============================================================================
//  6  TRAVERSAL
// ===============================================================================

// walk
//   Depth-first pre-order traversal.  Calls fn(node, depth) for every node.
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Fn>
void walk(
    tree_node<_Data, _F, _I>& node,
    _Fn&& fn,
    std::size_t depth = 0)
{
    fn(node, depth);
    for (auto& child : node.children)
    {
        walk(child, fn, depth + 1);
    }
}

// walk (const)
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Fn>
void walk(
    const tree_node<_Data, _F, _I>& node,
    _Fn&& fn,
    std::size_t depth = 0)
{
    fn(node, depth);
    for (const auto& child : node.children)
    {
        walk(child, fn, depth + 1);
    }
}

// walk_visible
//   Like walk, but skips collapsed subtrees.
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Fn>
void walk_visible(
    tree_node<_Data, _F, _I>& node,
    _Fn&& fn,
    std::size_t depth = 0)
{
    fn(node, depth);
    if (node.children_visible())
    {
        for (auto& child : node.children)
        {
            walk_visible(child, fn, depth + 1);
        }
    }
}

// walk_visible (const)
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Fn>
void walk_visible(
    const tree_node<_Data, _F, _I>& node,
    _Fn&& fn,
    std::size_t depth = 0)
{
    fn(node, depth);
    if (node.children_visible())
    {
        for (const auto& child : node.children)
        {
            walk_visible(child, fn, depth + 1);
        }
    }
}

// walk_post
//   Depth-first post-order (children before parent).  Needed for 
// bottom-up check propagation.
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Fn>
void walk_post(
    tree_node<_Data, _F, _I>& node,
    _Fn&& fn,
    std::size_t depth = 0)
{
    for (auto& child : node.children)
    {
        walk_post(child, fn, depth + 1);
    }
    fn(node, depth);
}

// find_if
//   Returns pointer to first node (depth-first) matching predicate, or nullptr.
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Pred>
tree_node<_Data, _F, _I>* find_if(
    tree_node<_Data, _F, _I>& root,
    _Pred pred)
{
    if (pred(root))
    {
        return &root;
    }
    for (auto& child : root.children)
    {
        auto* found = find_if(child, pred);
        if (found)
        {
            return found;
        }
    }
    return nullptr;
}

// find_if (const)
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Pred>
const tree_node<_Data, _F, _I>* find_if(
    const tree_node<_Data, _F, _I>& root,
    _Pred pred)
{
    if (pred(root))
    {
        return &root;
    }
    for (const auto& child : root.children)
    {
        auto* found = find_if(child, pred);
        if (found)
        {
            return found;
        }
    }
    return nullptr;
}

// count_nodes
//   Total node count in the subtree (including root).
template <typename _Data,
          unsigned _F,
          typename _I>
std::size_t count_nodes(const tree_node<_Data, _F, _I>& root)
{
    std::size_t n = 1;
    for (const auto& child : root.children)
    {
        n += count_nodes(child);
    }
    return n;
}

// max_depth
//   Deepest level in the subtree (root = 0).
template <typename _Data,
          unsigned _F,
          typename _I>
std::size_t max_depth(const tree_node<_Data, _F, _I>& root, std::size_t d = 0)
{
    std::size_t m = d;
    for (const auto& child : root.children)
    {
        m = std::max(m, max_depth(child, d + 1));
    }
    return m;
}


/*****************************************************************************/

// ===============================================================================
//  7  FLATTEN
// ===============================================================================
//   Produces a flat list of node references suitable for indexed rendering.
//   Each entry records the node pointer, its depth, and tree-line hints.

template <typename _Node>
struct flat_entry
{
    _Node*      node;
    std::size_t depth;
    std::size_t flat_index;
    bool        is_last_child;  // for drawing └-- vs ├--
    bool        has_children;
};

// flatten
//   Flattens the entire subtree regardless of collapse state.
template <typename _Data,
          unsigned _F,
          typename _I>
std::vector<flat_entry<tree_node<_Data, _F, _I>>> flatten(
    tree_node<_Data, _F, _I>& root)
{
    using node_type = tree_node<_Data, _F, _I>;
    std::vector<flat_entry<node_type>> result;

    struct impl {
        static void go(node_type& n, std::size_t depth, bool last,
                       std::vector<flat_entry<node_type>>& out)
        {
            out.push_back({ &n, depth, out.size(), last, !n.is_leaf() });
            for (std::size_t i = 0; i < n.children.size(); ++i)
            {
                go(n.children[i], depth + 1, i == n.children.size() - 1, out);
            }
        }
    };

    impl::go(root, 0, true, result);
    return result;
}

// flatten_visible
//   Flattens only visible nodes (respects collapsed subtrees).
template <typename _Data,
          unsigned _F,
          typename _I>
std::vector<flat_entry<tree_node<_Data, _F, _I>>> flatten_visible(
    tree_node<_Data, _F, _I>& root)
{
    using node_type = tree_node<_Data, _F, _I>;
    std::vector<flat_entry<node_type>> result;

    struct impl {
        static void go(node_type& n, std::size_t depth, bool last,
                       std::vector<flat_entry<node_type>>& out)
        {
            out.push_back({ &n, depth, out.size(), last, !n.is_leaf() });
            if (n.children_visible())
            {
                for (std::size_t i = 0; i < n.children.size(); ++i)
                {
                    go(n.children[i],
                       depth + 1,
                       i == n.children.size() - 1,
                       out);
                }
            }
        }
    };

    impl::go(root, 0, true, result);
    return result;
}

// flatten_roots
//   Flattens multiple root nodes (for tree_view with a forest).
template <typename _Data,
          unsigned _F,
          typename _I>
std::vector<flat_entry<tree_node<_Data, _F, _I>>> flatten_roots_visible(
    std::vector<tree_node<_Data, _F, _I>>& roots)
{
    using node_type = tree_node<_Data, _F, _I>;
    std::vector<flat_entry<node_type>> result;

    struct impl {
        static void go(node_type& n, std::size_t depth, bool last,
                       std::vector<flat_entry<node_type>>& out)
        {
            out.push_back({ &n, depth, out.size(), last, !n.is_leaf() });
            if (n.children_visible())
            {
                for (std::size_t i = 0; i < n.children.size(); ++i)
                {
                    go(n.children[i],
                       depth + 1,
                       i == n.children.size() - 1,
                       out);
                }
            }
        }
    };

    for (std::size_t i = 0; i < roots.size(); ++i)
    {
        impl::go(roots[i], 0, i == roots.size() - 1, result);
    }
    return result;
}


/*****************************************************************************/

// ===============================================================================
//  8  PATH-BASED ACCESS
// ===============================================================================
//   A tree_path is a sequence of child indices from root to target.
//     e.g. {2, 0, 1} -> root.children[2].children[0].children[1]

using tree_path = std::vector<std::size_t>;

// node_at_path
//   Follows a path from a node.  Returns nullptr if any index is out of range.
template <typename _Data,
          unsigned _F,
          typename _I>
tree_node<_Data, _F, _I>* node_at_path(
    tree_node<_Data, _F, _I>& root,
    const tree_path& path)
{
    auto* cur = &root;
    for (auto idx : path)
    {
        if (idx >= cur->children.size())
        {
            return nullptr;
        }
        cur = &cur->children[idx];
    }
    return cur;
}

// node_at_path (from roots vector)
template <typename _Data,
          unsigned _F,
          typename _I>
tree_node<_Data, _F, _I>* node_at_path(
    std::vector<tree_node<_Data, _F, _I>>& roots,
    const tree_path& path)
{
    if (path.empty())
    {
        return nullptr;
    }
    if (path[0] >= roots.size())
    {
        return nullptr;
    }
    auto* cur = &roots[path[0]];
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        if (path[i] >= cur->children.size())
        {
            return nullptr;
        }
        cur = &cur->children[path[i]];
    }
    return cur;
}

// path_to
//   Finds the path from root to the node where pred(node) is true.
//   Returns empty optional if not found.
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Pred>
std::optional<tree_path> path_to(
    const tree_node<_Data, _F, _I>& root,
    _Pred pred)
{
    if (pred(root))
    {
        return tree_path{};
    }

    for (std::size_t i = 0; i < root.children.size(); ++i)
    {
        auto sub = path_to(root.children[i], pred);
        if (sub)
        {
            sub->insert(sub->begin(), i);
            return sub;
        }
    }
    return std::nullopt;
}

// path_to_ptr
//   Path from root to a specific node by address.
template <typename _Data,
          unsigned _F,
          typename _I>
std::optional<tree_path> path_to_ptr(
    const tree_node<_Data, _F, _I>& root,
    const tree_node<_Data, _F, _I>* target)
{
    return path_to(root, [target](const auto& n) { return &n == target; });
}


/*****************************************************************************/

// ===============================================================================
//  9  CHECKBOX OPERATIONS  (tf_checkable)
// ===============================================================================

// set_check
//   Sets a single node's check state.  No propagation.
template <typename _Data,
          unsigned _F,
          typename _I>
void set_check(
    tree_node<_Data, _F, _I>& node,
    check_state state)
{
    static_assert(has_feat(_F, tf_checkable), 
        "set_check requires tf_checkable");
    node.checked = state;
}

// propagate_check_down
//   Cascades a check state to all descendants.
template <typename _Data,
          unsigned _F,
          typename _I>
void propagate_check_down(
    tree_node<_Data, _F, _I>& node,
    check_state state)
{
    static_assert(has_feat(_F, tf_checkable), 
        "propagate_check_down requires tf_checkable");
    node.checked = state;
    for (auto& child : node.children)
    {
        propagate_check_down(child, state);
    }
}

// propagate_check_up
//   Recalculates a node's check state from its children.
//   unchecked if all children unchecked, checked if all checked,
//   indeterminate otherwise.
template <typename _Data,
          unsigned _F,
          typename _I>
void propagate_check_up(tree_node<_Data, _F, _I>& node)
{
    static_assert(has_feat(_F, tf_checkable), 
        "propagate_check_up requires tf_checkable");
    if (node.is_leaf())
    {
        return;
    }

    bool any_checked   = false;
    bool any_unchecked = false;
    bool any_indet     = false;

    for (const auto& child : node.children)
    {
        switch (child.checked)
        {
            case check_state::checked:       any_checked   = true; break;
            case check_state::unchecked:     any_unchecked = true; break;
            case check_state::indeterminate: any_indet     = true; break;
        }
    }

    if (any_indet || (any_checked && any_unchecked))
    {
        node.checked = check_state::indeterminate;
    }
    else if (any_checked)
    {
        node.checked = check_state::checked;
    }
    else
    {
        node.checked = check_state::unchecked;
    }
}

// toggle_check
//   Toggles a node according to a policy.
//   unchecked/indeterminate -> checked,  checked -> unchecked.
template <typename _Data,
          unsigned _F,
          typename _I>
void toggle_check(
    tree_node<_Data, _F, _I>& node,
    check_policy policy = check_policy::independent)
{
    static_assert(has_feat(_F, tf_checkable), 
        "toggle_check requires tf_checkable");

    check_state target = (node.checked == check_state::checked)
                         ? check_state::unchecked
                         : check_state::checked;

    if (policy == check_policy::independent)
    {
        node.checked = target;
    }
    else
    {
        // cascade down for both cascade_down and cascade_both
        propagate_check_down(node, target);
    }
    // cascade_both: caller is responsible for walking ancestors upward
    // and calling propagate_check_up on each.  The tree_view does this
    // automatically in its toggle_check_at_cursor method.
}

// sync_check_tree
//   Full tree synchronisation: walk bottom-up, recalculate every
// non-leaf node from its children.  Use after bulk changes.
template <typename _Data,
          unsigned _F,
          typename _I>
void sync_check_tree(tree_node<_Data, _F, _I>& root)
{
    static_assert(has_feat(_F, tf_checkable), 
        "sync_check_tree requires tf_checkable");
    walk_post(root, [](auto& node, std::size_t) {
        if (!node.is_leaf()) 
        {
            propagate_check_up(node);
        }
    });
}

// count_checked
//   Counts nodes in each check state.
template <typename _Data,
          unsigned _F,
          typename _I>
struct check_counts { std::size_t checked = 0, unchecked = 0, indeterminate = 0; };

template <typename _Data,
          unsigned _F,
          typename _I>
check_counts<_Data, _F, _I> count_checked(
    const tree_node<_Data, _F, _I>& root)
{
    static_assert(has_feat(_F, tf_checkable), 
        "count_checked requires tf_checkable");
    check_counts<_Data, _F, _I> c;
    walk(root, [&c](const auto& n, std::size_t) {
        switch (n.checked)
        {
            case check_state::checked:       ++c.checked; break;
            case check_state::unchecked:     ++c.unchecked; break;
            case check_state::indeterminate: ++c.indeterminate; break;
        }
    });
    return c;
}


/*****************************************************************************/

// ===============================================================================
//  10  COLLAPSE OPERATIONS  (tf_collapsible)
// ===============================================================================

// set_expanded
template <typename _Data,
          unsigned _F,
          typename _I>
void set_expanded(tree_node<_Data, _F, _I>& node, bool exp)
{
    static_assert(has_feat(_F, tf_collapsible), 
        "set_expanded requires tf_collapsible");
    node.expanded = exp;
}

// toggle_expanded
template <typename _Data,
          unsigned _F,
          typename _I>
void toggle_expanded(tree_node<_Data, _F, _I>& node)
{
    static_assert(has_feat(_F, tf_collapsible), 
        "toggle_expanded requires tf_collapsible");
    node.expanded = !node.expanded;
}

// expand_all
//   Recursively expands every node in the subtree.
template <typename _Data,
          unsigned _F,
          typename _I>
void expand_all(tree_node<_Data, _F, _I>& root)
{
    static_assert(has_feat(_F, tf_collapsible), 
        "expand_all requires tf_collapsible");
    walk(root, [](auto& n, std::size_t) { n.expanded = true; });
}

// collapse_all
//   Recursively collapses every node in the subtree.
template <typename _Data,
          unsigned _F,
          typename _I>
void collapse_all(tree_node<_Data, _F, _I>& root)
{
    static_assert(has_feat(_F, tf_collapsible), 
        "collapse_all requires tf_collapsible");
    walk(root, [](auto& n, std::size_t) { n.expanded = false; });
}

// expand_to
//   Expands all ancestors along a path so the target becomes visible.
template <typename _Data,
          unsigned _F,
          typename _I>
void expand_to(
    tree_node<_Data, _F, _I>& root,
    const tree_path& path)
{
    static_assert(has_feat(_F, tf_collapsible), 
        "expand_to requires tf_collapsible");
    auto* cur = &root;
    // expand every node along the path.  After the loop, cur points to
    // the target node itself — which is NOT expanded, only made visible.
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        cur->expanded = true;
        if (path[i] >= cur->children.size())
        {
            return;
        }
        cur = &cur->children[path[i]];
    }
}

// count_visible
//   Number of visible nodes in the subtree (respecting collapse state).
template <typename _Data,
          unsigned _F,
          typename _I>
std::size_t count_visible(const tree_node<_Data, _F, _I>& root)
{
    std::size_t n = 1;
    if (root.children_visible())
    {
        for (const auto& child : root.children)
        {
            n += count_visible(child);
        }
    }
    return n;
}


/*****************************************************************************/

// ===============================================================================
//  11  ICON HELPERS  (tf_icons)
// ===============================================================================

// set_icon
template <typename _Data,
          unsigned _F,
          typename _I>
void set_icon(
    tree_node<_Data, _F, _I>& node,
    non_deduced<_I> icon)
{
    static_assert(has_feat(_F, tf_icons), 
        "set_icon requires tf_icons");
    node.icon = std::move(icon);
}

// set_icons
//   Sets both normal and expanded icons.
template <typename _Data,
          unsigned _F,
          typename _I>
void set_icons(
    tree_node<_Data, _F, _I>& node,
    non_deduced<_I> normal,
    non_deduced<_I> expanded)
{
    static_assert(has_feat(_F, tf_icons), 
        "set_icons requires tf_icons");
    node.icon          = std::move(normal);
    node.expanded_icon = std::move(expanded);
    node.use_expanded  = true;
}

// effective_icon
//   Returns the icon to display based on current expand state.
template <typename _Data,
          unsigned _F,
          typename _I>
const _I& effective_icon(const tree_node<_Data, _F, _I>& node)
{
    static_assert(has_feat(_F, tf_icons), 
        "effective_icon requires tf_icons");
    if constexpr (has_feat(_F, tf_collapsible))
    {
        if (node.use_expanded && node.expanded)
        {
            return node.expanded_icon;
        }
    }
    return node.icon;
}


/*****************************************************************************/

// ===============================================================================
//  12  CONTEXT HELPERS  (tf_context)
// ===============================================================================

// has_action
//   Tests whether a node has a specific context action available.
template <typename _Data,
          unsigned _F,
          typename _I>
bool has_action(
    const tree_node<_Data, _F, _I>& node,
    context_action action)
{
    static_assert(has_feat(_F, tf_context), 
        "has_action requires tf_context");
    return (node.context_actions & static_cast<unsigned>(action)) != 0;
}

// set_actions
template <typename _Data,
          unsigned _F,
          typename _I>
void set_actions(
    tree_node<_Data, _F, _I>& node,
    unsigned actions)
{
    static_assert(has_feat(_F, tf_context), 
        "set_actions requires tf_context");
    node.context_actions = actions;
}


/*****************************************************************************/

// ===============================================================================
//  13  SORT / PARTITION
// ===============================================================================

// sort_children
//   Sorts immediate children by comparator.  Does not recurse.
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Cmp>
void sort_children(
    tree_node<_Data, _F, _I>& node,
    _Cmp cmp)
{
    std::sort(node.children.begin(), node.children.end(), cmp);
}

// sort_tree
//   Recursively sorts every level of the tree.
template <typename _Data,
          unsigned _F,
          typename _I,
          typename _Cmp>
void sort_tree(
    tree_node<_Data, _F, _I>& root,
    _Cmp cmp)
{
    sort_children(root, cmp);
    for (auto& child : root.children)
    {
        sort_tree(child, cmp);
    }
}

// partition_directories_first
//   Convenience: partitions children so entries with children (directories)
// appear before leaves.  Stable within each group.
template <typename _Data,
          unsigned _F,
          typename _I>
void partition_directories_first(tree_node<_Data, _F, _I>& node)
{
    std::stable_partition(
        node.children.begin(), node.children.end(),
        [](const auto& n) { return !n.is_leaf(); });
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_TREE_NODE_