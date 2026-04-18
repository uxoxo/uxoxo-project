/*******************************************************************************
* uxoxo [platform]                                          imgui_ui_tree.hpp
*
*   ImGui UI layout tree schema and builder.  Populates a ui_tree with a
* node hierarchy describing the entire ImGui-rendered interface:
*
*   app_root (required)
*   ├── menu_bar (required)
*   │   ├── menu "File" (fixed)
*   │   │   ├── item "New Tree" (optional)
*   │   │   ├── separator (generated)
*   │   │   └── item "Quit" (required)
*   │   ├── menu "Edit" (fixed)
*   │   │   ├── item "Undo" (styleable)
*   │   │   ├── item "Redo" (styleable)
*   │   │   ├── separator (generated)
*   │   │   └── item "Clear History" (optional)
*   │   └── menu "View" (fixed)
*   │       └── item "Toggle Console" (styleable)
*   ├── workspace (required)
*   │   ├── panel "Tree Inspector" (movable)
*   │   └── panel "Canvas" (movable)
*   ├── console (optional)            [visible=false]
*   └── status_bar (required)
*       └── button "~" (required)     [action=toggle_console]
*
*   All UI state lives in property_value entries on these nodes.
* Visibility, position, size, dock mode, and enabled state are all
* tree properties — not separate structs.  The frame loop reads from
* the tree; user interactions write mutations back.
*
*   Structure:
*     1.  tag constants
*     2.  property key constants
*     3.  imgui_layout_ids (returned by builder)
*     4.  build_imgui_layout (builder function)
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/platform/imgui/imgui_ui_tree.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.15
*******************************************************************************/

#ifndef UXOXO_IMGUI_UI_TREE_
#define UXOXO_IMGUI_UI_TREE_ 1

// std
#include <cstdint>
#include <string>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../core/tree/ui_tree.hpp"


NS_UXOXO
NS_PLATFORM
NS_IMGUI


// -- type imports -----------------------------------------------------------
using uxoxo::ui_tree::ui_tree;
using uxoxo::ui_tree::ui_payload;
using uxoxo::ui_tree::mutation;
using uxoxo::ui_tree::DConstraintKind;
using uxoxo::ui_tree::DMutationResult;
using uxoxo::ui_tree::property_value;
using djinterp::node_id;
using djinterp::null_node;


// =============================================================================
//  1.  TAG CONSTANTS
// =============================================================================

namespace imgui_tags
{
    D_INLINE constexpr const char* app_root       = "app_root";
    D_INLINE constexpr const char* menu_bar       = "menu_bar";
    D_INLINE constexpr const char* menu           = "menu";
    D_INLINE constexpr const char* menu_item      = "menu_item";
    D_INLINE constexpr const char* separator      = "separator";
    D_INLINE constexpr const char* toolbar        = "toolbar";
    D_INLINE constexpr const char* toolbar_button = "toolbar_button";
    D_INLINE constexpr const char* workspace      = "workspace";
    D_INLINE constexpr const char* panel          = "panel";
    D_INLINE constexpr const char* console        = "console";
    D_INLINE constexpr const char* status_bar     = "status_bar";

}   // namespace imgui_tags


// =============================================================================
//  2.  PROPERTY KEY CONSTANTS
// =============================================================================

namespace imgui_props
{
    // -- shared layout properties ----------------------------------------
    D_INLINE constexpr const char* title      = "title";
    D_INLINE constexpr const char* label      = "label";
    D_INLINE constexpr const char* visible    = "visible";
    D_INLINE constexpr const char* enabled    = "enabled";
    D_INLINE constexpr const char* x          = "x";
    D_INLINE constexpr const char* y          = "y";
    D_INLINE constexpr const char* width      = "width";
    D_INLINE constexpr const char* height     = "height";

    // -- toolbar / button ------------------------------------------------
    D_INLINE constexpr const char* action     = "action";
    D_INLINE constexpr const char* toggled    = "toggled";
    D_INLINE constexpr const char* shortcut   = "shortcut";
    D_INLINE constexpr const char* tooltip    = "tooltip";

    // -- console ---------------------------------------------------------
    D_INLINE constexpr const char* dock       = "dock";
    D_INLINE constexpr const char* dock_ratio = "dock_ratio";

    // -- action values ---------------------------------------------------
    D_INLINE constexpr const char* act_toggle_console = "toggle_console";
    D_INLINE constexpr const char* act_undo           = "undo";
    D_INLINE constexpr const char* act_redo           = "redo";
    D_INLINE constexpr const char* act_clear_history  = "clear_history";
    D_INLINE constexpr const char* act_quit           = "quit";
    D_INLINE constexpr const char* act_new_tree       = "new_tree";

    // -- dock mode values ------------------------------------------------
    D_INLINE constexpr const char* dock_bottom   = "bottom";
    D_INLINE constexpr const char* dock_top      = "top";
    D_INLINE constexpr const char* dock_floating = "floating";

}   // namespace imgui_props


