/*******************************************************************************
* uxoxo [component]                                          qt_dom_tree.hpp
*
*   Qt-specific DOM tree layer built on dom_tree_view.hpp.  This module
* provides the complete Qt Widgets element taxonomy, static per-type
* metadata, child validation rules, and Qt-specific classification
* helpers.
*
*   dom_tree_view.hpp handles the generic DOM machinery (element payload,
* tree_node hierarchy, tree_view navigation, properties, events, traits).
* This module adds:
*
*   - DQtElementType         — exhaustive Qt widget class enum
*   - DQtNodeKind            — coarse dispatch category
*   - DQtChildPolicy         — what children a parent accepts
*   - qt_element_info        — static per-type metadata record
*   - qt_child_rule          — per-parent child constraint
*   - qt_element_info_for()  — metadata lookup
*   - qt_child_rule_for()    — child rule lookup
*   - validate_qt_child()    — type + cardinality check
*   - Classification helpers — is_qt_layout, is_qt_button, etc.
*   - qt_dom_node / qt_dom_view — convenience aliases
*
*   No Qt headers are included.  This module describes the Qt widget
* taxonomy structurally for use by any renderer or tooling layer.
*
*
* TABLE OF CONTENTS
* =================
* I.    DQtNodeKind             — coarse category
* II.   DQtElementType          — Qt class tag enum
* III.  qt_element_info         — static metadata
* IV.   qt_element_info_for()   — metadata lookup
* V.    Classification Helpers  — range-check queries
* VI.   DQtChildPolicy          — child constraint enum
* VII.  qt_child_rule           — per-parent rule
* VIII. validate_qt_child()     — insertion validation
* IX.   qt_dom_node / qt_dom_view — convenience aliases
*
*
* path:      /inc/uxoxo/component/qt_dom_tree.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.09
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_QT_DOM_TREE_
#define  UXOXO_COMPONENT_QT_DOM_TREE_ 1

#include <cstddef>
#include <cstdint>
#include <string>

#include "../uxoxo.hpp"
#include "./dom_tree_view.hpp"


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  DQtNodeKind
// ═══════════════════════════════════════════════════════════════════════════════

// DQtNodeKind
//   enum: coarse category for fast dispatch.  Every element type
// maps to exactly one node kind.
enum class DQtNodeKind : std::uint8_t
{
    meta        = 0,    // document, fragment, text — DOM-only
    window      = 1,    // top-level windows and subwindows
    widget      = 2,    // visual QWidget-derived objects
    layout      = 3,    // QLayout-derived geometry managers
    command     = 4,    // QAction, command surfaces
    helper      = 5,    // non-visual helpers (QButtonGroup, etc.)
    workspace   = 6,    // MDI area, dock widgets
    dialog      = 7     // standard utility dialogs
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  DQtElementType
// ═══════════════════════════════════════════════════════════════════════════════

// DQtElementType
//   enum: identifies the specific Qt class a DOM node represents.
// Values are range-bucketed by category for fast classification.
//
//   0x0000  meta / DOM-only         0x0800  display
//   0x0100  windows                 0x0900  generic containers
//   0x0200  layouts                 0x0A00  page containers
//   0x0300  buttons                 0x0B00  item views
//   0x0400  text input              0x0C00  command surfaces
//   0x0500  numeric / range input   0x0D00  command objects
//   0x0600  date/time input         0x0E00  helpers
//   0x0700  selection / choice      0x0F00  workspace
//                                   0x1000  standard dialogs
//                                   0x1100  utility widgets
//                                   0xFF00  user-defined
enum class DQtElementType : std::uint16_t
{
    // -- 0x00xx  meta / DOM-only ------------------------------------------
    document                = 0x0000,
    fragment                = 0x0001,
    text                    = 0x0002,

    // -- 0x01xx  windows --------------------------------------------------
    main_window             = 0x0100,   // QMainWindow
    dialog_window           = 0x0101,   // QDialog
    mdi_subwindow           = 0x0102,   // QMdiSubWindow

    // -- 0x02xx  layouts --------------------------------------------------
    layout_hbox             = 0x0200,   // QHBoxLayout
    layout_vbox             = 0x0201,   // QVBoxLayout
    layout_grid             = 0x0202,   // QGridLayout
    layout_form             = 0x0203,   // QFormLayout
    layout_stacked          = 0x0204,   // QStackedLayout
    layout_box              = 0x0205,   // QBoxLayout (abstract)
    layout_spacer           = 0x0206,   // QSpacerItem

    // -- 0x03xx  buttons --------------------------------------------------
    abstract_button         = 0x0300,   // QAbstractButton (abstract)
    push_button             = 0x0301,   // QPushButton
    tool_button             = 0x0302,   // QToolButton
    command_link_button     = 0x0303,   // QCommandLinkButton
    check_box               = 0x0304,   // QCheckBox
    radio_button            = 0x0305,   // QRadioButton

    // -- 0x04xx  text input -----------------------------------------------
    line_edit               = 0x0400,   // QLineEdit
    text_edit               = 0x0401,   // QTextEdit
    plain_text_edit         = 0x0402,   // QPlainTextEdit
    key_sequence_edit       = 0x0403,   // QKeySequenceEdit

    // -- 0x05xx  numeric / range ------------------------------------------
    abstract_spinbox        = 0x0500,   // QAbstractSpinBox (abstract)
    spin_box                = 0x0501,   // QSpinBox
    double_spin_box         = 0x0502,   // QDoubleSpinBox
    abstract_slider         = 0x0503,   // QAbstractSlider (abstract)
    slider                  = 0x0504,   // QSlider
    dial                    = 0x0505,   // QDial
    scroll_bar              = 0x0506,   // QScrollBar

    // -- 0x06xx  date/time ------------------------------------------------
    date_time_edit          = 0x0600,   // QDateTimeEdit
    date_edit               = 0x0601,   // QDateEdit
    time_edit               = 0x0602,   // QTimeEdit
    calendar_widget         = 0x0603,   // QCalendarWidget

    // -- 0x07xx  selection / choice ---------------------------------------
    combo_box               = 0x0700,   // QComboBox
    font_combo_box          = 0x0701,   // QFontComboBox

    // -- 0x08xx  display --------------------------------------------------
    label                   = 0x0800,   // QLabel
    lcd_number              = 0x0801,   // QLCDNumber
    progress_bar            = 0x0802,   // QProgressBar

    // -- 0x09xx  generic containers ---------------------------------------
    widget_base             = 0x0900,   // QWidget
    frame                   = 0x0901,   // QFrame
    group_box               = 0x0902,   // QGroupBox
    abstract_scroll_area    = 0x0903,   // QAbstractScrollArea (abstract)
    scroll_area             = 0x0904,   // QScrollArea
    splitter                = 0x0905,   // QSplitter

    // -- 0x0Axx  page containers ------------------------------------------
    stacked_widget          = 0x0A00,   // QStackedWidget
    tab_widget              = 0x0A01,   // QTabWidget
    toolbox                 = 0x0A02,   // QToolBox
    wizard                  = 0x0A03,   // QWizard
    wizard_page             = 0x0A04,   // QWizardPage

    // -- 0x0Bxx  item views -----------------------------------------------
    abstract_item_view      = 0x0B00,   // QAbstractItemView (abstract)
    list_view               = 0x0B01,   // QListView
    tree_view               = 0x0B02,   // QTreeView
    table_view              = 0x0B03,   // QTableView
    column_view             = 0x0B04,   // QColumnView
    header_view             = 0x0B05,   // QHeaderView
    list_widget             = 0x0B06,   // QListWidget
    tree_widget             = 0x0B07,   // QTreeWidget
    table_widget            = 0x0B08,   // QTableWidget

    // -- 0x0Cxx  command surfaces -----------------------------------------
    menu_bar                = 0x0C00,   // QMenuBar
    menu                    = 0x0C01,   // QMenu
    tool_bar                = 0x0C02,   // QToolBar
    status_bar              = 0x0C03,   // QStatusBar
    tab_bar                 = 0x0C04,   // QTabBar

    // -- 0x0Dxx  command objects ------------------------------------------
    action                  = 0x0D00,   // QAction
    action_group            = 0x0D01,   // QActionGroup

    // -- 0x0Exx  helpers --------------------------------------------------
    button_group            = 0x0E00,   // QButtonGroup
    data_widget_mapper      = 0x0E01,   // QDataWidgetMapper
    completer               = 0x0E02,   // QCompleter
    dialog_button_box       = 0x0E03,   // QDialogButtonBox

    // -- 0x0Fxx  workspace ------------------------------------------------
    dock_widget             = 0x0F00,   // QDockWidget
    mdi_area                = 0x0F01,   // QMdiArea

    // -- 0x10xx  standard dialogs -----------------------------------------
    color_dialog            = 0x1000,   // QColorDialog
    file_dialog             = 0x1001,   // QFileDialog
    font_dialog             = 0x1002,   // QFontDialog
    input_dialog            = 0x1003,   // QInputDialog
    message_box             = 0x1004,   // QMessageBox
    error_message           = 0x1005,   // QErrorMessage
    progress_dialog         = 0x1006,   // QProgressDialog

    // -- 0x11xx  utility --------------------------------------------------
    size_grip               = 0x1100,   // QSizeGrip
    focus_frame             = 0x1101,   // QFocusFrame

    // -- 0xFFxx  user-defined ---------------------------------------------
    user_defined            = 0xFF00
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  qt_element_info
// ═══════════════════════════════════════════════════════════════════════════════

// qt_element_info
//   struct: static per-type metadata.
struct qt_element_info
{
    DQtElementType  type;
    DQtNodeKind     kind;
    const char*     qt_class;       // e.g. "QPushButton"
    const char*     node_path;      // dotted path, e.g. "widget.button.push"
    bool            visual;
    bool            placeable;      // can be placed in a designer surface
    bool            abstract;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  qt_element_info_for()
// ═══════════════════════════════════════════════════════════════════════════════

namespace internal {

    #define QT_INFO(ev, nk, cls, path, vis, plc, abs) \
        { DQtElementType::ev, DQtNodeKind::nk,        \
          cls, path, vis, plc, abs }

    inline constexpr qt_element_info
    qt_element_info_table[] =
    {
        // meta
        QT_INFO(document,             meta,      "",                    "meta.document",                        false, false, false),
        QT_INFO(fragment,             meta,      "",                    "meta.fragment",                        false, false, false),
        QT_INFO(text,                 meta,      "",                    "meta.text",                            false, false, false),

        // windows
        QT_INFO(main_window,          window,    "QMainWindow",         "window.main",                          true,  true,  false),
        QT_INFO(dialog_window,        window,    "QDialog",             "window.dialog",                        true,  true,  false),
        QT_INFO(mdi_subwindow,        window,    "QMdiSubWindow",       "window.subwindow",                     true,  true,  false),

        // layouts
        QT_INFO(layout_hbox,          layout,    "QHBoxLayout",         "layout.hbox",                          false, true,  false),
        QT_INFO(layout_vbox,          layout,    "QVBoxLayout",         "layout.vbox",                          false, true,  false),
        QT_INFO(layout_grid,          layout,    "QGridLayout",         "layout.grid",                          false, true,  false),
        QT_INFO(layout_form,          layout,    "QFormLayout",         "layout.form",                          false, true,  false),
        QT_INFO(layout_stacked,       layout,    "QStackedLayout",      "layout.stacked",                       false, true,  false),
        QT_INFO(layout_box,           layout,    "QBoxLayout",          "layout.box",                           false, false, true ),
        QT_INFO(layout_spacer,        layout,    "QSpacerItem",         "layout.spacer",                        false, true,  false),

        // buttons
        QT_INFO(abstract_button,      widget,    "QAbstractButton",     "widget.abstract.button",               true,  false, true ),
        QT_INFO(push_button,          widget,    "QPushButton",         "widget.button.push",                   true,  true,  false),
        QT_INFO(tool_button,          widget,    "QToolButton",         "widget.button.tool",                   true,  true,  false),
        QT_INFO(command_link_button,  widget,    "QCommandLinkButton",  "widget.button.commandlink",            true,  true,  false),
        QT_INFO(check_box,            widget,    "QCheckBox",           "widget.input.choice.checkbox",         true,  true,  false),
        QT_INFO(radio_button,         widget,    "QRadioButton",        "widget.input.choice.radiobutton",      true,  true,  false),

        // text input
        QT_INFO(line_edit,            widget,    "QLineEdit",           "widget.input.text.line",               true,  true,  false),
        QT_INFO(text_edit,            widget,    "QTextEdit",           "widget.input.text.rich",               true,  true,  false),
        QT_INFO(plain_text_edit,      widget,    "QPlainTextEdit",      "widget.input.text.plain",              true,  true,  false),
        QT_INFO(key_sequence_edit,    widget,    "QKeySequenceEdit",    "widget.input.text.keysequence",        true,  true,  false),

        // numeric / range
        QT_INFO(abstract_spinbox,     widget,    "QAbstractSpinBox",    "widget.abstract.spinbox",              true,  false, true ),
        QT_INFO(spin_box,             widget,    "QSpinBox",            "widget.input.numeric.spinbox",         true,  true,  false),
        QT_INFO(double_spin_box,      widget,    "QDoubleSpinBox",      "widget.input.numeric.doublespinbox",   true,  true,  false),
        QT_INFO(abstract_slider,      widget,    "QAbstractSlider",     "widget.abstract.slider",               true,  false, true ),
        QT_INFO(slider,               widget,    "QSlider",             "widget.input.range.slider",            true,  true,  false),
        QT_INFO(dial,                 widget,    "QDial",               "widget.input.range.dial",              true,  true,  false),
        QT_INFO(scroll_bar,           widget,    "QScrollBar",          "widget.input.range.scrollbar",         true,  true,  false),

        // date/time
        QT_INFO(date_time_edit,       widget,    "QDateTimeEdit",       "widget.input.datetime.datetimeedit",   true,  true,  false),
        QT_INFO(date_edit,            widget,    "QDateEdit",           "widget.input.datetime.dateedit",       true,  true,  false),
        QT_INFO(time_edit,            widget,    "QTimeEdit",           "widget.input.datetime.timeedit",       true,  true,  false),
        QT_INFO(calendar_widget,      widget,    "QCalendarWidget",     "widget.input.datetime.calendar",       true,  true,  false),

        // selection
        QT_INFO(combo_box,            widget,    "QComboBox",           "widget.input.choice.combobox",         true,  true,  false),
        QT_INFO(font_combo_box,       widget,    "QFontComboBox",       "widget.input.choice.fontcombobox",     true,  true,  false),

        // display
        QT_INFO(label,                widget,    "QLabel",              "widget.display.label",                 true,  true,  false),
        QT_INFO(lcd_number,           widget,    "QLCDNumber",          "widget.display.numeric",               true,  true,  false),
        QT_INFO(progress_bar,         widget,    "QProgressBar",        "widget.display.progress",              true,  true,  false),

        // generic containers
        QT_INFO(widget_base,          widget,    "QWidget",             "widget.base",                          true,  true,  false),
        QT_INFO(frame,                widget,    "QFrame",              "widget.container.frame",               true,  true,  false),
        QT_INFO(group_box,            widget,    "QGroupBox",           "widget.container.groupbox",            true,  true,  false),
        QT_INFO(abstract_scroll_area, widget,    "QAbstractScrollArea", "widget.abstract.scrollarea",           true,  false, true ),
        QT_INFO(scroll_area,          widget,    "QScrollArea",         "widget.container.scrollarea",          true,  true,  false),
        QT_INFO(splitter,             widget,    "QSplitter",           "widget.container.splitter",            true,  true,  false),

        // page containers
        QT_INFO(stacked_widget,       widget,    "QStackedWidget",      "widget.pages.stackedwidget",           true,  true,  false),
        QT_INFO(tab_widget,           widget,    "QTabWidget",          "widget.pages.tabwidget",               true,  true,  false),
        QT_INFO(toolbox,              widget,    "QToolBox",            "widget.pages.toolbox",                 true,  true,  false),
        QT_INFO(wizard,               dialog,    "QWizard",             "widget.pages.wizard",                  true,  true,  false),
        QT_INFO(wizard_page,          widget,    "QWizardPage",         "widget.pages.wizardpage",              true,  true,  false),

        // item views
        QT_INFO(abstract_item_view,   widget,    "QAbstractItemView",   "widget.abstract.itemview",             true,  false, true ),
        QT_INFO(list_view,            widget,    "QListView",           "widget.view.modelbased.listview",      true,  true,  false),
        QT_INFO(tree_view,            widget,    "QTreeView",           "widget.view.modelbased.treeview",      true,  true,  false),
        QT_INFO(table_view,           widget,    "QTableView",          "widget.view.modelbased.tableview",     true,  true,  false),
        QT_INFO(column_view,          widget,    "QColumnView",         "widget.view.modelbased.columnview",    true,  true,  false),
        QT_INFO(header_view,          widget,    "QHeaderView",         "widget.view.header",                   true,  false, false),
        QT_INFO(list_widget,          widget,    "QListWidget",         "widget.view.itembased.listwidget",     true,  true,  false),
        QT_INFO(tree_widget,          widget,    "QTreeWidget",         "widget.view.itembased.treewidget",     true,  true,  false),
        QT_INFO(table_widget,         widget,    "QTableWidget",        "widget.view.itembased.tablewidget",    true,  true,  false),

        // command surfaces
        QT_INFO(menu_bar,             command,   "QMenuBar",            "command.surface.menubar",              true,  false, false),
        QT_INFO(menu,                 command,   "QMenu",               "command.surface.menu",                 true,  false, false),
        QT_INFO(tool_bar,             command,   "QToolBar",            "command.surface.toolbar",              true,  false, false),
        QT_INFO(status_bar,           command,   "QStatusBar",          "command.surface.statusbar",            true,  false, false),
        QT_INFO(tab_bar,              widget,    "QTabBar",             "widget.navigation.tabbar",             true,  true,  false),

        // command objects
        QT_INFO(action,               command,   "QAction",             "command.action",                       false, false, false),
        QT_INFO(action_group,         command,   "QActionGroup",        "command.action.group",                 false, false, false),

        // helpers
        QT_INFO(button_group,         helper,    "QButtonGroup",        "helper.group.buttons",                 false, false, false),
        QT_INFO(data_widget_mapper,   helper,    "QDataWidgetMapper",   "helper.binding.mapper",                false, false, false),
        QT_INFO(completer,            helper,    "QCompleter",          "helper.input.completer",               false, false, false),
        QT_INFO(dialog_button_box,    widget,    "QDialogButtonBox",    "widget.dialog.buttonbox",              true,  true,  false),

        // workspace
        QT_INFO(dock_widget,          workspace, "QDockWidget",         "workspace.dockwidget",                 true,  false, false),
        QT_INFO(mdi_area,             workspace, "QMdiArea",            "workspace.mdiarea",                    true,  true,  false),

        // standard dialogs
        QT_INFO(color_dialog,         dialog,    "QColorDialog",        "dialog.standard.color",                true,  false, false),
        QT_INFO(file_dialog,          dialog,    "QFileDialog",         "dialog.standard.file",                 true,  false, false),
        QT_INFO(font_dialog,          dialog,    "QFontDialog",         "dialog.standard.font",                 true,  false, false),
        QT_INFO(input_dialog,         dialog,    "QInputDialog",        "dialog.standard.input",                true,  false, false),
        QT_INFO(message_box,          dialog,    "QMessageBox",         "dialog.standard.message",              true,  false, false),
        QT_INFO(error_message,        dialog,    "QErrorMessage",       "dialog.standard.errormessage",         true,  false, false),
        QT_INFO(progress_dialog,      dialog,    "QProgressDialog",     "dialog.standard.progress",             true,  false, false),

        // utility
        QT_INFO(size_grip,            widget,    "QSizeGrip",           "widget.utility.sizegrip",              true,  true,  false),
        QT_INFO(focus_frame,          widget,    "QFocusFrame",         "widget.utility.focusframe",            true,  false, false),
    };

    inline constexpr std::size_t qt_element_info_table_size =
        sizeof(qt_element_info_table) / sizeof(qt_element_info_table[0]);

    #undef QT_INFO

}   // namespace internal


// qt_element_info_for
//   function: returns the metadata entry for a given type, or nullptr.
inline const qt_element_info*
qt_element_info_for(DQtElementType _type) noexcept
{
    for (std::size_t i = 0;
         i < internal::qt_element_info_table_size;
         ++i)
    {
        if (internal::qt_element_info_table[i].type == _type)
        {
            return &internal::qt_element_info_table[i];
        }
    }

    return nullptr;
}

// qt_node_kind_of
//   function: returns the coarse kind for a type.
inline DQtNodeKind
qt_node_kind_of(DQtElementType _type) noexcept
{
    const auto* info = qt_element_info_for(_type);

    return info ? info->kind : DQtNodeKind::meta;
}

// qt_class_name_of
//   function: returns the Qt class name string, or "".
inline const char*
qt_class_name_of(DQtElementType _type) noexcept
{
    const auto* info = qt_element_info_for(_type);

    return info ? info->qt_class : "";
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  CLASSIFICATION HELPERS
// ═══════════════════════════════════════════════════════════════════════════════
//   Range-check queries on element type values.  Branchless at -O1.

inline bool
is_qt_meta(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return (v <= 0x00FF);
}

inline bool
is_qt_window(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0100) && (v <= 0x01FF) );
}

inline bool
is_qt_layout(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0200) && (v <= 0x02FF) );
}

inline bool
is_qt_button(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0300) && (v <= 0x03FF) );
}

inline bool
is_qt_input(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0400) && (v <= 0x07FF) );
}

inline bool
is_qt_display(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0800) && (v <= 0x08FF) );
}

inline bool
is_qt_container(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0900) && (v <= 0x09FF) );
}

inline bool
is_qt_page_container(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0A00) && (v <= 0x0AFF) );
}

inline bool
is_qt_item_view(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0B00) && (v <= 0x0BFF) );
}

inline bool
is_qt_command(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( (v >= 0x0C00) && (v <= 0x0DFF) );
}

inline bool
is_qt_dialog(DQtElementType _t) noexcept
{
    auto v = static_cast<std::uint16_t>(_t);

    return ( _t == DQtElementType::dialog_window ||
             _t == DQtElementType::wizard        ||
             ((v >= 0x1000) && (v <= 0x10FF)) );
}

inline bool
is_qt_visual(DQtElementType _t) noexcept
{
    const auto* info = qt_element_info_for(_t);

    return (info && info->visual);
}

inline bool
is_qt_abstract(DQtElementType _t) noexcept
{
    const auto* info = qt_element_info_for(_t);

    return (info && info->abstract);
}

inline bool
is_qt_placeable(DQtElementType _t) noexcept
{
    const auto* info = qt_element_info_for(_t);

    return (info && info->placeable);
}

// accepts_qt_children
//   Returns true if the element type can logically contain children.
inline bool
accepts_qt_children(DQtElementType _t) noexcept
{
    return ( is_qt_meta(_t)           ||
             is_qt_window(_t)         ||
             is_qt_layout(_t)         ||
             is_qt_container(_t)      ||
             is_qt_page_container(_t) ||
             is_qt_command(_t)        ||
             _t == DQtElementType::mdi_area    ||
             _t == DQtElementType::dock_widget );
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §6  DQtChildPolicy
// ═══════════════════════════════════════════════════════════════════════════════

// DQtChildPolicy
//   enum: what children a parent element accepts.
enum class DQtChildPolicy : std::uint8_t
{
    none            = 0,    // no children (leaf)
    any_visual      = 1,    // any visual widget
    any_widget      = 2,    // widgets only (no layouts, actions)
    widgets_layouts = 3,    // widgets, layouts, spacers
    single_widget   = 4,    // exactly one child widget
    pages_only      = 5,    // page widgets only
    custom          = 6     // checked via allowed_types list
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §7  qt_child_rule
// ═══════════════════════════════════════════════════════════════════════════════

// qt_child_rule
//   struct: per-parent child constraint.
struct qt_child_rule
{
    DQtElementType          parent;
    DQtChildPolicy          policy;
    int                     max_children;       // -1 = unlimited
    const DQtElementType*   allowed_types;
    std::size_t             allowed_count;
};


namespace internal {

    // explicit allowed-type lists
    inline constexpr DQtElementType menu_bar_children[] =
        { DQtElementType::menu, DQtElementType::action };

    inline constexpr DQtElementType menu_children[] =
        { DQtElementType::action, DQtElementType::action_group,
          DQtElementType::menu };

    inline constexpr DQtElementType tool_bar_children[] =
        { DQtElementType::action, DQtElementType::push_button,
          DQtElementType::tool_button, DQtElementType::combo_box,
          DQtElementType::line_edit, DQtElementType::label,
          DQtElementType::widget_base };

    inline constexpr DQtElementType action_group_children[] =
        { DQtElementType::action };

    inline constexpr DQtElementType button_group_children[] =
        { DQtElementType::push_button, DQtElementType::tool_button,
          DQtElementType::check_box, DQtElementType::radio_button,
          DQtElementType::command_link_button };

    inline constexpr DQtElementType wizard_children[] =
        { DQtElementType::wizard_page };

    inline constexpr DQtElementType mdi_area_children[] =
        { DQtElementType::mdi_subwindow };

    #define QT_RULE(pt, pol, mx)                  \
        { DQtElementType::pt, DQtChildPolicy::pol, mx, nullptr, 0 }
    #define QT_RULE_LIST(pt, pol, mx, arr)         \
        { DQtElementType::pt, DQtChildPolicy::pol, mx, arr, \
          sizeof(arr) / sizeof(arr[0]) }

    inline constexpr qt_child_rule
    qt_child_rule_table[] =
    {
        // meta
        QT_RULE(document,             any_visual,       -1),
        QT_RULE(fragment,             any_visual,       -1),
        QT_RULE(text,                 none,              0),

        // windows
        QT_RULE(main_window,          widgets_layouts,  -1),
        QT_RULE(dialog_window,        widgets_layouts,  -1),
        QT_RULE(mdi_subwindow,        single_widget,     1),

        // layouts
        QT_RULE(layout_hbox,          widgets_layouts,  -1),
        QT_RULE(layout_vbox,          widgets_layouts,  -1),
        QT_RULE(layout_grid,          widgets_layouts,  -1),
        QT_RULE(layout_form,          widgets_layouts,  -1),
        QT_RULE(layout_stacked,       widgets_layouts,  -1),
        QT_RULE(layout_box,           widgets_layouts,  -1),
        QT_RULE(layout_spacer,        none,              0),

        // buttons — leaves
        QT_RULE(abstract_button,      none,              0),
        QT_RULE(push_button,          none,              0),
        QT_RULE(tool_button,          none,              0),
        QT_RULE(command_link_button,  none,              0),
        QT_RULE(check_box,            none,              0),
        QT_RULE(radio_button,         none,              0),

        // text input — leaves
        QT_RULE(line_edit,            none,              0),
        QT_RULE(text_edit,            none,              0),
        QT_RULE(plain_text_edit,      none,              0),
        QT_RULE(key_sequence_edit,    none,              0),

        // numeric / range — leaves
        QT_RULE(abstract_spinbox,     none,              0),
        QT_RULE(spin_box,             none,              0),
        QT_RULE(double_spin_box,      none,              0),
        QT_RULE(abstract_slider,      none,              0),
        QT_RULE(slider,               none,              0),
        QT_RULE(dial,                 none,              0),
        QT_RULE(scroll_bar,           none,              0),

        // date/time — leaves
        QT_RULE(date_time_edit,       none,              0),
        QT_RULE(date_edit,            none,              0),
        QT_RULE(time_edit,            none,              0),
        QT_RULE(calendar_widget,      none,              0),

        // selection — leaves
        QT_RULE(combo_box,            none,              0),
        QT_RULE(font_combo_box,       none,              0),

        // display — leaves
        QT_RULE(label,                none,              0),
        QT_RULE(lcd_number,           none,              0),
        QT_RULE(progress_bar,         none,              0),

        // generic containers
        QT_RULE(widget_base,          widgets_layouts,  -1),
        QT_RULE(frame,                widgets_layouts,  -1),
        QT_RULE(group_box,            widgets_layouts,  -1),
        QT_RULE(abstract_scroll_area, widgets_layouts,  -1),
        QT_RULE(scroll_area,          single_widget,     1),
        QT_RULE(splitter,             any_widget,       -1),

        // page containers
        QT_RULE(stacked_widget,       pages_only,       -1),
        QT_RULE(tab_widget,           pages_only,       -1),
        QT_RULE(toolbox,              pages_only,       -1),
        QT_RULE_LIST(wizard,          custom,           -1, wizard_children),
        QT_RULE(wizard_page,          widgets_layouts,  -1),

        // item views — leaves
        QT_RULE(abstract_item_view,   none,              0),
        QT_RULE(list_view,            none,              0),
        QT_RULE(tree_view,            none,              0),
        QT_RULE(table_view,           none,              0),
        QT_RULE(column_view,          none,              0),
        QT_RULE(header_view,          none,              0),
        QT_RULE(list_widget,          none,              0),
        QT_RULE(tree_widget,          none,              0),
        QT_RULE(table_widget,         none,              0),

        // command surfaces
        QT_RULE_LIST(menu_bar,        custom,           -1, menu_bar_children),
        QT_RULE_LIST(menu,            custom,           -1, menu_children),
        QT_RULE_LIST(tool_bar,        custom,           -1, tool_bar_children),
        QT_RULE(status_bar,           any_widget,       -1),
        QT_RULE(tab_bar,              none,              0),

        // command objects
        QT_RULE(action,               none,              0),
        QT_RULE_LIST(action_group,    custom,           -1, action_group_children),

        // helpers
        QT_RULE_LIST(button_group,    custom,           -1, button_group_children),
        QT_RULE(data_widget_mapper,   none,              0),
        QT_RULE(completer,            none,              0),
        QT_RULE(dialog_button_box,    none,              0),

        // workspace
        QT_RULE(dock_widget,          single_widget,     1),
        QT_RULE_LIST(mdi_area,        custom,           -1, mdi_area_children),

        // standard dialogs — no DOM children
        QT_RULE(color_dialog,         none,              0),
        QT_RULE(file_dialog,          none,              0),
        QT_RULE(font_dialog,          none,              0),
        QT_RULE(input_dialog,         none,              0),
        QT_RULE(message_box,          none,              0),
        QT_RULE(error_message,        none,              0),
        QT_RULE(progress_dialog,      none,              0),

        // utility — leaves
        QT_RULE(size_grip,            none,              0),
        QT_RULE(focus_frame,          none,              0),
    };

    inline constexpr std::size_t qt_child_rule_table_size =
        sizeof(qt_child_rule_table) / sizeof(qt_child_rule_table[0]);

    #undef QT_RULE
    #undef QT_RULE_LIST

}   // namespace internal


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §8  validate_qt_child()
// ═══════════════════════════════════════════════════════════════════════════════

// qt_child_rule_for
//   Returns the child rule for a parent type, or nullptr.
inline const qt_child_rule*
qt_child_rule_for(DQtElementType _parent) noexcept
{
    for (std::size_t i = 0;
         i < internal::qt_child_rule_table_size;
         ++i)
    {
        if (internal::qt_child_rule_table[i].parent == _parent)
        {
            return &internal::qt_child_rule_table[i];
        }
    }

    return nullptr;
}

// qt_validation_result
//   struct: pass/fail + diagnostic string.
struct qt_validation_result
{
    bool        valid = true;
    std::string diagnostic;

    explicit operator bool() const noexcept { return valid; }
};

inline qt_validation_result qt_pass() noexcept
{
    return {true, {}};
}

inline qt_validation_result qt_fail(const char* _msg)
{
    return {false, std::string(_msg)};
}

inline qt_validation_result qt_fail(const std::string& _msg)
{
    return {false, _msg};
}

// validate_qt_child_type
//   Checks whether child type is allowed under parent type.
inline qt_validation_result
validate_qt_child_type(DQtElementType _parent,
                       DQtElementType _child)
{
    const auto* rule = qt_child_rule_for(_parent);

    if (!rule)
    {
        return qt_pass();
    }

    switch (rule->policy)
    {
        case DQtChildPolicy::none:
        {
            return qt_fail(
                std::string(qt_class_name_of(_parent)) +
                " does not accept children");
        }

        case DQtChildPolicy::any_visual:
        {
            if (is_qt_visual(_child))
            {
                return qt_pass();
            }

            return qt_fail("only visual elements are allowed here");
        }

        case DQtChildPolicy::any_widget:
        {
            if ( is_qt_visual(_child) &&
                 !is_qt_layout(_child) &&
                 !is_qt_command(_child) )
            {
                return qt_pass();
            }

            return qt_fail("only widget elements are allowed here");
        }

        case DQtChildPolicy::widgets_layouts:
        {
            if ( is_qt_visual(_child) ||
                 is_qt_layout(_child) )
            {
                return qt_pass();
            }

            return qt_fail("only widgets and layouts are allowed here");
        }

        case DQtChildPolicy::single_widget:
        {
            if ( is_qt_visual(_child) &&
                 !is_qt_layout(_child) )
            {
                return qt_pass();
            }

            return qt_fail("only a single widget is allowed here");
        }

        case DQtChildPolicy::pages_only:
        {
            if ( is_qt_visual(_child) &&
                 !is_qt_layout(_child) &&
                 !is_qt_command(_child) )
            {
                return qt_pass();
            }

            return qt_fail("only page widgets are allowed here");
        }

        case DQtChildPolicy::custom:
        {
            for (std::size_t i = 0;
                 i < rule->allowed_count;
                 ++i)
            {
                if (rule->allowed_types[i] == _child)
                {
                    return qt_pass();
                }
            }

            return qt_fail(
                std::string(qt_class_name_of(_child)) +
                " is not allowed as child of " +
                qt_class_name_of(_parent));
        }
    }

    return qt_pass();
}

// validate_qt_child_insert
//   Full check: type compatibility + cardinality.
inline qt_validation_result
validate_qt_child_insert(DQtElementType _parent,
                         DQtElementType _child,
                         std::size_t    _current_count)
{
    auto type_result = validate_qt_child_type(_parent, _child);

    if (!type_result)
    {
        return type_result;
    }

    const auto* rule = qt_child_rule_for(_parent);

    if ( rule &&
         rule->max_children >= 0 &&
         static_cast<int>(_current_count) >= rule->max_children )
    {
        return qt_fail(
            std::string(qt_class_name_of(_parent)) +
            " already has the maximum number of children (" +
            std::to_string(rule->max_children) + ")");
    }

    return qt_pass();
}

// validate_qt_not_abstract
//   Checks that a type is not abstract.
inline qt_validation_result
validate_qt_not_abstract(DQtElementType _type)
{
    if (is_qt_abstract(_type))
    {
        return qt_fail(
            std::string(qt_class_name_of(_type)) +
            " is abstract and cannot be instantiated");
    }

    return qt_pass();
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §9  CONVENIENCE ALIASES
// ═══════════════════════════════════════════════════════════════════════════════

// qt_dom_element
//   alias: dom_element specialised on the Qt taxonomy.
using qt_dom_element = dom_element<DQtElementType>;

// qt_dom_node
//   alias: tree_node with Qt DOM payload.
// Default features: collapsible + context.
template<unsigned _Feat = (vf_collapsible | vf_context),
         typename _Icon = int>
using qt_dom_node = dom_node<DQtElementType, _Feat, _Icon>;

// qt_dom_view
//   alias: tree_view with Qt DOM payload.
template<unsigned _Feat = (vf_collapsible | vf_context),
         typename _Icon = int>
using qt_dom_view = dom_view<DQtElementType, _Feat, _Icon>;


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §10  QT-SPECIFIC DOM FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// qt_find_by_tag
//   Searches a qt_dom_node tree for the first node with matching tag.
template<unsigned _F, typename _I>
qt_dom_node<_F, _I>*
qt_find_by_tag(qt_dom_node<_F, _I>& _root,
               DQtElementType       _tag)
{
    return find_by_tag(_root, _tag);
}

// qt_collect_by_tag
//   Collects all nodes with matching tag.
template<unsigned _F, typename _I>
std::vector<qt_dom_node<_F, _I>*>
qt_collect_by_tag(qt_dom_node<_F, _I>& _root,
                  DQtElementType       _tag)
{
    return collect_by_tag(_root, _tag);
}

// qt_collect_by_kind
//   Collects all nodes whose tag maps to the given kind.
template<unsigned _F, typename _I>
std::vector<qt_dom_node<_F, _I>*>
qt_collect_by_kind(qt_dom_node<_F, _I>& _root,
                   DQtNodeKind          _kind)
{
    std::vector<qt_dom_node<_F, _I>*> out;

    walk(_root, [&out, _kind](auto& n, std::size_t)
    {
        if (qt_node_kind_of(n.data.tag) == _kind)
        {
            out.push_back(&n);
        }
    });

    return out;
}

// qt_info
//   Returns the metadata for a node's element type.
template<unsigned _F, typename _I>
const qt_element_info*
qt_info(const qt_dom_node<_F, _I>& _node) noexcept
{
    return qt_element_info_for(_node.data.tag);
}

// qt_validated_add_child
//   Validates and adds a child if allowed.  Returns pointer to
// the new child on success, or nullptr on validation failure.
// Optionally fills _result with the diagnostic.
template<unsigned _F, typename _I>
qt_dom_node<_F, _I>*
qt_validated_add_child(qt_dom_node<_F, _I>&    _parent,
                       qt_dom_element          _child_elem,
                       qt_validation_result*   _result = nullptr)
{
    auto abs = validate_qt_not_abstract(_child_elem.tag);

    if (!abs)
    {
        if (_result) { *_result = abs; }

        return nullptr;
    }

    auto compat = validate_qt_child_insert(
        _parent.data.tag,
        _child_elem.tag,
        _parent.child_count());

    if (!compat)
    {
        if (_result) { *_result = compat; }

        return nullptr;
    }

    auto& child = emplace_child(_parent,
        static_cast<qt_dom_element&&>(_child_elem));

    if (_result) { *_result = qt_pass(); }

    return &child;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_QT_DOM_TREE_
