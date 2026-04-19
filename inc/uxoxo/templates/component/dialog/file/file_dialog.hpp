/*******************************************************************************
* uxoxo [component]                                             file_dialog.hpp
*
* Generic file dialog:
*   A framework-agnostic file dialog template.  Extends dialog<> with
* filesystem-navigation state — current path, listing of entries,
* filters, selection, view mode, sort, navigation history, and an
* optional sidebar — without prescribing HOW any of it is rendered.
*
*   The content area is deliberately abstract.  file_dialog exposes
* `entries`, `selected_indices`, `view_mode`, `sort_by`, and
* `sort_ascending`; the platform adapter decides whether to draw
* those as a tree (tree_view), a details table (table_view), an
* icon grid, a column browser, or a TUI list.  This keeps the same
* dialog usable from a GTK-style browser, a Windows Explorer-style
* details list, a macOS column view, or a ncurses-style picker.
*
*   Entries are templated on _Entry (default: file_entry POD).  If
* the caller already has a file_tree or similar node type, plug it
* in directly — the dialog only requires that entries expose `name`,
* `is_directory`, and be contained in a std::vector-compatible slot.
*
*   Feature flags gate optional panes (sidebar, preview, recent,
* status/search bar, create-folder, history, etc.) via EBO mixins
* so you pay only for what you enable.
*
* Contents:
*   1.  Feature flags (file_dialog_feat)
*   2.  Enums (file_view_mode, file_sort_by, file_select_mode,
*              file_path_mode)
*   3.  Support structs (file_entry, file_filter, file_bookmark)
*   4.  EBO mixins
*   5.  file_dialog struct
*   6.  Free functions
*   7.  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/dialog/file/file_dialog.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_FILE_DIALOG_
#define  UXOXO_COMPONENT_FILE_DIALOG_ 1

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./dialog.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  FILE DIALOG FEATURE FLAGS
// ===============================================================================
//   file_dialog owns its own 0-31 feature bitfield, independent of
// dialog_feat.  A file_dialog takes both _DlgFeat (dialog layer) and
// _FdFeat (file-dialog layer) template parameters.

enum file_dialog_feat : unsigned
{
    fdf_none            = 0,
    fdf_filename_input  = 1u << 0,   // expose a filename entry field
    fdf_filter_select   = 1u << 1,   // expose a filter picker
    fdf_sidebar         = 1u << 2,   // bookmarks / places sidebar
    fdf_history         = 1u << 3,   // back/forward navigation stack
    fdf_hidden_toggle   = 1u << 4,   // show-hidden-files flag
    fdf_create_folder   = 1u << 5,   // "new folder" affordance
    fdf_preview_pane    = 1u << 6,   // preview pane for the selection
    fdf_recent          = 1u << 7,   // recent locations list
    fdf_path_edit       = 1u << 8,   // editable path bar (vs. read-only breadcrumb)
    fdf_search          = 1u << 9,   // search / filter-by-name box
    fdf_multi_select    = 1u << 10,  // permit multiple selection
    fdf_dir_select      = 1u << 11,  // permit directory as a final selection
    fdf_status_preview  = 1u << 12,  // status line shows current selection info
    fdf_standard        = fdf_filter_select   |
                          fdf_history         |
                          fdf_sidebar         |
                          fdf_path_edit,
    fdf_all             = fdf_filename_input  |
                          fdf_filter_select   |
                          fdf_sidebar         |
                          fdf_history         |
                          fdf_hidden_toggle   |
                          fdf_create_folder   |
                          fdf_preview_pane    |
                          fdf_recent          |
                          fdf_path_edit       |
                          fdf_search          |
                          fdf_multi_select    |
                          fdf_dir_select      |
                          fdf_status_preview
};

constexpr unsigned operator|(
    file_dialog_feat _a,
    file_dialog_feat _b
) noexcept
{
    return ( static_cast<unsigned>(_a) | 
             static_cast<unsigned>(_b) );
}

constexpr bool has_fdf(
    unsigned          _f,
    file_dialog_feat  _bit
) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


// ===============================================================================
//  2.  ENUMS
// ===============================================================================

// file_view_mode
//   enum: how the entry listing is presented.  Purely advisory — the
// platform adapter may substitute an equivalent (e.g. render `icons`
// as a large-tile view in a GUI and as a compact name list in a TUI).
enum class file_view_mode : std::uint8_t
{
    list,         // simple name-only list
    details,      // columns: name + mtime + size (+ type)
    icons,        // icon grid
    thumbnails,   // preview-thumbnail grid
    tree,         // hierarchical tree
    columns,      // macOS-style column browser
    compact       // TUI-friendly single-column compact view
};

// file_sort_by
//   enum: which field the entry listing is sorted on.  `custom`
// means the caller has already sorted `entries` and the dialog
// should preserve that order.
enum class file_sort_by : std::uint8_t
{
    name,
    date_modified,
    date_created,
    size,
    type,
    extension,
    custom
};

// file_select_mode
//   enum: what kind of selection the dialog accepts.  Distinct from
// the open/save mode carried by file_open_save_dialog — this governs
// selection multiplicity, not lifecycle semantics.
enum class file_select_mode : std::uint8_t
{
    single_file,
    multiple_files,
    single_directory,
    multiple_directories,
    any_single,
    any_multiple
};

// file_path_mode
//   enum: how the path bar presents the current location.  The
// editable variants are surfaced only when fdf_path_edit is set.
enum class file_path_mode : std::uint8_t
{
    breadcrumb,   // crumb-trail navigation
    text,         // plain text path
    editable,     // user-editable text field
    combo         // breadcrumb with click-to-edit
};


// ===============================================================================
//  3.  SUPPORT STRUCTS
// ===============================================================================

// file_entry
//   struct: default POD entry type.  Adapters or callers needing
// richer metadata (permissions, owner, extended attributes) may
// substitute their own _Entry type; only `name` and `is_directory`
// are assumed by the default free functions.
struct file_entry
{
    std::string   name;           // display name (basename)
    std::string   path;           // absolute path
    bool          is_directory = false;
    bool          is_symlink   = false;
    bool          is_hidden    = false;
    std::uint64_t size          = 0;     // bytes; 0 for directories
    std::int64_t  mtime         = 0;     // unix epoch seconds
    std::int64_t  ctime         = 0;     // unix epoch seconds
    std::string   type_hint;             // extension / mime / platform string
    int           icon_id       = -1;    // optional atlas id
};

// file_filter
//   struct: one named pattern group in the filter picker, e.g.
// {"Blender files", {"*.blend", "*.blend1"}}.  An empty `patterns`
// vector is treated as "all files".
struct file_filter
{
    std::string               label;
    std::vector<std::string>  patterns;

    [[nodiscard]] bool
    accepts_all() const noexcept
    {
        return patterns.empty();
    }
};

// file_bookmark
//   struct: one entry in the sidebar / places pane.
struct file_bookmark
{
    std::string  label;   // display label
    std::string  path;    // target path
    int          icon_id = -1;
    bool         is_system = false;   // OS-provided (Home, Desktop, ...)
};


// ===============================================================================
//  4.  EBO MIXINS
// ===============================================================================

namespace file_dialog_mixin 
{

    // -- filename input -----------------------------------------------
    template <bool _Enable>
    struct filename_data
    {};

    template <>
    struct filename_data<true>
    {
        std::string filename_text;
    };

    // -- filter picker ------------------------------------------------
    template <bool _Enable>
    struct filter_data
    {};

    template <>
    struct filter_data<true>
    {
        std::vector<file_filter>  filters;
        std::size_t               active_filter = 0;
    };

    // -- sidebar ------------------------------------------------------
    template <bool _Enable>
    struct sidebar_data
    {};

    template <>
    struct sidebar_data<true>
    {
        std::vector<file_bookmark>  bookmarks;
        bool                        sidebar_visible = true;
    };

    // -- history ------------------------------------------------------
    template <bool _Enable>
    struct history_data
    {};

    template <>
    struct history_data<true>
    {
        std::vector<std::string>  back_stack;
        std::vector<std::string>  forward_stack;
    };

    // -- hidden toggle ------------------------------------------------
    template <bool _Enable>
    struct hidden_data
    {};

    template <>
    struct hidden_data<true>
    {
        bool show_hidden = false;
    };

    // -- create folder ------------------------------------------------
    template <bool _Enable>
    struct create_folder_data
    {};

    template <>
    struct create_folder_data<true>
    {
        std::function<bool(const std::string& /*parent*/,
                           const std::string& /*name*/)>  on_create_folder;
    };

    // -- preview pane -------------------------------------------------
    template <bool _Enable>
    struct preview_data
    {};

    template <>
    struct preview_data<true>
    {
        bool         preview_visible = true;
        std::string  preview_path;       // entry currently previewed
    };

    // -- recent locations ---------------------------------------------
    template <bool _Enable>
    struct recent_data
    {};

    template <>
    struct recent_data<true>
    {
        std::vector<std::string>  recent_paths;
        std::size_t               recent_max = 16;
    };

    // -- search -------------------------------------------------------
    template <bool _Enable>
    struct search_data
    {};

    template <>
    struct search_data<true>
    {
        std::string  search_text;
    };

}   // namespace file_dialog_mixin


