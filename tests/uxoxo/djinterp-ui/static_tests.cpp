/*******************************************************************************
* djinterp [test]                                              static_tests.cpp
*
*   Compile-time verification of the component trait system.  If this file
* compiles, every SFINAE detection idiom is working correctly.
*
*   Build:  g++ -std=c++17 -I. -c static_tests.cpp
*******************************************************************************/

#include <ui/ui.hpp>

using namespace djinterp::ui;
namespace ct = djinterp::ui::component_traits;


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  PRIMITIVE MEMBER DETECTION
// ═══════════════════════════════════════════════════════════════════════════════

// label: has content, not focusable
static_assert( ct::has_content_v<label>,            "label must have content");
static_assert(!ct::is_focusable_v<label>,           "label must not be focusable");
static_assert(!ct::has_children_v<label>,           "label must not have children");
static_assert(!ct::has_value_v<label>,              "label must not have value");

// heading: has content + heading_level
static_assert( ct::has_content_v<heading>,          "heading must have content");
static_assert( ct::has_heading_level_v<heading>,    "heading must have heading_level");
static_assert(!ct::is_focusable_v<heading>,         "heading must not be focusable");

// separator: nothing
static_assert(!ct::has_content_v<separator>,        "separator must not have content");
static_assert(!ct::is_focusable_v<separator>,       "separator must not be focusable");
static_assert(!ct::has_children_v<separator>,       "separator must not have children");
static_assert( ct::has_orientation_v<separator>,     "separator must have orientation");

// button: has label, focusable, has activated
static_assert( ct::has_label_v<button>,             "button must have label");
static_assert( ct::is_focusable_v<button>,          "button must be focusable");
static_assert( ct::has_activated_v<button>,         "button must have activated");
static_assert(!ct::has_value_v<button>,             "button must not have value");
static_assert(!ct::has_checked_v<button>,           "button must not have checked");

// textbox: has value + cursor, focusable
static_assert( ct::has_value_v<textbox>,            "textbox must have value");
static_assert( ct::has_cursor_v<textbox>,           "textbox must have cursor");
static_assert( ct::has_placeholder_v<textbox>,      "textbox must have placeholder");
static_assert( ct::is_focusable_v<textbox>,         "textbox must be focusable");
static_assert( ct::has_activated_v<textbox>,        "textbox must have activated");
static_assert( ct::has_value_changed_v<textbox>,    "textbox must have value_changed");
static_assert(!ct::has_prompt_v<textbox>,           "textbox must not have prompt");

// checkbox: has label + checked, focusable
static_assert( ct::has_label_v<checkbox>,           "checkbox must have label");
static_assert( ct::has_checked_v<checkbox>,         "checkbox must have checked");
static_assert( ct::is_focusable_v<checkbox>,        "checkbox must be focusable");
static_assert( ct::has_toggled_v<checkbox>,         "checkbox must have toggled");

// radio_group: has options + selected, focusable
static_assert( ct::has_options_v<radio_group>,      "radio_group must have options");
static_assert( ct::has_selected_v<radio_group>,     "radio_group must have selected");
static_assert( ct::is_focusable_v<radio_group>,     "radio_group must be focusable");
static_assert( ct::has_selection_changed_v<radio_group>, "radio_group must have selection_changed");

// list_view: has items + selected + columns, focusable, scrollable
static_assert( ct::has_items_v<list_view>,              "list_view must have items");
static_assert( ct::has_selected_v<list_view>,           "list_view must have selected");
static_assert( ct::has_columns_v<list_view>,            "list_view must have columns");
static_assert( ct::has_scroll_offset_v<list_view>,      "list_view must have scroll_offset");
static_assert( ct::is_focusable_v<list_view>,           "list_view must be focusable");
static_assert( ct::is_scrollable_v<list_view>,          "list_view must be scrollable");
static_assert( ct::has_activated_v<list_view>,          "list_view must have activated");
static_assert( ct::has_selection_changed_v<list_view>,  "list_view must have selection_changed");

