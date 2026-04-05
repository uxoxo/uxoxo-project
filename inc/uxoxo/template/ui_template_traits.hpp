/*******************************************************************************
* uxoxo [ui]                                             ui_template_traits.hpp
*
*   SFINAE-based capability detection for UI backends (renderers).  These traits
* determine what a backend IS and what it CAN DO at compile time, enabling the
* template layer to do maximum work before handing off to framework-specific
* code.
*
*   The pattern mirrors component_traits.hpp:
*     1. `detail` namespace holds fine-grained has_X detectors
*     2. Top-level traits compose them via std::conjunction / std::disjunction
*     3. _v variable templates expose boolean results
*
*   A ui_template queries these traits to decide:
*     - Which node types can be natively rendered vs. emulated
*     - Whether shortcuts, menus, clipboard, mouse, etc. are available
*     - How to translate abstract layout intent into backend primitives
*     - What modality (TUI, GUI, Web, VUI) governs rendering
*
*   Sub-sections:
*     1.  Primitive backend member detection
*     2.  Convenience aliases (_v variable templates)
*     3.  Composite capability traits
*     4.  Modality classification
*     5.  Backend requirement levels
*     6.  Feature negotiation helpers
*
*
* file:      /inc/uxoxo/template/ui_template_traits.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.31
*******************************************************************************/

#ifndef UXOXO_UI_TEMPLATE_TRAITS_
#define UXOXO_UI_TEMPLATE_TRAITS_ 1

#include <cstddef>
#include <string>
#include <type_traits>
//#include <djinterp>


NS_UXOXO
NS_TEMPLATES
NS_TRAITS


// 
//  1  PRIMITIVE BACKEND MEMBER DETECTION
// 
//   Fine-grained detectors.  Each checks for exactly one static constexpr
// flag, method, or type alias on a backend type.  Higher-level traits compose
// these via std::conjunction.
//
//   Backend authors expose capabilities by defining any subset of:
//     static constexpr bool supports_X = true;
//   and/or by providing methods with specific signatures.

namespace detail
{

    //  rendering methods 

    // has_render_node    backend can render a node tree
    template <typename, typename = void>
    struct has_render_node : std::false_type {};
    template <typename _B>
    struct has_render_node<_B, std::void_t<
        decltype(std::declval<_B>().render_node(
            std::declval<const ui::node&>()))
    >> : std::true_type {};

    // has_render_component    backend can render individual component_var
    template <typename, typename = void>
    struct has_render_component : std::false_type {};
    template <typename _B>
    struct has_render_component<_B, std::void_t<
        decltype(std::declval<_B>().render_component(
            std::declval<const ui::component_var&>()))
    >> : std::true_type {};

    // has_begin_frame / has_end_frame    backend uses frame-based rendering
    template <typename, typename = void>
    struct has_begin_frame : std::false_type {};
    template <typename _B>
    struct has_begin_frame<_B, std::void_t<
        decltype(std::declval<_B>().begin_frame())
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_end_frame : std::false_type {};
    template <typename _B>
    struct has_end_frame<_B, std::void_t<
        decltype(std::declval<_B>().end_frame())
    >> : std::true_type {};

    // has_event_loop    backend owns an event loop
    template <typename, typename = void>
    struct has_event_loop : std::false_type {};
    template <typename _B>
    struct has_event_loop<_B, std::void_t<
        decltype(std::declval<_B>().run())
    >> : std::true_type {};

    /***********************************************************************/

    //  static capability flags 
    //   Each detects `static constexpr bool supports_X = true` on a backend.
    //   When the flag is absent or false, the trait yields false_type.

    // supports_native_menu    backend can render a platform menu bar
    //   (e.g. Qt QMenuBar, Win32 HMENU, macOS NSMenu)
    template <typename, typename = void>
    struct has_native_menu_flag : std::false_type {};
    template <typename _B>
    struct has_native_menu_flag<_B, std::enable_if_t<_B::supports_native_menu>>
        : std::true_type {};

    // supports_mouse    backend can receive mouse/pointer events
    template <typename, typename = void>
    struct has_mouse_flag : std::false_type {};
    template <typename _B>
    struct has_mouse_flag<_B, std::enable_if_t<_B::supports_mouse>>
        : std::true_type {};

    // supports_keyboard    backend can receive keyboard events
    template <typename, typename = void>
    struct has_keyboard_flag : std::false_type {};
    template <typename _B>
    struct has_keyboard_flag<_B, std::enable_if_t<_B::supports_keyboard>>
        : std::true_type {};

