/*******************************************************************************
* uxoxo [component]                                   file_open_save_dialog.hpp
*
* File open/save dialog:
*   A unified open/save/directory-selection dialog built atop
* file_dialog.  The underlying navigation and listing machinery is
* identical across all three modes — what differs is the semantics
* of "accept":
*
*     open_file           validate that a single existing file is selected
*     open_multiple       validate that >= 1 existing files are selected
*     save_file           compose a target path from current_path +
*                          filename_text + default_extension, optionally
*                          prompting on overwrite
*     select_directory    validate that the current path (or selection)
*                          is a directory
*     select_directories  same, but allowing multiple
*
*   The dialog itself does not touch the filesystem.  Existence,
* writability, and overwrite checks are delegated to caller-supplied
* predicates; the platform adapter is expected to wire them to
* std::filesystem or a project-specific file_tree backend.
*
*   Because the accept lifecycle is role-driven by the inherited
* dialog layer, this module slots naturally into dlg_activate_button:
* a button with role `open` or `save` routes through dlg_accept,
* which this module overrides via fos_try_accept for validation.
*
* Contents:
*   1.  Feature flags (file_open_save_feat)
*   2.  Enum (file_dialog_mode)
*   3.  file_open_save_dialog struct
*   4.  Free functions
*   5.  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/dialog/file/
*                 file_open_save_dialog.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_FILE_OPEN_SAVE_DIALOG_
#define  UXOXO_COMPONENT_FILE_OPEN_SAVE_DIALOG_ 1

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
#include "./file_dialog.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  FILE OPEN/SAVE FEATURE FLAGS
// ===============================================================================

enum file_open_save_feat : unsigned
{
    fos_none              = 0,
    fos_overwrite_prompt  = 1u << 0,  // prompt when saving over existing file
    fos_auto_extension    = 1u << 1,  // append default_extension on save
    fos_require_existing  = 1u << 2,  // open mode: reject non-existent path
    fos_create_on_save    = 1u << 3,  // save mode: create parent dirs if needed
    fos_confirm_overwrite = fos_overwrite_prompt,   // alias
    fos_standard_open     = fos_require_existing,
    fos_standard_save     = fos_overwrite_prompt  | 
                            fos_auto_extension,
    fos_all               = fos_overwrite_prompt  |
                            fos_auto_extension    |
                            fos_require_existing  | 
                            fos_create_on_save
};

constexpr unsigned operator|(
    file_open_save_feat _a,
    file_open_save_feat _b) noexcept
{
    return ( static_cast<unsigned>(_a) |
             static_cast<unsigned>(_b) )
}

constexpr bool has_fos(
    unsigned             _f,                   
    file_open_save_feat  _bit
) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


// ===============================================================================
//  2.  FILE DIALOG MODE
// ===============================================================================

// file_dialog_mode
//   enum: what the dialog's "accept" action means semantically.  The
// adapter typically labels the accept button differently per mode
// (Open / Save / Select / Choose Folder) and routes accept validation
// accordingly.
enum class file_dialog_mode : std::uint8_t
{
    open_file,
    open_multiple,
    save_file,
    select_directory,
    select_directories
};


// ===============================================================================
//  3.  FILE OPEN/SAVE DIALOG
// ===============================================================================

// file_open_save_dialog
//   struct: file_dialog specialization carrying open/save lifecycle
// semantics.  Adds a mode selector, proposed filename for save mode,
// default extension, and three mode-specific accept callbacks plus a
// caller-installed overwrite predicate.
//
//   All filesystem probing (existence, directory-check, writability)
// is delegated via the `path_exists` / `is_directory` / `should_overwrite`
// predicates, which the adapter installs.  This keeps the dialog free
// of any <filesystem> dependency and lets it run atop a virtual /
// sandboxed / network / archive file backend equally well.

template <typename _Entry   = file_entry,
          unsigned _DlgFeat = df_titled | df_closable | df_resizable
                            | df_movable | df_sized,
          unsigned _FdFeat  = fdf_standard | fdf_filename_input,
          unsigned _FosFeat = fos_standard_save>
struct file_open_save_dialog
    : public file_dialog<_Entry, _DlgFeat, _FdFeat>
{
    using base_type      = file_dialog<_Entry, _DlgFeat, _FdFeat>;
    using path_list      = std::vector<std::string>;
    using path_predicate = std::function<bool(const std::string&)>;
    using overwrite_fn   = std::function<bool(const std::string&)>;
    using open_accept_fn = std::function<void(const path_list&)>;
    using save_accept_fn = std::function<void(const std::string&)>;
    using dir_accept_fn  = std::function<void(const path_list&)>;

    static constexpr unsigned open_save_features = _FosFeat;
    static constexpr bool has_overwrite_prompt   =
        has_fos(_FosFeat, fos_overwrite_prompt);
    static constexpr bool has_auto_extension     =
        has_fos(_FosFeat, fos_auto_extension);
    static constexpr bool has_require_existing   =
        has_fos(_FosFeat, fos_require_existing);
    static constexpr bool has_create_on_save     =
        has_fos(_FosFeat, fos_create_on_save);

    // -- mode ---------------------------------------------------------
    file_dialog_mode  mode = file_dialog_mode::open_file;

    // -- save-side state ----------------------------------------------
    std::string  default_extension;      // ".blend", "" for none
    std::string  proposed_filename;      // pre-populated name for save

    // -- filesystem predicates (installed by adapter) -----------------
    //   path_exists     returns true if the path resolves to any entry
    //   is_directory    returns true if path is a directory
    //   should_overwrite called only when has_overwrite_prompt and the
    //                   save target exists; returns true to proceed
    path_predicate  path_exists;
    path_predicate  is_directory;
    overwrite_fn    should_overwrite;

    // -- accept callbacks (per-mode) ----------------------------------
    open_accept_fn  on_open_accepted;
    save_accept_fn  on_save_accepted;
    dir_accept_fn   on_directory_accepted;

    // -- last-error slot (set by fos_try_accept on rejection) ---------
    std::string  last_error;

    // -- construction -------------------------------------------------
    file_open_save_dialog() = default;
};


// ===============================================================================
//  4.  FREE FUNCTIONS
// ===============================================================================

// fos_set_mode
template <typename _E, unsigned _DF, unsigned _FF, unsigned _OS>
void
fos_set_mode(
    file_open_save_dialog<_E, _DF, _FF, _OS>&  _fd,
    file_dialog_mode                           _m
)
{
    _fd.mode = _m;

    // multi-select flag tracks the mode when it's available
    if constexpr (has_fdf(_FF, fdf_multi_select))
    {
        _fd.select_mode =
            ( (_m == file_dialog_mode::open_multiple)       ? file_select_mode::multiple_files
            : (_m == file_dialog_mode::select_directory)    ? file_select_mode::single_directory
            : (_m == file_dialog_mode::select_directories)  ? file_select_mode::multiple_directories
            : (_m == file_dialog_mode::save_file)           ? file_select_mode::single_file
            :                                                 file_select_mode::single_file );
    }

    return;
}

// fos_set_proposed_filename
template <typename _E, unsigned _DF, unsigned _FF, unsigned _OS>
void
fos_set_proposed_filename(
    file_open_save_dialog<_E, _DF, _FF, _OS>&  _fd,
    std::string                                _name
)
{
    _fd.proposed_filename = _name;

    // mirror into the filename input if that mixin is active
    if constexpr (has_fdf(_FF, fdf_filename_input))
    {
        _fd.filename_text = std::move(_name);
    }

    return;
}

// fos_set_default_extension
template <typename _E, unsigned _DF, unsigned _FF, unsigned _OS>
void
fos_set_default_extension(
    file_open_save_dialog<_E, _DF, _FF, _OS>&  _fd,
    std::string                                _ext
)
{
    _fd.default_extension = std::move(_ext);

    return;
}


// -- internal helpers -----------------------------------------------------

namespace fos_detail {

    // joins a directory and a leaf with a single separator.  Uses
    // whichever separator already appears in `_dir`, defaulting to '/'.
    inline std::string
    join_path(
        const std::string& _dir,
        const std::string& _leaf
    )
    {
        if (_dir.empty())
        {
            return _leaf;
        }

        const char sep =
            (_dir.find('\\') != std::string::npos) ? '\\' : '/';

        const char last = _dir.back();

        if ( (last == '/') || (last == '\\') )
        {
            return _dir + _leaf;
        }

        std::string out;
        out.reserve(_dir.size() + 1 + _leaf.size());
        out.append(_dir);
        out.push_back(sep);
        out.append(_leaf);

        return out;
    }

    // returns true if `_name` already ends in `_ext` (case-sensitive).
    inline bool
    has_extension(
        const std::string& _name,
        const std::string& _ext
    )
    {
        if ( (_ext.empty()) ||
             (_name.size() < _ext.size()) )
        {
            return false;
        }

        return (_name.compare(_name.size() - _ext.size(),
                              _ext.size(),
                              _ext) == 0);
    }

}   // namespace fos_detail


// fos_effective_save_path
//   computes the path that save_file mode would commit to: joins
// current_path + filename_text (or proposed_filename) and appends
// default_extension when fos_auto_extension is set and the name
// doesn't already carry it.
template <typename _E, unsigned _DF, unsigned _FF, unsigned _OS>
std::string
fos_effective_save_path(
    const file_open_save_dialog<_E, _DF, _FF, _OS>& _fd
)
{
    std::string name;

    if constexpr (has_fdf(_FF, fdf_filename_input))
    {
        name = _fd.filename_text;
    }

    if (name.empty())
    {
        name = _fd.proposed_filename;
    }

    if (name.empty())
    {
        return std::string();
    }

    if constexpr (has_fos(_OS, fos_auto_extension))
    {
        if ( (!_fd.default_extension.empty()) &&
             (!fos_detail::has_extension(name, _fd.default_extension)) )
        {
            name += _fd.default_extension;
        }
    }

    return fos_detail::join_path(_fd.current_path, name);
}

// fos_effective_open_paths
//   returns the paths that an open-mode accept would commit to.
// Draws from selected_indices + entries; an empty result is a
// rejection signal to fos_try_accept.
template <typename _E, unsigned _DF, unsigned _FF, unsigned _OS>
std::vector<std::string>
fos_effective_open_paths(
    const file_open_save_dialog<_E, _DF, _FF, _OS>& _fd
)
{
    std::vector<std::string> out;
    out.reserve(_fd.selected_indices.size());

    for (std::size_t i : _fd.selected_indices)
    {
        if (i < _fd.entries.size())
        {
            out.push_back(_fd.entries[i].path);
        }
    }

    return out;
}

// fos_try_accept
//   validates the current state against `mode` semantics, fires the
// appropriate accept callback on success, and resolves the dialog
// via dlg_accept.  Returns true on success; on failure writes a
// message to `last_error` and leaves the dialog open.
//
//   Typical wiring:  install a button with role `open` or `save`,
// and have the adapter route its dlg_activate_button invocation
// through fos_try_accept first.  On success the dialog resolves;
// on failure the adapter surfaces last_error to the user.
template <typename _E, unsigned _DF, unsigned _FF, unsigned _OS>
bool
fos_try_accept(
    file_open_save_dialog<_E, _DF, _FF, _OS>& _fd
)
{
    _fd.last_error.clear();

    switch (_fd.mode)
    {
        case file_dialog_mode::open_file:
        case file_dialog_mode::open_multiple:
        {
            auto paths = fos_effective_open_paths(_fd);

            if (paths.empty())
            {
                _fd.last_error = "No file selected.";

                return false;
            }

            if ( (_fd.mode == file_dialog_mode::open_file) &&
                 (paths.size() > 1) )
            {
                _fd.last_error = "Please select a single file.";

                return false;
            }

            if constexpr (has_fos(_OS, fos_require_existing))
            {
                if (_fd.path_exists)
                {
                    for (const auto& p : paths)
                    {
                        if (!_fd.path_exists(p))
                        {
                            _fd.last_error =
                                std::string("Path does not exist: ") + p;

                            return false;
                        }
                    }
                }
            }

            if (_fd.on_open_accepted)
            {
                _fd.on_open_accepted(paths);
            }

            dlg_accept(static_cast<dialog<_DF>&>(_fd), nullptr);

            return true;
        }

        case file_dialog_mode::save_file:
        {
            std::string target = fos_effective_save_path(_fd);

            if (target.empty())
            {
                _fd.last_error = "Please enter a filename.";

                return false;
            }

            if constexpr (has_fos(_OS, fos_overwrite_prompt))
            {
                if ( (_fd.path_exists)       &&
                     (_fd.path_exists(target)) )
                {
                    if (_fd.should_overwrite)
                    {
                        if (!_fd.should_overwrite(target))
                        {
                            _fd.last_error = "Overwrite declined.";

                            return false;
                        }
                    }
                }
            }

            if (_fd.on_save_accepted)
            {
                _fd.on_save_accepted(target);
            }

            dlg_accept(static_cast<dialog<_DF>&>(_fd), nullptr);

            return true;
        }

        case file_dialog_mode::select_directory:
        case file_dialog_mode::select_directories:
        {
            std::vector<std::string> paths;

            // directory-pick modes may commit either the selected
            // entries (when any are selected) or the current path
            // (the classic "pick this folder" pattern).
            if (!_fd.selected_indices.empty())
            {
                paths = fos_effective_open_paths(_fd);
            }
            else if (!_fd.current_path.empty())
            {
                paths.push_back(_fd.current_path);
            }
            else
            {
                _fd.last_error = "No directory selected.";

                return false;
            }

            if ( (_fd.mode == file_dialog_mode::select_directory) &&
                 (paths.size() > 1) )
            {
                _fd.last_error = "Please select a single directory.";

                return false;
            }

            if (_fd.is_directory)
            {
                for (const auto& p : paths)
                {
                    if (!_fd.is_directory(p))
                    {
                        _fd.last_error =
                            std::string("Not a directory: ") + p;

                        return false;
                    }
                }
            }

            if (_fd.on_directory_accepted)
            {
                _fd.on_directory_accepted(paths);
            }

            dlg_accept(static_cast<dialog<_DF>&>(_fd), nullptr);

            return true;
        }
    }

    return false;
}

// fos_cancel
//   convenience wrapper: resolves the dialog as cancelled.
template <typename _E, unsigned _DF, unsigned _FF, unsigned _OS>
void
fos_cancel(
    file_open_save_dialog<_E, _DF, _FF, _OS>& _fd
)
{
    dlg_cancel(static_cast<dialog<_DF>&>(_fd), nullptr);

    return;
}

// fos_reopen
//   convenience wrapper: resets the dialog and opens it.  Clears
// last_error, selection, and pending result.
template <typename _E, unsigned _DF, unsigned _FF, unsigned _OS>
void
fos_reopen(
    file_open_save_dialog<_E, _DF, _FF, _OS>& _fd
)
{
    _fd.last_error.clear();
    _fd.selected_indices.clear();

    dlg_open(static_cast<dialog<_DF>&>(_fd));

    fd_refresh(static_cast<file_dialog<_E, _DF, _FF>&>(_fd));

    return;
}


// ===============================================================================
//  5.  TRAITS
// ===============================================================================

namespace file_open_save_traits {
namespace detail {

    template <typename, typename = void>
    struct has_mode_member : std::false_type {};
    template <typename _Type>
    struct has_mode_member<_Type, std::void_t<
        decltype(std::declval<_Type>().mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_default_extension_member : std::false_type {};
    template <typename _Type>
    struct has_default_extension_member<_Type, std::void_t<
        decltype(std::declval<_Type>().default_extension)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_proposed_filename_member : std::false_type {};
    template <typename _Type>
    struct has_proposed_filename_member<_Type, std::void_t<
        decltype(std::declval<_Type>().proposed_filename)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_on_save_accepted_member : std::false_type {};
    template <typename _Type>
    struct has_on_save_accepted_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_save_accepted)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_on_open_accepted_member : std::false_type {};
    template <typename _Type>
    struct has_on_open_accepted_member<_Type, std::void_t<
        decltype(std::declval<_Type>().on_open_accepted)
    >> : std::true_type {};

}   // namespace detail

template <typename _Type>
inline constexpr bool has_mode_v =
    detail::has_mode_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_default_extension_v =
    detail::has_default_extension_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_proposed_filename_v =
    detail::has_proposed_filename_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_on_save_accepted_v =
    detail::has_on_save_accepted_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_on_open_accepted_v =
    detail::has_on_open_accepted_member<_Type>::value;

// is_file_open_save_dialog
//   type trait: satisfies file_dialog AND carries a mode + either
// an open or save accept callback.
template <typename _Type>
struct is_file_open_save_dialog : std::conjunction<
    file_dialog_traits::is_file_dialog<_Type>,
    detail::has_mode_member<_Type>,
    std::disjunction<
        detail::has_on_save_accepted_member<_Type>,
        detail::has_on_open_accepted_member<_Type>>
>
{};

template <typename _Type>
inline constexpr bool is_file_open_save_dialog_v =
    is_file_open_save_dialog<_Type>::value;

}   // namespace file_open_save_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_FILE_OPEN_SAVE_DIALOG_