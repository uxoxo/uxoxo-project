/*******************************************************************************
* uxoxo [imgui]                                                 imgui_scope.hpp
*
* RAII guards for ImGui state mutations:
*   Replaces the ~50 hand-rolled BeginDisabled/EndDisabled, PushID/PopID,
* and PushStyleColor/PopStyleColor pairs scattered across the platform
* layer with stack-allocated guard objects.  Each guard mutates ImGui
* state in its constructor and restores it in its destructor, so early
* returns, exceptions, and conditional draw paths cannot leak unbalanced
* state into the next frame.
*
*   The guards are non-copyable and non-movable - they exist purely for
* their side effects and are not intended to be passed around.  They
* take no virtual functions and contain at most one `int` member, so
* they are trivially optimisable in any release build.
*
*   Two convenience helpers wrap the common case of "wrap a render in
* whatever disabling makes sense for this component":
*
*     - make_disabled_scope(_c) reads `_c.enabled` and `_c.read_only`
*       (when present) via the structural detectors in component_traits
*       and returns a scoped_disabled configured accordingly.  Components
*       without those members short-circuit to "not disabled".
*
*     - id_scope(&_c) is a one-line replacement for the
*       PushID(static_cast<const void*>(&_c)) / PopID() pair used in
*       collapsible_panel, magnification_control, stacked_view, and
*       elsewhere.
*
*   The variadic scoped_color constructor accepts (slot, color) pairs
* via the small color_push aggregate.  The destructor pops the count
* observed at construction; pushing additional colors via push() after
* construction increments the count, so guards may grow conditionally
* inside the scope without unbalancing the pop.
*
* Contents:
*   1.  scoped_disabled       - BeginDisabled / EndDisabled
*   2.  scoped_id             - PushID / PopID
*   3.  color_push aggregate
*   4.  scoped_color          - PushStyleColor / PopStyleColor
*   5.  scoped_style_var      - PushStyleVar / PopStyleVar
*   6.  scoped_item_flag      - PushItemFlag / PopItemFlag
*   7.  make_disabled_scope   - structural helper
*
*
* path:      /inc/uxoxo/platform/imgui/core/imgui_scope.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.08
*******************************************************************************/

#ifndef  UXOXO_IMGUI_SCOPE_
#define  UXOXO_IMGUI_SCOPE_ 1

// std
#include <type_traits>
#include <utility>
// imgui
#include <imgui.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/component_traits.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  1.  SCOPED DISABLED
// ===========================================================================

// scoped_disabled
//   class: RAII guard around ImGui::BeginDisabled / EndDisabled.  When
// `_disabled` is true at construction, BeginDisabled is invoked and
// EndDisabled is invoked at destruction.  When false, neither is
// called - the guard becomes a no-op.  Always pair the guard with the
// state you observed at construction; mutating `_disabled` later is
// not supported.
class scoped_disabled
{
public:
    explicit scoped_disabled(
            bool _disabled
        ) noexcept
            : m_active(_disabled)
        {
            if (m_active)
            {
                ImGui::BeginDisabled();
            }
        }

    ~scoped_disabled() noexcept
        {
            if (m_active)
            {
                ImGui::EndDisabled();
            }
        }

    scoped_disabled(const scoped_disabled&)            = delete;
    scoped_disabled& operator=(const scoped_disabled&) = delete;
    scoped_disabled(scoped_disabled&&)                 = delete;
    scoped_disabled& operator=(scoped_disabled&&)      = delete;

private:
    bool m_active;
};


// ===========================================================================
//  2.  SCOPED ID
// ===========================================================================

// scoped_id
//   class: RAII guard around ImGui::PushID / PopID.  Disambiguates
// widgets that share a label or carry no label by hashing a unique
// pointer (typically the component's address) onto the ID stack.
// Replaces the recurring `PushID(static_cast<const void*>(&_c))`
// idiom used across collapsible_panel, magnification_control,
// stacked_view, message_box, and combo_box.
class scoped_id
{
public:
    explicit scoped_id(
            const void* _key
        ) noexcept
        {
            ImGui::PushID(_key);
        }