// ===============================================================================
//  5.  FILE DIALOG
// ===============================================================================

// file_dialog
//   struct: framework-agnostic file dialog.  Inherits the button bar,
// result state, and geometry of dialog<_DlgFeat>, and adds filesystem
// navigation state gated by _FdFeat.
//
//   _Entry    entry type held in the listing (default: file_entry).
//   _DlgFeat  dialog-layer feature bitfield.
//   _FdFeat   file-dialog-layer feature bitfield.

template <typename _Entry   = file_entry,
          unsigned _DlgFeat = df_titled | df_closable | df_resizable
                            | df_movable | df_sized,
          unsigned _FdFeat  = fdf_standard>
struct file_dialog
    : public dialog<_DlgFeat>
      file_dialog_mixin::filename_data      <has_fdf(_FdFeat, fdf_filename_input)>,
      file_dialog_mixin::filter_data        <has_fdf(_FdFeat, fdf_filter_select)>,
      file_dialog_mixin::sidebar_data       <has_fdf(_FdFeat, fdf_sidebar)>,
      file_dialog_mixin::history_data       <has_fdf(_FdFeat, fdf_history)>,
      file_dialog_mixin::hidden_data        <has_fdf(_FdFeat, fdf_hidden_toggle)>,
      file_dialog_mixin::create_folder_data <has_fdf(_FdFeat, fdf_create_folder)>,
      file_dialog_mixin::preview_data       <has_fdf(_FdFeat, fdf_preview_pane)>,
      file_dialog_mixin::recent_data        <has_fdf(_FdFeat, fdf_recent)>,
      file_dialog_mixin::search_data        <has_fdf(_FdFeat, fdf_search)>
{
    using entry_type     = _Entry;
    using entry_list     = std::vector<_Entry>;
    using index_list     = std::vector<std::size_t>;
    using path_change_fn = std::function<void(const std::string&)>;
    using selection_fn   = std::function<void(const entry_list&,
                                              const index_list&)>;
    using refresh_fn     = std::function<void(const std::string&,
                                              entry_list&)>;

    static constexpr unsigned dialog_features     = _DlgFeat;
    static constexpr unsigned file_features       = _FdFeat;

    static constexpr bool has_filename_input = has_fdf(_FdFeat, fdf_filename_input);
    static constexpr bool has_filter_select  = has_fdf(_FdFeat, fdf_filter_select);
    static constexpr bool has_sidebar        = has_fdf(_FdFeat, fdf_sidebar);
    static constexpr bool has_history        = has_fdf(_FdFeat, fdf_history);
    static constexpr bool has_hidden_toggle  = has_fdf(_FdFeat, fdf_hidden_toggle);
    static constexpr bool has_create_folder  = has_fdf(_FdFeat, fdf_create_folder);
    static constexpr bool has_preview_pane   = has_fdf(_FdFeat, fdf_preview_pane);
    static constexpr bool has_recent         = has_fdf(_FdFeat, fdf_recent);
    static constexpr bool has_path_edit      = has_fdf(_FdFeat, fdf_path_edit);
    static constexpr bool has_search         = has_fdf(_FdFeat, fdf_search);
    static constexpr bool has_multi_select   = has_fdf(_FdFeat, fdf_multi_select);
    static constexpr bool has_dir_select     = has_fdf(_FdFeat, fdf_dir_select);
    static constexpr bool has_status_preview = has_fdf(_FdFeat, fdf_status_preview);

    // -- path state ---------------------------------------------------
    std::string       current_path;
    std::string       root_path;        // optional sandbox root ("" = none)
    file_path_mode    path_mode = file_path_mode::breadcrumb;

    // -- listing ------------------------------------------------------
    entry_list        entries;

    // -- selection ----------------------------------------------------
    index_list        selected_indices;
    file_select_mode  select_mode = file_select_mode::single_file;

    // -- view / sort --------------------------------------------------
    file_view_mode    view_mode      = file_view_mode::details;
    file_sort_by      sort_by        = file_sort_by::name;
    bool              sort_ascending = true;

    // -- callbacks ----------------------------------------------------
    //   on_path_change:   fired when current_path is updated
    //   on_selection:     fired when selected_indices changes
    //   on_refresh:       asked to repopulate `entries` for a path;
    //                     the adapter is expected to install this
    //   on_entry_activate: fired on double-click / Enter on an entry
    path_change_fn   on_path_change;
    selection_fn     on_selection;
    refresh_fn       on_refresh;
    std::function<void(const _Entry&, std::size_t /*index*/)>  on_entry_activate;

    // -- construction -------------------------------------------------
    file_dialog() = default;
};


