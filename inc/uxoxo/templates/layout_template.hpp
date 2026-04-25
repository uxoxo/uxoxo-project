/*******************************************************************************
* uxoxo [layout]                                            layout_template.hpp
*
* Generic layout template base:
*   A layout template composes multiple component templates (inputs, outputs,
* tables, buttons, tabs, etc.) into a larger, vendor-agnostic unit that can be
* specialized by consumers into concrete dialogs, windows, panels, or panes.
* Where component templates target a single UI primitive, layout templates
* are about spatial composition and cross-component coordination: which
* components live where, which are master vs. detail, which actions apply to
* the selection vs. the dialog itself, and what the lifecycle hooks look like.
*
*   The CRTP pattern matches the one used by djinterp's connection_template:
* a `_helper` type parameter lets concrete layouts plug their own build,
* apply, cancel, and teardown behavior into the base without runtime
* virtual dispatch.  A DLayoutKind template parameter acts as the archetype
* tag, letting traits and overload sets branch on layout shape at compile
* time.
*
*   This header intentionally defines only the vendor-agnostic scaffolding.
* Specific layouts (e.g. a connection manager, a preferences pane, a
* wizard) derive from layout_template<_helper, _Kind> and add their own
* region members.  Free operations on layouts live in layout_common.hpp
* (TBA) and are dispatched via the structural detection in
* layout_template_traits.hpp.
*
* Contents:
*   1  DLayoutKind          - archetype tag for layout templates
*   2  DRegionRole          - role a region plays within a layout
*   3  layout_metadata      - title / description / tooltip bundle
*   4  layout_dimensions    - width / height hints and constraints
*   5  layout_template      - CRTP base for all layout templates
*
*
* path:      /inc/uxoxo/layout/layout_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.24
*******************************************************************************/

#ifndef  UXOXO_LAYOUT_TEMPLATE_
#define  UXOXO_LAYOUT_TEMPLATE_ 1

// std
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../uxoxo.hpp"


// -----------------------------------------------------------------------------
// NS_LAYOUT  (local fallback)
//   Forward-compatible shim: when uxoxo.hpp graduates a U_KEYWORD_LAYOUT /
// NS_LAYOUT pair, this block becomes a no-op.  Until then, layout headers
// open `namespace layout` under NS_UXOXO via this macro.
// -----------------------------------------------------------------------------
#ifndef NS_LAYOUT
    #define NS_LAYOUT   D_NAMESPACE(layout)
#endif


NS_UXOXO
NS_LAYOUT


// ===============================================================================
//  1  LAYOUT KIND
// ===============================================================================

// DLayoutKind
//   enum: archetype classification for layout templates.  Acts as a
// compile-time tag for layout_template specializations and as a runtime
// hint for inspectors, WYSIWYG editors, and serialization.  Unknown or
// bespoke layouts use `custom`.
enum class DLayoutKind : std::uint8_t
{
    custom             = 0,   // bespoke / unclassified
    form_dialog        = 1,   // single form body plus action bar
    master_detail      = 2,   // master list or tree + detail pane
    session_manager    = 3,   // master-detail of saved sessions + actions
                              // (HeidiSQL / WinSCP / SSMS pattern)
    preferences_pane   = 4,   // category tree + settings pane
    tabbed_dialog      = 5,   // tab strip + per-tab content + actions
    wizard             = 6,   // ordered step pages + next / back / finish
    split_view         = 7,   // generic N-pane splitter (no master bias)
    dashboard          = 8,   // grid of output widgets
    toolbar_window     = 9,   // toolbar-heavy content window
    status_bar         = 10   // persistent bottom-dock informational strip
};




// ===============================================================================
//  2  REGION ROLE
// ===============================================================================

