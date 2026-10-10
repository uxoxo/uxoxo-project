/*******************************************************************************
* djinterp [test]                                              static_tests.cpp
*
*   Build:  g++ -std=c++17 -I. -o test static_tests.cpp && ./test
*******************************************************************************/

#include <container/menu_bar.hpp>
#include <container/tree_view.hpp>
#include <container/list_view.hpp>
#include <container/radio_group.hpp>

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace djinterp::container;
namespace mt  = djinterp::container::menu_traits;
namespace mbt = djinterp::container::menu_bar_traits;

// ═══════════════════════════════════════════════════════════════════════════════
//  §1  ABSTRACT MENU TRAITS — tagless detection of foreign types
// ═══════════════════════════════════════════════════════════════════════════════

// std::vector<std::string> IS a menu (has value_type + begin/end + size)
static_assert( mt::is_menu_v<std::vector<std::string>>,
    "vector<string> IS a menu");
static_assert( mt::is_sized_menu_v<std::vector<std::string>>,
    "vector<string> IS a sized menu");

// int is NOT a menu
static_assert(!mt::is_menu_v<int>);
// std::string IS a menu (value_type=char + begin/end) — that's structural, not semantic
static_assert( mt::is_menu_v<std::string>);

// ── user-defined types ───────────────────────────────────────────────────

// minimal: just value_type + begin/end
struct minimal_menu {
    using value_type = std::string;
    const std::string* begin() const { return nullptr; }
    const std::string* end()   const { return nullptr; }
};
static_assert( mt::is_menu_v<minimal_menu>);
static_assert(!mt::is_sized_menu_v<minimal_menu>);  // no .size()

// with separator support (flag on item)
struct sep_item { std::string text; bool is_separator = false; };
struct sep_menu {
    using value_type = sep_item;
    std::vector<sep_item> items;
    auto begin() const { return items.begin(); }
    auto end()   const { return items.end(); }
    std::size_t size() const { return items.size(); }
};
static_assert( mt::is_menu_v<sep_menu>);
static_assert( mt::supports_separators_v<sep_menu>);  // sep_item has .is_separator

// with shortcut support
struct sc_item { std::string text; std::string shortcut; };
struct sc_menu {
    using value_type = sc_item;
    std::vector<sc_item> items;
    auto begin() const { return items.begin(); }
    auto end()   const { return items.end(); }
};
static_assert( mt::is_menu_v<sc_menu>);
static_assert( mt::supports_shortcuts_v<sc_menu>);  // sc_item has .shortcut

// with mnemonic
struct mn_item { std::string text; char mnemonic = 0; };
struct mn_menu {
    using value_type = mn_item;
    std::vector<mn_item> items;
    auto begin() const { return items.begin(); }
    auto end()   const { return items.end(); }
};
static_assert( mt::supports_shortcuts_v<mn_menu>);
static_assert( mt::has_mnemonic_v<mn_menu>);

// with submenu
struct sub_item { std::string text; void* submenu = nullptr; };
struct sub_menu {
    using value_type = sub_item;
    std::vector<sub_item> items;
    auto begin() const { return items.begin(); }
    auto end()   const { return items.end(); }
};
static_assert( mt::supports_submenus_v<sub_menu>);

// items with labels
struct labeled_item { std::string label; };
struct labeled_menu {
    using value_type = labeled_item;
    std::vector<labeled_item> items;
    auto begin() const { return items.begin(); }
    auto end()   const { return items.end(); }
};
static_assert( mt::items_have_labels_v<labeled_menu>);
static_assert(!mt::items_have_labels_v<std::vector<std::string>>); // string has no .label

// items with actions
struct act_item { std::string label; void(*action)() = nullptr; };
struct act_menu {
    using value_type = act_item;
    std::vector<act_item> items;
    auto begin() const { return items.begin(); }
    auto end()   const { return items.end(); }
};
static_assert( mt::items_are_activatable_v<act_menu>);
static_assert(!mt::items_are_activatable_v<std::vector<std::string>>);

// separator_indicator priority
static_assert(std::is_void_v<mt::separator_indicator<std::vector<std::string>>::type>,
    "vector<string> has no separator support");
static_assert(!std::is_void_v<mt::separator_indicator<sep_menu>::type>,
    "sep_menu has separator support via flag");

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  OUR CONCRETE MENU TYPES — satisfy abstract traits
// ═══════════════════════════════════════════════════════════════════════════════

using bm  = menu<std::string>;
using fm  = menu<std::string, mf_all>;
using bmi = menu_item<std::string>;
using fmi = menu_item<std::string, mf_all>;

// our menu satisfies is_menu
static_assert( mt::is_menu_v<bm>,      "menu<> IS a menu");
static_assert( mt::is_menu_v<fm>,      "menu<mf_all> IS a menu");
static_assert( mt::is_sized_menu_v<bm>,"menu<> IS a sized menu");