// ===============================================================================
//  6.  FREE FUNCTIONS
// ===============================================================================

// fd_refresh
//   repopulates `entries` via the installed on_refresh callback.
// No-op if no callback is installed.
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_refresh(
    file_dialog<_E, _DF, _FF>& _fd
)
{
    if (_fd.on_refresh)
    {
        _fd.entries.clear();
        _fd.on_refresh(_fd.current_path, _fd.entries);
    }

    return;
}

// fd_navigate_to
//   changes current_path, pushes the old path onto the back_stack
// (when fdf_history is enabled), clears forward_stack, clears
// selection, and fires on_path_change + on_refresh.
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_navigate_to(
    file_dialog<_E, _DF, _FF>&  _fd,
    std::string                 _path
)
{
    if constexpr (has_fdf(_FF, fdf_history))
    {
        if (!_fd.current_path.empty())
        {
            _fd.back_stack.push_back(_fd.current_path);
        }

        _fd.forward_stack.clear();
    }

    _fd.current_path = std::move(_path);
    _fd.selected_indices.clear();

    if (_fd.on_path_change)
    {
        _fd.on_path_change(_fd.current_path);
    }

    fd_refresh(_fd);

    return;
}

// fd_go_back
template <typename _E, unsigned _DF, unsigned _FF>
bool
fd_go_back(
    file_dialog<_E, _DF, _FF>& _fd
)
{
    static_assert(has_fdf(_FF, fdf_history),
                  "requires fdf_history");

    if (_fd.back_stack.empty())
    {
        return false;
    }

    _fd.forward_stack.push_back(_fd.current_path);
    _fd.current_path = _fd.back_stack.back();
    _fd.back_stack.pop_back();

    _fd.selected_indices.clear();

    if (_fd.on_path_change)
    {
        _fd.on_path_change(_fd.current_path);
    }

    fd_refresh(_fd);

    return true;
}

