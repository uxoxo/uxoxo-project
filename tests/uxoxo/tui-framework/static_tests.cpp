// ── static_tests.cpp ─────────────────────────────────────────────────────────
// Compile-time verification of the trait system. If this compiles, the SFINAE
// detection idioms are working correctly. No runtime code needed.
//
// Build:  g++ -std=c++17 -c static_tests.cpp

#include "ui/ui.h"
#include "tui/tui.h"

using namespace ui;
using namespace ui::traits;

// ── Component capability traits ──────────────────────────────────────────────

// Text: has content, no children, not focusable
static_assert( has_content_v<Text>,    "Text should have content");
static_assert(!has_children_v<Text>,   "Text should not have children");
static_assert(!is_focusable_v<Text>,   "Text should not be focusable");
static_assert(!has_items_v<Text>,      "Text should not have items");

// Button: has label, focusable, has activated signal
static_assert( has_label_v<Button>,      "Button should have label");
static_assert( is_focusable_v<Button>,   "Button should be focusable");
static_assert( has_activated_v<Button>,  "Button should have activated signal");
static_assert(!has_children_v<Button>,   "Button should not have children");
static_assert(!has_value_v<Button>,      "Button should not have value");

// TextInput: has value, cursor, focusable, has activated + value_changed
static_assert( has_value_v<TextInput>,          "TextInput should have value");
static_assert( has_cursor_v<TextInput>,         "TextInput should have cursor");
static_assert( is_focusable_v<TextInput>,       "TextInput should be focusable");
static_assert( has_activated_v<TextInput>,      "TextInput should have activated");
static_assert( has_value_changed_v<TextInput>,  "TextInput should have value_changed");
static_assert(!has_children_v<TextInput>,       "TextInput should not have children");

// Checkbox: focusable, has label
static_assert( is_focusable_v<Checkbox>,  "Checkbox should be focusable");
static_assert( has_label_v<Checkbox>,     "Checkbox should have label");

// ListComp: has items, selected, focusable
static_assert( has_items_v<ListComp>,     "ListComp should have items");
static_assert( has_selected_v<ListComp>,  "ListComp should have selected");
static_assert( is_focusable_v<ListComp>,  "ListComp should be focusable");
static_assert(!has_children_v<ListComp>,  "ListComp should not have children");

// ProgressBarComp: has value, not focusable
static_assert( has_value_v<ProgressBarComp>,  "ProgressBar should have value");
static_assert(!is_focusable_v<ProgressBarComp>, "ProgressBar should not be focusable");

// Container: has children, not focusable
static_assert( has_children_v<Container>,  "Container should have children");
static_assert(!is_focusable_v<Container>,  "Container should not be focusable");
static_assert(!has_value_v<Container>,     "Container should not have value");

// Region: has children
static_assert( has_children_v<Region>,     "Region should have children");
static_assert(!is_focusable_v<Region>,     "Region should not be focusable");

// Heading: has content, heading_level
static_assert( has_content_v<Heading>,       "Heading should have content");
static_assert( has_heading_level_v<Heading>, "Heading should have heading_level");
static_assert(!is_focusable_v<Heading>,      "Heading should not be focusable");

// ── Backend trait ────────────────────────────────────────────────────────────

#ifndef _WIN32
static_assert(is_backend_v<tui::AnsiBackend>, "AnsiBackend should satisfy Backend concept");
#else
static_assert(is_backend_v<tui::Win32Backend>, "Win32Backend should satisfy Backend concept");
#endif

// ── Verify variant holds all types ───────────────────────────────────────────

static_assert(std::variant_size_v<ComponentVar> == 10,
    "ComponentVar should hold exactly 10 component types");

// ── Cross-checks: traits are negative for wrong types ────────────────────────

static_assert(!has_content_v<Button>,      "Button shouldn't have content");
static_assert(!has_children_v<Button>,     "Button shouldn't have children");
static_assert(!has_items_v<Container>,     "Container shouldn't have items");
static_assert(!has_cursor_v<Button>,       "Button shouldn't have cursor");
static_assert(!has_heading_level_v<Text>,  "Text shouldn't have heading_level");
static_assert(!is_backend_v<int>,          "int is not a Backend");
static_assert(!is_backend_v<Text>,         "Text is not a Backend");

int main() {
    // All checks are at compile time. If we get here, everything passed.
    return 0;
}
