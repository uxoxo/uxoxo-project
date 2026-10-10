/*******************************************************************************
* djinterp [container]                                           tree_node.hpp
*
*   Hierarchical tree node template with zero-cost optional features.
*   Shared infrastructure (enums, EBO mixins, feature flags) lives in
*   view_common.hpp.  This header adds only the collapse mixin, the
*   tree_node struct, and all tree-specific operations.
*
* author(s): Samuel 'teer' Neal-Blim
* file:   \inc\container\tree_node.hpp                         date: 2025.05.19
*******************************************************************************/

#ifndef  DJINTERP_CONTAINER_TREE_NODE_
#define  DJINTERP_CONTAINER_TREE_NODE_ 1

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <djinterp>
#include <container/view_common.hpp>

NS_DJINTERP
namespace container {

// ═══════════════════════════════════════════════════════════════════════════════
//  §1  COLLAPSE MIXIN  (tree-specific — lists have no children to collapse)
// ═══════════════════════════════════════════════════════════════════════════════

namespace entry_mixin {
    template <bool _Enable>
    struct collapse_data {};
    template <>
    struct collapse_data<true> { bool expanded = true; };
}

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  TREE NODE
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _Data, unsigned _Feat = vf_none, typename _Icon = int>
struct tree_node
    : entry_mixin::checkable_data <has_feat(_Feat, vf_checkable)>
    , entry_mixin::icon_data      <has_feat(_Feat, vf_icons), _Icon>
    , entry_mixin::collapse_data  <has_feat(_Feat, vf_collapsible)>
    , entry_mixin::rename_data    <has_feat(_Feat, vf_renamable)>
    , entry_mixin::context_data   <has_feat(_Feat, vf_context)>
{
    using data_type  = _Data;
    using icon_type  = _Icon;
    using self_type  = tree_node<_Data, _Feat, _Icon>;
    using child_list = std::vector<self_type>;
    static constexpr unsigned features = _Feat;
    static constexpr bool is_checkable   = has_feat(_Feat, vf_checkable);
    static constexpr bool has_icons      = has_feat(_Feat, vf_icons);
    static constexpr bool is_collapsible = has_feat(_Feat, vf_collapsible);
    static constexpr bool is_renamable   = has_feat(_Feat, vf_renamable);
    static constexpr bool has_context    = has_feat(_Feat, vf_context);

    _Data       data;
    child_list  children;

    tree_node() = default;
    explicit tree_node(_Data d) : data(std::move(d)) {}
    tree_node(_Data d, child_list kids) : data(std::move(d)), children(std::move(kids)) {}

    [[nodiscard]] bool        is_leaf()     const noexcept { return children.empty(); }
    [[nodiscard]] std::size_t child_count() const noexcept { return children.size(); }
    [[nodiscard]] bool children_visible() const noexcept {
        if constexpr (is_collapsible) return this->expanded;
        else return true;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  NODE MUTATION
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _D, unsigned _F, typename _I>
tree_node<_D,_F,_I>& add_child(tree_node<_D,_F,_I>& p, tree_node<_D,_F,_I> c) {
    p.children.push_back(std::move(c)); return p.children.back();
}
template <typename _D, unsigned _F, typename _I>
tree_node<_D,_F,_I>& emplace_child(tree_node<_D,_F,_I>& p, non_deduced<_D> d) {
    p.children.emplace_back(std::move(d)); return p.children.back();
}
template <typename _D, unsigned _F, typename _I>
bool remove_child(tree_node<_D,_F,_I>& p, std::size_t i) {
    if (i >= p.children.size()) return false;
    p.children.erase(p.children.begin() + static_cast<std::ptrdiff_t>(i));
    return true;
}
template <typename _D, unsigned _F, typename _I, typename _Pred>
std::size_t remove_child_if(tree_node<_D,_F,_I>& p, _Pred pred) {
    auto& c = p.children;
    auto it = std::remove_if(c.begin(), c.end(), pred);
    std::size_t n = static_cast<std::size_t>(std::distance(it, c.end()));
    c.erase(it, c.end()); return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  TRAVERSAL
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _D, unsigned _F, typename _I, typename _Fn>
void walk(tree_node<_D,_F,_I>& n, _Fn&& fn, std::size_t d = 0) {
    fn(n, d); for (auto& c : n.children) walk(c, fn, d + 1);
}
template <typename _D, unsigned _F, typename _I, typename _Fn>
void walk(const tree_node<_D,_F,_I>& n, _Fn&& fn, std::size_t d = 0) {
    fn(n, d); for (const auto& c : n.children) walk(c, fn, d + 1);
}
template <typename _D, unsigned _F, typename _I, typename _Fn>
void walk_visible(tree_node<_D,_F,_I>& n, _Fn&& fn, std::size_t d = 0) {
    fn(n, d);
    if (n.children_visible()) for (auto& c : n.children) walk_visible(c, fn, d + 1);
}
template <typename _D, unsigned _F, typename _I, typename _Fn>
void walk_visible(const tree_node<_D,_F,_I>& n, _Fn&& fn, std::size_t d = 0) {
    fn(n, d);
    if (n.children_visible()) for (const auto& c : n.children) walk_visible(c, fn, d + 1);
}
template <typename _D, unsigned _F, typename _I, typename _Fn>
void walk_post(tree_node<_D,_F,_I>& n, _Fn&& fn, std::size_t d = 0) {
    for (auto& c : n.children) walk_post(c, fn, d + 1);
    fn(n, d);
}
template <typename _D, unsigned _F, typename _I, typename _Pred>
tree_node<_D,_F,_I>* find_if(tree_node<_D,_F,_I>& root, _Pred pred) {
    if (pred(root)) return &root;
    for (auto& c : root.children) { auto* f = find_if(c, pred); if (f) return f; }
    return nullptr;
}
template <typename _D, unsigned _F, typename _I, typename _Pred>
const tree_node<_D,_F,_I>* find_if(const tree_node<_D,_F,_I>& root, _Pred pred) {
    if (pred(root)) return &root;
    for (const auto& c : root.children) { auto* f = find_if(c, pred); if (f) return f; }
    return nullptr;
}
template <typename _D, unsigned _F, typename _I>
std::size_t count_nodes(const tree_node<_D,_F,_I>& r) {
    std::size_t n = 1; for (const auto& c : r.children) n += count_nodes(c); return n;
}
template <typename _D, unsigned _F, typename _I>
std::size_t max_depth(const tree_node<_D,_F,_I>& r, std::size_t d = 0) {
    std::size_t m = d; for (const auto& c : r.children) m = std::max(m, max_depth(c, d+1)); return m;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  FLATTEN
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _Node>
struct flat_entry {
    _Node* node; std::size_t depth; std::size_t flat_index;
    bool is_last_child; bool has_children;
};

template <typename _D, unsigned _F, typename _I>
std::vector<flat_entry<tree_node<_D,_F,_I>>> flatten(tree_node<_D,_F,_I>& root) {
    using N = tree_node<_D,_F,_I>; std::vector<flat_entry<N>> r;
    struct X { static void go(N& n, std::size_t d, bool last, std::vector<flat_entry<N>>& o) {
        o.push_back({&n,d,o.size(),last,!n.is_leaf()});
        for (std::size_t i=0;i<n.children.size();++i) go(n.children[i],d+1,i==n.children.size()-1,o);
    }};
    X::go(root,0,true,r); return r;
}
template <typename _D, unsigned _F, typename _I>
std::vector<flat_entry<tree_node<_D,_F,_I>>> flatten_visible(tree_node<_D,_F,_I>& root) {
    using N = tree_node<_D,_F,_I>; std::vector<flat_entry<N>> r;
    struct X { static void go(N& n, std::size_t d, bool last, std::vector<flat_entry<N>>& o) {
        o.push_back({&n,d,o.size(),last,!n.is_leaf()});
        if (n.children_visible()) for (std::size_t i=0;i<n.children.size();++i)
            go(n.children[i],d+1,i==n.children.size()-1,o);
    }};
    X::go(root,0,true,r); return r;
}
template <typename _D, unsigned _F, typename _I>
std::vector<flat_entry<tree_node<_D,_F,_I>>> flatten_roots_visible(std::vector<tree_node<_D,_F,_I>>& roots) {
    using N = tree_node<_D,_F,_I>; std::vector<flat_entry<N>> r;
    struct X { static void go(N& n, std::size_t d, bool last, std::vector<flat_entry<N>>& o) {
        o.push_back({&n,d,o.size(),last,!n.is_leaf()});
        if (n.children_visible()) for (std::size_t i=0;i<n.children.size();++i)
            go(n.children[i],d+1,i==n.children.size()-1,o);
    }};
    for (std::size_t i=0;i<roots.size();++i) X::go(roots[i],0,i==roots.size()-1,r);
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  §6  PATH-BASED ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

using tree_path = std::vector<std::size_t>;

template <typename _D, unsigned _F, typename _I>
tree_node<_D,_F,_I>* node_at_path(tree_node<_D,_F,_I>& root, const tree_path& p) {
    auto* c = &root; for (auto i : p) { if (i>=c->children.size()) return nullptr; c=&c->children[i]; } return c;
}
template <typename _D, unsigned _F, typename _I>
tree_node<_D,_F,_I>* node_at_path(std::vector<tree_node<_D,_F,_I>>& roots, const tree_path& p) {
    if (p.empty()||p[0]>=roots.size()) return nullptr;
    auto* c=&roots[p[0]]; for (std::size_t i=1;i<p.size();++i) { if (p[i]>=c->children.size()) return nullptr; c=&c->children[p[i]]; } return c;
}
template <typename _D, unsigned _F, typename _I, typename _Pred>
std::optional<tree_path> path_to(const tree_node<_D,_F,_I>& root, _Pred pred) {
    if (pred(root)) return tree_path{};
    for (std::size_t i=0;i<root.children.size();++i) {
        auto s=path_to(root.children[i],pred); if (s) { s->insert(s->begin(),i); return s; }
    } return std::nullopt;
}
template <typename _D, unsigned _F, typename _I>
std::optional<tree_path> path_to_ptr(const tree_node<_D,_F,_I>& root, const tree_node<_D,_F,_I>* t) {
    return path_to(root,[t](const auto& n){return &n==t;});
}

// ═══════════════════════════════════════════════════════════════════════════════
//  §7  CHECKBOX OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _D, unsigned _F, typename _I>
void set_check(tree_node<_D,_F,_I>& n, check_state s) {
    static_assert(has_feat(_F,vf_checkable),"requires vf_checkable"); n.checked=s;
}
template <typename _D, unsigned _F, typename _I>
void propagate_check_down(tree_node<_D,_F,_I>& n, check_state s) {
    static_assert(has_feat(_F,vf_checkable),"requires vf_checkable");
    n.checked=s; for (auto& c:n.children) propagate_check_down(c,s);
}
template <typename _D, unsigned _F, typename _I>
void propagate_check_up(tree_node<_D,_F,_I>& n) {
    static_assert(has_feat(_F,vf_checkable),"requires vf_checkable");
    if (n.is_leaf()) return;
    bool ac=false,au=false,ai=false;
    for (const auto& c:n.children) { switch(c.checked) {
        case check_state::checked:ac=true;break;case check_state::unchecked:au=true;break;
        case check_state::indeterminate:ai=true;break; } }
    if (ai||(ac&&au)) n.checked=check_state::indeterminate;
    else if (ac) n.checked=check_state::checked;
    else n.checked=check_state::unchecked;
}
template <typename _D, unsigned _F, typename _I>
void toggle_check(tree_node<_D,_F,_I>& n, check_policy pol=check_policy::independent) {
    static_assert(has_feat(_F,vf_checkable),"requires vf_checkable");
    auto t=(n.checked==check_state::checked)?check_state::unchecked:check_state::checked;
    if (pol==check_policy::independent) n.checked=t; else propagate_check_down(n,t);
}
template <typename _D, unsigned _F, typename _I>
void sync_check_tree(tree_node<_D,_F,_I>& root) {
    static_assert(has_feat(_F,vf_checkable),"requires vf_checkable");
    walk_post(root,[](auto& n,std::size_t){if(!n.is_leaf())propagate_check_up(n);});
}
template <typename _D, unsigned _F, typename _I>
struct check_counts { std::size_t checked=0,unchecked=0,indeterminate=0; };
template <typename _D, unsigned _F, typename _I>
check_counts<_D,_F,_I> count_checked(const tree_node<_D,_F,_I>& root) {
    static_assert(has_feat(_F,vf_checkable),"requires vf_checkable");
    check_counts<_D,_F,_I> cc;
    walk(root,[&](const auto& n,std::size_t){ switch(n.checked) {
        case check_state::checked:++cc.checked;break;case check_state::unchecked:++cc.unchecked;break;
        case check_state::indeterminate:++cc.indeterminate;break;} });
    return cc;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  §8  COLLAPSE OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _D, unsigned _F, typename _I>
void set_expanded(tree_node<_D,_F,_I>& n, bool e) {
    static_assert(has_feat(_F,vf_collapsible),"requires vf_collapsible"); n.expanded=e;
}
template <typename _D, unsigned _F, typename _I>
void toggle_expanded(tree_node<_D,_F,_I>& n) {
    static_assert(has_feat(_F,vf_collapsible),"requires vf_collapsible"); n.expanded=!n.expanded;
}
template <typename _D, unsigned _F, typename _I>
void expand_all(tree_node<_D,_F,_I>& r) {
    static_assert(has_feat(_F,vf_collapsible),"requires vf_collapsible");
    walk(r,[](auto& n,std::size_t){n.expanded=true;});
}
template <typename _D, unsigned _F, typename _I>
void collapse_all(tree_node<_D,_F,_I>& r) {
    static_assert(has_feat(_F,vf_collapsible),"requires vf_collapsible");
    walk(r,[](auto& n,std::size_t){n.expanded=false;});
}
template <typename _D, unsigned _F, typename _I>
void expand_to(tree_node<_D,_F,_I>& root, const tree_path& path) {
    static_assert(has_feat(_F,vf_collapsible),"requires vf_collapsible");
    auto* c=&root;
    for (std::size_t i=0;i<path.size();++i) { c->expanded=true; if(path[i]>=c->children.size())return; c=&c->children[path[i]]; }
}
template <typename _D, unsigned _F, typename _I>
std::size_t count_visible(const tree_node<_D,_F,_I>& r) {
    std::size_t n=1; if(r.children_visible()) for(const auto& c:r.children) n+=count_visible(c); return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  §9  ICON HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _D, unsigned _F, typename _I>
void set_icon(tree_node<_D,_F,_I>& n, non_deduced<_I> icon) {
    static_assert(has_feat(_F,vf_icons),"requires vf_icons"); n.icon=std::move(icon);
}
template <typename _D, unsigned _F, typename _I>
void set_icons(tree_node<_D,_F,_I>& n, non_deduced<_I> normal, non_deduced<_I> expanded) {
    static_assert(has_feat(_F,vf_icons),"requires vf_icons");
    n.icon=std::move(normal); n.expanded_icon=std::move(expanded); n.use_expanded=true;
}
template <typename _D, unsigned _F, typename _I>
const _I& effective_icon(const tree_node<_D,_F,_I>& n) {
    static_assert(has_feat(_F,vf_icons),"requires vf_icons");
    if constexpr (has_feat(_F,vf_collapsible)) { if(n.use_expanded&&n.expanded) return n.expanded_icon; }
    return n.icon;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  §10  CONTEXT / SORT
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _D, unsigned _F, typename _I>
bool has_action(const tree_node<_D,_F,_I>& n, context_action a) {
    static_assert(has_feat(_F,vf_context),"requires vf_context");
    return (n.context_actions & static_cast<unsigned>(a))!=0;
}
template <typename _D, unsigned _F, typename _I>
void set_actions(tree_node<_D,_F,_I>& n, unsigned a) {
    static_assert(has_feat(_F,vf_context),"requires vf_context"); n.context_actions=a;
}
template <typename _D, unsigned _F, typename _I, typename _Cmp>
void sort_children(tree_node<_D,_F,_I>& n, _Cmp cmp) { std::sort(n.children.begin(),n.children.end(),cmp); }
template <typename _D, unsigned _F, typename _I, typename _Cmp>
void sort_tree(tree_node<_D,_F,_I>& r, _Cmp cmp) { sort_children(r,cmp); for(auto& c:r.children) sort_tree(c,cmp); }
template <typename _D, unsigned _F, typename _I>
void partition_directories_first(tree_node<_D,_F,_I>& n) {
    std::stable_partition(n.children.begin(),n.children.end(),[](const auto& x){return !x.is_leaf();});
}

}   // namespace container
NS_END
#endif
