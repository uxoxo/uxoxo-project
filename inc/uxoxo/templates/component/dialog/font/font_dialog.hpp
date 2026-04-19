/*******************************************************************************
* uxoxo [component]                                             font_dialog.hpp
*
* Font dialog:
*   A framework-agnostic font chooser template.  Extends dialog<> with
* typography selection state — family, style, weight, size, effects,
* preview, and filtering — without prescribing HOW fonts are rendered,
* enumerated, or matched.
*
*   The font descriptor itself is supplied by `font.hpp`: the default
* `_FontDesc` is `font::font<font::ff_gui_standard>`, but callers may
* substitute any font-like type — the uxoxo `font<_Feat>` at a
* different feature level, or a backend-specific struct (LOGFONT,
* FcPattern wrapper, NSFont ref, FT_Face + index, game-engine ref).
* Structural detection via `font::traits` determines which dialog
* panes can edit the selection at all.
*
*   The dialog holds BOTH a catalogue of available font families and a
* current selection.  The catalogue is populated by the adapter via
* `on_enumerate`, which runs whenever filters change or refresh is
* requested; the dialog never enumerates the system itself.  Preview
* rendering is likewise delegated — the dialog just carries a sample
* string and a flag indicating whether the preview is stale.
*
*   Feature flags gate optional dialog panes (preview, sample editor,
* search, recent, favourites, filter controls) via EBO mixins.  Font
* attribute panes (effects, color, background, script hint) are gated
* by two conditions at the adapter layer: the corresponding `ftdf_*`
* UI-visibility flag AND `font::traits::has_font_*_v<_FontDesc>`.
*
*   Because the accept lifecycle is role-driven by the inherited
* dialog layer, this module slots naturally into dlg_activate_button:
* a button with role `ok` or `apply` routes through dlg_accept, which
* this module complements via ftd_try_accept for validation (selection
* must be non-empty, size within bounds).
*
* Contents:
*   1.  Feature flags (font_dialog_feat)
*   2.  Dialog-local enums (font_filter_scalable)
*   3.  Dialog-local support structs (font_script_filter)
*   4.  EBO mixins
*   5.  font_dialog struct
*   6.  Free functions
*   7.  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/templates/component/dialog/font_dialog.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_FONT_DIALOG_
#define  UXOXO_COMPONENT_FONT_DIALOG_ 1

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
#include "../../font/font.hpp"
#include "../../font/font_traits.hpp"
#include "./dialog.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1.  FONT DIALOG FEATURE FLAGS
// ===============================================================================
//   These flags gate DIALOG panes, not font attributes.  A pane flag
// only means "expose this UI in the dialog"; whether the underlying
// selection can actually carry the corresponding attribute is a
// separate question answered by `font::traits::has_font_*_v<_FontDesc>`
// at the adapter layer.

enum font_dialog_feat : unsigned
{
    ftdf_none            = 0,
    ftdf_size_list       = 1u << 0,   // discrete size list (bitmap / preset)
    ftdf_size_slider     = 1u << 1,   // continuous size slider
    ftdf_style_list      = 1u << 2,   // style / weight picker pane
    ftdf_weight_numeric  = 1u << 3,   // numeric weight (100-1000) field
    ftdf_effects_pane    = 1u << 4,   // underline / strike / etc. pane visibility
    ftdf_color_pane      = 1u << 5,   // foreground color pane visibility
    ftdf_background      = 1u << 6,   // background color pane visibility
    ftdf_preview         = 1u << 7,   // live preview pane
    ftdf_sample_edit     = 1u << 8,   // user-editable sample text
    ftdf_script_filter   = 1u << 9,   // writing-system / script filter
    ftdf_monospace_only  = 1u << 10,  // hard monospace-only toggle
    ftdf_scalable_only   = 1u << 11,  // hard scalable-only toggle
    ftdf_search          = 1u << 12,  // search-by-name box
    ftdf_recent          = 1u << 13,  // recently-chosen fonts list
    ftdf_favourites      = 1u << 14,  // user-starred fonts

    ftdf_standard        = ftdf_size_list    | ftdf_style_list
                         | ftdf_preview,
    ftdf_rich            = ftdf_size_list    | ftdf_style_list
                         | ftdf_weight_numeric | ftdf_effects_pane
                         | ftdf_color_pane   | ftdf_preview
                         | ftdf_sample_edit  | ftdf_search,
    ftdf_all             = ftdf_size_list    | ftdf_size_slider
                         | ftdf_style_list   | ftdf_weight_numeric
                         | ftdf_effects_pane | ftdf_color_pane
                         | ftdf_background   | ftdf_preview
                         | ftdf_sample_edit  | ftdf_script_filter
                         | ftdf_monospace_only | ftdf_scalable_only
                         | ftdf_search       | ftdf_recent
                         | ftdf_favourites
};

constexpr unsigned operator|(font_dialog_feat _a,
                             font_dialog_feat _b) noexcept
{
    return static_cast<unsigned>(_a) | static_cast<unsigned>(_b);
}

constexpr bool has_ftdf(unsigned          _f,
                        font_dialog_feat  _bit) noexcept
{
    return (_f & static_cast<unsigned>(_bit)) != 0;
}


// ===============================================================================
//  2.  DIALOG-LOCAL ENUMS
// ===============================================================================

// font_filter_scalable
//   enum: three-state outline/bitmap filter.  Distinct from the hard
// ftdf_scalable_only feature flag: that flag toggles whether the
// filter UI is exposed at all; this enum carries the user's choice
// when it is.  Dialog-local because it's a catalogue filter, not a
// property of a selected font.
enum class font_filter_scalable : std::uint8_t
{
    any,
    scalable_only,
    bitmap_only
};


// ===============================================================================
//  3.  DIALOG-LOCAL SUPPORT STRUCTS
// ===============================================================================

// font_script_filter
//   struct: carried when ftdf_script_filter is enabled; constrains
// the enumerated family list to those covering a given script/
// language.  Dialog-local because it's a catalogue filter — the
// per-font script hint (when a font type carries it) lives on the
// font itself via font::traits::has_font_script_hint.
struct font_script_filter
{
    std::string   script_tag;         // OpenType tag: "latn", "cyrl", ...
    std::string   language_tag;       // BCP-47: "en", "zh-Hans", ...
    std::string   sample_codepoints;  // UTF-8 glyphs that must render
};


// ===============================================================================
//  4.  EBO MIXINS
// ===============================================================================
//   Only panes whose STATE is dialog-local need a mixin.  Font
// attributes (underline, color, background, script hint) are edited
// directly on `selection`; their ftdf_* flags govern pane visibility
// but carry no storage.

namespace font_dialog_mixin {

    // -- size list ----------------------------------------------------
    template <bool _Enable>
    struct size_list_data
    {};

    template <>
    struct size_list_data<true>
    {
        std::vector<float>  size_options =
            { 6, 7, 8, 9, 10, 11, 12, 14, 16, 18,
              20, 22, 24, 26, 28, 36, 48, 72 };
    };

    // -- size slider --------------------------------------------------
    template <bool _Enable>
    struct size_slider_data
    {};

    template <>
    struct size_slider_data<true>
    {
        float  size_min  = 4.0f;
        float  size_max  = 256.0f;
        float  size_step = 0.5f;
    };

    // -- style list ---------------------------------------------------
    template <bool _Enable>
    struct style_list_data
    {};

    template <>
    struct style_list_data<true>
    {
        std::vector<std::string>  style_options;   // per-family, refreshed on family change
        std::size_t               selected_style_index = 0;
    };

    // -- numeric weight -----------------------------------------------
    template <bool _Enable>
    struct weight_numeric_data
    {};

    template <>
    struct weight_numeric_data<true>
    {
        std::uint16_t  weight_min  = 100;
        std::uint16_t  weight_max  = 1000;
        std::uint16_t  weight_step = 100;
    };

    // -- preview ------------------------------------------------------
    template <bool _Enable>
    struct preview_data
    {};

    template <>
    struct preview_data<true>
    {
        bool  preview_visible = true;
        bool  preview_stale   = true;   // set whenever selection changes
    };

    // -- sample edit --------------------------------------------------
    template <bool _Enable>
    struct sample_edit_data
    {};

    template <>
    struct sample_edit_data<true>
    {
        std::string  sample_text =
            "The quick brown fox jumps over the lazy dog.";
    };

    // -- script filter ------------------------------------------------
    template <bool _Enable>
    struct script_filter_data
    {};

    template <>
    struct script_filter_data<true>
    {
        font_script_filter  script_filter;
    };

    // -- monospace-only filter ---------------------------------------
    template <bool _Enable>
    struct monospace_filter_data
    {};

    template <>
    struct monospace_filter_data<true>
    {
        bool  monospace_only = false;
    };

    // -- scalable filter ---------------------------------------------
    template <bool _Enable>
    struct scalable_filter_data
    {};

    template <>
    struct scalable_filter_data<true>
    {
        font_filter_scalable  scalable_filter = font_filter_scalable::any;
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

    // -- recent -------------------------------------------------------
    template <bool _Enable, typename _FontDesc>
    struct recent_data
    {};

    template <typename _FontDesc>
    struct recent_data<true, _FontDesc>
    {
        std::vector<_FontDesc>  recent;
        std::size_t             recent_max = 12;
    };

    // -- favourites ---------------------------------------------------
    template <bool _Enable, typename _FontDesc>
    struct favourites_data
    {};

    template <typename _FontDesc>
    struct favourites_data<true, _FontDesc>
    {
        std::vector<_FontDesc>  favourites;
    };

}   // namespace font_dialog_mixin


// ===============================================================================
//  5.  FONT DIALOG
// ===============================================================================

// font_dialog
//   struct: framework-agnostic font chooser.  Inherits the button bar,
// result state, and geometry of dialog<_DlgFeat>, and adds typography
// selection state gated by _FontFeat.
//
//   _FontDesc   font descriptor type.  Defaults to a fully-featured
//               `font::font<font::ff_gui_standard>`.  Must at minimum
//               satisfy `font::traits::is_font_like_v`.
//   _DlgFeat    dialog-layer feature bitfield.
//   _FontFeat   font-dialog-layer feature bitfield.

template <typename _FontDesc = font::font<font::ff_gui_standard>,
          unsigned _DlgFeat  = df_titled | df_closable | df_resizable
                             | df_movable | df_sized,
          unsigned _FontFeat = ftdf_standard>
struct font_dialog
    : public dialog<_DlgFeat>
    , font_dialog_mixin::size_list_data        <has_ftdf(_FontFeat, ftdf_size_list)>
    , font_dialog_mixin::size_slider_data      <has_ftdf(_FontFeat, ftdf_size_slider)>
    , font_dialog_mixin::style_list_data       <has_ftdf(_FontFeat, ftdf_style_list)>
    , font_dialog_mixin::weight_numeric_data   <has_ftdf(_FontFeat, ftdf_weight_numeric)>
    , font_dialog_mixin::preview_data          <has_ftdf(_FontFeat, ftdf_preview)>
    , font_dialog_mixin::sample_edit_data      <has_ftdf(_FontFeat, ftdf_sample_edit)>
    , font_dialog_mixin::script_filter_data    <has_ftdf(_FontFeat, ftdf_script_filter)>
    , font_dialog_mixin::monospace_filter_data <has_ftdf(_FontFeat, ftdf_monospace_only)>
    , font_dialog_mixin::scalable_filter_data  <has_ftdf(_FontFeat, ftdf_scalable_only)>
    , font_dialog_mixin::search_data           <has_ftdf(_FontFeat, ftdf_search)>
    , font_dialog_mixin::recent_data           <has_ftdf(_FontFeat, ftdf_recent), _FontDesc>
    , font_dialog_mixin::favourites_data       <has_ftdf(_FontFeat, ftdf_favourites), _FontDesc>
{
    // _FontDesc must satisfy the minimum font-like interface.
    static_assert(font::traits::is_font_like_v<_FontDesc>,
                  "font_dialog: _FontDesc must satisfy "
                  "font::traits::is_font_like "
                  "(expose family, size, weight, slant).");

    using descriptor_type   = _FontDesc;
    using family_list       = std::vector<font::font_family_info>;
    using family_index_list = std::vector<std::size_t>;
    using enumerate_fn      = std::function<void(family_list&)>;
    using family_change_fn  = std::function<void(const font::font_family_info&)>;
    using select_change_fn  = std::function<void(const _FontDesc&)>;
    using accept_fn         = std::function<void(const _FontDesc&)>;

    static constexpr unsigned dialog_features = _DlgFeat;
    static constexpr unsigned font_features   = _FontFeat;

    static constexpr bool has_size_list       = has_ftdf(_FontFeat, ftdf_size_list);
    static constexpr bool has_size_slider     = has_ftdf(_FontFeat, ftdf_size_slider);
    static constexpr bool has_style_list      = has_ftdf(_FontFeat, ftdf_style_list);
    static constexpr bool has_weight_numeric  = has_ftdf(_FontFeat, ftdf_weight_numeric);
    static constexpr bool has_effects_pane    = has_ftdf(_FontFeat, ftdf_effects_pane);
    static constexpr bool has_color_pane      = has_ftdf(_FontFeat, ftdf_color_pane);
    static constexpr bool has_background_pane = has_ftdf(_FontFeat, ftdf_background);
    static constexpr bool has_preview         = has_ftdf(_FontFeat, ftdf_preview);
    static constexpr bool has_sample_edit     = has_ftdf(_FontFeat, ftdf_sample_edit);
    static constexpr bool has_script_filter   = has_ftdf(_FontFeat, ftdf_script_filter);
    static constexpr bool has_monospace_only  = has_ftdf(_FontFeat, ftdf_monospace_only);
    static constexpr bool has_scalable_only   = has_ftdf(_FontFeat, ftdf_scalable_only);
    static constexpr bool has_search          = has_ftdf(_FontFeat, ftdf_search);
    static constexpr bool has_recent          = has_ftdf(_FontFeat, ftdf_recent);
    static constexpr bool has_favourites      = has_ftdf(_FontFeat, ftdf_favourites);

    // Font-attribute editability — composes pane-visibility flag with
    // a structural check on the descriptor type.  Adapters can use
    // these directly to decide whether to render a pane's controls.
    static constexpr bool can_edit_underline =
        has_effects_pane && font::traits::has_font_underline_v<_FontDesc>;
    static constexpr bool can_edit_strikethrough =
        has_effects_pane && font::traits::has_font_strikethrough_v<_FontDesc>;
    static constexpr bool can_edit_overline =
        has_effects_pane && font::traits::has_font_overline_v<_FontDesc>;
    static constexpr bool can_edit_small_caps =
        has_effects_pane && font::traits::has_font_small_caps_v<_FontDesc>;
    static constexpr bool can_edit_color =
        has_color_pane && font::traits::has_font_color_v<_FontDesc>;
    static constexpr bool can_edit_background =
        has_background_pane && font::traits::has_font_background_v<_FontDesc>;

    // -- catalogue ----------------------------------------------------
    family_list        families;
    std::size_t        selected_family_index = 0;   // into `families`; size() => none

    // -- current selection --------------------------------------------
    _FontDesc          selection;

    // -- callbacks ----------------------------------------------------
    //   on_enumerate      adapter populates `families`
    //   on_family_change  user picked a different family (before selection is rebuilt)
    //   on_select_change  fired whenever `selection` changes
    //   on_accept_desc    fired on successful accept
    enumerate_fn       on_enumerate;
    family_change_fn   on_family_change;
    select_change_fn   on_select_change;
    accept_fn          on_accept_desc;

    // -- last-error slot (set by ftd_try_accept on rejection) --------
    std::string        last_error;

    // -- construction -------------------------------------------------
    font_dialog() = default;
};


// ===============================================================================
//  6.  FREE FUNCTIONS
// ===============================================================================

// ---------------------------------------------------------------------
// internal helpers
// ---------------------------------------------------------------------
namespace font_dialog_detail {

    // mark_preview_stale
    //   helper: sets the preview_stale flag when the preview mixin is
    // enabled.  No-op otherwise.  Consolidates a pattern that the
    // setters used to repeat inline.
    template <typename _FD>
    void
    mark_preview_stale(
        _FD& _fd
    ) noexcept
    {
        if constexpr (std::remove_reference_t<decltype(_fd)>::has_preview)
        {
            _fd.preview_stale = true;
        }

        return;
    }

}   // namespace font_dialog_detail


// ftd_refresh
//   asks the adapter to repopulate `families` via on_enumerate.
// Does NOT clear the current selection; selection is clamped against
// the new catalogue by ftd_reconcile_selection afterwards.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_refresh(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    if (_fd.on_enumerate)
    {
        _fd.families.clear();
        _fd.on_enumerate(_fd.families);
    }

    return;
}

// ftd_find_family
//   returns the index of the family matching `_family` (case-sensitive),
// or families.size() if not found.
template <typename _D, unsigned _DF, unsigned _FF>
std::size_t
ftd_find_family(
    const font_dialog<_D, _DF, _FF>&  _fd,
    const std::string&                _family
)
{
    for (std::size_t i = 0; i < _fd.families.size(); ++i)
    {
        if (_fd.families[i].family == _family)
        {
            return i;
        }
    }

    return _fd.families.size();
}

// ftd_reconcile_selection
//   clamps selected_family_index against the catalogue.  If the
// selection's family is present, points selected_family_index at it;
// otherwise sets it to families.size() (= "none").  Also refreshes
// the style_options list when that mixin is enabled.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_reconcile_selection(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    _fd.selected_family_index = ftd_find_family(_fd, _fd.selection.family);

    if constexpr (has_ftdf(_FF, ftdf_style_list))
    {
        if (_fd.selected_family_index < _fd.families.size())
        {
            _fd.style_options =
                _fd.families[_fd.selected_family_index].styles;

            // try to preserve selected style by name
            _fd.selected_style_index = 0;

            for (std::size_t i = 0; i < _fd.style_options.size(); ++i)
            {
                if (_fd.style_options[i] == _fd.selection.style_name)
                {
                    _fd.selected_style_index = i;
                    break;
                }
            }
        }
        else
        {
            _fd.style_options.clear();
            _fd.selected_style_index = 0;
        }
    }

    font_dialog_internal::mark_preview_stale(_fd);

    return;
}

// ftd_set_family
//   switches the current family by name, repopulates the style pane,
// rebuilds `selection.family`, and fires on_family_change /
// on_select_change.  Unknown families are accepted (so the dialog
// can display a requested font that isn't enumerated yet); they
// leave selected_family_index == families.size().
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_family(
    font_dialog<_D, _DF, _FF>&  _fd,
    std::string                 _family
)
{
    _fd.selection.family      = std::move(_family);
    _fd.selected_family_index = ftd_find_family(_fd, _fd.selection.family);

    if (_fd.selected_family_index < _fd.families.size())
    {
        const auto& info = _fd.families[_fd.selected_family_index];

        // if the current style isn't offered by this family, fall back
        // to the first available (typical host behaviour).
        if constexpr (has_ftdf(_FF, ftdf_style_list))
        {
            _fd.style_options        = info.styles;
            _fd.selected_style_index = 0;

            bool style_found = false;

            for (std::size_t i = 0; i < info.styles.size(); ++i)
            {
                if (info.styles[i] == _fd.selection.style_name)
                {
                    _fd.selected_style_index = i;
                    style_found              = true;
                    break;
                }
            }

            if ( (!style_found) && (!info.styles.empty()) )
            {
                _fd.selection.style_name = info.styles.front();
            }
        }

        if (_fd.on_family_change)
        {
            _fd.on_family_change(info);
        }
    }
    else
    {
        if constexpr (has_ftdf(_FF, ftdf_style_list))
        {
            _fd.style_options.clear();
            _fd.selected_style_index = 0;
        }
    }

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_style
//   sets the style by name.  When the style_list mixin is enabled,
// also updates selected_style_index.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_style(
    font_dialog<_D, _DF, _FF>&  _fd,
    std::string                 _style
)
{
    _fd.selection.style_name = std::move(_style);

    if constexpr (has_ftdf(_FF, ftdf_style_list))
    {
        for (std::size_t i = 0; i < _fd.style_options.size(); ++i)
        {
            if (_fd.style_options[i] == _fd.selection.style_name)
            {
                _fd.selected_style_index = i;
                break;
            }
        }
    }

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_size
//   sets the size.  Clamps to the slider bounds when ftdf_size_slider
// is enabled.  The unit is whatever the descriptor's size_unit
// already is; to change unit, set it on the descriptor directly.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_size(
    font_dialog<_D, _DF, _FF>&  _fd,
    float                       _size
)
{
    if constexpr (has_ftdf(_FF, ftdf_size_slider))
    {
        if (_size < _fd.size_min)
        {
            _size = _fd.size_min;
        }

        if (_size > _fd.size_max)
        {
            _size = _fd.size_max;
        }
    }

    _fd.selection.size = _size;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_weight
//   symbolic weight setter; clears any override from the numeric
// weight field so that the symbolic value takes effect.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_weight(
    font_dialog<_D, _DF, _FF>&  _fd,
    font::font_weight           _w
)
{
    _fd.selection.weight         = _w;
    _fd.selection.weight_numeric = 0;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_weight_numeric
//   numeric weight setter.  A nonzero value overrides the symbolic
// weight field; passing 0 clears the override.  Clamped to the
// configured bounds when ftdf_weight_numeric is enabled.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_weight_numeric(
    font_dialog<_D, _DF, _FF>&  _fd,
    std::uint16_t               _w
)
{
    if constexpr (has_ftdf(_FF, ftdf_weight_numeric))
    {
        if (_w != 0)
        {
            if (_w < _fd.weight_min)
            {
                _w = _fd.weight_min;
            }

            if (_w > _fd.weight_max)
            {
                _w = _fd.weight_max;
            }
        }
    }

    _fd.selection.weight_numeric = _w;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_slant
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_slant(
    font_dialog<_D, _DF, _FF>&  _fd,
    font::font_slant            _s
)
{
    _fd.selection.slant = _s;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_selection
//   replaces the entire selection descriptor.  Reconciles the
// catalogue index and fires on_select_change.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_selection(
    font_dialog<_D, _DF, _FF>&  _fd,
    _D                          _desc
)
{
    _fd.selection = std::move(_desc);

    ftd_reconcile_selection(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_clear_selection
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_clear_selection(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    _fd.selection             = _D();
    _fd.selected_family_index = _fd.families.size();

    if constexpr (has_ftdf(_FF, ftdf_style_list))
    {
        _fd.style_options.clear();
        _fd.selected_style_index = 0;
    }

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}


// ---------------------------------------------------------------------
// effect setters (routed directly to `selection`)
// ---------------------------------------------------------------------
//   Each setter is static_assert-guarded: it requires BOTH the
// ftdf_effects_pane dialog flag AND that the descriptor type itself
// exposes the attribute (via font::traits).  Callers who want to
// edit effects on a descriptor that doesn't support them must
// substitute a richer _FontDesc.

// ftd_set_underline
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_underline(
    font_dialog<_D, _DF, _FF>&  _fd,
    bool                        _on
)
{
    static_assert(has_ftdf(_FF, ftdf_effects_pane),
                  "requires ftdf_effects_pane");
    static_assert(font::traits::has_font_underline_v<_D>,
                  "_FontDesc does not expose an `underline` member "
                  "(enable font::ff_underline or supply a descriptor "
                  "type that carries it).");

    _fd.selection.underline = _on;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_strikethrough
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_strikethrough(
    font_dialog<_D, _DF, _FF>&  _fd,
    bool                        _on
)
{
    static_assert(has_ftdf(_FF, ftdf_effects_pane),
                  "requires ftdf_effects_pane");
    static_assert(font::traits::has_font_strikethrough_v<_D>,
                  "_FontDesc does not expose a `strikethrough` member.");

    _fd.selection.strikethrough = _on;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_overline
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_overline(
    font_dialog<_D, _DF, _FF>&  _fd,
    bool                        _on
)
{
    static_assert(has_ftdf(_FF, ftdf_effects_pane),
                  "requires ftdf_effects_pane");
    static_assert(font::traits::has_font_overline_v<_D>,
                  "_FontDesc does not expose an `overline` member.");

    _fd.selection.overline = _on;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_small_caps
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_small_caps(
    font_dialog<_D, _DF, _FF>&  _fd,
    bool                        _on
)
{
    static_assert(has_ftdf(_FF, ftdf_effects_pane),
                  "requires ftdf_effects_pane");
    static_assert(font::traits::has_font_small_caps_v<_D>,
                  "_FontDesc does not expose a `small_caps` member.");

    _fd.selection.small_caps = _on;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}


// ---------------------------------------------------------------------
// color setters (routed directly to `selection`)
// ---------------------------------------------------------------------

// ftd_set_foreground
//   sets the foreground color on the descriptor.  The color type
// `_Color` must be assignable to the descriptor's foreground slot;
// typically the descriptor's `color_type` alias.
template <typename _D, unsigned _DF, unsigned _FF, typename _Color>
void
ftd_set_foreground(
    font_dialog<_D, _DF, _FF>&  _fd,
    _Color                      _c
)
{
    static_assert(has_ftdf(_FF, ftdf_color_pane),
                  "requires ftdf_color_pane");
    static_assert(font::traits::has_font_color_v<_D>,
                  "_FontDesc does not expose a `foreground` member.");

    _fd.selection.foreground = std::move(_c);

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_set_background
template <typename _D, unsigned _DF, unsigned _FF, typename _Color>
void
ftd_set_background(
    font_dialog<_D, _DF, _FF>&  _fd,
    _Color                      _c
)
{
    static_assert(has_ftdf(_FF, ftdf_background),
                  "requires ftdf_background");
    static_assert(font::traits::has_font_background_v<_D>,
                  "_FontDesc does not expose `background` "
                  "and `background_enabled` members.");

    _fd.selection.background         = std::move(_c);
    _fd.selection.background_enabled = true;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}

// ftd_clear_background
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_clear_background(
    font_dialog<_D, _DF, _FF>&  _fd
)
{
    static_assert(has_ftdf(_FF, ftdf_background),
                  "requires ftdf_background");
    static_assert(font::traits::has_font_background_v<_D>,
                  "_FontDesc does not expose `background_enabled`.");

    _fd.selection.background_enabled = false;

    font_dialog_internal::mark_preview_stale(_fd);

    if (_fd.on_select_change)
    {
        _fd.on_select_change(_fd.selection);
    }

    return;
}


// ---------------------------------------------------------------------
// preview / sample
// ---------------------------------------------------------------------

// ftd_set_sample_text
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_sample_text(
    font_dialog<_D, _DF, _FF>&  _fd,
    std::string                 _text
)
{
    static_assert(has_ftdf(_FF, ftdf_sample_edit),
                  "requires ftdf_sample_edit");

    _fd.sample_text = std::move(_text);

    font_dialog_internal::mark_preview_stale(_fd);

    return;
}

// ftd_invalidate_preview
//   marks the preview as stale, e.g. after the adapter detects a
// font-cache change or a DPI event.  No-op when preview is disabled.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_invalidate_preview(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    font_dialog_internal::mark_preview_stale(_fd);

    return;
}

// ftd_mark_preview_fresh
//   the adapter calls this after successfully rendering the preview.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_mark_preview_fresh(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    static_assert(has_ftdf(_FF, ftdf_preview),
                  "requires ftdf_preview");

    _fd.preview_stale = false;

    return;
}


// ---------------------------------------------------------------------
// filters
// ---------------------------------------------------------------------

// ftd_toggle_monospace_only
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_toggle_monospace_only(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    static_assert(has_ftdf(_FF, ftdf_monospace_only),
                  "requires ftdf_monospace_only");

    _fd.monospace_only = !_fd.monospace_only;

    ftd_refresh(_fd);
    ftd_reconcile_selection(_fd);

    return;
}

// ftd_set_scalable_filter
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_set_scalable_filter(
    font_dialog<_D, _DF, _FF>&  _fd,
    font_filter_scalable        _s
)
{
    static_assert(has_ftdf(_FF, ftdf_scalable_only),
                  "requires ftdf_scalable_only");

    _fd.scalable_filter = _s;

    ftd_refresh(_fd);
    ftd_reconcile_selection(_fd);

    return;
}


// ---------------------------------------------------------------------
// recent / favourites
// ---------------------------------------------------------------------

// ftd_push_recent
//   pushes a descriptor onto the recent list, deduping by family +
// style_name.  Bounded by recent_max.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_push_recent(
    font_dialog<_D, _DF, _FF>&  _fd,
    _D                          _desc
)
{
    static_assert(has_ftdf(_FF, ftdf_recent),
                  "requires ftdf_recent");

    // remove an existing entry with the same family + style
    for (auto it = _fd.recent.begin(); it != _fd.recent.end(); ++it)
    {
        if ( (it->family     == _desc.family) &&
             (it->style_name == _desc.style_name) )
        {
            _fd.recent.erase(it);
            break;
        }
    }

    _fd.recent.insert(_fd.recent.begin(), std::move(_desc));

    while (_fd.recent.size() > _fd.recent_max)
    {
        _fd.recent.pop_back();
    }

    return;
}

// ftd_add_favourite
//   toggles a descriptor's presence in favourites.  Returns true if
// the descriptor was added, false if it was removed.
template <typename _D, unsigned _DF, unsigned _FF>
bool
ftd_add_favourite(
    font_dialog<_D, _DF, _FF>&  _fd,
    _D                          _desc
)
{
    static_assert(has_ftdf(_FF, ftdf_favourites),
                  "requires ftdf_favourites");

    for (auto it = _fd.favourites.begin(); it != _fd.favourites.end(); ++it)
    {
        if ( (it->family     == _desc.family) &&
             (it->style_name == _desc.style_name) )
        {
            _fd.favourites.erase(it);

            return false;
        }
    }

    _fd.favourites.push_back(std::move(_desc));

    return true;
}


// ---------------------------------------------------------------------
// effective size / accept flow
// ---------------------------------------------------------------------

// ftd_effective_size
//   returns the selection's size.  Preserved for API compatibility;
// a thin wrapper so call sites don't reach into the descriptor
// directly.
template <typename _D, unsigned _DF, unsigned _FF>
float
ftd_effective_size(
    const font_dialog<_D, _DF, _FF>& _fd
) noexcept
{
    return _fd.selection.size;
}

// ftd_try_accept
//   validates the selection (family must be non-empty, size positive)
// and resolves the dialog as accepted on success.  On failure writes
// a message to last_error and leaves the dialog open.  Also pushes
// the descriptor onto the recent list when that mixin is enabled.
template <typename _D, unsigned _DF, unsigned _FF>
bool
ftd_try_accept(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    _fd.last_error.clear();

    if (_fd.selection.family.empty())
    {
        _fd.last_error = "No font family selected.";

        return false;
    }

    if (_fd.selection.size <= 0.0f)
    {
        _fd.last_error = "Font size must be positive.";

        return false;
    }

    if constexpr (has_ftdf(_FF, ftdf_size_slider))
    {
        if ( (_fd.selection.size < _fd.size_min) ||
             (_fd.selection.size > _fd.size_max) )
        {
            _fd.last_error = "Font size is out of range.";

            return false;
        }
    }

    if constexpr (has_ftdf(_FF, ftdf_recent))
    {
        ftd_push_recent(_fd, _fd.selection);
    }

    if (_fd.on_accept_desc)
    {
        _fd.on_accept_desc(_fd.selection);
    }

    dlg_accept(static_cast<dialog<_DF>&>(_fd), nullptr);

    return true;
}

// ftd_cancel
//   convenience: resolves the dialog as cancelled.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_cancel(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    dlg_cancel(static_cast<dialog<_DF>&>(_fd), nullptr);

    return;
}

// ftd_reopen
//   convenience: clears transient state and opens the dialog.
template <typename _D, unsigned _DF, unsigned _FF>
void
ftd_reopen(
    font_dialog<_D, _DF, _FF>& _fd
)
{
    _fd.last_error.clear();

    font_dialog_internal::mark_preview_stale(_fd);

    dlg_open(static_cast<dialog<_DF>&>(_fd));

    ftd_refresh(_fd);
    ftd_reconcile_selection(_fd);

    return;
}


// ===============================================================================
//  7.  TRAITS
// ===============================================================================

namespace font_dialog_traits {
NS_INTERNAL

    template <typename, typename = void>
    struct has_selection_member : std::false_type {};
    template <typename _Type>
    struct has_selection_member<_Type, std::void_t<
        decltype(std::declval<_Type>().selection)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_families_member : std::false_type {};
    template <typename _Type>
    struct has_families_member<_Type, std::void_t<
        decltype(std::declval<_Type>().families)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_selected_family_index_member : std::false_type {};
    template <typename _Type>
    struct has_selected_family_index_member<_Type, std::void_t<
        decltype(std::declval<_Type>().selected_family_index)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_size_options_member : std::false_type {};
    template <typename _Type>
    struct has_size_options_member<_Type, std::void_t<
        decltype(std::declval<_Type>().size_options)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_style_options_member : std::false_type {};
    template <typename _Type>
    struct has_style_options_member<_Type, std::void_t<
        decltype(std::declval<_Type>().style_options)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_preview_stale_member : std::false_type {};
    template <typename _Type>
    struct has_preview_stale_member<_Type, std::void_t<
        decltype(std::declval<_Type>().preview_stale)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_sample_text_member : std::false_type {};
    template <typename _Type>
    struct has_sample_text_member<_Type, std::void_t<
        decltype(std::declval<_Type>().sample_text)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_recent_member : std::false_type {};
    template <typename _Type>
    struct has_recent_member<_Type, std::void_t<
        decltype(std::declval<_Type>().recent)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_favourites_member : std::false_type {};
    template <typename _Type>
    struct has_favourites_member<_Type, std::void_t<
        decltype(std::declval<_Type>().favourites)
    >> : std::true_type {};

}   // NS_INTERNAL

template <typename _Type>
inline constexpr bool has_selection_v =
    internal::has_selection_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_families_v =
    internal::has_families_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_size_options_v =
    internal::has_size_options_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_style_options_v =
    internal::has_style_options_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_preview_v =
    internal::has_preview_stale_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_sample_edit_v =
    internal::has_sample_text_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_recent_v =
    internal::has_recent_member<_Type>::value;
template <typename _Type>
inline constexpr bool has_favourites_v =
    internal::has_favourites_member<_Type>::value;

// is_font_dialog
//   type trait: has selection + families + selected_family_index.
// A font_dialog is structurally distinguished from a plain dialog by
// these three members; it still satisfies dialog_traits::is_dialog
// when it inherits dialog<>.
template <typename _Type>
struct is_font_dialog : std::conjunction<
    internal::has_selection_member<_Type>,
    internal::has_families_member<_Type>,
    internal::has_selected_family_index_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_font_dialog_v =
    is_font_dialog<_Type>::value;

// is_previewing_font_dialog
template <typename _Type>
struct is_previewing_font_dialog : std::conjunction<
    is_font_dialog<_Type>,
    internal::has_preview_stale_member<_Type>
>
{};

template <typename _Type>
inline constexpr bool is_previewing_font_dialog_v =
    is_previewing_font_dialog<_Type>::value;

}   // namespace font_dialog_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_FONT_DIALOG_