// fd_go_forward
template <typename _E, unsigned _DF, unsigned _FF>
bool
fd_go_forward(
    file_dialog<_E, _DF, _FF>& _fd
)
{
    static_assert(has_fdf(_FF, fdf_history),
                  "requires fdf_history");

    if (_fd.forward_stack.empty())
    {
        return false;
    }

    _fd.back_stack.push_back(_fd.current_path);
    _fd.current_path = _fd.forward_stack.back();
    _fd.forward_stack.pop_back();

    _fd.selected_indices.clear();

    if (_fd.on_path_change)
    {
        _fd.on_path_change(_fd.current_path);
    }

    fd_refresh(_fd);

    return true;
}

// fd_go_up
//   navigates to the parent directory of current_path by stripping
// the trailing path component.  Accepts both '/' and '\\' separators.
template <typename _E, unsigned _DF, unsigned _FF>
bool
fd_go_up(
    file_dialog<_E, _DF, _FF>& _fd
)
{
    if (_fd.current_path.empty())
    {
        return false;
    }

    std::string p = _fd.current_path;

    // strip any trailing separator (but keep a root like "/" or "C:\\")
    while ( (p.size() > 1) &&
            ( (p.back() == '/') || (p.back() == '\\') ) )
    {
        p.pop_back();
    }

    const auto slash = p.find_last_of("/\\");

    if ( (slash == std::string::npos) ||
         (slash == 0) )
    {
        // at root already
        if (slash == 0)
        {
            p = p.substr(0, 1);
        }
        else
        {
            return false;
        }
    }
    else
    {
        p = p.substr(0, slash);
    }

    fd_navigate_to(_fd, std::move(p));

    return true;
}