    explicit scoped_id(
            int _key
        ) noexcept
        {
            ImGui::PushID(_key);
        }

    explicit scoped_id(
            const char* _key
        ) noexcept
        {
            ImGui::PushID(_key);
        }

    ~scoped_id() noexcept
        {
            ImGui::PopID();
        }

    scoped_id(const scoped_id&)            = delete;
    scoped_id& operator=(const scoped_id&) = delete;
    scoped_id(scoped_id&&)                 = delete;
    scoped_id& operator=(scoped_id&&)      = delete;
};


// ===========================================================================
//  3.  COLOR PUSH AGGREGATE
// ===========================================================================

// color_push
//   struct: lightweight (slot, color) pair consumed by scoped_color's
// variadic constructor.  Aggregate-initializable so call sites read
// naturally:
//
//     scoped_color colors {
//         { ImGuiCol_Button,        palette::get<palette::btn_bg_tag>() },
//         { ImGuiCol_ButtonHovered, palette::get<palette::btn_hover_tag>() }
//     };
//
struct color_push
{
    ImGuiCol slot;
    ImVec4   color;
};


// ===========================================================================
//  4.  SCOPED COLOR
// ===========================================================================

// scoped_color
//   class: RAII guard around ImGui::PushStyleColor / PopStyleColor.
// Pushes any number of (slot, color) pairs at construction and pops
// the matching count at destruction.  Additional pushes via push()
// after construction increment the count so the destructor stays
// balanced; this supports the common pattern of "always push 3
// colors, plus one more if disabled".
class scoped_color
{
public:
    scoped_color() noexcept
            : m_count(0)
        {}

    scoped_color(
            std::initializer_list<color_push> _pushes
        ) noexcept
            : m_count(static_cast<int>(_pushes.size()))
        {
            for (const color_push& p : _pushes)
            {
                ImGui::PushStyleColor(p.slot, p.color);
            }
        }

    ~scoped_color() noexcept
        {
            if (m_count > 0)
            {
                ImGui::PopStyleColor(m_count);
            }
        }

    scoped_color(const scoped_color&)            = delete;
    scoped_color& operator=(const scoped_color&) = delete;
    scoped_color(scoped_color&&)                 = delete;
    scoped_color& operator=(scoped_color&&)      = delete;

    /*
    push
      Pushes one more (slot, color) pair onto the ImGui style stack
    and increments the internal count so the destructor's PopStyleColor
    matches.  Use when the number of colors to push is conditional on
    runtime state that cannot be expressed in the constructor's
    initializer list.

    Parameter(s):
      _slot:  the ImGui color slot.
      _color: the color to push.
    Return:
      none.
    */
    void
    push(
        ImGuiCol      _slot,
        const ImVec4& _color
    ) noexcept
    {
        ImGui::PushStyleColor(_slot, _color);
        ++m_count;

        return;
    }

private:
    int m_count;
};


// ===========================================================================
//  5.  SCOPED STYLE VAR
// ===========================================================================

// scoped_style_var
//   class: RAII guard around ImGui::PushStyleVar / PopStyleVar.  Two
// constructor overloads - one for float-valued vars (FrameRounding,
// Alpha, ...) and one for ImVec2-valued vars (FramePadding, ItemSpacing,
// ...).  Multiple pushes via push() share a single PopStyleVar(N)
// at destruction.
class scoped_style_var
{
public:
    scoped_style_var() noexcept
            : m_count(0)
        {}

    scoped_style_var(
            ImGuiStyleVar _var,
            float         _value
        ) noexcept
            : m_count(1)
        {
            ImGui::PushStyleVar(_var, _value);
        }

    scoped_style_var(
            ImGuiStyleVar _var,
            ImVec2        _value
        ) noexcept
            : m_count(1)
        {
            ImGui::PushStyleVar(_var, _value);
        }

    ~scoped_style_var() noexcept
        {
            if (m_count > 0)
            {
                ImGui::PopStyleVar(m_count);
            }
        }

