/*******************************************************************************
* djinterp [test]                                              static_tests.cpp
*
*   Compile-time and runtime verification of the tree template library.
*
*   §1:  sizeof / EBO verification — disabled features cost 0 bytes
*   §2:  tree_node traits (node-level SFINAE detection)
*   §3:  tree_view traits (view-level SFINAE detection)
*   §4:  composite identity traits + cross-checks
*   §5:  node construction + mutation
*   §6:  traversal (walk, walk_visible, walk_post, find_if)
*   §7:  flatten + flatten_visible
*   §8:  path-based access
*   §9:  checkbox operations + propagation
*   §10: collapse operations
*   §11: icon operations
*   §12: context action operations
*   §13: sort / partition
*   §14: tree_view navigation
*   §15: tree_view selection
*   §16: tree_view rename
*   §17: tree_view context menu
*   §18: tree_view search
*
*   Build:  g++ -std=c++17 -I. -o test static_tests.cpp && ./test
*******************************************************************************/

#include <container/tree_view.hpp>

#include <cassert>
#include <cstdio>
#include <string>

using namespace djinterp::container;
namespace tt = djinterp::container::tree_traits;


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  SIZEOF / EBO VERIFICATION
// ═══════════════════════════════════════════════════════════════════════════════

using bare_node   = tree_node<std::string>;
using check_node  = tree_node<std::string, tf_checkable>;
using icon_node   = tree_node<std::string, tf_icons>;
using coll_node   = tree_node<std::string, tf_collapsible>;
using rename_node = tree_node<std::string, tf_renamable>;
using ctx_node    = tree_node<std::string, tf_context>;
using full_node   = tree_node<std::string, tf_all>;
using ci_node     = tree_node<std::string, tf_collapsible | tf_icons>;

// disabled features must cost 0 bytes over base
// base = sizeof(string) + sizeof(vector<tree_node>)
static_assert(sizeof(bare_node) <= sizeof(std::string) + sizeof(std::vector<bare_node>) + 8,
    "bare node should be compact (data + children + EBO padding)");

// each feature adds known cost
static_assert(sizeof(check_node)  > sizeof(bare_node),    "checkable adds check_state (1 byte + padding)");
static_assert(sizeof(coll_node)   > sizeof(bare_node),    "collapsible adds bool");
static_assert(sizeof(rename_node) > sizeof(bare_node),    "renamable adds bool");
static_assert(sizeof(ctx_node)    > sizeof(bare_node),    "context adds unsigned");
static_assert(sizeof(icon_node)   > sizeof(bare_node),    "icons adds icon+expanded_icon+bool");
static_assert(sizeof(full_node)   > sizeof(ci_node),      "all features > partial features");

// feature flag constants
static_assert(bare_node::features    == tf_none);
static_assert(check_node::features   == static_cast<unsigned>(tf_checkable));
static_assert(full_node::features    == static_cast<unsigned>(tf_all));

static_assert(!bare_node::is_checkable);
static_assert( check_node::is_checkable);
static_assert(!bare_node::is_collapsible);
static_assert( coll_node::is_collapsible);
static_assert(!bare_node::has_icons);
static_assert( icon_node::has_icons);
static_assert(!bare_node::is_renamable);
static_assert( rename_node::is_renamable);
static_assert(!bare_node::has_context);
static_assert( ctx_node::has_context);
static_assert( full_node::is_checkable);
static_assert( full_node::is_collapsible);
static_assert( full_node::has_icons);
static_assert( full_node::is_renamable);
static_assert( full_node::has_context);


// ═══════════════════════════════════════════════════════════════════════════════
//  §2  NODE-LEVEL TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

// bare_node: has data + children, nothing else
static_assert( tt::has_data_v<bare_node>);
static_assert( tt::has_children_v<bare_node>);
static_assert( tt::has_is_leaf_v<bare_node>);
static_assert( tt::has_features_v<bare_node>);
static_assert( tt::has_data_type_v<bare_node>);
static_assert(!tt::has_checked_v<bare_node>);
static_assert(!tt::has_icon_v<bare_node>);
static_assert(!tt::has_expanded_v<bare_node>);
static_assert(!tt::has_renamable_v<bare_node>);
static_assert(!tt::has_context_actions_v<bare_node>);