// fd_set_entries
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_set_entries(
    file_dialog<_E, _DF, _FF>&  _fd,
    std::vector<_E>             _entries
)
{
    _fd.entries = std::move(_entries);
    _fd.selected_indices.clear();

    return;
}

// fd_add_entry
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_add_entry(
    file_dialog<_E, _DF, _FF>&  _fd,
    _E                          _entry
)
{
    _fd.entries.push_back(std::move(_entry));

    return;
}

// fd_clear_entries
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_clear_entries(
    file_dialog<_E, _DF, _FF>& _fd
)
{
    _fd.entries.clear();
    _fd.selected_indices.clear();

    return;
}

// fd_select
//   selects a single entry by index.  Clears any previous selection.
// Out-of-range indices are ignored.
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_select(
    file_dialog<_E, _DF, _FF>&  _fd,
    std::size_t                 _i
)
{
    if (_i >= _fd.entries.size())
    {
        return;
    }

    _fd.selected_indices.clear();
    _fd.selected_indices.push_back(_i);

    if (_fd.on_selection)
    {
        _fd.on_selection(_fd.entries, _fd.selected_indices);
    }

    return;
}

// fd_toggle_select
//   toggles an index's membership in the selection.  Requires
// fdf_multi_select.
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_toggle_select(
    file_dialog<_E, _DF, _FF>&  _fd,
    std::size_t                 _i
)
{
    static_assert(has_fdf(_FF, fdf_multi_select),
                  "requires fdf_multi_select");

    if (_i >= _fd.entries.size())
    {
        return;
    }

    auto it = _fd.selected_indices.begin();

    while (it != _fd.selected_indices.end())
    {
        if (*it == _i)
        {
            _fd.selected_indices.erase(it);

            if (_fd.on_selection)
            {
                _fd.on_selection(_fd.entries, _fd.selected_indices);
            }

            return;
        }

        ++it;
    }

    _fd.selected_indices.push_back(_i);

    if (_fd.on_selection)
    {
        _fd.on_selection(_fd.entries, _fd.selected_indices);
    }

    return;
}