// progress_bar: has value, not focusable
static_assert( ct::has_value_v<progress_bar>,       "progress_bar must have value");
static_assert(!ct::is_focusable_v<progress_bar>,    "progress_bar must not be focusable");
static_assert(!ct::has_cursor_v<progress_bar>,      "progress_bar must not have cursor");

// scrollbar: has value + orientation, not focusable
static_assert( ct::has_value_v<scrollbar>,          "scrollbar must have value");
static_assert( ct::has_orientation_v<scrollbar>,     "scrollbar must have orientation");
static_assert(!ct::is_focusable_v<scrollbar>,       "scrollbar must not be focusable");

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  CONTAINER TYPES
// ═══════════════════════════════════════════════════════════════════════════════

// container: has children, not focusable
static_assert( ct::has_children_v<container>,       "container must have children");
static_assert(!ct::is_focusable_v<container>,       "container must not be focusable");
static_assert(!ct::has_title_v<container>,          "container must not have title");

// panel: has children + title
static_assert( ct::has_children_v<panel>,           "panel must have children");
static_assert( ct::has_title_v<panel>,              "panel must have title");
static_assert(!ct::is_focusable_v<panel>,           "panel must not be focusable");
static_assert(!ct::has_modal_v<panel>,              "panel must not have modal");

// split_view: has children + orientation + ratio
static_assert( ct::has_children_v<split_view>,      "split_view must have children");
static_assert( ct::has_orientation_v<split_view>,    "split_view must have orientation");
static_assert( ct::has_ratio_v<split_view>,         "split_view must have ratio");

// dialog: has children + title + modal
static_assert( ct::has_children_v<dialog>,          "dialog must have children");
static_assert( ct::has_title_v<dialog>,             "dialog must have title");
static_assert( ct::has_modal_v<dialog>,             "dialog must have modal");
static_assert( ct::has_submitted_v<dialog>,         "dialog must have submitted");
static_assert( ct::has_cancelled_v<dialog>,         "dialog must have cancelled");

// tab_bar: has items + selected + children
static_assert( ct::has_items_v<tab_bar>,            "tab_bar must have items");
static_assert( ct::has_selected_v<tab_bar>,         "tab_bar must have selected");
static_assert( ct::has_children_v<tab_bar>,         "tab_bar must have children");

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  MC COMPOSITES
// ═══════════════════════════════════════════════════════════════════════════════

// menu_bar: has items + selected, focusable
static_assert( ct::has_items_v<menu_bar>,           "menu_bar must have items");
static_assert( ct::has_selected_v<menu_bar>,        "menu_bar must have selected");
static_assert( ct::is_focusable_v<menu_bar>,        "menu_bar must be focusable");
static_assert( ct::has_activated_v<menu_bar>,       "menu_bar must have activated");

// function_bar: has slots, not focusable
static_assert( ct::has_slots_member_v<function_bar>, "function_bar must have slots");
static_assert(!ct::is_focusable_v<function_bar>,     "function_bar must not be focusable");

// status_bar: has content, not focusable
static_assert( ct::has_content_v<status_bar>,       "status_bar must have content");
static_assert(!ct::is_focusable_v<status_bar>,      "status_bar must not be focusable");

// command_line: has value + cursor + prompt, focusable
static_assert( ct::has_value_v<command_line>,       "command_line must have value");
static_assert( ct::has_cursor_v<command_line>,      "command_line must have cursor");
static_assert( ct::has_prompt_v<command_line>,      "command_line must have prompt");
static_assert( ct::is_focusable_v<command_line>,    "command_line must be focusable");
static_assert( ct::has_submitted_v<command_line>,   "command_line must have submitted");

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  COMPOSITE IDENTITY TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

static_assert( ct::is_label_v<label>,                  "label IS a label");
static_assert(!ct::is_label_v<button>,                 "button is NOT a label");

