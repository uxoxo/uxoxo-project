/*******************************************************************************
* uxoxo [layout]                                  connection_manager_template.hpp
*
* Connection / session manager layout template:
*   A generic, vendor-agnostic layout for managing saved connection or session
* profiles - the dialog pattern popularized by HeidiSQL's session manager,
* WinSCP's login window, FileZilla's site manager, PuTTY's session list, and
* SSMS's Connect to Server dialog.
*
*   The layout has two primary regions:
*     - MASTER:  a filterable list / table / tree of saved sessions plus the
*                session-level actions (new, save, delete, duplicate, ...)
*     - DETAIL:  an editor for the currently selected session's settings
*
* and two action bars:
*     - MASTER ACTIONS:   new / save / delete / duplicate / rename / ...
*     - PRIMARY ACTIONS:  open (connect) / cancel / more
*
*   The template is deliberately "dumb" about rendering: it owns the session
* collection, the selected-index cursor, the filter text, a feature-flag set
* describing which actions are enabled, and a bundle of callback hooks.  It
* does NOT own the master view, the detail form, or the action buttons -
* those live in the concrete _helper so that TUI, ImGui, and GLFW layouts
* can each provide their own component instances without the template caring.
*
*   The MASTER region can be placed on either the left or the right (or
* top / bottom for stacked layouts).  DMasterPlacement is a runtime value
* so users can flip the orientation without recompiling - matching tools
* like JetBrains DataGrip that let the user choose.
*
*   SESSION TYPE:
*   _Session is any user-defined type that represents a saved profile.
* Common examples:
*     - a database connection_config (host / port / user / password / ...)
*     - an SSH session descriptor (host / key path / tunnels / ...)
*     - an FTP / SFTP site
*     - an RDP target
*   The template does not constrain _Session - it is stored in a
* std::vector and passed by const-reference to the action hooks.
*
*   LAYER DIAGRAM:
*     connection_manager_template<_helper, _Session>
*       -> layout_template<_helper, DLayoutKind::session_manager>
*
*   CRTP HOOKS required on _helper (in addition to those required by
* layout_template):
*     void open_selected_helper()               - open / connect
*     void test_selected_helper()               - optional: test only
*     void import_sessions_helper(const string&)- optional
*     void export_sessions_helper(const string&)- optional
*     void render_master_helper()               - draw master region
*     void render_detail_helper()               - draw detail region
*
* Contents:
*   1  DMasterPlacement           - where the master region sits
*   2  DSessionAction             - identifier for session-level actions
*   3  connection_manager_template - the template class itself
*
*
* path:      /inc/uxoxo/layout/connection_manager_template.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.24
*******************************************************************************/

#ifndef  UXOXO_LAYOUT_CONNECTION_MANAGER_TEMPLATE_
#define  UXOXO_LAYOUT_CONNECTION_MANAGER_TEMPLATE_ 1

// std
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../uxoxo.hpp"
#include "./layout_template.hpp"


#ifndef NS_LAYOUT
    #define NS_LAYOUT   D_NAMESPACE(layout)
#endif


NS_UXOXO
NS_LAYOUT


// ===============================================================================
//  1  MASTER PLACEMENT
// ===============================================================================

// DMasterPlacement
//   enum: position of the MASTER region relative to the DETAIL region.
// Runtime value so the user can flip orientation without recompiling; the
// _helper is expected to honor the flag during rendering.  left / right
// are the common choices; top / bottom support stacked layouts and narrow
// form factors.
enum class DMasterPlacement : std::uint8_t
{
    left   = 0,   // master on left, detail on right  (HeidiSQL default)
    right  = 1,   // master on right, detail on left
    top    = 2,   // master above detail
    bottom = 3    // master below detail
};




// ===============================================================================
//  2  SESSION ACTIONS
// ===============================================================================

// DSessionAction
//   enum: identifier for each session-level action, used for the
// `can_` feature flags and for action dispatch tables.  Primary dialog
// actions (open / cancel / more) are distinguished from master-level
// actions because they operate on the dialog as a whole rather than on
// the current selection.
enum class DSessionAction : std::uint8_t
{
    // master-level (operate on sessions collection / selection)
    create         = 0,   // create a new blank session
    save           = 1,   // persist edits to selected session
    duplicate      = 2,   // clone the selected session
    rename         = 3,   // rename the selected session
    remove         = 4,   // delete the selected session
    import_all     = 5,   // import sessions from a file
    export_all     = 6,   // export sessions to a file