// fd_clear_selection
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_clear_selection(
    file_dialog<_E, _DF, _FF>& _fd
)
{
    _fd.selected_indices.clear();

    if (_fd.on_selection)
    {
        _fd.on_selection(_fd.entries, _fd.selected_indices);
    }

    return;
}

// fd_activate_entry
//   invoked when the user double-clicks / hits Enter on an entry.
// For directories, navigates into them.  For files, fires
// on_entry_activate.  Out-of-range indices are ignored.
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_activate_entry(
    file_dialog<_E, _DF, _FF>&  _fd,
    std::size_t                 _i
)
{
    if (_i >= _fd.entries.size())
    {
        return;
    }

    const _E& e = _fd.entries[_i];

    if (e.is_directory)
    {
        fd_navigate_to(_fd, e.path);

        return;
    }

    if (_fd.on_entry_activate)
    {
        _fd.on_entry_activate(e, _i);
    }

    return;
}

// fd_set_view_mode
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_set_view_mode(
    file_dialog<_E, _DF, _FF>&  _fd,
    file_view_mode              _m
)
{
    _fd.view_mode = _m;

    return;
}

// fd_set_sort
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_set_sort(
    file_dialog<_E, _DF, _FF>&  _fd,
    file_sort_by                _by,
    bool                        _ascending = true
)
{
    _fd.sort_by        = _by;
    _fd.sort_ascending = _ascending;

    return;
}

// fd_add_filter
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_add_filter(
    file_dialog<_E, _DF, _FF>&  _fd,
    file_filter                 _filter
)
{
    static_assert(has_fdf(_FF, fdf_filter_select),
                  "requires fdf_filter_select");

    _fd.filters.push_back(std::move(_filter));

    return;
}

// fd_set_active_filter
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_set_active_filter(
    file_dialog<_E, _DF, _FF>&  _fd,
    std::size_t                 _i
)
{
    static_assert(has_fdf(_FF, fdf_filter_select),
                  "requires fdf_filter_select");

    if (_i < _fd.filters.size())
    {
        _fd.active_filter = _i;
    }

    return;
}

// fd_current_filter
template <typename _E, unsigned _DF, unsigned _FF>
const file_filter*
fd_current_filter(
    const file_dialog<_E, _DF, _FF>& _fd
)
{
    static_assert(has_fdf(_FF, fdf_filter_select),
                  "requires fdf_filter_select");

    if ( (_fd.filters.empty()) ||
         (_fd.active_filter >= _fd.filters.size()) )
    {
        return nullptr;
    }

    return &_fd.filters[_fd.active_filter];
}

// fd_add_bookmark
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_add_bookmark(
    file_dialog<_E, _DF, _FF>&  _fd,
    file_bookmark               _b
)
{
    static_assert(has_fdf(_FF, fdf_sidebar),
                  "requires fdf_sidebar");

    _fd.bookmarks.push_back(std::move(_b));

    return;
}

// fd_toggle_hidden
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_toggle_hidden(
    file_dialog<_E, _DF, _FF>& _fd
)
{
    static_assert(has_fdf(_FF, fdf_hidden_toggle),
                  "requires fdf_hidden_toggle");

    _fd.show_hidden = !_fd.show_hidden;

    fd_refresh(_fd);

    return;
}

// fd_push_recent
//   pushes a path onto the recent-locations list, bounded by
// recent_max.  Duplicates are promoted to the front.
template <typename _E, unsigned _DF, unsigned _FF>
void
fd_push_recent(
    file_dialog<_E, _DF, _FF>&  _fd,
    std::string                 _path
)
{
    static_assert(has_fdf(_FF, fdf_recent),
                  "requires fdf_recent");

    // remove any existing copy
    for (auto it = _fd.recent_paths.begin();
         it != _fd.recent_paths.end();
         ++it)
    {
        if (*it == _path)
        {
            _fd.recent_paths.erase(it);
            break;
        }
    }

    _fd.recent_paths.insert(_fd.recent_paths.begin(), std::move(_path));

    while (_fd.recent_paths.size() > _fd.recent_max)
    {
        _fd.recent_paths.pop_back();
    }

    return;
}

