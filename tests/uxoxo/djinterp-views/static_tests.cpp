/*******************************************************************************
* djinterp [test]                                              static_tests.cpp
*
*   Build:  g++ -std=c++17 -I. -o test static_tests.cpp && ./test
*******************************************************************************/

#include <container/tree_view.hpp>
#include <container/list_view.hpp>
#include <container/radio_group.hpp>
#include <container/text_input.hpp>

#include <cassert>
#include <cstdio>
#include <string>

using namespace djinterp::container;
namespace vt  = djinterp::container::view_traits;
namespace rt  = djinterp::container::radio_traits;
namespace tit = djinterp::container::text_input_traits;

// ═══════════════════════════════════════════════════════════════════════════════
//  RADIO — COMPILE-TIME
// ═══════════════════════════════════════════════════════════════════════════════

using bro = radio_option<std::string>;
using fro = radio_option<std::string, vf_icons | vf_context>;
using brg = radio_group<std::string>;
using frg = radio_group<std::string, vf_icons | vf_context>;

// option traits
static_assert( rt::is_radio_option_v<bro>);
static_assert( rt::is_radio_option_v<fro>);
static_assert(!rt::is_radio_option_v<std::string>);
static_assert(!rt::is_radio_option_v<int>);
static_assert( rt::has_enabled_v<bro>);
static_assert( vt::has_data_v<bro>);
// radio_option is NOT a tree_node (no children)
static_assert(!tree_traits::is_tree_node_v<bro>);
// radio_option is NOT a list_entry (has enabled member; structural overlap is fine,
// the trait system distinguishes by the concrete type or the composite traits)

// group traits
static_assert( rt::is_radio_group_v<brg>);
static_assert( rt::is_radio_group_v<frg>);
static_assert(!rt::is_radio_group_v<std::string>);
static_assert( vt::is_focusable_v<brg>);
static_assert(!vt::is_scrollable_v<brg>);
static_assert( rt::has_options_v<brg>);
static_assert( rt::has_orient_v<brg>);
static_assert( rt::has_focused_v<brg>);
static_assert( rt::has_wrap_v<brg>);

// NOT confused with tree/list
static_assert(!tree_traits::is_tree_view_v<brg>);
static_assert(!list_traits::is_list_view_v<brg>);

// feature detection
static_assert(!fro::has_icons == false);   // wait, fro::has_icons should be true
static_assert( fro::has_icons);
static_assert( fro::has_context);
static_assert(!bro::has_icons);
static_assert(!bro::has_context);

// sizeof — bare option should be small
static_assert(sizeof(bro) < sizeof(fro), "features add bytes");

// ═══════════════════════════════════════════════════════════════════════════════
//  TEXT INPUT — COMPILE-TIME
// ═══════════════════════════════════════════════════════════════════════════════

using bti = text_input<>;
using mti = text_input<tif_multiline>;
using hti = text_input<tif_history>;
using vti = text_input<tif_validation>;
using kti = text_input<tif_masked>;
using ati = text_input<tif_all>;
using iti = text_input<vf_icons | tif_history>;

// identity
static_assert( tit::is_text_input_v<bti>);
static_assert( tit::is_text_input_v<ati>);
static_assert(!tit::is_text_input_v<std::string>);
static_assert(!tit::is_text_input_v<int>);
static_assert( vt::is_focusable_v<bti>);

// NOT confused with other types
static_assert(!tree_traits::is_tree_view_v<bti>);
static_assert(!list_traits::is_list_view_v<bti>);
static_assert(!rt::is_radio_group_v<bti>);

// feature composites
static_assert(!tit::is_multiline_input_v<bti>);
static_assert( tit::is_multiline_input_v<mti>);
static_assert( tit::is_multiline_input_v<ati>);

static_assert(!tit::is_history_input_v<bti>);
static_assert( tit::is_history_input_v<hti>);
static_assert( tit::is_history_input_v<ati>);