// DRegionRole
//   enum: semantic role a sub-region plays inside a layout.  Renderers map
// these roles to concrete placement (top / left / right / bottom) based on
// the active layout kind, but the logical role is what components and
// ADL-dispatched operations key off.  A layout is free to omit any region
// that does not apply.
enum class DRegionRole : std::uint8_t
{
    header           = 0,   // title bar, filter, search affordance
    master           = 1,   // primary list / tree / nav pane
    detail           = 2,   // primary content for the current selection
    aside            = 3,   // secondary / tooltip / inspector pane
    footer           = 4,   // status strip
    toolbar          = 5,   // primary toolbar (usually above content)
    primary_actions  = 6,   // dialog-level actions (open, cancel, finish)
    master_actions   = 7,   // selection-level actions (new, save, delete)
    content          = 8    // generic body region for layouts without a
                            // strong master / detail split
};




// ===============================================================================
//  3  LAYOUT METADATA
// ===============================================================================

// layout_metadata
//   type: small value bundle carrying human-facing text attached to a
// layout.  Kept separate from layout_template so concrete layouts can
// reuse it, serialize it, or swap it without touching CRTP scaffolding.
struct layout_metadata
{
    std::string  title;         // window / dialog caption
    std::string  description;   // subtitle or help blurb
    std::string  tooltip;       // hover-level hint
    std::string  help_topic;    // help system key (optional)
};




// ===============================================================================
//  4  LAYOUT DIMENSIONS
// ===============================================================================

// layout_dimensions
//   type: size hints and constraints.  All values are hints to the
// renderer; a concrete platform (GLFW, ImGui, TUI) may honor, clamp, or
// ignore them.  A value of 0 means "unset" / "renderer default".
struct layout_dimensions
{
    std::int32_t  width_hint   = 0;   // preferred width in logical pixels
    std::int32_t  height_hint  = 0;   // preferred height in logical pixels
    std::int32_t  min_width    = 0;   // lower bound on user resize
    std::int32_t  min_height   = 0;
    std::int32_t  max_width    = 0;   // 0 means "unbounded"
    std::int32_t  max_height   = 0;
};




// ===============================================================================
//  5  LAYOUT TEMPLATE  (CRTP BASE)
// ===============================================================================
//   Base class template for all layouts.  Concrete layouts derive via the
// usual CRTP pattern:
//
//     class connection_manager_helper
//         : public layout_template<connection_manager_helper,
//                                  DLayoutKind::session_manager>
//     {
//         // region members (master list, detail pane, action bar, ...)
//         // build_helper(), apply_helper(), cancel_helper() definitions
//     };
//
//   The `_helper` suffix convention on the hook methods matches
// database_connection's CRTP forwarding style and avoids name collisions
// with the public forwarding methods on the base.
//
//   Concrete layouts are expected to provide:
//     void build_helper()       - construct / show the layout
//     void teardown_helper()    - release platform resources
//     void apply_helper()       - commit pending changes
//     void cancel_helper()      - discard pending changes
//     bool has_pending_helper() - whether apply would do work
//
//   Any hook the concrete layout does not define is detected via the
// traits layer; callers can branch with if-constexpr rather than
// overloading on tags.

// layout_template
//   class template: vendor-agnostic CRTP base for layout templates.
// Carries the common metadata, dimension, visibility, enablement, and
// lifecycle scaffolding shared by every concrete layout, and forwards
// lifecycle operations to the derived _helper.
//
// Template parameters:
//   _helper:   the concrete CRTP implementation type
//   _Kind  :   the DLayoutKind archetype tag for this layout
template <typename    _helper,
          DLayoutKind _Kind = DLayoutKind::custom>
class layout_template
{
public:

    // ---------------------------------------------------------------------
    //  type aliases
    // ---------------------------------------------------------------------

    using self_type      = _helper;
    using metadata_type  = layout_metadata;
    using dimension_type = layout_dimensions;

    // kind
    //   value: compile-time archetype for this layout.  Traits and free
    // operations may branch on this to pick kind-specific behavior.
    static constexpr DLayoutKind kind = _Kind;