// items have labels (the .label member)
static_assert( mt::items_have_labels_v<bm>);

// separator support: our items have .is_separator() method but not a
// .is_separator member — the method is on the item, detected via the
// flag member (type field check is semantic, not SFINAE).
// Actually our menu itself has add_separator(), which IS detected:
static_assert( mt::supports_separators_v<bm>);

// feature flags
static_assert(!bmi::has_shortcuts);
static_assert( fmi::has_shortcuts);
static_assert( fmi::has_icons);
static_assert( fmi::is_checkable);
static_assert( fmi::has_submenus);

// sizeof
static_assert(sizeof(bmi) < sizeof(fmi), "features add bytes");

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  STATIC MENU — satisfies abstract traits
// ═══════════════════════════════════════════════════════════════════════════════

using sm4 = static_menu<std::string, 4>;
static_assert( mt::is_menu_v<sm4>,       "static_menu IS a menu");
static_assert( mt::is_sized_menu_v<sm4>, "static_menu IS a sized menu");

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  MENU BAR TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

using bmb  = menu_bar<std::string>;
using fmb  = menu_bar<std::string, mf_all>;
using bmbe = menu_bar_entry<std::string>;

static_assert( mbt::is_menu_bar_v<bmb>);
static_assert( mbt::is_menu_bar_v<fmb>);
static_assert(!mbt::is_menu_bar_v<bm>);         // menu is NOT a menu_bar
static_assert(!mbt::is_menu_bar_v<std::string>);
static_assert(!mbt::is_menu_bar_v<int>);

static_assert( mbt::is_menu_bar_entry_v<bmbe>);
static_assert(!mbt::is_menu_bar_entry_v<bmi>);   // menu_item is NOT an entry

static_assert( mbt::has_entries_v<bmb>);
static_assert( mbt::has_focused_v<bmb>);
static_assert( mbt::has_active_v<bmb>);
static_assert( mbt::has_dd_cursor_v<bmb>);
static_assert( mbt::has_open_dropdown_v<bmb>);

static_assert( view_traits::is_focusable_v<bmb>);
static_assert(!view_traits::is_scrollable_v<bmb>);

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  CROSS-CHECKS — menu types not confused with other components
// ═══════════════════════════════════════════════════════════════════════════════

static_assert(!tree_traits::is_tree_view_v<bmb>);
static_assert(!list_traits::is_list_view_v<bmb>);
static_assert(!radio_traits::is_radio_group_v<bmb>);

static_assert(!mbt::is_menu_bar_v<tree_view<std::string>>);
static_assert(!mbt::is_menu_bar_v<list_view<std::string>>);
static_assert(!mbt::is_menu_bar_v<radio_group<std::string>>);


/*****************************************************************************/
// RUNTIME
/*****************************************************************************/