    scoped_style_var(const scoped_style_var&)            = delete;
    scoped_style_var& operator=(const scoped_style_var&) = delete;
    scoped_style_var(scoped_style_var&&)                 = delete;
    scoped_style_var& operator=(scoped_style_var&&)      = delete;

    void
    push(
        ImGuiStyleVar _var,
        float         _value
    ) noexcept
    {
        ImGui::PushStyleVar(_var, _value);
        ++m_count;

        return;
    }

    void
    push(
        ImGuiStyleVar _var,
        ImVec2        _value
    ) noexcept
    {
        ImGui::PushStyleVar(_var, _value);
        ++m_count;

        return;
    }

private:
    int m_count;
};


// ===========================================================================
//  6.  SCOPED ITEM FLAG
// ===========================================================================

// scoped_item_flag
//   class: RAII guard around ImGui::PushItemFlag / PopItemFlag.  Used
// by the checkbox renderer for ImGuiItemFlags_MixedValue and by any
// future component that needs to scope an item flag to a single
// widget.  Requires imgui_internal.h on the consumer side, since
// PushItemFlag is part of the internal API.
class scoped_item_flag
{
public:
    scoped_item_flag(
            ImGuiItemFlags _flag,
            bool           _enabled
        ) noexcept
        {
            ImGui::PushItemFlag(_flag, _enabled);
        }

    ~scoped_item_flag() noexcept
        {
            ImGui::PopItemFlag();
        }

    scoped_item_flag(const scoped_item_flag&)            = delete;
    scoped_item_flag& operator=(const scoped_item_flag&) = delete;
    scoped_item_flag(scoped_item_flag&&)                 = delete;
    scoped_item_flag& operator=(scoped_item_flag&&)      = delete;
};


// ===========================================================================
//  7.  STRUCTURAL HELPERS
// ===========================================================================

NS_INTERNAL

    // disabled_predicate
    //   trait: structurally derives the disabled state of `_c` from
    // its enabled and read_only members.  Three specializations cover
    // the (enabled-only), (read_only-only), and (both-present) cases;
    // a fourth fallback handles components with neither member and
    // returns false unconditionally.
    template<typename _Type,
              bool     _HasEnabled  = has_enabled_v<_Type>,
              bool     _HasReadOnly = has_read_only_v<_Type>>
    struct disabled_predicate
    {
        static bool eval(const _Type&) noexcept
        {
            return false;
        }
    };

    template<typename _Type>
    struct disabled_predicate<_Type, true, false>
    {
        static bool eval(const _Type& _c) noexcept
        {
            return !_c.enabled;
        }
    };

    template<typename _Type>
    struct disabled_predicate<_Type, false, true>
    {
        static bool eval(const _Type& _c) noexcept
        {
            return _c.read_only;
        }
    };

    template<typename _Type>
    struct disabled_predicate<_Type, true, true>
    {
        static bool eval(const _Type& _c) noexcept
        {
            return ( (!_c.enabled) ||
                     (_c.read_only) );
        }
    };

NS_END  // internal

/*
make_disabled_scope
  Returns a scoped_disabled configured from the structural state of
`_c`.  The function consults component_traits to detect whether
`_c.enabled` and `_c.read_only` exist and chooses the appropriate
predicate at compile time.  Components missing both members produce
a no-op guard that compiles to nothing.

  This is the canonical way to implement the "if disabled,
BeginDisabled" pattern that appears in every interactive renderer.
Replaces the bracketed `if (!_c.enabled) { ImGui::BeginDisabled(); }
... if (!_c.enabled) { ImGui::EndDisabled(); }` cascade.

Parameter(s):
  _c: the component whose enabled / read_only state determines
      whether ImGui's disabled scope is entered.
Return:
  A scoped_disabled active iff the component reports a disabled state.
*/
template<typename _Type>
[[nodiscard]] inline scoped_disabled
make_disabled_scope(
    const _Type& _c
) noexcept
{
    return scoped_disabled(internal::disabled_predicate<_Type>::eval(_c));
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_SCOPE_
