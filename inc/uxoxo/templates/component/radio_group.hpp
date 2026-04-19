/*******************************************************************************
* uxoxo [component]                                             radio_group.hpp
*
*   One-of-N selector.  A flat list of labelled options where exactly one can
* be selected at any time.  Pure data aggregate — no observer, no renderer.
*
*   Template parameters:
*     _Data   per-option payload (default: std::string — just labels)
*     _Feat   bitwise OR of view_feat flags (vf_icons, vf_context supported)
*     _Icon   icon storage type when vf_icons is set
*
*   Zero-cost features:
*     vf_icons    icon per option
*     vf_context  per-option context action bitfield
*     (vf_checkable/vf_renamable/vf_collapsible are meaningless for radio)
*
*   The option template uses an EBO mixin just like tree_node / list_entry:
*   disabled features compile to 0 bytes.
*
*
* path:      /inc/uxoxo/component/radio_group.hpp  
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.26
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_RADIO_GROUP_
#define  UXOXO_COMPONENT_RADIO_GROUP_ 1

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  RADIO OPTION
// ═══════════════════════════════════════════════════════════════════════════════

// orientation
//   Re-declared here for self-containedness if view_common hasn't defined it.
//   (If it has, this is a compatible redeclaration via the same enum values.)
enum class orientation : std::uint8_t
{
    horizontal,
    vertical
};

template <typename _Data = std::string,
          unsigned _Feat = vf_none,
          typename _Icon = int>
struct radio_option
    : entry_mixin::icon_data    <has_feat(_Feat, vf_icons), _Icon>
    , entry_mixin::context_data <has_feat(_Feat, vf_context)>
{
    using data_type = _Data;
    using icon_type = _Icon;

    static constexpr unsigned features = _Feat;
    static constexpr bool has_icons   = has_feat(_Feat, vf_icons);
    static constexpr bool has_context = has_feat(_Feat, vf_context);

    _Data data;
    bool  enabled = true;       // per-option disable

    radio_option() = default;
    explicit radio_option(_Data d) : data(std::move(d)) {}
    radio_option(_Data d, bool en) : data(std::move(d)), enabled(en) {}
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  RADIO GROUP
// ═══════════════════════════════════════════════════════════════════════════════

template <typename _Data = std::string,
          unsigned _Feat = vf_none,
          typename _Icon = int>
struct radio_group
    : view_mixin::context_state<has_feat(_Feat, vf_context)>
{
    using option_type = radio_option<_Data, _Feat, _Icon>;
    using data_type   = _Data;
    using icon_type   = _Icon;

    static constexpr unsigned features = _Feat;
    static constexpr bool has_icons   = has_feat(_Feat, vf_icons);
    static constexpr bool has_context = has_feat(_Feat, vf_context);
    static constexpr bool focusable   = true;
    static constexpr bool scrollable  = false;

    // ── data ─────────────────────────────────────────────────────────────
    std::vector<option_type> options;
    std::size_t              selected = 0;       // currently selected index
    std::size_t              focused  = 0;       // cursor (may differ from selected)
    orientation              orient   = orientation::vertical;
    bool                     wrap     = true;     // wrap navigation at edges

    // ── add options ──────────────────────────────────────────────────────

    option_type& add(option_type opt)
    {
        options.push_back(std::move(opt));
        return options.back();
    }

    option_type& emplace(non_deduced<_Data> d, bool enabled = true)
    {
        options.emplace_back(std::move(d), enabled);
        return options.back();
    }

    // ── queries ──────────────────────────────────────────────────────────

    [[nodiscard]] std::size_t count() const noexcept { return options.size(); }
    [[nodiscard]] bool empty() const noexcept { return options.empty(); }

    [[nodiscard]] const option_type* selected_option() const
    {
        return (selected < options.size()) ? &options[selected] : nullptr;
    }

    [[nodiscard]] option_type* selected_option()
    {
        return (selected < options.size()) ? &options[selected] : nullptr;
    }

    [[nodiscard]] const _Data* selected_data() const
    {
        auto* opt = selected_option();
        return opt ? &opt->data : nullptr;
    }

    [[nodiscard]] const option_type* focused_option() const
    {
        return (focused < options.size()) ? &options[focused] : nullptr;
    }

    // ── selection ────────────────────────────────────────────────────────

    // select
    //   Sets the selected index.  Skips disabled options.  Returns true
    //   if selection changed.
    bool select(std::size_t idx)
    {
        if (idx >= options.size())
        {
            return false;
        }
        if (!options[idx].enabled)
        {
            return false;
        }
        if (idx == selected)
        {
            return false;
        }
        selected = idx;
        focused  = idx;
        return true;
    }

    // confirm
    //   Sets selected = focused (user presses Enter/Space on focused item).
    bool confirm()
    {
        return select(focused);
    }

    // ── navigation ───────────────────────────────────────────────────────

    // next
    //   Moves focus to the next enabled option.  Skips disabled options.
    bool next()
    {
        if (options.empty())
        {
            return false;
        }
        std::size_t start = focused;
        std::size_t n = options.size();

        for (std::size_t i = 1; i <= n; ++i)
        {
            std::size_t idx;
            if (wrap)
            {
                idx = (start + i) % n;
            }
            else
            {
                idx = start + i;
                if (idx >= n)
                {
                    return false;
                }
            }
            if (options[idx].enabled)
            {
                focused = idx;
                return true;
            }
        }
        return false;
    }

    // prev
    //   Moves focus to the previous enabled option.
    bool prev()
    {
        if (options.empty())
        {
            return false;
        }
        std::size_t start = focused;
        std::size_t n = options.size();

        for (std::size_t i = 1; i <= n; ++i)
        {
            std::size_t idx;
            if (wrap)
            {
                idx = (start + n - i) % n;
            }
            else
            {
                if (i > start)
                {
                    return false;
                }
                idx = start - i;
            }
            if (options[idx].enabled)
            {
                focused = idx;
                return true;
            }
        }
        return false;
    }

    // home / end
    bool home()
    {
        for (std::size_t i = 0; i < options.size(); ++i)
        {
            if (options[i].enabled) { focused = i; return true; }
        }
        return false;
    }

    bool end()
    {
        for (std::size_t i = options.size(); i > 0; --i)
        {
            if (options[i - 1].enabled) { focused = i - 1; return true; }
        }
        return false;
    }

    // ── context menu ─────────────────────────────────────────────────────

    bool open_context(int x = 0, int y = 0)
    {
        static_assert(has_context, "requires vf_context");
        if (focused >= options.size())
        {
            return false;
        }
        this->context_open  = true;
        this->context_index = focused;
        this->context_x = x;
        this->context_y = y;
        return true;
    }

    void close_context()
    {
        static_assert(has_context, "requires vf_context");
        this->context_open = false;
    }

    option_type* context_option()
    {
        static_assert(has_context, "requires vf_context");
        if (!this->context_open || this->context_index >= options.size())
        {
            return nullptr;
        }
        return &options[this->context_index];
    }

    // ── search ───────────────────────────────────────────────────────────

    template <typename _Match>
    bool search_next(_Match match)
    {
        if (options.empty())
        {
            return false;
        }
        std::size_t n = options.size();
        for (std::size_t i = 1; i <= n; ++i)
        {
            std::size_t idx = (focused + i) % n;
            if (options[idx].enabled && match(options[idx].data))
            {
                focused = idx;
                return true;
            }
        }
        return false;
    }

    // ── bulk queries ─────────────────────────────────────────────────────

    [[nodiscard]] std::size_t enabled_count() const
    {
        std::size_t n = 0;
        for (const auto& o : options)
        {
            if (o.enabled)
            {
                ++n;
            }
        }
        return n;
    }

    // find_by
    //   Returns index of first option matching predicate, or -1.
    template <typename _Pred>
    std::size_t find_by(_Pred pred) const
    {
        for (std::size_t i = 0; i < options.size(); ++i)
        {
            if (pred(options[i].data))
            {
                return i;
            }
        }
        return static_cast<std::size_t>(-1);
    }

    // select_by
    //   Finds and selects the first matching option.
    template <typename _Pred>
    bool select_by(_Pred pred)
    {
        auto idx = find_by(pred);
        if (idx == static_cast<std::size_t>(-1))
        {
            return false;
        }
        return select(idx);
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  FREE-FUNCTION HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

// set_icon  (radio_option)
template <typename _D,
          unsigned _F,
          typename _I>
void set_icon(radio_option<_D, _F, _I>& opt, non_deduced<_I> icon)
{
    static_assert(has_feat(_F, vf_icons), "requires vf_icons");
    opt.icon = std::move(icon);
}

// has_action  (radio_option)
template <typename _D,
          unsigned _F,
          typename _I>
bool has_action(const radio_option<_D, _F, _I>& opt, context_action a)
{
    static_assert(has_feat(_F, vf_context), "requires vf_context");
    return (opt.context_actions & static_cast<unsigned>(a)) != 0;
}

// set_actions  (radio_option)
template <typename _D,
          unsigned _F,
          typename _I>
void set_actions(radio_option<_D, _F, _I>& opt, unsigned actions)
{
    static_assert(has_feat(_F, vf_context), "requires vf_context");
    opt.context_actions = actions;
}

// enable_all
template <typename _D,
          unsigned _F,
          typename _I>
void enable_all(radio_group<_D, _F, _I>& rg)
{
    for (auto& o : rg.options)
    {
        o.enabled = true;
    }
}

// disable_all
template <typename _D,
          unsigned _F,
          typename _I>
void disable_all(radio_group<_D, _F, _I>& rg)
{
    for (auto& o : rg.options)
    {
        o.enabled = false;
    }
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  RADIO TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace radio_traits {
namespace detail {
    template <typename,
              typename = void>
    struct has_options_member : std::false_type {};
    template <typename _T>
    struct has_options_member<_T, std::void_t<
        decltype(std::declval<_T>().options)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_orient_member : std::false_type {};
    template <typename _T>
    struct has_orient_member<_T, std::void_t<
        decltype(std::declval<_T>().orient)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_focused_member : std::false_type {};
    template <typename _T>
    struct has_focused_member<_T, std::void_t<
        decltype(std::declval<_T>().focused)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_wrap_member : std::false_type {};
    template <typename _T>
    struct has_wrap_member<_T, std::void_t<
        decltype(std::declval<_T>().wrap)
    >> : std::true_type {};

    template <typename,
              typename = void>
    struct has_enabled_member : std::false_type {};
    template <typename _T>
    struct has_enabled_member<_T, std::void_t<
        decltype(std::declval<_T>().enabled)
    >> : std::true_type {};
}

template <typename _T> inline constexpr bool has_options_v = detail::has_options_member<_T>::value;
template <typename _T> inline constexpr bool has_orient_v  = detail::has_orient_member<_T>::value;
template <typename _T> inline constexpr bool has_focused_v = detail::has_focused_member<_T>::value;
template <typename _T> inline constexpr bool has_wrap_v    = detail::has_wrap_member<_T>::value;
template <typename _T> inline constexpr bool has_enabled_v = detail::has_enabled_member<_T>::value;

// is_radio_option
//   Has data + enabled, no children (distinguishes from tree_node).
template <typename _Type>
struct is_radio_option : std::conjunction<
    view_traits::detail::has_data_member<_Type>,
    detail::has_enabled_member<_Type>,
    std::negation<tree_traits::detail::has_children_member<_Type>>
> {};
template <typename _T> inline constexpr bool is_radio_option_v = is_radio_option<_T>::value;

// is_radio_group
//   Has options + selected + focused + orient.
template <typename _Type>
struct is_radio_group : std::conjunction<
    detail::has_options_member<_Type>,
    view_traits::detail::has_selected_member<_Type>,
    detail::has_focused_member<_Type>,
    detail::has_orient_member<_Type>,
    view_traits::detail::has_focusable_flag<_Type>
> {};
template <typename _T> inline constexpr bool is_radio_group_v = is_radio_group<_T>::value;


}   // namespace radio_traits


NS_END  // component
NS_END  // uxoxo

#endif  // UXOXO_COMPONENT_RADIO_GROUP_