int main()
{
    std::printf("── sizeof ──────────────────────────────────\n");
    std::printf("  menu_item<>           : %zu\n", sizeof(bmi));
    std::printf("  menu_item<mf_all>     : %zu\n", sizeof(fmi));
    std::printf("  menu<>                : %zu\n", sizeof(bm));
    std::printf("  menu_bar<>            : %zu\n", sizeof(bmb));
    std::printf("  menu_bar_entry<>      : %zu\n", sizeof(bmbe));
    std::printf("  static_menu<string,4> : %zu\n", sizeof(sm4));
    std::printf("  ✓ sizeof checks\n");

    // ── menu construction + mutation ─────────────────────────────────────
    std::printf("── menu: construction ─────────────────────\n");
    {
        menu<> m("File");
        m.emplace("New");
        m.emplace("Open");
        m.add_separator();
        m.emplace("Save");
        m.emplace("Save As...");
        m.add_separator();
        m.emplace("Exit");

        assert(m.size() == 7);
        assert(m.title == "File");
        assert(m[0].label == "New");
        assert(m[2].is_separator());
        assert(!m[0].is_separator());
        assert(m.selectable_count() == 5);  // 7 - 2 separators

        std::printf("  ✓ emplace, add_separator, selectable_count\n");
    }

    // ── menu navigation helpers ──────────────────────────────────────────
    std::printf("── menu: navigation ───────────────────────\n");
    {
        menu<> m("Edit");
        m.emplace("Undo");        // 0
        m.add_separator();         // 1
        m.emplace("Cut");         // 2
        m.emplace("Copy");        // 3
        m.emplace("Paste");       // 4

        assert(m.first_selectable() == 0);
        assert(m.next_selectable(0) == 2);  // skip separator
        assert(m.next_selectable(2) == 3);
        assert(m.next_selectable(4) == 0);  // wrap

        assert(m.prev_selectable(2) == 0);  // skip separator
        assert(m.prev_selectable(0) == 4);  // wrap

        // disabled item
        m[3].enabled = false;  // disable "Copy"
        assert(m.next_selectable(2) == 4);  // skip disabled Copy
        assert(m.selectable_count() == 3);  // Undo, Cut, Paste

        std::printf("  ✓ first/next/prev_selectable, skip sep + disabled\n");
    }

    // ── menu features ────────────────────────────────────────────────────
    std::printf("── menu: features ─────────────────────────\n");
    {
        menu<std::string, mf_all> m("View");
        auto& item = m.emplace("Toolbar");
        set_shortcut(item, "Alt+T");
        set_icon(item, 42);
        set_checked(item, true);

        assert(item.shortcut == "Alt+T");
        assert(item.icon == 42);
        assert(item.checked);

        toggle_checked(item);
        assert(!item.checked);

        // submenu
        auto sub = std::make_unique<menu<std::string, mf_all>>("Panels");
        sub->emplace("Left");
        sub->emplace("Right");
        auto& sub_item = m.emplace("Panels");
        attach_submenu(sub_item, std::move(sub));
        assert(sub_item.has_submenu());

        std::printf("  ✓ shortcut, icon, checked, toggle, submenu\n");
    }

    // ── menu find ────────────────────────────────────────────────────────
    std::printf("── menu: find ─────────────────────────────\n");
    {
        menu<> m("File");
        m.emplace("New"); m.emplace("Open"); m.emplace("Save");

        auto idx = m.find_by([](const std::string& s) { return s == "Open"; });
        assert(idx == 1);

        idx = m.find_by([](const std::string& s) { return s == "MISSING"; });
        assert(idx == static_cast<std::size_t>(-1));

        std::printf("  ✓ find_by\n");
    }

    // ── static_menu ──────────────────────────────────────────────────────
    std::printf("── static_menu ────────────────────────────\n");
    {
        static_menu sm("File", "Edit", "View", "Help");
        // deduced as static_menu<const char*, 4>

        assert(sm.size() == 4);
        assert(!sm.empty());

        // range-for
        int count = 0;
        for (const auto& item : sm) {
            (void)item;
            ++count;
        }
        assert(count == 4);

        std::printf("  ✓ construction, size, range-for\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  MENU BAR
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── menu_bar: construction ─────────────────\n");
    {
        menu_bar<> mb;

        // build file menu
        auto file_dd = std::make_unique<menu<>>("File");
        file_dd->emplace("New");
        file_dd->emplace("Open");
        file_dd->add_separator();
        file_dd->emplace("Exit");

        // build edit menu
        auto edit_dd = std::make_unique<menu<>>("Edit");
        edit_dd->emplace("Undo");
        edit_dd->emplace("Cut");
        edit_dd->emplace("Copy");
        edit_dd->emplace("Paste");

        mb.emplace("File", std::move(file_dd));
        mb.emplace("Edit", std::move(edit_dd));
        mb.emplace("View");  // no dropdown
        mb.emplace("Help");  // no dropdown

        assert(mb.count() == 4);
        assert(!mb.empty());
        assert(mb.focused == 0);
        assert(!mb.active);

        // focused entry
        auto* fe = mb.focused_entry();
        assert(fe && fe->label == "File");
        assert(fe->has_dropdown());

        // "View" has no dropdown
        assert(!mb.entries[2].has_dropdown());

        std::printf("  ✓ emplace, count, focused_entry, has_dropdown\n");
    }

    std::printf("── menu_bar: bar navigation ───────────────\n");
    {
        menu_bar<> mb;
        mb.emplace("File"); mb.emplace("Edit"); mb.emplace("View"); mb.emplace("Help");

        assert(mb.focused == 0);
        mb.next(); assert(mb.focused == 1);
        mb.next(); assert(mb.focused == 2);
        mb.next(); assert(mb.focused == 3);
        mb.next(); assert(mb.focused == 0);  // wrap

        mb.prev(); assert(mb.focused == 3);
        mb.prev(); assert(mb.focused == 2);

        // skip disabled
        mb.entries[1].enabled = false;  // disable "Edit"
        mb.focused = 0;
        mb.next(); assert(mb.focused == 2);  // skipped Edit
        mb.prev(); assert(mb.focused == 0);  // skipped Edit back

        std::printf("  ✓ next, prev, wrap, skip disabled\n");
    }

    std::printf("── menu_bar: dropdown management ──────────\n");
    {
        menu_bar<> mb;

        auto dd = std::make_unique<menu<>>("File");
        dd->emplace("New");       // 0
        dd->add_separator();       // 1
        dd->emplace("Open");      // 2
        dd->emplace("Exit");      // 3

        mb.emplace("File", std::move(dd));
        mb.emplace("Edit");

        // open dropdown
        assert(mb.open_dropdown());
        assert(mb.active);
        assert(mb.dd_cursor == 0);  // first selectable

        // navigate dropdown
        assert(mb.dd_next());
        assert(mb.dd_cursor == 2);  // skipped separator

        assert(mb.dd_next());
        assert(mb.dd_cursor == 3);  // Exit

        assert(mb.dd_next());
        assert(mb.dd_cursor == 0);  // wrap to New

        assert(mb.dd_prev());
        assert(mb.dd_cursor == 3);

        // home / end
        mb.dd_home(); assert(mb.dd_cursor == 0);
        mb.dd_end();  assert(mb.dd_cursor == 3);

        // active_item
        auto* ai = mb.active_item();
        assert(ai && ai->label == "Exit");

        // close
        mb.close_dropdown();
        assert(!mb.active);
        assert(mb.active_menu() == nullptr);

        // toggle
        mb.toggle_dropdown();
        assert(mb.active);
        mb.toggle_dropdown();
        assert(!mb.active);

        std::printf("  ✓ open/close/toggle, dd_next/prev/home/end, active_item\n");
    }

    std::printf("── menu_bar: activate ─────────────────────\n");
    {
        menu_bar<> mb;

        auto dd = std::make_unique<menu<>>("File");
        dd->emplace("New");
        dd->emplace("Open");

        mb.emplace("File", std::move(dd));
        mb.emplace("Help");

        // activate on entry with dropdown → opens it
        auto* e = mb.activate();
        assert(e && e->label == "File");
        assert(mb.active);

        // activate_dd_item → returns item, closes dropdown
        mb.dd_cursor = 1;  // "Open"
        auto* item = mb.activate_dd_item();
        assert(item && item->label == "Open");
        assert(!mb.active);  // closed after activation

        // activate on entry without dropdown
        mb.focused = 1;
        e = mb.activate();
        assert(e && e->label == "Help");
        assert(!mb.active);  // no dropdown to open

        std::printf("  ✓ activate, activate_dd_item, auto-close\n");
    }

    std::printf("── menu_bar: bar nav opens dropdown ───────\n");
    {
        menu_bar<> mb;

        auto dd1 = std::make_unique<menu<>>("F");
        dd1->emplace("New");
        auto dd2 = std::make_unique<menu<>>("E");
        dd2->emplace("Undo");

        mb.emplace("File", std::move(dd1));
        mb.emplace("Edit", std::move(dd2));

        mb.open_dropdown();
        assert(mb.active && mb.focused == 0);

        // move right while dropdown open → opens Edit's dropdown
        mb.next();
        assert(mb.focused == 1);
        assert(mb.active);  // still active
        auto* am = mb.active_menu();
        assert(am && am->title == "E");

        std::printf("  ✓ bar nav while dropdown open switches to new dropdown\n");
    }

    std::printf("── menu_bar: search ───────────────────────\n");
    {
        menu_bar<> mb;

        auto dd = std::make_unique<menu<>>("File");
        dd->emplace("New");
        dd->emplace("Open");
        dd->emplace("Save");

        mb.emplace("File", std::move(dd));
        mb.emplace("Edit");
        mb.emplace("View");

        // search bar
        bool f = mb.search_bar([](const std::string& l) { return l == "View"; });
        assert(f && mb.focused == 2);

        // search dropdown
        mb.focused = 0;
        mb.open_dropdown();
        f = mb.search_dropdown([](const std::string& l) { return l == "Save"; });
        assert(f && mb.dd_cursor == 2);

        std::printf("  ✓ search_bar, search_dropdown\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  ABSTRACT TRAITS ON CONCRETE TYPES — runtime confirmation
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── abstract traits on concrete types ──────\n");
    {
        // our menu satisfies all the abstract detectors appropriately
        static_assert(mt::is_menu_v<menu<>>);
        static_assert(mt::is_sized_menu_v<menu<>>);
        static_assert(mt::supports_separators_v<menu<>>);  // has add_separator()
        static_assert(mt::items_have_labels_v<menu<>>);    // items have .label

        // full-featured menu items satisfy more
        using fm = menu<std::string, mf_all>;
        static_assert(mt::is_menu_v<fm>);
        static_assert(mt::supports_shortcuts_v<fm>);  // items have .shortcut
        static_assert(mt::supports_submenus_v<fm>);   // items have .submenu
        static_assert(mt::has_item_checked_v<fm>);    // items have .checked
        static_assert(mt::has_item_icon_v<fm>);       // items have .icon
        static_assert(mt::has_item_enabled_v<fm>);    // items have .enabled

        std::printf("  ✓ concrete types satisfy abstract menu_traits\n");
    }

    std::printf("\n── ALL TESTS PASSED ─────────────────────────\n");
    return 0;
}