// fd_selected_entries
//   returns a vector of pointers into `entries` for the current
// selection.  Pointers are stable only until the next mutation of
// `entries`.
template <typename _E, unsigned _DF, unsigned _FF>
std::vector<const _E*>
fd_selected_entries(
    const file_dialog<_E, _DF, _FF>& _fd
)
{
    std::vector<const _E*> out;
    out.reserve(_fd.selected_indices.size());

    for (std::size_t i : _fd.selected_indices)
    {
        if (i < _fd.entries.size())
        {
            out.push_back(&_fd.entries[i]);
        }
    }

    return out;
}


// ===============================================================================
//  7.  TRAITS
// ===============================================================================

namespace file_dialog_traits {
NS_INTERNAL

    template <typename, typename = void>
    struct has_current_path_member : std::false_type {};
    template <typename _Type>
    struct has_current_path_member<_Type, std::void_t<
        decltype(std::declval<_Type>().current_path)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_entries_member : std::false_type {};
    template <typename _Type>
    struct has_entries_member<_Type, std::void_t<
        decltype(std::declval<_Type>().entries)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_selected_indices_member : std::false_type {};
    template <typename _Type>
    struct has_selected_indices_member<_Type, std::void_t<
        decltype(std::declval<_Type>().selected_indices)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_view_mode_member : std::false_type {};
    template <typename _Type>
    struct has_view_mode_member<_Type, std::void_t<
        decltype(std::declval<_Type>().view_mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_filters_member : std::false_type {};
    template <typename _Type>
    struct has_filters_member<_Type, std::void_t<
        decltype(std::declval<_Type>().filters)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_bookmarks_member : std::false_type {};
    template <typename _Type>
    struct has_bookmarks_member<_Type, std::void_t<
        decltype(std::declval<_Type>().bookmarks)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_back_stack_member : std::false_type {};
    template <typename _Type>
    struct has_back_stack_member<_Type, std::void_t<
        decltype(std::declval<_Type>().back_stack)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_filename_text_member : std::false_type {};
    template <typename _Type>
    struct has_filename_text_member<_Type, std::void_t<
        decltype(std::declval<_Type>().filename_text)
    >> : std::true_type {};

}   // NS_INTERNAL

template <typename _Type>
inline constexpr bool has_current_path_v =
    internal::has_current_path_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_entries_v =
    internal::has_entries_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_view_mode_v =
    internal::has_view_mode_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_filters_v =
    internal::has_filters_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_bookmarks_v =
    internal::has_bookmarks_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_history_v =
    internal::has_back_stack_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_filename_input_v =
    internal::has_filename_text_member<_Type>::value;

// is_file_dialog
//   type trait: has current_path + entries + selected_indices + view_mode.
// A file_dialog is structurally distinguished from a plain dialog by
// these four members; it still satisfies dialog_traits::is_dialog
// when it inherits dialog<>.
template <typename _Type>
struct is_file_dialog : std::conjunction<
    internal::has_current_path_member<_Type>,
    internal::has_entries_member<_Type>,
    internal::has_selected_indices_member<_Type>,
    internal::has_view_mode_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_file_dialog_v =
    is_file_dialog<_Type>::value;

// is_filtered_file_dialog
template <typename _Type>
struct is_filtered_file_dialog : std::conjunction<
    is_file_dialog<_Type>,
    internal::has_filters_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_filtered_file_dialog_v =
    is_filtered_file_dialog<_Type>::value;

// is_navigable_file_dialog
template <typename _Type>
struct is_navigable_file_dialog : std::conjunction<
    is_file_dialog<_Type>,
    internal::has_back_stack_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_navigable_file_dialog_v =
    is_navigable_file_dialog<_Type>::value;

}   // namespace file_dialog_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_FILE_DIALOG_