    // primary (operate on the dialog)
    open           = 64,  // open / connect to the selected session
    test           = 65,  // test the selected session without opening
    cancel         = 66,  // dismiss the dialog
    more           = 67   // split-button / overflow menu
};




// ===============================================================================
//  3  CONNECTION MANAGER TEMPLATE
// ===============================================================================

// connection_manager_template
//   class template: vendor-agnostic base for connection / session manager
// layouts.  Carries the session collection, selection state, filter text,
// action feature flags, and callback hooks shared by all session
// managers; forwards platform-specific rendering and primary actions to
// the derived _helper via CRTP.
//
// Template parameters:
//   _helper:   the concrete CRTP implementation type
//   _Session:  the saved-profile data type (a database connection_config,
//              an SSH session record, an FTP site, etc.).  No constraints;
//              stored in std::vector, passed by const-reference to hooks.
template <typename _helper,
          typename _Session>
class connection_manager_template
    : public layout_template<_helper, DLayoutKind::session_manager>
{
public:

    // ---------------------------------------------------------------------
    //  type aliases
    // ---------------------------------------------------------------------

    using session_type         = _Session;
    using session_storage_type = std::vector<_Session>;
    using index_type           = std::size_t;
    using selection_type       = std::optional<index_type>;
    using base_type            =
        layout_template<_helper, DLayoutKind::session_manager>;


    // ---------------------------------------------------------------------
    //  construction
    // ---------------------------------------------------------------------

    connection_manager_template()
        : base_type()
        , master_placement(DMasterPlacement::left)
        , sessions{}
        , selected_index{}
        , filter_text{}
        , can_create(true)
        , can_save(true)
        , can_remove(true)
        , can_duplicate(false)
        , can_rename(false)
        , can_import(false)
        , can_export(false)
        , can_test(false)
        , can_open(true)
        , show_filter(true)
        , show_more_button(false)
    {
    }

    explicit connection_manager_template(layout_metadata _metadata)
        : base_type(std::move(_metadata))
        , master_placement(DMasterPlacement::left)
        , sessions{}
        , selected_index{}
        , filter_text{}
        , can_create(true)
        , can_save(true)
        , can_remove(true)
        , can_duplicate(false)
        , can_rename(false)
        , can_import(false)
        , can_export(false)
        , can_test(false)
        , can_open(true)
        , show_filter(true)
        , show_more_button(false)
    {
    }

    ~connection_manager_template() = default;

    // inherit non-copyable / movable semantics from layout_template
    connection_manager_template(const connection_manager_template&) = delete;

    connection_manager_template&
    operator=(const connection_manager_template&) = delete;

    connection_manager_template(
        connection_manager_template&&) noexcept = default;

    connection_manager_template&
    operator=(connection_manager_template&&) noexcept = default;


    // ---------------------------------------------------------------------
    //  selection management
    // ---------------------------------------------------------------------

    // has_selection
    //   function: returns whether any session is currently selected.
    [[nodiscard]] bool has_selection() const noexcept
    {
        return selected_index.has_value()
            && (selected_index.value() < sessions.size());
    }

    // select
    //   function: sets the current selection to the given index.  No-op
    // if the index is out of range.  Fires on_selection_changed if the
    // selection actually moved.
    void select(index_type _idx)
    {
        // guard against out-of-range indices
        if (_idx >= sessions.size())
        {
            return;
        }

        const bool changed = (!selected_index.has_value())
                          || (selected_index.value() != _idx);

        selected_index = _idx;

        if (changed && on_selection_changed)
        {
            on_selection_changed(_idx);
        }

        return;
    }

    // clear_selection
    //   function: drops the current selection.  Fires on_selection_cleared
    // if one was present.
    void clear_selection()
    {
        const bool had = selected_index.has_value();

        selected_index.reset();

        if (had && on_selection_cleared)
        {
            on_selection_cleared();
        }

        return;
    }

    // selected_session
    //   function: returns a pointer to the currently selected session, or
    // nullptr if nothing is selected.
    [[nodiscard]] _Session* selected_session() noexcept
    {
        if (!has_selection())
        {
            return nullptr;
        }

        return &sessions[selected_index.value()];
    }

    // selected_session (const)
    //   function: returns a const pointer to the currently selected
    // session, or nullptr if nothing is selected.
    [[nodiscard]] const _Session* selected_session() const noexcept
    {
        if (!has_selection())
        {
            return nullptr;
        }

        return &sessions[selected_index.value()];
    }


    // ---------------------------------------------------------------------
    //  collection manipulation
    // ---------------------------------------------------------------------

    // add_session
    //   function: appends a session to the collection and returns its
    // index.  Marks the layout dirty.
    index_type add_session(_Session _s)
    {
        sessions.push_back(std::move(_s));

        this->dirty = true;

        if (on_session_added)
        {
            on_session_added(sessions.size() - 1);
        }

        return sessions.size() - 1;
    }

    // remove_session
    //   function: removes the session at the given index.  Adjusts the
    // current selection to stay valid.  Marks the layout dirty.
    // Returns true if a session was actually removed.
    bool remove_session(index_type _idx)
    {
        // guard against out-of-range indices
        if (_idx >= sessions.size())
        {
            return false;
        }

        sessions.erase(sessions.begin()
                       + static_cast<std::ptrdiff_t>(_idx));

        // keep the selection consistent with the new size
        if (selected_index.has_value())
        {
            const index_type cur = selected_index.value();

            if (cur == _idx)
            {
                // selected row was the one removed
                selected_index.reset();
            }
            else if (cur > _idx)
            {
                // selection shifted up by one
                selected_index = cur - 1;
            }
        }

        this->dirty = true;

        if (on_session_removed)
        {
            on_session_removed(_idx);
        }

        return true;
    }

    // duplicate_session
    //   function: inserts a copy of the session at _idx immediately after
    // it, selects the copy, and returns its new index.  Returns
    // std::nullopt if _idx is out of range.
    std::optional<index_type>
    duplicate_session(index_type _idx)
    {
        // guard against out-of-range indices
        if (_idx >= sessions.size())
        {
            return std::nullopt;
        }

        const index_type insert_at = _idx + 1;

        sessions.insert(sessions.begin()
                        + static_cast<std::ptrdiff_t>(insert_at),
                        sessions[_idx]);

        this->dirty    = true;
        selected_index = insert_at;

        if (on_session_duplicated)
        {
            on_session_duplicated(_idx, insert_at);
        }

        return insert_at;
    }

    // session_count
    //   function: number of sessions currently stored.
    [[nodiscard]] index_type session_count() const noexcept
    {
        return sessions.size();
    }

    // is_empty
    //   function: returns whether the session collection is empty.
    [[nodiscard]] bool is_empty() const noexcept
    {
        return sessions.empty();
    }


    // ---------------------------------------------------------------------
    //  filtering
    // ---------------------------------------------------------------------

    // matches_filter
    //   function: predicate helper used by the _helper when rendering
    // the master view.  Uses filter_predicate when set, otherwise a
    // trivial "always matches" default.
    [[nodiscard]] bool
    matches_filter(const _Session& _s) const
    {
        // empty filter always matches
        if (filter_text.empty())
        {
            return true;
        }

        if (filter_predicate)
        {
            return filter_predicate(_s, filter_text);
        }

        // no predicate installed - accept everything
        return true;
    }


    // ---------------------------------------------------------------------
    //  CRTP-forwarded actions
    //   Each of these forwards to a _helper method; _helper is free to
    // gate behavior on the corresponding can_* flag or ignore it.
    // ---------------------------------------------------------------------

    // open_selected
    //   function: forwards to the derived _helper to open / connect to
    // the currently selected session.  This is the PRIMARY action of
    // the dialog - equivalent to HeidiSQL's "Open" button.
    void open_selected()
    {
        this->impl().open_selected_helper();

        if (on_session_opened && selected_session())
        {
            on_session_opened(*selected_session());
        }

        return;
    }

    // test_selected
    //   function: forwards to the derived _helper to test the selected
    // session without opening it.  Useful for validating credentials
    // before committing.
    void test_selected()
    {
        this->impl().test_selected_helper();

        return;
    }

    // import_sessions
    //   function: forwards to the derived _helper to import sessions
    // from the given path.  Format is helper-defined (JSON, INI,
    // per-vendor registry, ...).
    void import_sessions(const std::string& _path)
    {
        this->impl().import_sessions_helper(_path);

        return;
    }

    // export_sessions
    //   function: forwards to the derived _helper to export the session
    // collection to the given path.
    void export_sessions(const std::string& _path) const
    {
        this->impl().export_sessions_helper(_path);

        return;
    }


    // ---------------------------------------------------------------------
    //  layout_template hook defaults
    //   Sensible defaults for the lifecycle hooks required by
    // layout_template.  Concrete _helpers may (and usually will) hide
    // these with their own definitions via name hiding in the derived
    // class; these defaults keep the template instantiable on its own
    // during scaffolding and testing.
    // ---------------------------------------------------------------------

    // build_helper
    //   function: default - no-op.  _helper overrides to build platform
    // UI resources.
    void build_helper()
    {
        return;
    }

    // teardown_helper
    //   function: default - no-op.
    void teardown_helper()
    {
        return;
    }

    // apply_helper
    //   function: default - clears dirty flag only.  _helper overrides
    // to flush pending edits into the selected session and persist.
    void apply_helper()
    {
        return;
    }

    // cancel_helper
    //   function: default - no-op.
    void cancel_helper()
    {
        return;
    }

    // has_pending_helper
    //   function: default - returns the base dirty flag.
    [[nodiscard]] bool has_pending_helper() const
    {
        return this->dirty;
    }


    // =====================================================================
    //  public members
    // =====================================================================
    //   Following the uxoxo convention (see component_mixin.hpp and the
    // layout_template base): structural detection via SFINAE keys off
    // these member names, so they remain public and named consistently.

    // ---- placement ------------------------------------------------------

    DMasterPlacement  master_placement;   // master region position

    // ---- master-side data ----------------------------------------------

    session_storage_type  sessions;       // saved profiles
    selection_type        selected_index; // current selection (if any)
    std::string           filter_text;    // current master filter

    // ---- feature flags -------------------------------------------------
    //   Enable / disable individual action buttons at runtime.  Flag
    // names match DSessionAction enumerators one-for-one.

    bool  can_create;       // new session button
    bool  can_save;         // save button
    bool  can_remove;       // delete button (using `remove` to avoid
                            // collision with C `delete` conventions;
                            // DSessionAction::remove mirrors this)
    bool  can_duplicate;    // duplicate button
    bool  can_rename;       // rename button / inline edit
    bool  can_import;       // import from file
    bool  can_export;       // export to file
    bool  can_test;         // test connection without opening
    bool  can_open;         // primary open / connect button
    bool  show_filter;      // show the master filter box
    bool  show_more_button; // show the "More" split-button


    // ---- predicates ----------------------------------------------------

    // filter_predicate
    //   callable: returns true if a session matches the given filter
    // text.  _helper or user installs this to implement the master
    // search box; the template simply calls it.
    std::function<bool(const _Session&,
                       const std::string&)>   filter_predicate;


    // ---- callback hooks ------------------------------------------------
    //   Selection / collection events.  Fired by the template's public
    // methods when the corresponding change occurs.  Checked for
    // truthiness at call sites (std::function); safe to leave unset.

    std::function<void(index_type)>      on_selection_changed;
    std::function<void()>                on_selection_cleared;

    std::function<void(index_type)>      on_session_added;
    std::function<void(index_type)>      on_session_removed;
    std::function<void(index_type,
                       index_type)>      on_session_duplicated; // (src, dst)

    //   Primary-action events.  on_session_opened fires after the
    // _helper's open_selected_helper returns, so _helper has an
    // opportunity to connect before the callback sees the result.

    std::function<void(const _Session&)> on_session_opened;
};


NS_END  // layout
NS_END  // uxoxo


#endif  // UXOXO_LAYOUT_CONNECTION_MANAGER_TEMPLATE_