// =============================================================================
//  3.  IMGUI LAYOUT IDS
// =============================================================================

// imgui_layout_ids
//   struct: node identifiers returned by the ImGui layout builder.
// Gives the application direct access to every significant node
// without requiring tree queries by tag.
struct imgui_layout_ids
{
    node_id root               = null_node;

    // menu bar
    node_id menu_bar           = null_node;
    node_id menu_file          = null_node;
    node_id menu_edit          = null_node;
    node_id menu_view          = null_node;
    node_id item_new_tree      = null_node;
    node_id item_quit          = null_node;
    node_id item_undo          = null_node;
    node_id item_redo          = null_node;
    node_id item_clear_history = null_node;
    node_id item_toggle_console= null_node;

    // workspace
    node_id workspace          = null_node;
    node_id panel_inspector    = null_node;
    node_id panel_canvas       = null_node;

    // console
    node_id console            = null_node;

    // status bar
    node_id status_bar         = null_node;
    node_id btn_console_toggle = null_node;
};


// =============================================================================
//  4.  BUILD IMGUI LAYOUT
// =============================================================================

NS_INTERNAL

    // insert_child
    //   function: inserts a child node under a parent and returns its
    // node_id.  Sets tag, label, constraint, and can_receive_children
    // on the new node's payload.
    D_INLINE node_id
    insert_child(
        ui_tree&           _tree,
        node_id            _parent,
        const char*        _tag,
        const std::string& _label,
        DConstraintKind    _constraint,
        bool               _can_receive
    )
    {
        mutation m = mutation::make_insert(_tag, _parent);
        _tree.apply(m);

        // the new node is the last child of the parent
        node_id last = null_node;
        node_id cur  = _tree.first_child_of(_parent);

        while (cur != null_node)
        {
            last = cur;
            cur  = _tree.next_sibling_of(cur);
        }

        if (last != null_node)
        {
            ui_payload& pl = _tree.payload(last);
            pl.label                = _label;
            pl.node_constraint.kind = _constraint;
            pl.can_receive_children = _can_receive;
        }

        return last;
    }

    // insert_separator
    //   function: inserts a separator child node.
    D_INLINE node_id
    insert_separator(
        ui_tree& _tree,
        node_id  _parent
    )
    {
        return insert_child(_tree,
                            _parent,
                            imgui_tags::separator,
                            "",
                            DConstraintKind::generated,
                            false);
    }

NS_END  // internal