static_assert( ct::is_heading_v<heading>,              "heading IS a heading");
static_assert(!ct::is_heading_v<label>,                "label is NOT a heading");

static_assert( ct::is_button_v<button>,                "button IS a button");
static_assert(!ct::is_button_v<checkbox>,              "checkbox is NOT a button");
static_assert(!ct::is_button_v<label>,                 "label is NOT a button");

static_assert( ct::is_textbox_v<textbox>,              "textbox IS a textbox");
static_assert(!ct::is_textbox_v<command_line>,         "command_line is NOT a textbox");

static_assert( ct::is_checkbox_v<checkbox>,            "checkbox IS a checkbox");
static_assert(!ct::is_checkbox_v<button>,              "button is NOT a checkbox");

static_assert( ct::is_radio_group_v<radio_group>,      "radio_group IS a radio_group");

static_assert( ct::is_list_view_v<list_view>,          "list_view IS a list_view");
static_assert(!ct::is_list_view_v<radio_group>,        "radio_group is NOT a list_view");

static_assert( ct::is_progress_bar_v<progress_bar>,    "progress_bar IS a progress_bar");
static_assert(!ct::is_progress_bar_v<textbox>,         "textbox is NOT a progress_bar");

static_assert( ct::is_container_v<container>,          "container IS a container");
static_assert(!ct::is_container_v<panel>,              "panel is NOT a (bare) container");

static_assert( ct::is_panel_v<panel>,                  "panel IS a panel");
static_assert(!ct::is_panel_v<dialog>,                 "dialog is NOT a panel");

static_assert( ct::is_split_view_v<split_view>,        "split_view IS a split_view");

static_assert( ct::is_dialog_v<dialog>,                "dialog IS a dialog");
static_assert(!ct::is_dialog_v<panel>,                 "panel is NOT a dialog");

static_assert( ct::is_command_line_v<command_line>,    "command_line IS a command_line");
static_assert(!ct::is_command_line_v<textbox>,         "textbox is NOT a command_line");

static_assert( ct::is_function_bar_v<function_bar>,    "function_bar IS a function_bar");
static_assert( ct::is_menu_bar_v<menu_bar>,            "menu_bar IS a menu_bar");

static_assert( ct::is_scrollbar_v<scrollbar>,          "scrollbar IS a scrollbar");
static_assert(!ct::is_scrollbar_v<progress_bar>,       "progress_bar is NOT a scrollbar");

static_assert( ct::is_tab_bar_v<tab_bar>,              "tab_bar IS a tab_bar");

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  NEGATIVE CROSS-CHECKS
// ═══════════════════════════════════════════════════════════════════════════════

// make sure unrelated types don't accidentally satisfy traits
static_assert(!ct::has_content_v<button>,              "button shouldn't have content");
static_assert(!ct::has_children_v<button>,             "button shouldn't have children");
static_assert(!ct::has_items_v<container>,             "container shouldn't have items");
static_assert(!ct::has_cursor_v<button>,               "button shouldn't have cursor");
static_assert(!ct::has_heading_level_v<label>,         "label shouldn't have heading_level");
static_assert(!ct::has_prompt_v<textbox>,              "textbox shouldn't have prompt");
static_assert(!ct::has_modal_v<panel>,                 "panel shouldn't have modal");
static_assert(!ct::has_ratio_v<container>,             "container shouldn't have ratio");
static_assert(!ct::has_checked_v<button>,              "button shouldn't have checked");
static_assert(!ct::has_options_v<list_view>,           "list_view shouldn't have options");

// ═══════════════════════════════════════════════════════════════════════════════
//  §6  VARIANT EXHAUSTIVENESS
// ═══════════════════════════════════════════════════════════════════════════════

static_assert(std::variant_size_v<component_var> == 19,
    "component_var should hold exactly 19 component types");


int main() 
{
    // all checks are at compile time.  
    // if we get here, every trait detection is correct.
    return 0;
}
