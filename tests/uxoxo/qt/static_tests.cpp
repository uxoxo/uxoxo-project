/*******************************************************************************
* djinterp [test]                                              static_tests.cpp
*
*   Tests qt_adapter.hpp shared adapter, qt_tree_view.hpp traits and callback
*   types, and cross-checks with qt_menu_bar.  All without Qt present.
*
*   Build:  g++ -std=c++17 -I. -o test static_tests.cpp && ./test
*******************************************************************************/

#include <container/tree_view.hpp>
#include <container/menu_bar.hpp>
#include <ui/qt/qt_adapter.hpp>
#include <ui/qt/qt_menu_bar.hpp>
#include <ui/qt/qt_tree_view.hpp>

#include <cassert>
#include <cstdio>
#include <string>
#include <functional>

using namespace djinterp::container;
namespace at   = djinterp::ui::qt::adapter_traits;
namespace tvt  = djinterp::ui::qt::qt_tree_view_traits;
namespace mbt  = djinterp::ui::qt::qt_menu_bar_traits;


// ═══════════════════════════════════════════════════════════════════════════════
//  COMPILE-TIME: shared adapter traits
// ═══════════════════════════════════════════════════════════════════════════════

using def_adapter = djinterp::ui::qt::qt_adapter<std::string, int>;
using str_adapter = djinterp::ui::qt::qt_adapter<std::string, std::string>;

static_assert( at::is_qt_adapter_v<def_adapter>);
static_assert( at::is_qt_adapter_v<str_adapter>);
static_assert( at::has_data_type_v<def_adapter>);
static_assert( at::has_icon_type_v<def_adapter>);
static_assert(!at::is_qt_adapter_v<int>);
static_assert(!at::is_qt_adapter_v<std::string>);

// backward compat alias
using menu_adapter = djinterp::ui::qt::qt_menu_adapter<std::string, int>;
static_assert( at::is_qt_adapter_v<menu_adapter>);
static_assert(std::is_same_v<menu_adapter, def_adapter>);

// ═══════════════════════════════════════════════════════════════════════════════
//  COMPILE-TIME: qt_tree_view_traits (works without Qt)
// ═══════════════════════════════════════════════════════════════════════════════

// tree_view model types exist
using tv_bare = tree_view<std::string>;
using tv_full = tree_view<std::string, vf_tree_all>;

static_assert( tree_traits::is_tree_view_v<tv_bare>);
static_assert( tree_traits::is_tree_view_v<tv_full>);

// callback types instantiate correctly (no Qt needed)
using act_cb = djinterp::ui::qt::tree_action_callback<std::string, vf_none, int>;
using chk_cb = djinterp::ui::qt::tree_check_callback<std::string, vf_checkable, int>;
using ctx_cb = djinterp::ui::qt::tree_context_callback<std::string, vf_context, int>;
using ren_cb = djinterp::ui::qt::tree_rename_callback<std::string, vf_renamable, int>;

static_assert(sizeof(act_cb) > 0);
static_assert(sizeof(chk_cb) > 0);
static_assert(sizeof(ctx_cb) > 0);
static_assert(sizeof(ren_cb) > 0);

// simple_callback (from qt_adapter)
using simple_cb = djinterp::ui::qt::simple_callback<std::string>;
static_assert(sizeof(simple_cb) > 0);

// ═══════════════════════════════════════════════════════════════════════════════
//  COMPILE-TIME: cross-check — traits don't confuse types
// ═══════════════════════════════════════════════════════════════════════════════

// qt_tree_view_traits should not detect menu_bar_traits types
static_assert(!tvt::has_read_back_v<std::string>);
static_assert(!tvt::is_qt_tree_view_bridge_v<std::string>);
static_assert(!tvt::is_qt_tree_view_bridge_v<int>);

// menu bar traits should not detect tree types
static_assert(!mbt::has_widget_v<tv_bare>);

// tree_view is not a menu
static_assert(!menu_traits::is_menu_v<tv_bare>);

// menu_bar is not a tree
static_assert(!tree_traits::is_tree_view_v<menu_bar<std::string>>);


/*****************************************************************************/
// RUNTIME
/*****************************************************************************/