    // supports_clipboard    backend can read/write the system clipboard
    template <typename, typename = void>
    struct has_clipboard_flag : std::false_type {};
    template <typename _B>
    struct has_clipboard_flag<_B, std::enable_if_t<_B::supports_clipboard>>
        : std::true_type {};

    // supports_color    backend can render in color
    template <typename, typename = void>
    struct has_color_flag : std::false_type {};
    template <typename _B>
    struct has_color_flag<_B, std::enable_if_t<_B::supports_color>>
        : std::true_type {};

    // supports_true_color    backend supports 24-bit RGB
    template <typename, typename = void>
    struct has_true_color_flag : std::false_type {};
    template <typename _B>
    struct has_true_color_flag<_B, std::enable_if_t<_B::supports_true_color>>
        : std::true_type {};

    // supports_unicode    backend can render full Unicode
    template <typename, typename = void>
    struct has_unicode_flag : std::false_type {};
    template <typename _B>
    struct has_unicode_flag<_B, std::enable_if_t<_B::supports_unicode>>
        : std::true_type {};

    // supports_resize    backend handles dynamic window/terminal resizing
    template <typename, typename = void>
    struct has_resize_flag : std::false_type {};
    template <typename _B>
    struct has_resize_flag<_B, std::enable_if_t<_B::supports_resize>>
        : std::true_type {};

    // supports_modal    backend can present modal overlays/dialogs
    template <typename, typename = void>
    struct has_modal_flag : std::false_type {};
    template <typename _B>
    struct has_modal_flag<_B, std::enable_if_t<_B::supports_modal>>
        : std::true_type {};

    // supports_drag_drop    backend supports drag and drop
    template <typename, typename = void>
    struct has_drag_drop_flag : std::false_type {};
    template <typename _B>
    struct has_drag_drop_flag<_B, std::enable_if_t<_B::supports_drag_drop>>
        : std::true_type {};

    // supports_custom_fonts    backend can use custom typefaces
    template <typename, typename = void>
    struct has_custom_fonts_flag : std::false_type {};
    template <typename _B>
    struct has_custom_fonts_flag<_B, std::enable_if_t<_B::supports_custom_fonts>>
        : std::true_type {};

    // supports_native_scrollbar    backend provides a native scrollbar widget
    template <typename, typename = void>
    struct has_native_scrollbar_flag : std::false_type {};
    template <typename _B>
    struct has_native_scrollbar_flag<_B,
        std::enable_if_t<_B::supports_native_scrollbar>>
        : std::true_type {};

    // supports_accessibility    backend exposes accessibility APIs
    //   (ARIA for web, AT-SPI for Linux, MSAA/UIA for Windows)
    template <typename, typename = void>
    struct has_accessibility_flag : std::false_type {};
    template <typename _B>
    struct has_accessibility_flag<_B,
        std::enable_if_t<_B::supports_accessibility>>
        : std::true_type {};

    /***********************************************************************/

    //  modality identity flags 
    //   Exactly one should be true per backend.

    // is_tui    terminal-based backend  (ncurses, FTXUI, etc.)
    template <typename, typename = void>
    struct has_tui_flag : std::false_type {};
    template <typename _B>
    struct has_tui_flag<_B, std::enable_if_t<_B::modality_tui>>
        : std::true_type {};

    // is_gui    native GUI backend  (Qt, GTK, Win32, Cocoa, etc.)
    template <typename, typename = void>
    struct has_gui_flag : std::false_type {};
    template <typename _B>
    struct has_gui_flag<_B, std::enable_if_t<_B::modality_gui>>
        : std::true_type {};

    // is_web    web/browser backend  (Emscripten, WASM, etc.)
    template <typename, typename = void>
    struct has_web_flag : std::false_type {};
    template <typename _B>
    struct has_web_flag<_B, std::enable_if_t<_B::modality_web>>
        : std::true_type {};

    // is_vui    voice-driven backend
    template <typename, typename = void>
    struct has_vui_flag : std::false_type {};
    template <typename _B>
    struct has_vui_flag<_B, std::enable_if_t<_B::modality_vui>>
        : std::true_type {};

    /***********************************************************************/

    //  shortcut / key-binding support 

    // has_register_shortcut    backend can register keyboard shortcuts
    template <typename, typename = void>
    struct has_register_shortcut : std::false_type {};
    template <typename _B>
    struct has_register_shortcut<_B, std::void_t<
        decltype(std::declval<_B>().register_shortcut(
            std::declval<int>(),
            std::declval<int>(),
            std::declval<std::function<void()>>()))
    >> : std::true_type {};