/*
build_imgui_layout
    Populates a ui_tree with the full ImGui UI layout hierarchy:
menu bar, toolbar, workspace (panels), console, and status bar.
Every node carries appropriate constraints governing what the
user / fairy can modify.

Parameter(s):
    _tree:             the tree to populate (must be empty).
    _inspector_width:  initial inspector panel width in pixels.
    _console_ratio:    initial console dock ratio (0.0–1.0).
Return:
    an imgui_layout_ids struct with node ids for every significant
    node in the layout.
*/
D_INLINE imgui_layout_ids
build_imgui_layout(
    ui_tree& _tree,
    double   _inspector_width = 280.0,
    double   _console_ratio   = 0.30
)
{
    imgui_layout_ids ids;

    // =====================================================================
    //  ROOT
    // =====================================================================
    ids.root = _tree.set_root(imgui_tags::app_root, "uxoxo");

    {
        ui_payload& pl = _tree.payload(ids.root);
        pl.node_constraint.kind = DConstraintKind::required;
        pl.can_receive_children = true;
    }

    // =====================================================================
    //  MENU BAR
    // =====================================================================
    ids.menu_bar = internal::insert_child(
        _tree, ids.root,
        imgui_tags::menu_bar, "Main Menu",
        DConstraintKind::required, true);

    // -- File menu -------------------------------------------------------
    ids.menu_file = internal::insert_child(
        _tree, ids.menu_bar,
        imgui_tags::menu, "File",
        DConstraintKind::fixed, true);

    ids.item_new_tree = internal::insert_child(
        _tree, ids.menu_file,
        imgui_tags::menu_item, "New Tree",
        DConstraintKind::optional, false);

    _tree.payload(ids.item_new_tree).properties[imgui_props::action] =
        std::string(imgui_props::act_new_tree);
    _tree.payload(ids.item_new_tree).properties[imgui_props::enabled] =
        true;

    internal::insert_separator(_tree, ids.menu_file);

    ids.item_quit = internal::insert_child(
        _tree, ids.menu_file,
        imgui_tags::menu_item, "Quit",
        DConstraintKind::required, false);

    _tree.payload(ids.item_quit).properties[imgui_props::action] =
        std::string(imgui_props::act_quit);
    _tree.payload(ids.item_quit).properties[imgui_props::shortcut] =
        std::string("Alt+F4");
    _tree.payload(ids.item_quit).properties[imgui_props::enabled] =
        true;

    // -- Edit menu -------------------------------------------------------
    ids.menu_edit = internal::insert_child(
        _tree, ids.menu_bar,
        imgui_tags::menu, "Edit",
        DConstraintKind::fixed, true);

    ids.item_undo = internal::insert_child(
        _tree, ids.menu_edit,
        imgui_tags::menu_item, "Undo",
        DConstraintKind::styleable, false);

    _tree.payload(ids.item_undo).properties[imgui_props::action] =
        std::string(imgui_props::act_undo);
    _tree.payload(ids.item_undo).properties[imgui_props::shortcut] =
        std::string("Ctrl+Z");
    _tree.payload(ids.item_undo).properties[imgui_props::enabled] =
        false;

    ids.item_redo = internal::insert_child(
        _tree, ids.menu_edit,
        imgui_tags::menu_item, "Redo",
        DConstraintKind::styleable, false);

    _tree.payload(ids.item_redo).properties[imgui_props::action] =
        std::string(imgui_props::act_redo);
    _tree.payload(ids.item_redo).properties[imgui_props::shortcut] =
        std::string("Ctrl+Y");
    _tree.payload(ids.item_redo).properties[imgui_props::enabled] =
        false;

    internal::insert_separator(_tree, ids.menu_edit);

    ids.item_clear_history = internal::insert_child(
        _tree, ids.menu_edit,
        imgui_tags::menu_item, "Clear History",
        DConstraintKind::optional, false);

    _tree.payload(ids.item_clear_history).properties[imgui_props::action] =
        std::string(imgui_props::act_clear_history);
    _tree.payload(ids.item_clear_history).properties[imgui_props::enabled] =
        true;

    // -- View menu -------------------------------------------------------
    ids.menu_view = internal::insert_child(
        _tree, ids.menu_bar,
        imgui_tags::menu, "View",
        DConstraintKind::fixed, true);

    ids.item_toggle_console = internal::insert_child(
        _tree, ids.menu_view,
        imgui_tags::menu_item, "Toggle Console",
        DConstraintKind::styleable, false);

    _tree.payload(ids.item_toggle_console).properties[imgui_props::action] =
        std::string(imgui_props::act_toggle_console);
    _tree.payload(ids.item_toggle_console).properties[imgui_props::shortcut] =
        std::string("~");
    _tree.payload(ids.item_toggle_console).properties[imgui_props::enabled] =
        true;

    // =====================================================================
    //  WORKSPACE
    // =====================================================================
    ids.workspace = internal::insert_child(
        _tree, ids.root,
        imgui_tags::workspace, "Workspace",
        DConstraintKind::required, true);

    // -- inspector panel -------------------------------------------------
    ids.panel_inspector = internal::insert_child(
        _tree, ids.workspace,
        imgui_tags::panel, "Tree Inspector",
        DConstraintKind::movable, true);

    {
        ui_payload& ip = _tree.payload(ids.panel_inspector);
        ip.properties[imgui_props::title]   =
            std::string("Tree Inspector");
        ip.properties[imgui_props::visible] = true;
        ip.properties[imgui_props::width]   = _inspector_width;
        ip.properties[imgui_props::dock]    =
            std::string("left");
    }

    // -- canvas panel ----------------------------------------------------
    ids.panel_canvas = internal::insert_child(
        _tree, ids.workspace,
        imgui_tags::panel, "Canvas",
        DConstraintKind::movable, true);

    {
        ui_payload& cp = _tree.payload(ids.panel_canvas);
        cp.properties[imgui_props::title]   =
            std::string("Canvas");
        cp.properties[imgui_props::visible] = true;
        cp.properties[imgui_props::dock]    =
            std::string("fill");
    }

    // =====================================================================
    //  CONSOLE
    // =====================================================================
    ids.console = internal::insert_child(
        _tree, ids.root,
        imgui_tags::console, "Console",
        DConstraintKind::optional, false);

    {
        ui_payload& dc = _tree.payload(ids.console);
        dc.properties[imgui_props::visible]    = false;
        dc.properties[imgui_props::dock]       =
            std::string(imgui_props::dock_bottom);
        dc.properties[imgui_props::dock_ratio] = _console_ratio;
    }

    // =====================================================================
    //  STATUS BAR
    // =====================================================================
    ids.status_bar = internal::insert_child(
        _tree, ids.root,
        imgui_tags::status_bar, "Status",
        DConstraintKind::required, true);

    _tree.payload(ids.status_bar).properties[imgui_props::height] =
        24.0;

    // ~ console toggle button (bottom-left)
    ids.btn_console_toggle = internal::insert_child(
        _tree, ids.status_bar,
        imgui_tags::toolbar_button, "~",
        DConstraintKind::required, false);

    {
        ui_payload& btn = _tree.payload(ids.btn_console_toggle);
        btn.properties[imgui_props::action]  =
            std::string(imgui_props::act_toggle_console);
        btn.properties[imgui_props::toggled] = false;
        btn.properties[imgui_props::tooltip] =
            std::string("Toggle Console  (~)");
    }

    // construction is not undoable
    _tree.clear_history();

    return ids;
}


NS_END  // imgui
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_UI_TREE_