    // ---------------------------------------------------------------------
    //  construction
    // ---------------------------------------------------------------------

    layout_template()
        : metadata{}
        , dimensions{}
        , visible(false)
        , enabled(true)
        , modal(false)
        , resizable(true)
        , closable(true)
        , dirty(false)
    {
    }

    explicit layout_template(metadata_type _metadata)
        : metadata(std::move(_metadata))
        , dimensions{}
        , visible(false)
        , enabled(true)
        , modal(false)
        , resizable(true)
        , closable(true)
        , dirty(false)
    {
    }

    ~layout_template() = default;

    // disable copying - layouts may own platform resources
    layout_template(const layout_template&)            = delete;
    layout_template& operator=(const layout_template&) = delete;

    // enable moving
    layout_template(layout_template&&) noexcept            = default;
    layout_template& operator=(layout_template&&) noexcept = default;


    // ---------------------------------------------------------------------
    //  lifecycle forwarding (CRTP)
    // ---------------------------------------------------------------------

    // build
    //   function: forwards to the derived _helper to construct or show
    // the layout.  Consumers call this when the layout should become
    // visible and interactive.
    void build()
    {
        impl().build_helper();

        return;
    }

    // teardown
    //   function: forwards to the derived _helper to release any
    // platform-backed resources owned by the layout.
    void teardown()
    {
        impl().teardown_helper();

        return;
    }

    // apply
    //   function: forwards to the derived _helper to commit any pending
    // changes.  Clears the dirty flag on return.
    void apply()
    {
        impl().apply_helper();

        dirty = false;

        return;
    }

    // cancel
    //   function: forwards to the derived _helper to discard any pending
    // changes.  Clears the dirty flag on return.
    void cancel()
    {
        impl().cancel_helper();

        dirty = false;

        return;
    }

    // has_pending
    //   function: returns true if apply() would have work to do.
    // Forwards to the derived _helper.
    [[nodiscard]] bool has_pending() const
    {
        return impl().has_pending_helper();
    }


    // ---------------------------------------------------------------------
    //  metadata access
    // ---------------------------------------------------------------------

    // get_kind
    //   function: returns the compile-time archetype tag.  Runtime
    // accessor for consumers that erased the template parameter.
    [[nodiscard]] static constexpr
    DLayoutKind get_kind() noexcept
    {
        return _Kind;
    }


    // ---------------------------------------------------------------------
    //  public members
    //   Accessed directly by free functions in layout_common.hpp, matching
    // the component pattern (public data, structural dispatch, no
    // getter / setter boilerplate on flags).
    // ---------------------------------------------------------------------

    metadata_type     metadata;     // title / description / tooltip / help
    dimension_type    dimensions;   // size hints and constraints

    bool              visible;      // currently on-screen
    bool              enabled;      // accepts user input as a whole
    bool              modal;        // blocks interaction with other layouts
    bool              resizable;    // user may resize
    bool              closable;     // user may dismiss (x button / esc)
    bool              dirty;        // has unsaved / unapplied changes


    // ---------------------------------------------------------------------
    //  optional callbacks
    //   Free-function dispatchers in layout_common.hpp (TBA) invoke these
    // when present.  Detection traits key off the member names, not
    // emptiness.
    // ---------------------------------------------------------------------

    std::function<void()>  on_open;    // fired after build()
    std::function<void()>  on_close;   // fired before teardown()
    std::function<void()>  on_apply;   // fired after successful apply()
    std::function<void()>  on_cancel;  // fired after cancel()


protected:

    // ---------------------------------------------------------------------
    //  CRTP helpers
    // ---------------------------------------------------------------------

    _helper& impl() noexcept
    {
        return static_cast<_helper&>(*this);
    }

    const _helper& impl() const noexcept
    {
        return static_cast<const _helper&>(*this);
    }
};


NS_END  // layout
NS_END  // uxoxo


#endif  // UXOXO_LAYOUT_TEMPLATE_