// helper: build sample tree (7 nodes)
template <unsigned F>
tree_node<std::string, F> make_tree()
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
    std::printf("── sizeof ──────────────────────────────────\n");
    std::printf("  qt_adapter<string,int>  : %zu\n", sizeof(def_adapter));
    std::printf("  tree_action_callback    : %zu\n", sizeof(act_cb));
    std::printf("  tree_check_callback     : %zu\n", sizeof(chk_cb));
    std::printf("  tree_context_callback   : %zu\n", sizeof(ctx_cb));
    std::printf("  tree_rename_callback    : %zu\n", sizeof(ren_cb));
    std::printf("  ✓ sizeof checks\n");

    // ═════════════════════════════════════════════════════════════════════
    //  CALLBACK TYPE VERIFICATION
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── callback types: callable ───────────────\n");
    {
        // action callback
        bool called = false;
        act_cb act = [&called](const tree_node<std::string>& node,
                               std::size_t depth) {
            called = true;
            assert(node.data == "test");
            assert(depth == 2);
        };
        tree_node<std::string> test_node("test");
        act(test_node, 2);
        assert(called);

        // check callback
        called = false;
        chk_cb chk = [&called](tree_node<std::string, vf_checkable>& node,
                                check_state state) {
            called = true;
            assert(state == check_state::checked);
            node.checked = state;
        };
        tree_node<std::string, vf_checkable> chk_node("chk");
        chk(chk_node, check_state::checked);
        assert(called);
        assert(chk_node.checked == check_state::checked);

        // context callback
        called = false;
        ctx_cb ctx = [&called](tree_node<std::string, vf_context>& node,
                               unsigned actions, int x, int y) {
            called = true;
            assert(x == 100 && y == 200);
            (void)node; (void)actions;
        };
        tree_node<std::string, vf_context> ctx_node("ctx");
        ctx(ctx_node, ctx_all, 100, 200);
        assert(called);

        // rename callback
        called = false;
        ren_cb ren = [&called](tree_node<std::string, vf_renamable>& node,
                               const std::string& old_label,
                               const std::string& new_text) {
            called = true;
            assert(old_label == "old");
            assert(new_text == "new");
            (void)node;
        };
        tree_node<std::string, vf_renamable> ren_node("ren");
        ren(ren_node, "old", "new");
        assert(called);

        std::printf("  ✓ action, check, context, rename callbacks callable\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  MODEL OPERATIONS (verify tree_view still works correctly)
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── tree_view model: regression ────────────\n");
    {
        tree_view<std::string, vf_collapsible | vf_checkable> tv;
        tv.roots.push_back(make_tree<vf_collapsible | vf_checkable>());

        // initial state
        assert(tv.visible_count() == 7);
        assert(tv.cursor_node()->data == "root");

        // navigation
        tv.cursor_down();
        assert(tv.cursor_node()->data == "a");
        tv.cursor_right();
        assert(tv.cursor_node()->data == "a1");

        // collapse
        tv.cursor = 1;  // back to "a"
        tv.cursor_left();  // collapse a
        assert(tv.visible_count() == 5);  // a1, a2 hidden

        // checkbox operations
        tv.policy = check_policy::cascade_both;
        tv.cursor = 0;
        tv.toggle_check_at_cursor();  // check root → all checked
        auto checked = tv.checked_nodes();
        assert(checked.size() == 7);

        // check_all / uncheck_all
        tv.uncheck_all();
        assert(tv.checked_nodes().empty());
        tv.check_all();
        assert(tv.checked_nodes().size() == 7);
        tv.toggle_check_all();  // all checked → uncheck
        assert(tv.checked_nodes().empty());

        std::printf("  ✓ nav, collapse, checkbox, check_all/uncheck_all\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  FEATURE FLAG VERIFICATION
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── feature flags ──────────────────────────\n");
    {
        // bare tree: no features
        using bare_node = tree_node<std::string>;
        static_assert(!bare_node::is_checkable);
        static_assert(!bare_node::has_icons);
        static_assert(!bare_node::is_collapsible);
        static_assert(!bare_node::is_renamable);
        static_assert(!bare_node::has_context);

        // full tree: all features
        using full_node = tree_node<std::string, vf_tree_all>;
        static_assert( full_node::is_checkable);
        static_assert( full_node::has_icons);
        static_assert( full_node::is_collapsible);
        static_assert( full_node::is_renamable);
        static_assert( full_node::has_context);

        // sizeof difference
        static_assert(sizeof(bare_node) < sizeof(full_node));

        std::printf("  bare_node: %zu bytes\n", sizeof(bare_node));
        std::printf("  full_node: %zu bytes\n", sizeof(full_node));
        std::printf("  ✓ EBO feature flags verified\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  QT BRIDGE STATUS
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── Qt bridge status ───────────────────────\n");
    {
    #if D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS
        std::printf("  Qt detected: qt_tree_view class available\n");

        // verify trait detection on the bridge class itself
        using bridge_t = djinterp::ui::qt::qt_tree_view<std::string, vf_tree_all>;
        static_assert(tvt::is_qt_tree_view_bridge_v<bridge_t>);
        static_assert(tvt::has_widget_v<bridge_t>);
        static_assert(tvt::has_sync_v<bridge_t>);
        static_assert(tvt::has_model_v<bridge_t>);
        static_assert(tvt::has_read_back_v<bridge_t>);
    #else
        std::printf("  Qt not detected: adapter/callback/traits verified\n");
        std::printf("  (qt_tree_view class excluded by preprocessor guard)\n");
    #endif

    #if D_ENV_LANG_IS_CPP17_OR_HIGHER
        std::printf("  C++ standard: C++17+ (if constexpr path)\n");
    #elif D_ENV_LANG_IS_CPP14_OR_HIGHER
        std::printf("  C++ standard: C++14 (tag dispatch path)\n");
    #else
        std::printf("  C++ standard: C++11 (tag dispatch path)\n");
    #endif

        std::printf("  ✓ environment detection functional\n");
    }

    std::printf("\n── ALL TESTS PASSED ─────────────────────────\n");
    return 0;
}