// feature-enabled nodes gain the right members
static_assert( tt::has_checked_v<check_node>);
static_assert( tt::has_icon_v<icon_node>);
static_assert( tt::has_expanded_icon_v<icon_node>);
static_assert( tt::has_expanded_v<coll_node>);
static_assert( tt::has_renamable_v<rename_node>);
static_assert( tt::has_context_actions_v<ctx_node>);

// full_node has everything
static_assert( tt::has_checked_v<full_node>);
static_assert( tt::has_icon_v<full_node>);
static_assert( tt::has_expanded_v<full_node>);
static_assert( tt::has_renamable_v<full_node>);
static_assert( tt::has_context_actions_v<full_node>);


// ═══════════════════════════════════════════════════════════════════════════════
//  §3  VIEW-LEVEL TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

using bare_view   = tree_view<std::string>;
using full_view   = tree_view<std::string, tf_all>;
using rename_view = tree_view<std::string, tf_renamable>;
using ctx_view    = tree_view<std::string, tf_context>;
using check_view  = tree_view<std::string, tf_checkable>;

// all views have core members
static_assert( tt::has_roots_v<bare_view>);
static_assert( tt::has_cursor_v<bare_view>);
static_assert( tt::has_scroll_offset_v<bare_view>);
static_assert( tt::has_selected_v<bare_view>);
static_assert( tt::is_focusable_v<bare_view>);
static_assert( tt::is_scrollable_v<bare_view>);

// feature-dependent view state
static_assert(!tt::has_editing_v<bare_view>);
static_assert( tt::has_editing_v<rename_view>);
static_assert( tt::has_editing_v<full_view>);

static_assert(!tt::has_context_open_v<bare_view>);
static_assert( tt::has_context_open_v<ctx_view>);
static_assert( tt::has_context_open_v<full_view>);

static_assert(!tt::has_policy_v<bare_view>);
static_assert( tt::has_policy_v<check_view>);
static_assert( tt::has_policy_v<full_view>);


// ═══════════════════════════════════════════════════════════════════════════════
//  §4  COMPOSITE IDENTITY + CROSS-CHECKS
// ═══════════════════════════════════════════════════════════════════════════════

// is_tree_node
static_assert( tt::is_tree_node_v<bare_node>);
static_assert( tt::is_tree_node_v<full_node>);
static_assert(!tt::is_tree_node_v<std::string>);
static_assert(!tt::is_tree_node_v<int>);

// is_tree_view
static_assert( tt::is_tree_view_v<bare_view>);
static_assert( tt::is_tree_view_v<full_view>);
static_assert(!tt::is_tree_view_v<bare_node>);
static_assert(!tt::is_tree_view_v<std::string>);

// feature composites — node
static_assert(!tt::is_checkable_node_v<bare_node>);
static_assert( tt::is_checkable_node_v<check_node>);
static_assert(!tt::is_icon_node_v<bare_node>);
static_assert( tt::is_icon_node_v<icon_node>);
static_assert(!tt::is_collapsible_node_v<bare_node>);
static_assert( tt::is_collapsible_node_v<coll_node>);
static_assert(!tt::is_renamable_node_v<bare_node>);
static_assert( tt::is_renamable_node_v<rename_node>);
static_assert(!tt::is_context_node_v<bare_node>);
static_assert( tt::is_context_node_v<ctx_node>);
static_assert( tt::is_checkable_node_v<full_node>);
static_assert( tt::is_collapsible_node_v<full_node>);
static_assert( tt::is_icon_node_v<full_node>);
static_assert( tt::is_renamable_node_v<full_node>);
static_assert( tt::is_context_node_v<full_node>);

// feature composites — view
static_assert(!tt::is_editable_view_v<bare_view>);
static_assert( tt::is_editable_view_v<rename_view>);
static_assert(!tt::is_context_view_v<bare_view>);
static_assert( tt::is_context_view_v<ctx_view>);
static_assert(!tt::is_checkable_view_v<bare_view>);
static_assert( tt::is_checkable_view_v<check_view>);
static_assert( tt::is_editable_view_v<full_view>);
static_assert( tt::is_context_view_v<full_view>);
static_assert( tt::is_checkable_view_v<full_view>);