    // has_set_focus    backend can programmatically move focus
    template <typename, typename = void>
    struct has_set_focus : std::false_type {};
    template <typename _B>
    struct has_set_focus<_B, std::void_t<
        decltype(std::declval<_B>().set_focus(
            std::declval<const std::string&>()))
    >> : std::true_type {};

    // has_set_title    backend can set a window/terminal title
    template <typename, typename = void>
    struct has_set_title : std::false_type {};
    template <typename _B>
    struct has_set_title<_B, std::void_t<
        decltype(std::declval<_B>().set_title(
            std::declval<const std::string&>()))
    >> : std::true_type {};

    // has_set_size    backend can resize the root viewport
    template <typename, typename = void>
    struct has_set_size : std::false_type {};
    template <typename _B>
    struct has_set_size<_B, std::void_t<
        decltype(std::declval<_B>().set_size(
            std::declval<int>(),
            std::declval<int>()))
    >> : std::true_type {};

    // has_show_dialog    backend can present a dialog node modally
    template <typename, typename = void>
    struct has_show_dialog : std::false_type {};
    template <typename _B>
    struct has_show_dialog<_B, std::void_t<
        decltype(std::declval<_B>().show_dialog(
            std::declval<ui::node&>()))
    >> : std::true_type {};

    // has_apply_theme    backend can apply a theme object
    template <typename, typename = void>
    struct has_apply_theme : std::false_type {};
    template <typename _B>
    struct has_apply_theme<_B, std::void_t<
        decltype(std::declval<_B>().apply_theme(
            std::declval<const typename _B::theme_type&>()))
    >> : std::true_type {};

    // has_theme_type    backend defines a theme_type alias
    template <typename, typename = void>
    struct has_theme_type : std::false_type {};
    template <typename _B>
    struct has_theme_type<_B, std::void_t<typename _B::theme_type>>
        : std::true_type {};

}   // namespace detail


/*****************************************************************************/

// 
//  2  CONVENIENCE ALIASES  (_v variable templates)
// 

// rendering methods
template <typename _B> inline constexpr bool has_render_node_v      = detail::has_render_node<_B>::value;
template <typename _B> inline constexpr bool has_render_component_v = detail::has_render_component<_B>::value;
template <typename _B> inline constexpr bool has_begin_frame_v      = detail::has_begin_frame<_B>::value;
template <typename _B> inline constexpr bool has_end_frame_v        = detail::has_end_frame<_B>::value;
template <typename _B> inline constexpr bool has_event_loop_v       = detail::has_event_loop<_B>::value;

// capability flags
template <typename _B> inline constexpr bool supports_native_menu_v     = detail::has_native_menu_flag<_B>::value;
template <typename _B> inline constexpr bool supports_mouse_v           = detail::has_mouse_flag<_B>::value;
template <typename _B> inline constexpr bool supports_keyboard_v        = detail::has_keyboard_flag<_B>::value;
template <typename _B> inline constexpr bool supports_clipboard_v       = detail::has_clipboard_flag<_B>::value;
template <typename _B> inline constexpr bool supports_color_v           = detail::has_color_flag<_B>::value;
template <typename _B> inline constexpr bool supports_true_color_v      = detail::has_true_color_flag<_B>::value;
template <typename _B> inline constexpr bool supports_unicode_v         = detail::has_unicode_flag<_B>::value;
template <typename _B> inline constexpr bool supports_resize_v          = detail::has_resize_flag<_B>::value;
template <typename _B> inline constexpr bool supports_modal_v           = detail::has_modal_flag<_B>::value;
template <typename _B> inline constexpr bool supports_drag_drop_v       = detail::has_drag_drop_flag<_B>::value;
template <typename _B> inline constexpr bool supports_custom_fonts_v    = detail::has_custom_fonts_flag<_B>::value;
template <typename _B> inline constexpr bool supports_native_scrollbar_v = detail::has_native_scrollbar_flag<_B>::value;
template <typename _B> inline constexpr bool supports_accessibility_v   = detail::has_accessibility_flag<_B>::value;

// modality
template <typename _B> inline constexpr bool is_tui_backend_v = detail::has_tui_flag<_B>::value;
template <typename _B> inline constexpr bool is_gui_backend_v = detail::has_gui_flag<_B>::value;
template <typename _B> inline constexpr bool is_web_backend_v = detail::has_web_flag<_B>::value;
template <typename _B> inline constexpr bool is_vui_backend_v = detail::has_vui_flag<_B>::value;