static_assert(!tit::is_validated_input_v<bti>);
static_assert( tit::is_validated_input_v<vti>);

static_assert(!tit::is_masked_text_input_v<bti>);
static_assert( tit::is_masked_text_input_v<kti>);

// member detection
static_assert( tit::has_value_v<bti>);
static_assert( tit::has_cursor_v<bti>);
static_assert( tit::has_placeholder_v<bti>);
static_assert( tit::has_sel_anchor_v<bti>);
static_assert( tit::has_read_only_v<bti>);
static_assert(!tit::has_history_v<bti>);
static_assert( tit::has_history_v<hti>);
static_assert(!tit::has_validators_v<bti>);
static_assert( tit::has_validators_v<vti>);
static_assert(!tit::has_masked_v<bti>);
static_assert( tit::has_masked_v<kti>);
static_assert(!tit::has_word_wrap_v<bti>);
static_assert( tit::has_word_wrap_v<mti>);

// mixed feature flags (view_feat | text_input_feat)
static_assert( iti::has_icons);
static_assert( iti::has_history);
static_assert(!iti::is_multiline);

// sizeof
static_assert(sizeof(bti) < sizeof(ati), "all features add bytes");


/*****************************************************************************/
// RUNTIME
/*****************************************************************************/

int main()
{
    std::printf("── sizeof ──────────────────────────────────\n");
    std::printf("  radio_option<>       : %zu\n", sizeof(bro));
    std::printf("  radio_option<icons>  : %zu\n", sizeof(fro));
    std::printf("  radio_group<>        : %zu\n", sizeof(brg));
    std::printf("  radio_group<full>    : %zu\n", sizeof(frg));
    std::printf("  text_input<>         : %zu\n", sizeof(bti));
    std::printf("  text_input<multi>    : %zu\n", sizeof(mti));
    std::printf("  text_input<history>  : %zu\n", sizeof(hti));
    std::printf("  text_input<valid>    : %zu\n", sizeof(vti));
    std::printf("  text_input<masked>   : %zu\n", sizeof(kti));
    std::printf("  text_input<all>      : %zu\n", sizeof(ati));
    std::printf("  ✓ sizeof checks\n");

    // ═════════════════════════════════════════════════════════════════════
    //  RADIO GROUP TESTS
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── radio: construction ─────────────────────\n");
    {
        radio_group<> rg;
        rg.emplace("Red"); rg.emplace("Green"); rg.emplace("Blue");
        assert(rg.count() == 3);
        assert(!rg.empty());
        assert(rg.selected == 0);
        assert(rg.selected_data() && *rg.selected_data() == "Red");

        std::printf("  ✓ emplace, count, selected_data\n");
    }

    std::printf("── radio: selection ────────────────────────\n");
    {
        radio_group<> rg;
        rg.emplace("A"); rg.emplace("B"); rg.emplace("C");

        assert(rg.select(2));
        assert(rg.selected == 2);
        assert(*rg.selected_data() == "C");

        // select same → no change
        assert(!rg.select(2));

        // out of range
        assert(!rg.select(99));

        // disabled option → rejected
        rg.options[1].enabled = false;
        assert(!rg.select(1));
        assert(rg.selected == 2);  // unchanged

        std::printf("  ✓ select, disabled guard, out-of-range\n");
    }

    std::printf("── radio: navigation ──────────────────────\n");
    {
        radio_group<> rg;
        rg.emplace("A"); rg.emplace("B"); rg.emplace("C"); rg.emplace("D");
        rg.wrap = true;

        assert(rg.focused == 0);

        // next
        assert(rg.next()); assert(rg.focused == 1);
        assert(rg.next()); assert(rg.focused == 2);
        assert(rg.next()); assert(rg.focused == 3);

        // wrap around
        assert(rg.next()); assert(rg.focused == 0);

        // prev wraps too
        assert(rg.prev()); assert(rg.focused == 3);
        assert(rg.prev()); assert(rg.focused == 2);

        // home / end
        rg.end();   assert(rg.focused == 3);
        rg.home();  assert(rg.focused == 0);

        std::printf("  ✓ next, prev, wrap, home, end\n");
    }

    std::printf("── radio: skip disabled ───────────────────\n");
    {
        radio_group<> rg;
        rg.emplace("A"); rg.emplace("B", false); rg.emplace("C"); rg.emplace("D", false);
        // A(0,en), B(1,dis), C(2,en), D(3,dis)

        rg.focused = 0;
        assert(rg.next()); assert(rg.focused == 2); // skipped B
        assert(rg.next()); assert(rg.focused == 0); // skipped D, wrapped to A

        rg.focused = 2;
        assert(rg.prev()); assert(rg.focused == 0); // skipped B

        // home goes to first enabled
        rg.home(); assert(rg.focused == 0);
        // end goes to last enabled
        rg.end();  assert(rg.focused == 2); // D is disabled, so C

        std::printf("  ✓ navigation skips disabled options\n");
    }

    std::printf("── radio: no wrap ─────────────────────────\n");
    {
        radio_group<> rg;
        rg.emplace("A"); rg.emplace("B"); rg.emplace("C");
        rg.wrap = false;

        rg.focused = 2;
        assert(!rg.next()); // can't go past end
        assert(rg.focused == 2);

        rg.focused = 0;
        assert(!rg.prev()); // can't go before start
        assert(rg.focused == 0);

        std::printf("  ✓ wrap=false stops at boundaries\n");
    }

    std::printf("── radio: confirm ─────────────────────────\n");
    {
        radio_group<> rg;
        rg.emplace("A"); rg.emplace("B"); rg.emplace("C");

        rg.focused = 2;
        assert(rg.confirm());
        assert(rg.selected == 2);

        // confirm on same → no change
        assert(!rg.confirm());

        std::printf("  ✓ confirm (focus→selected)\n");
    }

    std::printf("── radio: search + find ───────────────────\n");
    {
        radio_group<> rg;
        rg.emplace("Apple"); rg.emplace("Banana"); rg.emplace("Cherry");

        bool f = rg.search_next([](const std::string& d) { return d[0] == 'C'; });
        assert(f && rg.focused == 2);

        auto idx = rg.find_by([](const std::string& d) { return d == "Banana"; });
        assert(idx == 1);

        assert(rg.select_by([](const std::string& d) { return d == "Cherry"; }));
        assert(rg.selected == 2);

        std::printf("  ✓ search_next, find_by, select_by\n");
    }

    std::printf("── radio: enabled_count ───────────────────\n");
    {
        radio_group<> rg;
        rg.emplace("A"); rg.emplace("B", false); rg.emplace("C");
        assert(rg.enabled_count() == 2);

        enable_all(rg);
        assert(rg.enabled_count() == 3);

        disable_all(rg);
        assert(rg.enabled_count() == 0);

        std::printf("  ✓ enabled_count, enable_all, disable_all\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  TEXT INPUT TESTS
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── text: construction ─────────────────────\n");
    {
        text_input<> ti;
        assert(ti.empty());
        assert(ti.cursor == 0);
        assert(ti.at_start());
        assert(ti.at_end());

        text_input<> ti2("hello");
        assert(ti2.value == "hello");
        assert(ti2.cursor == 5);
        assert(!ti2.empty());
        assert(ti2.length() == 5);

        text_input<> ti3("", "Enter name...");
        assert(ti3.placeholder == "Enter name...");

        std::printf("  ✓ default, from string, with placeholder\n");
    }

    std::printf("── text: basic editing ────────────────────\n");
    {
        text_input<> ti;

        // insert
        assert(ti_insert_char(ti, 'h'));
        assert(ti_insert_char(ti, 'i'));
        assert(ti.value == "hi");
        assert(ti.cursor == 2);

        // insert string
        ti.cursor = 0;
        assert(ti_insert(ti, "say "));
        assert(ti.value == "say hi");
        assert(ti.cursor == 4);

        // backspace
        ti.cursor = 4; // after "say "
        assert(ti_backspace(ti));
        assert(ti.value == "sayhi");
        assert(ti.cursor == 3);

        // delete forward
        assert(ti_delete_forward(ti));
        assert(ti.value == "sayi");

        // delete at boundaries
        ti.cursor = 0;
        assert(!ti_backspace(ti));
        ti.cursor = ti.value.size();
        assert(!ti_delete_forward(ti));

        std::printf("  ✓ insert_char, insert, backspace, delete_forward\n");
    }

    std::printf("── text: cursor movement ──────────────────\n");
    {
        text_input<> ti("hello world");

        ti.cursor = 5;
        assert(ti_move_left(ti));  assert(ti.cursor == 4);
        assert(ti_move_right(ti)); assert(ti.cursor == 5);

        assert(ti_home(ti));  assert(ti.cursor == 0);
        assert(ti_end(ti));   assert(ti.cursor == 11);

        // boundaries
        ti.cursor = 0;
        assert(!ti_move_left(ti));
        ti.cursor = 11;
        assert(!ti_move_right(ti));

        std::printf("  ✓ move_left, move_right, home, end, boundaries\n");
    }

    std::printf("── text: word movement ────────────────────\n");
    {
        text_input<> ti("one two three");
        // cursor at end (13)

        assert(ti_move_word_left(ti));
        assert(ti.cursor == 8);  // start of "three"

        assert(ti_move_word_left(ti));
        assert(ti.cursor == 4);  // start of "two"

        assert(ti_move_word_left(ti));
        assert(ti.cursor == 0);  // start of "one"

        assert(ti_move_word_right(ti));
        assert(ti.cursor == 4);  // start of "two"

        assert(ti_move_word_right(ti));
        assert(ti.cursor == 8);  // start of "three"

        std::printf("  ✓ move_word_left, move_word_right\n");
    }

    std::printf("── text: selection ────────────────────────\n");
    {
        text_input<> ti("hello world");

        // shift+right to select
        ti.cursor = 0;
        ti_move_right(ti, true); // extend selection
        ti_move_right(ti, true);
        ti_move_right(ti, true);
        assert(ti.has_selection);
        assert(ti.sel_anchor == 0);
        assert(ti.cursor == 3);
        assert(ti.selected_text() == "hel");
        assert(ti.selection_length() == 3);

        // move left without extend → collapse to start
        ti_move_left(ti, false);
        assert(!ti.has_selection);
        assert(ti.cursor == 0); // collapsed to selection start

        // select all
        ti_select_all(ti);
        assert(ti.has_selection);
        assert(ti.sel_anchor == 0);
        assert(ti.cursor == 11);
        assert(ti.selected_text() == "hello world");

        // deselect
        ti_deselect(ti);
        assert(!ti.has_selection);

        std::printf("  ✓ extend selection, collapse, select_all, deselect\n");
    }

    std::printf("── text: selection editing ────────────────\n");
    {
        text_input<> ti("hello world");

        // select "hello"
        ti.cursor = 0;
        ti.sel_anchor = 0;
        ti.cursor = 5;
        ti.has_selection = true;
        assert(ti.selected_text() == "hello");

        // backspace deletes selection
        assert(ti_backspace(ti));
        assert(ti.value == " world");
        assert(!ti.has_selection);
        assert(ti.cursor == 0);

        // insert replaces selection
        ti_set_value(ti, "hello world");
        ti.sel_anchor = 6; ti.cursor = 11; ti.has_selection = true;
        assert(ti_insert(ti, "there"));
        assert(ti.value == "hello there");

        // cut
        ti_select_all(ti);
        auto clip = ti_cut(ti);
        assert(clip == "hello there");
        assert(ti.value.empty());

        std::printf("  ✓ backspace on selection, insert replaces, cut\n");
    }

    std::printf("── text: clipboard ────────────────────────\n");
    {
        text_input<> ti("hello world");

        // copy without selection → empty
        auto c = ti_copy(ti);
        assert(c.empty());

        // copy with selection
        ti.sel_anchor = 0; ti.cursor = 5; ti.has_selection = true;
        c = ti_copy(ti);
        assert(c == "hello");
        assert(ti.value == "hello world"); // unchanged

        // paste
        ti_deselect(ti);
        ti.cursor = 5;
        assert(ti_paste(ti, " dear"));
        assert(ti.value == "hello dear world");

        std::printf("  ✓ copy, paste\n");
    }

    std::printf("── text: word/line delete ─────────────────\n");
    {
        text_input<> ti("one two three");

        ti.cursor = 8; // start of "three"
        assert(ti_delete_word_back(ti));
        assert(ti.value == "one three");

        ti_set_value(ti, "one two three");
        ti.cursor = 4; // start of "two"
        assert(ti_delete_word_forward(ti));
        assert(ti.value == "one three");

        ti_set_value(ti, "hello world");
        ti.cursor = 5;
        assert(ti_delete_to_start(ti));
        assert(ti.value == " world");

        ti_set_value(ti, "hello world");
        ti.cursor = 5;
        assert(ti_delete_to_end(ti));
        assert(ti.value == "hello");

        std::printf("  ✓ delete_word_back, delete_word_forward, delete_to_start, delete_to_end\n");
    }

    std::printf("── text: max_length ───────────────────────\n");
    {
        text_input<> ti;
        ti.max_length = 5;

        assert(ti_insert(ti, "hel"));  // 3 ≤ 5
        assert(ti_insert(ti, "lo"));   // 5 ≤ 5
        assert(!ti_insert(ti, "!"));   // 6 > 5 → rejected
        assert(ti.value == "hello");

        std::printf("  ✓ max_length enforced\n");
    }

    std::printf("── text: read_only ────────────────────────\n");
    {
        text_input<> ti("locked");
        ti.read_only = true;

        assert(!ti_insert_char(ti, 'x'));
        assert(!ti_backspace(ti));
        assert(!ti_delete_forward(ti));
        assert(ti.value == "locked");

        // movement still works
        assert(ti_home(ti));
        assert(ti.cursor == 0);

        std::printf("  ✓ read_only blocks edits, allows movement\n");
    }

    std::printf("── text: history ──────────────────────────\n");
    {
        text_input<tif_history> ti;

        // push some commands
        ti_set_value(ti, "ls -la");
        ti_history_push(ti);
        assert(ti.value.empty());
        assert(ti.history.size() == 1);

        ti_set_value(ti, "cd /home");
        ti_history_push(ti);

        ti_set_value(ti, "pwd");
        ti_history_push(ti);
        assert(ti.history.size() == 3);

        // navigate up through history
        ti_set_value(ti, "partial"); // current input
        assert(ti_history_prev(ti));
        assert(ti.value == "pwd");

        assert(ti_history_prev(ti));
        assert(ti.value == "cd /home");

        assert(ti_history_prev(ti));
        assert(ti.value == "ls -la");

        // can't go further back
        assert(!ti_history_prev(ti));

        // navigate forward
        assert(ti_history_next(ti));
        assert(ti.value == "cd /home");

        assert(ti_history_next(ti));
        assert(ti.value == "pwd");

        // past end → restore saved input
        assert(ti_history_next(ti));
        assert(ti.value == "partial");

        // can't go further forward
        assert(!ti_history_next(ti));

        std::printf("  ✓ history_push, history_prev, history_next, saved_input restore\n");
    }

    std::printf("── text: validation ───────────────────────\n");
    {
        text_input<tif_validation> ti;

        ti_add_validator(ti, make_not_empty_validator());
        ti_add_validator(ti, make_max_length_validator(10));

        // empty → error
        auto r = ti_validate(ti);
        assert(r.result == validation_result::error);

        // valid
        ti_set_value(ti, "hello");
        r = ti_validate(ti);
        assert(r.result == validation_result::valid);

        // too long → error
        ti_set_value(ti, "this is way too long");
        r = ti_validate(ti);
        assert(r.result == validation_result::error);

        // custom pattern
        ti_clear_validators(ti);
        ti_add_validator(ti, make_pattern_validator(
            [](const std::string& v) { return v.find('@') != std::string::npos; },
            "Must contain @"));

        ti_set_value(ti, "hello");
        r = ti_validate(ti);
        assert(r.result == validation_result::error);

        ti_set_value(ti, "user@host");
        r = ti_validate(ti);
        assert(r.result == validation_result::valid);

        std::printf("  ✓ validators, not_empty, max_length, pattern, clear_validators\n");
    }

    std::printf("── text: masked ───────────────────────────\n");
    {
        text_input<tif_masked> ti;
        ti.masked = true;
        ti.mask_char = '*';

        ti_insert(ti, "secret");
        assert(ti.value == "secret"); // actual value stored
        // masking is a renderer concern — the struct just holds the flag
        assert(ti.masked);
        assert(ti.mask_char == '*');

        std::printf("  ✓ masked flag + mask_char (rendering is external)\n");
    }

    std::printf("── text: set/clear ────────────────────────\n");
    {
        text_input<> ti("hello world");
        ti.sel_anchor = 0; ti.cursor = 5; ti.has_selection = true;

        ti_clear(ti);
        assert(ti.value.empty());
        assert(ti.cursor == 0);
        assert(!ti.has_selection);

        ti_set_value(ti, "new value");
        assert(ti.value == "new value");
        assert(ti.cursor == 9); // at end

        std::printf("  ✓ clear, set_value\n");
    }

    // ═════════════════════════════════════════════════════════════════════
    //  CROSS-CHECKS: all types distinguishable
    // ═════════════════════════════════════════════════════════════════════

    std::printf("── cross-checks ───────────────────────────\n");
    {
        // tree types
        using tn = tree_node<std::string>;
        using tv = tree_view<std::string>;
        // list types
        using le = list_entry<std::string>;
        using lv = list_view<std::string>;

        // every composite identity is exclusive
        static_assert( tree_traits::is_tree_node_v<tn>);
        static_assert(!list_traits::is_list_entry_v<tn>);
        static_assert(!rt::is_radio_option_v<tn>);
        static_assert(!tit::is_text_input_v<tn>);

        static_assert(!tree_traits::is_tree_node_v<le>);
        static_assert( list_traits::is_list_entry_v<le>);

        static_assert( tree_traits::is_tree_view_v<tv>);
        static_assert(!list_traits::is_list_view_v<tv>);
        static_assert(!rt::is_radio_group_v<tv>);
        static_assert(!tit::is_text_input_v<tv>);

        static_assert(!tree_traits::is_tree_view_v<lv>);
        static_assert( list_traits::is_list_view_v<lv>);
        static_assert(!rt::is_radio_group_v<lv>);

        static_assert(!tree_traits::is_tree_view_v<brg>);
        static_assert(!list_traits::is_list_view_v<brg>);
        static_assert( rt::is_radio_group_v<brg>);
        static_assert(!tit::is_text_input_v<brg>);

        static_assert(!tree_traits::is_tree_view_v<bti>);
        static_assert(!list_traits::is_list_view_v<bti>);
        static_assert(!rt::is_radio_group_v<bti>);
        static_assert( tit::is_text_input_v<bti>);

        std::printf("  ✓ all types mutually exclusive in trait detection\n");
    }

    std::printf("\n── ALL TESTS PASSED ─────────────────────────\n");
    return 0;
}