// data type extraction
static_assert(std::is_same_v<tt::tree_node_data_t<bare_node>, std::string>);
static_assert(std::is_same_v<tt::tree_node_data_t<full_node>, std::string>);
static_assert(std::is_void_v<tt::tree_node_data_t<int>>);


/*****************************************************************************/
//  RUNTIME TESTS
/*****************************************************************************/

// helper: build a sample tree
//   root
//   ├── a
//   │   ├── a1
//   │   └── a2
//   ├── b
//   └── c
//       └── c1
template <unsigned F>
tree_node<std::string, F> make_sample_tree()
{
    tree_node<std::string, F> root("root");

    auto& a = emplace_child(root, "a");
    emplace_child(a, "a1");
    emplace_child(a, "a2");
    emplace_child(root, "b");
    auto& c = emplace_child(root, "c");
    emplace_child(c, "c1");

    return root;
}

int main()
{
    std::printf("── §1 sizeof ───────────────────────────────\n");
    {
        std::printf("  bare_node   : %zu bytes\n", sizeof(bare_node));
        std::printf("  check_node  : %zu bytes\n", sizeof(check_node));
        std::printf("  icon_node   : %zu bytes\n", sizeof(icon_node));
        std::printf("  coll_node   : %zu bytes\n", sizeof(coll_node));
        std::printf("  rename_node : %zu bytes\n", sizeof(rename_node));
        std::printf("  ctx_node    : %zu bytes\n", sizeof(ctx_node));
        std::printf("  full_node   : %zu bytes\n", sizeof(full_node));
        std::printf("  bare_view   : %zu bytes\n", sizeof(bare_view));
        std::printf("  full_view   : %zu bytes\n", sizeof(full_view));
        std::printf("  ✓ all static_asserts passed\n");
    }

    std::printf("── §5 node construction + mutation ─────────\n");
    {
        auto root = make_sample_tree<tf_none>();
        assert(root.data == "root");
        assert(root.child_count() == 3);
        assert(!root.is_leaf());
        assert(root.children[0].data == "a");
        assert(root.children[0].child_count() == 2);
        assert(root.children[1].data == "b");
        assert(root.children[1].is_leaf());
        assert(root.children[2].data == "c");
        assert(root.children[2].children[0].data == "c1");

        // remove_child
        assert(remove_child(root, 1));  // remove "b"
        assert(root.child_count() == 2);
        assert(root.children[1].data == "c");

        // remove_child_if
        auto n = remove_child_if(root, [](const bare_node& n) {
            return n.data == "c";
        });
        assert(n == 1);
        assert(root.child_count() == 1);
        assert(root.children[0].data == "a");

        std::printf("  ✓ construction, add_child, remove_child, remove_child_if\n");
    }

    std::printf("── §6 traversal ────────────────────────────\n");
    {
        auto root = make_sample_tree<tf_none>();

        // walk — count nodes
        assert(count_nodes(root) == 7);

        // walk — collect names in order
        std::vector<std::string> names;
        walk(root, [&](const bare_node& n, std::size_t) {
            names.push_back(n.data);
        });
        assert(names.size() == 7);
        assert(names[0] == "root");
        assert(names[1] == "a");
        assert(names[2] == "a1");
        assert(names[3] == "a2");
        assert(names[4] == "b");
        assert(names[5] == "c");
        assert(names[6] == "c1");

        // walk_post — children before parent
        names.clear();
        walk_post(root, [&](const bare_node& n, std::size_t) {
            names.push_back(n.data);
        });
        assert(names[0] == "a1");
        assert(names[1] == "a2");
        assert(names[2] == "a");
        assert(names[3] == "b");
        assert(names[4] == "c1");
        assert(names[5] == "c");
        assert(names[6] == "root");

        // max_depth
        assert(max_depth(root) == 2);

        // find_if
        auto* found = find_if(root, [](const bare_node& n) { return n.data == "a2"; });
        assert(found != nullptr);
        assert(found->data == "a2");

        auto* not_found = find_if(root, [](const bare_node& n) { return n.data == "z"; });
        assert(not_found == nullptr);

        std::printf("  ✓ walk, walk_post, count_nodes, max_depth, find_if\n");
    }

    std::printf("── §7 flatten ──────────────────────────────\n");
    {
        auto root = make_sample_tree<tf_none>();

        auto flat = flatten(root);
        assert(flat.size() == 7);

        // check depths
        assert(flat[0].depth == 0);  // root
        assert(flat[1].depth == 1);  // a
        assert(flat[2].depth == 2);  // a1
        assert(flat[3].depth == 2);  // a2
        assert(flat[4].depth == 1);  // b
        assert(flat[5].depth == 1);  // c
        assert(flat[6].depth == 2);  // c1

        // check flat_index
        for (std::size_t i = 0; i < flat.size(); ++i)
            assert(flat[i].flat_index == i);

        // check has_children
        assert(flat[0].has_children);   // root
        assert(flat[1].has_children);   // a
        assert(!flat[2].has_children);  // a1
        assert(!flat[4].has_children);  // b
        assert(flat[5].has_children);   // c
        assert(!flat[6].has_children);  // c1

        // check is_last_child
        assert(flat[3].is_last_child);  // a2 is last child of a
        assert(!flat[4].is_last_child); // b is not last child of root
        assert(flat[5].is_last_child);  // c is last child of root
        assert(flat[6].is_last_child);  // c1 is last child of c

        std::printf("  ✓ flatten: depth, flat_index, has_children, is_last_child\n");
    }

    std::printf("── §7b flatten_visible (collapsible) ───────\n");
    {
        auto root = make_sample_tree<tf_collapsible>();

        // all expanded by default → same as flatten
        auto vis = flatten_visible(root);
        assert(vis.size() == 7);

        // collapse "a" → a1, a2 hidden
        root.children[0].expanded = false;
        vis = flatten_visible(root);
        assert(vis.size() == 5);  // root, a(collapsed), b, c, c1

        // collapse root → only root visible
        root.expanded = false;
        vis = flatten_visible(root);
        assert(vis.size() == 1);
        assert(vis[0].node->data == "root");

        std::printf("  ✓ flatten_visible respects collapsed state\n");
    }

    std::printf("── §8 path-based access ────────────────────\n");
    {
        auto root = make_sample_tree<tf_none>();

        // path to a2 = {0, 1}  (root.children[0].children[1])
        auto* n = node_at_path(root, {0, 1});
        assert(n != nullptr);
        assert(n->data == "a2");

        // path to c1 = {2, 0}
        n = node_at_path(root, {2, 0});
        assert(n != nullptr);
        assert(n->data == "c1");

        // empty path = root itself
        n = node_at_path(root, {});
        assert(n == &root);

        // invalid path
        n = node_at_path(root, {99});
        assert(n == nullptr);

        // path_to: find path to a2
        auto path = path_to(root, [](const bare_node& n) { return n.data == "a2"; });
        assert(path.has_value());
        assert(path->size() == 2);
        assert((*path)[0] == 0);
        assert((*path)[1] == 1);

        // path_to_ptr
        auto* a1 = &root.children[0].children[0];
        auto path2 = path_to_ptr(root, a1);
        assert(path2.has_value());
        assert(path2->size() == 2);
        assert((*path2)[0] == 0);
        assert((*path2)[1] == 0);

        std::printf("  ✓ node_at_path, path_to, path_to_ptr\n");
    }

    std::printf("── §9 checkbox operations ──────────────────\n");
    {
        auto root = make_sample_tree<tf_checkable>();

        // default state: all unchecked
        assert(root.checked == check_state::unchecked);
        assert(root.children[0].checked == check_state::unchecked);

        // set_check: single node
        set_check(root.children[1], check_state::checked);  // "b"
        assert(root.children[1].checked == check_state::checked);

        // toggle_check: independent
        toggle_check(root.children[1], check_policy::independent);
        assert(root.children[1].checked == check_state::unchecked);
        toggle_check(root.children[1], check_policy::independent);
        assert(root.children[1].checked == check_state::checked);

        // propagate_check_down: check "a" → a1, a2 also checked
        propagate_check_down(root.children[0], check_state::checked);
        assert(root.children[0].checked == check_state::checked);
        assert(root.children[0].children[0].checked == check_state::checked);
        assert(root.children[0].children[1].checked == check_state::checked);

        // propagate_check_up: mixed children → indeterminate
        root.children[0].children[0].checked = check_state::unchecked;
        propagate_check_up(root.children[0]);
        assert(root.children[0].checked == check_state::indeterminate);

        // all children unchecked → parent unchecked
        root.children[0].children[1].checked = check_state::unchecked;
        propagate_check_up(root.children[0]);
        assert(root.children[0].checked == check_state::unchecked);

        // sync_check_tree: full bottom-up sync
        root.children[2].children[0].checked = check_state::checked;  // c1
        sync_check_tree(root);
        assert(root.children[2].checked == check_state::checked);   // c → all children checked
        // root has mixed: a(unchecked), b(checked), c(checked)
        assert(root.checked == check_state::indeterminate);

        // count_checked
        auto cc = count_checked(root);
        assert(cc.checked == 3);      // b, c, c1
        assert(cc.indeterminate == 1); // root
        assert(cc.unchecked == 3);     // a, a1, a2

        std::printf("  ✓ set_check, toggle_check, propagate_down/up, sync_tree, count_checked\n");
    }

    std::printf("── §10 collapse operations ─────────────────\n");
    {
        auto root = make_sample_tree<tf_collapsible>();

        // default: all expanded
        assert(root.expanded);
        assert(root.children[0].expanded);
        assert(root.children_visible());

        // toggle
        toggle_expanded(root.children[0]);
        assert(!root.children[0].expanded);
        assert(!root.children[0].children_visible());

        // count_visible: a collapsed → a1, a2 hidden
        assert(count_visible(root) == 5);   // root, a, b, c, c1

        // collapse_all
        collapse_all(root);
        assert(!root.expanded);
        assert(count_visible(root) == 1);  // only root

        // expand_all
        expand_all(root);
        assert(root.expanded);
        assert(count_visible(root) == 7);

        // expand_to path
        collapse_all(root);
        expand_to(root, {2, 0});  // expand path to c1
        assert(root.expanded);
        assert(root.children[2].expanded);
        assert(count_visible(root) == 5);  // root, a(collapsed), b, c, c1

        std::printf("  ✓ toggle, collapse_all, expand_all, expand_to, count_visible\n");
    }

    std::printf("── §11 icon operations ─────────────────────\n");
    {
        auto root = make_sample_tree<static_cast<unsigned>(tf_icons | tf_collapsible)>();

        set_icon(root, 0x1F4C1);  // 📁
        assert(root.icon == 0x1F4C1);
        assert(effective_icon(root) == 0x1F4C1);

        // dual icons
        set_icons(root, 0x1F4C1, 0x1F4C2);  // 📁 → 📂
        assert(root.use_expanded);
        assert(effective_icon(root) == 0x1F4C2);  // expanded by default

        root.expanded = false;
        assert(effective_icon(root) == 0x1F4C1);  // collapsed → normal icon

        // string icons
        tree_node<std::string, tf_icons, std::string> s("docs");
        set_icons(s, "📁", "📂");
        assert(s.icon == "📁");
        assert(s.expanded_icon == "📂");

        std::printf("  ✓ set_icon, set_icons, effective_icon (int + string icon types)\n");
    }

    std::printf("── §12 context operations ──────────────────\n");
    {
        auto root = make_sample_tree<tf_context>();

        // default: all actions available
        assert(has_action(root, ctx_open));
        assert(has_action(root, ctx_rename));
        assert(has_action(root, ctx_delete));

        // restrict actions on root
        set_actions(root, ctx_open | ctx_properties);
        assert( has_action(root, ctx_open));
        assert(!has_action(root, ctx_rename));
        assert(!has_action(root, ctx_delete));
        assert( has_action(root, ctx_properties));

        // children still have full actions
        assert(has_action(root.children[0], ctx_rename));

        std::printf("  ✓ has_action, set_actions, per-node context flags\n");
    }

    std::printf("── §13 sort / partition ────────────────────\n");
    {
        auto root = make_sample_tree<tf_none>();

        // sort children reverse alphabetically
        sort_children(root, [](const bare_node& a, const bare_node& b) {
            return a.data > b.data;
        });
        assert(root.children[0].data == "c");
        assert(root.children[1].data == "b");
        assert(root.children[2].data == "a");

        // sort_tree: recursive sort (restore alphabetical)
        sort_tree(root, [](const bare_node& a, const bare_node& b) {
            return a.data < b.data;
        });
        assert(root.children[0].data == "a");
        assert(root.children[0].children[0].data == "a1");

        // partition_directories_first: dirs (have children) before leaves
        partition_directories_first(root);
        // a(dir), c(dir) should come before b(leaf)
        assert(!root.children[0].is_leaf());  // a or c
        assert(!root.children[1].is_leaf());  // c or a
        assert( root.children[2].is_leaf());  // b

        std::printf("  ✓ sort_children, sort_tree, partition_directories_first\n");
    }

    std::printf("── §14 tree_view navigation ────────────────\n");
    {
        tree_view<std::string, tf_collapsible> tv;
        tv.roots.push_back(make_sample_tree<tf_collapsible>());
        tv.page_size = 3;

        // initial state
        auto& entries = tv.entries();
        assert(entries.size() == 7);
        assert(tv.cursor == 0);

        // cursor_down
        assert(tv.cursor_down());
        assert(tv.cursor == 1);
        assert(tv.cursor_node()->data == "a");

        // navigate to bottom
        tv.cursor_end();
        assert(tv.cursor == 6);
        assert(tv.cursor_node()->data == "c1");

        // scroll should have adjusted
        assert(tv.scroll_offset == 4);  // 6 - 3 + 1

        // cursor_home
        tv.cursor_home();
        assert(tv.cursor == 0);
        assert(tv.scroll_offset == 0);

        // page_down
        tv.page_down();
        assert(tv.cursor == 3);  // moved by page_size

        // page_up
        tv.page_up();
        assert(tv.cursor == 0);

        std::printf("  ✓ cursor_down, cursor_end, cursor_home, page_down, page_up\n");
    }

    std::printf("── §14b cursor_left / cursor_right ─────────\n");
    {
        tree_view<std::string, tf_collapsible> tv;
        tv.roots.push_back(make_sample_tree<tf_collapsible>());

        // cursor on root (expanded), cursor_right → first child
        assert(tv.cursor == 0);
        assert(tv.cursor_right());
        assert(tv.cursor == 1);  // "a"

        // cursor_right on expanded "a" → first child "a1"
        assert(tv.cursor_right());
        assert(tv.cursor == 2);  // "a1"

        // cursor_right on leaf → no-op
        assert(!tv.cursor_right());
        assert(tv.cursor == 2);

        // cursor_left on "a1" → go to parent "a"
        assert(tv.cursor_left());
        assert(tv.cursor == 1);  // "a"

        // cursor_left on expanded "a" → collapse it
        assert(tv.cursor_left());
        assert(tv.cursor == 1);  // still on "a", now collapsed
        assert(!tv.cursor_node()->expanded);
        assert(tv.visible_count() == 5);  // root, a(collapsed), b, c, c1

        // cursor_left on collapsed "a" → go to parent "root"
        assert(tv.cursor_left());
        assert(tv.cursor == 0);  // root

        std::printf("  ✓ cursor_left (collapse then parent), cursor_right (expand then child)\n");
    }

    std::printf("── §15 selection ───────────────────────────\n");
    {
        tree_view<std::string> tv;
        tv.roots.push_back(make_sample_tree<tf_none>());
        tv.sel_mode = selection_mode::single;

        tv.cursor = 2;
        tv.select_at_cursor();
        assert(tv.selected.size() == 1);
        assert(tv.is_selected(2));
        assert(!tv.is_selected(0));

        // single: selecting again replaces
        tv.cursor = 4;
        tv.select_at_cursor();
        assert(tv.selected.size() == 1);
        assert(tv.is_selected(4));
        assert(!tv.is_selected(2));

        // multi-select
        tv.sel_mode = selection_mode::multi;
        tv.clear_selection();
        tv.cursor = 1;
        tv.toggle_select_at_cursor();
        tv.cursor = 3;
        tv.toggle_select_at_cursor();
        assert(tv.selected.size() == 2);
        assert(tv.is_selected(1));
        assert(tv.is_selected(3));

        // toggle off
        tv.cursor = 1;
        tv.toggle_select_at_cursor();
        assert(tv.selected.size() == 1);
        assert(!tv.is_selected(1));

        // range select
        tv.select_range(1, 4);
        assert(tv.selected.size() == 4);

        // selected_nodes
        auto nodes = tv.selected_nodes();
        assert(nodes.size() == 4);

        std::printf("  ✓ single select, multi toggle, range, selected_nodes\n");
    }

    std::printf("── §16 rename ──────────────────────────────\n");
    {
        tree_view<std::string, tf_renamable> tv;
        tv.roots.push_back(make_sample_tree<tf_renamable>());

        // begin_edit_with
        tv.cursor = 1;  // "a"
        assert(tv.begin_edit_with("a"));
        assert(tv.editing);
        assert(tv.edit_index == 1);
        assert(tv.edit_buffer == "a");
        assert(tv.edit_cursor == 1);

        // commit
        tv.edit_buffer = "alpha";
        assert(tv.commit_edit());
        assert(!tv.editing);
        assert(tv.edit_buffer == "alpha");  // caller reads this

        // cancel
        tv.cursor = 2;
        tv.begin_edit_with("a1");
        tv.edit_buffer = "partial";
        tv.cancel_edit();
        assert(!tv.editing);
        assert(tv.edit_buffer.empty());

        // non-renamable node
        tv.cursor_node()->renamable = false;
        assert(!tv.begin_edit());

        std::printf("  ✓ begin_edit_with, commit_edit, cancel_edit, non-renamable guard\n");
    }

    std::printf("── §17 context menu ────────────────────────\n");
    {
        tree_view<std::string, tf_context> tv;
        tv.roots.push_back(make_sample_tree<tf_context>());

        tv.cursor = 2;
        assert(tv.open_context(10, 20));
        assert(tv.context_open);
        assert(tv.context_index == 2);
        assert(tv.context_x == 10);
        assert(tv.context_y == 20);

        auto* cn = tv.context_node();
        assert(cn != nullptr);
        assert(cn->data == "a1");
        assert(has_action(*cn, ctx_rename));

        tv.close_context();
        assert(!tv.context_open);

        std::printf("  ✓ open_context, context_node, close_context\n");
    }

    std::printf("── §18 search ──────────────────────────────\n");
    {
        tree_view<std::string> tv;
        tv.roots.push_back(make_sample_tree<tf_none>());
        tv.rebuild_visible();

        // search_next: find "b"
        bool found = tv.search_next([](const std::string& d) { 
            return d == "b"; 
        });
        assert(found);
        assert(tv.cursor_node()->data == "b");

        // search_next again: wraps around (still finds "b" since only one)
        found = tv.search_next([](const std::string& d) { return d == "b"; });
        assert(found);
        assert(tv.cursor_node()->data == "b");

        // search_next: find "c1"
        found = tv.search_next([](const std::string& d) { return d == "c1"; });
        assert(found);
        assert(tv.cursor_node()->data == "c1");

        // search_prev: find "a2" (before c1)
        found = tv.search_prev([](const std::string& d) { return d == "a2"; });
        assert(found);
        assert(tv.cursor_node()->data == "a2");

        // search_next: no match
        found = tv.search_next([](const std::string& d) { return d == "zzz"; });
        assert(!found);

        std::printf("  ✓ search_next, search_prev, wrap-around, no-match\n");
    }

    std::printf("── §14c tree_view checkbox ─────────────────\n");
    {
        tree_view<std::string, tf_checkable> tv;
        tv.roots.push_back(make_sample_tree<tf_checkable>());
        tv.policy = check_policy::cascade_both;

        // toggle root → all checked
        tv.cursor = 0;
        tv.toggle_check_at_cursor();

        // verify all checked
        auto checked = tv.checked_nodes();
        assert(checked.size() == tv.visible_count());

        // uncheck a leaf → root becomes indeterminate
        tv.cursor = 2;  // "a1"
        tv.toggle_check_at_cursor();
        assert(tv.cursor_node()->checked == check_state::unchecked);
        // root should be indeterminate
        assert(tv.roots[0].checked == check_state::indeterminate);
        // "a" should be indeterminate
        assert(tv.roots[0].children[0].checked == check_state::indeterminate);

        std::printf("  ✓ toggle_check_at_cursor with cascade_both\n");
    }

    std::printf("\n── ALL TESTS PASSED ─────────────────────────\n");
    return 0;
}