// methods
template <typename _B> inline constexpr bool has_register_shortcut_v = detail::has_register_shortcut<_B>::value;
template <typename _B> inline constexpr bool has_set_focus_v         = detail::has_set_focus<_B>::value;
template <typename _B> inline constexpr bool has_set_title_v         = detail::has_set_title<_B>::value;
template <typename _B> inline constexpr bool has_set_size_v          = detail::has_set_size<_B>::value;
template <typename _B> inline constexpr bool has_show_dialog_v       = detail::has_show_dialog<_B>::value;
template <typename _B> inline constexpr bool has_apply_theme_v       = detail::has_apply_theme<_B>::value;
template <typename _B> inline constexpr bool has_theme_type_v        = detail::has_theme_type<_B>::value;


/*****************************************************************************/

// 
//  3  COMPOSITE CAPABILITY TRAITS
// 
//   Higher-level traits that compose the primitive detectors.  These answer
// questions like "can this backend handle modal dialogs natively?" or
// "does this backend support full interactive rendering?"

// can_render
//   Minimum rendering capability: either render_node or render_component.
template <typename _B>
struct can_render : std::disjunction<
    detail::has_render_node<_B>,
    detail::has_render_component<_B>
> {};

template <typename _B>
inline constexpr bool can_render_v = can_render<_B>::value;

/*****************************************************************************/

// uses_frame_rendering
//   Backend follows a begin_frame / end_frame cycle (immediate-mode or
// retained-mode with explicit flush).
template <typename _B>
struct uses_frame_rendering : std::conjunction<
    detail::has_begin_frame<_B>,
    detail::has_end_frame<_B>
> {};

template <typename _B>
inline constexpr bool uses_frame_rendering_v = uses_frame_rendering<_B>::value;

/*****************************************************************************/

// has_interactive_input
//   Backend supports at least one form of user input.
template <typename _B>
struct has_interactive_input : std::disjunction<
    detail::has_keyboard_flag<_B>,
    detail::has_mouse_flag<_B>
> {};

template <typename _B>
inline constexpr bool has_interactive_input_v = has_interactive_input<_B>::value;

/*****************************************************************************/

// can_show_modal
//   Backend can present modal dialogs: either natively or via a
// show_dialog() method.
template <typename _B>
struct can_show_modal : std::disjunction<
    detail::has_modal_flag<_B>,
    detail::has_show_dialog<_B>
> {};

template <typename _B>
inline constexpr bool can_show_modal_v = can_show_modal<_B>::value;

/*****************************************************************************/

// supports_theming
//   Backend defines a theme_type and an apply_theme method.
template <typename _B>
struct supports_theming : std::conjunction<
    detail::has_theme_type<_B>,
    detail::has_apply_theme<_B>
> {};

template <typename _B>
inline constexpr bool supports_theming_v = supports_theming<_B>::value;


/*****************************************************************************/

// 
//  4  MODALITY CLASSIFICATION
// 
//   Exactly one modality should be true per backend.  These are used by
// ui_template to make modality-dependent decisions (e.g. cell-based layout
// for TUI vs pixel-based for GUI).

// modality
//   enum: backend rendering modality.
enum class modality : std::uint8_t
{
    unknown,
    tui,        // terminal user interface  (ncurses, FTXUI)
    gui,        // graphical user interface  (Qt, GTK, Win32, Cocoa)
    web,        // browser/WASM             (Emscripten, web components)
    vui         // voice user interface
};

// backend_modality
//   trait: resolves the modality enum value for a backend type.
template <typename _B>
struct backend_modality
{
    static constexpr modality value =
        detail::has_tui_flag<_B>::value ? modality::tui :
        detail::has_gui_flag<_B>::value ? modality::gui :
        detail::has_web_flag<_B>::value ? modality::web :
        detail::has_vui_flag<_B>::value ? modality::vui :
                                          modality::unknown;
};

template <typename _B>
inline constexpr modality backend_modality_v = backend_modality<_B>::value;


/*****************************************************************************/

// 
//  5  BACKEND REQUIREMENT LEVELS
// 
//   These traits define tiers of backend capability.  A ui_template may
// require a minimum tier, and the compiler enforces this via static_assert.

// models_minimal_backend
//   The absolute minimum: can render nodes, has a modality identity,
// and can run an event loop.
template <typename _B>
struct models_minimal_backend : std::conjunction<
    can_render<_B>,
    detail::has_event_loop<_B>
> {};

template <typename _B>
inline constexpr bool models_minimal_backend_v = models_minimal_backend<_B>::value;

/*****************************************************************************/

// models_interactive_backend
//   Minimal + keyboard input + focus management.  Enough for basic
// forms and navigation.
template <typename _B>
struct models_interactive_backend : std::conjunction<
    models_minimal_backend<_B>,
    detail::has_keyboard_flag<_B>,
    detail::has_set_focus<_B>
> {};

template <typename _B>
inline constexpr bool models_interactive_backend_v = models_interactive_backend<_B>::value;

/*****************************************************************************/

// models_full_backend
//   Interactive + shortcuts + modal + title + resize.
// Everything needed for a full MC-like application.
template <typename _B>
struct models_full_backend : std::conjunction<
    models_interactive_backend<_B>,
    detail::has_register_shortcut<_B>,
    detail::has_modal_flag<_B>,
    detail::has_set_title<_B>,
    detail::has_resize_flag<_B>
> {};

template <typename _B>
inline constexpr bool models_full_backend_v = models_full_backend<_B>::value;


/*****************************************************************************/

// 
//  6  FEATURE NEGOTIATION HELPERS
// 
//   compile-time utilities for deciding how to translate abstract UI
// description into backend-specific primitives.  These are the "heavy
// lifting" functions that let the template layer make optimal decisions.

// menu_strategy
//   enum: how to render a menu_bar component.
enum class menu_strategy : std::uint8_t
{
    native,         // use backend's native menu system
    emulated        // render as a focusable component row
};

// resolve_menu_strategy
//   trait: selects native if the backend supports native menus,
// emulated otherwise.
template <typename _B>
struct resolve_menu_strategy
{
    static constexpr menu_strategy value =
        supports_native_menu_v<_B> ? menu_strategy::native
                                   : menu_strategy::emulated;
};

template <typename _B>
inline constexpr menu_strategy resolve_menu_strategy_v =
    resolve_menu_strategy<_B>::value;

/*****************************************************************************/

// scrollbar_strategy
//   enum: how to render scrollbar components.
enum class scrollbar_strategy : std::uint8_t
{
    native,         // use backend's native scrollbar widget
    character,      // draw with Unicode box-drawing characters (TUI)
    hidden          // omit entirely (e.g. VUI)
};

// resolve_scrollbar_strategy
//   trait: selects appropriate scrollbar rendering for the backend.
template <typename _B>
struct resolve_scrollbar_strategy
{
    static constexpr scrollbar_strategy value =
        supports_native_scrollbar_v<_B> ? scrollbar_strategy::native    :
        is_tui_backend_v<_B>            ? scrollbar_strategy::character :
        is_vui_backend_v<_B>            ? scrollbar_strategy::hidden    :
                                          scrollbar_strategy::character;
};

template <typename _B>
inline constexpr scrollbar_strategy resolve_scrollbar_strategy_v =
    resolve_scrollbar_strategy<_B>::value;

/*****************************************************************************/

// dialog_strategy
//   enum: how to present dialog components.
enum class dialog_strategy : std::uint8_t
{
    native_modal,   // OS-level modal dialog
    overlay,        // draw over existing content (TUI overlay, GUI popup)
    inline_expand   // expand inline (for backends that can't overlay)
};

// resolve_dialog_strategy
//   trait: selects dialog presentation for the backend.
template <typename _B>
struct resolve_dialog_strategy
{
    static constexpr dialog_strategy value =
        ( detail::has_show_dialog<_B>::value &&
          detail::has_modal_flag<_B>::value )   ? dialog_strategy::native_modal :
        ( detail::has_modal_flag<_B>::value  )  ? dialog_strategy::overlay      :
                                                  dialog_strategy::inline_expand;
};

template <typename _B>
inline constexpr dialog_strategy resolve_dialog_strategy_v =
    resolve_dialog_strategy<_B>::value;

/*****************************************************************************/

// color_depth
//   enum: color rendering capability tier.
enum class color_depth : std::uint8_t
{
    none,           // monochrome
    basic,          // 8/16 terminal colors
    full            // 24-bit true color
};

// resolve_color_depth
//   trait: determines color tier from backend flags.
template <typename _B>
struct resolve_color_depth
{
    static constexpr color_depth value =
        supports_true_color_v<_B> ? color_depth::full  :
        supports_color_v<_B>      ? color_depth::basic :
                                    color_depth::none;
};

template <typename _B>
inline constexpr color_depth resolve_color_depth_v =
    resolve_color_depth<_B>::value;


NS_END  // traits
NS_END  // templates
NS_END  // uxoxo

#endif  // UXOXO_UI_TEMPLATE_TRAITS_