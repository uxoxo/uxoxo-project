/*******************************************************************************
* uxoxo [imgui]                                            imgui_cvar_form.hpp
*
*   Dear ImGui implementation of uxoxo::component::cvar_form.
* Renders the form's n-ary tree as an MSVS-style two-pane layout
* (master tree on the left, settings detail on the right) and
* dispatches each leaf to the appropriate ImGui widget based on
* its control_kind.  Composite nodes are rendered via their
* archetype (composite_kind) plus an action toolbar drawn from
* the node's `actions` vector.
*
*   The form itself remains agnostic about the registry's
* cvar_type.  The renderer bridges that gap through eight
* per-primitive-type callbacks on the view state (get_bool /
* set_bool / get_int / set_int / get_double / set_double /
* get_string / set_string).  The user populates exactly the
* bridges their cvar_type needs; unset bridges produce disabled
* placeholders so missing wiring is visible at runtime instead
* of silently no-op'ing.
*
*   ARCHITECTURE:
*
*       cvar_form<_Registry>            (model, type-agnostic)
*           |
*           v
*       imgui_cvar_form_view_state      (type bridges + layout)
*           |
*           v
*       imgui_draw_cvar_form_*          (free-function renderers)
*           |
*           +-- master pane: walks `kind == category` recursively
*           |
*           +-- detail pane: walks the selected node's children,
*               dispatching on:
*                   kind = field      -> control_kind dispatch
*                   kind = composite  -> composite_kind dispatch
*                   kind = section /  -> chrome (separator + label)
*                          group
*                   kind = spacer /   -> visual elements
*                          label
*               and rendering an action toolbar from `actions`.
*
*   The master / detail panes are also exposed individually so
* users can compose them into custom layouts (e.g. a tabbed
* options dialog where each tab contains only the detail pane
* for one category).
*
*   USAGE:
*
*     using my_registry = djinterp::container::registry_table<...>;
*
*     uxoxo::component::cvar_form<my_registry>          form(reg);
*     uxoxo::imgui::imgui_cvar_form_view_state<
*         my_registry>                                  vs;
*
*     // build the form (categories / sections / fields) ...
*
*     // wire the type bridges (variant-shaped cvar_type example):
*     vs.get_bool = [](const my_registry& r, const auto& k)
*         { return std::get<bool>(r.get(k)); };
*     vs.set_bool = [](my_registry& r, const auto& k, bool v)
*         { r.set(k, v); };
*     // ... similar for int / double / string ...
*
*     // each frame inside an ImGui::Begin/End:
*     ImGui::Begin("Options");
*     uxoxo::imgui::imgui_draw_cvar_form(form, vs);
*     ImGui::End();
*
*   THREAD SAFETY:  ImGui itself is single-threaded; the renderer
* inherits that contract.
*
* DEPENDENCIES:
*   imgui.h                            Dear ImGui (user-provided)
*   cvar_form.hpp                      layer-2 component template
*
* TABLE OF CONTENTS
* =================
* I.    imgui_cvar_form_view_state
* II.   action toolbar rendering
* III.  field control rendering
* IV.   composite rendering
* V.    detail / master pane rendering
* VI.   imgui_draw_cvar_form  (top-level)
*
*
* path:      /inc/uxoxo/platform/imgui/cvar/imgui_cvar_form.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.10
*******************************************************************************/

#ifndef UXOXO_IMGUI_CVAR_FORM_
#define UXOXO_IMGUI_CVAR_FORM_ 1

// std
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
// imgui
#include <imgui.h>
// uxoxo
#include "../../../uxoxo.hpp"
#include "../../../templates/component/cvar/cvar_form.hpp"


NS_UXOXO
NS_IMGUI


// ===========================================================================
//  I.   IMGUI_CVAR_FORM_VIEW_STATE
// ===========================================================================

// imgui_cvar_form_view_state
//   struct: persistent per-instance configuration consumed by
// imgui_draw_cvar_form.  Holds the type bridges (one
// get / set callback pair per primitive type the renderer
// understands), the layout configuration, and optional override
// hooks for control kinds without a native ImGui equivalent
// (date / time / file / folder pickers).
//
//   Type bridges: the user populates exactly the pairs their
// cvar_type can produce.  An unset get_X bridge causes any field
// of the matching control_kind to render as a disabled
// placeholder rather than silently no-op'ing - missing wiring
// is visible at runtime.
template<typename _Registry>
struct imgui_cvar_form_view_state
{
    // -----------------------------------------------------------------
    //  type aliases
    // -----------------------------------------------------------------

    using registry_type = _Registry;
    using key_type      = typename _Registry::key_type;
    using node_type     = component::cvar_form_node<_Registry>;


    // -----------------------------------------------------------------
    //  type bridges (registry <-> primitive types)
    // -----------------------------------------------------------------

    // get_X / set_X
    //   field: extract / store a primitive value at the given
    // registry key.  The user populates the bridges their
    // cvar_type can produce; missing bridges cause the renderer
    // to draw a disabled placeholder for fields of the matching
    // control kind.

    std::function<bool(const _Registry&, const key_type&)>
        get_bool;
    std::function<int(const _Registry&, const key_type&)>
        get_int;
    std::function<double(const _Registry&, const key_type&)>
        get_double;
    std::function<std::string(const _Registry&, const key_type&)>
        get_string;

    std::function<void(_Registry&, const key_type&, bool)>
        set_bool;
    std::function<void(_Registry&, const key_type&, int)>
        set_int;
    std::function<void(_Registry&, const key_type&, double)>
        set_double;
    std::function<void(_Registry&, const key_type&, const std::string&)>
        set_string;


    // -----------------------------------------------------------------
    //  layout configuration
    // -----------------------------------------------------------------

    // master_pane_ratio
    //   field: fraction of the available content width assigned
    // to the master pane.  The remainder goes to the detail
    // pane.  Clamped at render time.
    float           master_pane_ratio        = 0.30f;

    // section_indent
    //   field: pixels of left-indent applied to each section /
    // group nesting level inside the detail pane.
    float           section_indent           = 12.0f;

    // show_descriptions
    //   field: when true, a field's metadata.description is
    // rendered below the control as disabled text.
    bool            show_descriptions        = true;

    // show_action_shortcuts
    //   field: when true, an action's shortcut hint (e.g.
    // "Ctrl+N") is appended to its tooltip.
    bool            show_action_shortcuts    = true;

    // text_input_buf_size
    //   field: size of the per-render scratch buffer used by
    // text_input / multiline_text / password_input controls.
    // Strings longer than this are silently truncated; bump
    // for editors of large blobs.
    std::size_t     text_input_buf_size      = 1024;


    // -----------------------------------------------------------------
    //  override hook
    // -----------------------------------------------------------------

    // fallback_render
    //   field: optional callback invoked for control kinds the
    // renderer has no built-in support for (date_picker,
    // time_picker, datetime_picker, color_picker, file_picker,
    // folder_picker).  When unset, the renderer falls back to
    // a text_input representation so the field is at least
    // viewable.
    //
    //   The callback returns true if the user mutated the
    // value (the form is then marked dirty).
    using fallback_render_fn =
        std::function<bool(_Registry&,
                           const node_type&,
                           imgui_cvar_form_view_state&)>;

    fallback_render_fn  fallback_render;
};


// ===========================================================================
//  II.  ACTION TOOLBAR RENDERING
// ===========================================================================

namespace detail
{

    // imgui_render_action_toolbar
    //   helper: emits a horizontal row of buttons drawn from
    // _node.actions.  Skips invisible actions and disables
    // buttons whose is_enabled() returns false.  Fires
    // on_invoke when the button is clicked.
    //
    //   When _node.actions is empty this is a no-op, so callers
    // can blindly invoke it on any node.
    template<typename _Registry>
    void
    imgui_render_action_toolbar(
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        if (_node.actions.empty())
        {
            return;
        }

        bool first = true;

        for (const auto& act : _node.actions)
        {
            if (!act.visible)
            {
                continue;
            }

            if (!first)
            {
                ImGui::SameLine();
            }
            first = false;

            const bool en = act.is_enabled();

            if (!en)
            {
                ImGui::BeginDisabled();
            }

            ImGui::PushID(act.id.c_str());

            const char* label = act.label.empty()
                                    ? act.id.c_str()
                                    : act.label.c_str();

            if (ImGui::Button(label))
            {
                if (act.on_invoke)
                {
                    act.on_invoke();
                }
            }

            // Tooltip + shortcut hint
            if ( !act.tooltip.empty() ||
                 (_vs.show_action_shortcuts && !act.shortcut.empty()) )
            {
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();

                    if (!act.tooltip.empty())
                    {
                        ImGui::TextUnformatted(act.tooltip.c_str());
                    }

                    if ( _vs.show_action_shortcuts &&
                         !act.shortcut.empty() )
                    {
                        ImGui::TextDisabled("%s", act.shortcut.c_str());
                    }

                    ImGui::EndTooltip();
                }
            }

            ImGui::PopID();

            if (!en)
            {
                ImGui::EndDisabled();
            }
        }

        return;
    }


// ===========================================================================
//  III. FIELD CONTROL RENDERING
// ===========================================================================

    // imgui_render_field_unbridged
    //   helper: draws a disabled placeholder for a field whose
    // control_kind has no bridge populated on the view state.
    // Surfaces the missing wiring at runtime instead of letting
    // the field silently no-op.
    template<typename _Registry>
    void
    imgui_render_field_unbridged(
        const component::cvar_form_node<_Registry>&  _node,
        const char*                                  _bridge_name
    )
    {
        ImGui::BeginDisabled();
        ImGui::Text("%s [no %s bridge]",
                    _node.metadata.label.empty()
                        ? _node.id.c_str()
                        : _node.metadata.label.c_str(),
                    _bridge_name);
        ImGui::EndDisabled();

        return;
    }

    // imgui_render_field_description
    //   helper: emits the per-field description as disabled text
    // below the control, when enabled in the view state.
    template<typename _Registry>
    void
    imgui_render_field_description(
        const component::cvar_form_node<_Registry>&  _node,
        const imgui_cvar_form_view_state<_Registry>& _vs
    )
    {
        if ( _vs.show_descriptions &&
             !_node.metadata.description.empty() )
        {
            ImGui::Indent(16.0f);
            ImGui::TextDisabled("%s",
                _node.metadata.description.c_str());
            ImGui::Unindent(16.0f);
        }

        return;
    }

    // imgui_field_label
    //   helper: returns the label string the renderer should
    // hand to ImGui for the field, falling back to the node id
    // when no metadata.label is set.
    template<typename _Registry>
    const char*
    imgui_field_label(
        const component::cvar_form_node<_Registry>& _node
    )
    {
        return _node.metadata.label.empty()
                   ? _node.id.c_str()
                   : _node.metadata.label.c_str();
    }


    // -----------------------------------------------------------------
    //  per-control-kind renderers
    // -----------------------------------------------------------------

    // imgui_render_checkbox
    template<typename _Registry>
    bool
    imgui_render_checkbox(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        if (!_vs.get_bool)
        {
            imgui_render_field_unbridged(_node, "bool");
            return false;
        }

        bool v = _vs.get_bool(_reg, _node.cvar_key);

        const bool changed =
            ImGui::Checkbox(imgui_field_label(_node), &v);

        if (changed && _vs.set_bool)
        {
            _vs.set_bool(_reg, _node.cvar_key, v);
        }

        return changed;
    }

    // imgui_render_text_input
    //   helper: text_input / multiline_text / password_input share
    // a buffer-backed implementation; the flags variant selects
    // between them.
    template<typename _Registry>
    bool
    imgui_render_text_input(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs,
        bool                                         _multiline,
        bool                                         _password
    )
    {
        if (!_vs.get_string)
        {
            imgui_render_field_unbridged(_node, "string");
            return false;
        }

        std::string current = _vs.get_string(_reg, _node.cvar_key);

        // Reusable per-render scratch buffer.  Size from view state.
        thread_local std::vector<char> tl_buf;

        const std::size_t buf_size =
            (_vs.text_input_buf_size > 0)
                ? _vs.text_input_buf_size
                : 1024;

        tl_buf.assign(buf_size, 0);

        const std::size_t copy_n =
            std::min<std::size_t>(current.size(), buf_size - 1);

        std::memcpy(tl_buf.data(), current.data(), copy_n);
        tl_buf[copy_n] = 0;

        ImGuiInputTextFlags flags = 0;

        if (_password)
        {
            flags |= ImGuiInputTextFlags_Password;
        }

        if (_node.metadata.read_only)
        {
            flags |= ImGuiInputTextFlags_ReadOnly;
        }

        bool changed = false;

        if (_multiline)
        {
            changed = ImGui::InputTextMultiline(
                imgui_field_label(_node),
                tl_buf.data(), tl_buf.size(),
                ImVec2(0, 0), flags);
        }
        else
        {
            if (!_node.metadata.placeholder.empty())
            {
                flags |= 0;  // ImGui handles placeholder via hint
                             // overload (InputTextWithHint); some
                             // older ImGui releases lack it, so the
                             // placeholder is documented but not
                             // forced here.
            }

            changed = ImGui::InputText(
                imgui_field_label(_node),
                tl_buf.data(), tl_buf.size(), flags);
        }

        if (changed && _vs.set_string)
        {
            _vs.set_string(_reg, _node.cvar_key,
                           std::string(tl_buf.data()));
        }

        return changed;
    }

    // imgui_render_combo_box
    template<typename _Registry>
    bool
    imgui_render_combo_box(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        if (!_vs.get_string)
        {
            imgui_render_field_unbridged(_node, "string");
            return false;
        }

        const std::string current =
            _vs.get_string(_reg, _node.cvar_key);

        // Find the option whose stored value matches current.
        const auto& opts = _node.metadata.options;

        const char* current_label = current.c_str();

        for (const auto& o : opts)
        {
            if (o.first == current)
            {
                current_label = o.second.c_str();
                break;
            }
        }

        bool changed = false;

        if (ImGui::BeginCombo(imgui_field_label(_node), current_label))
        {
            for (const auto& o : opts)
            {
                const bool sel = (o.first == current);

                if (ImGui::Selectable(o.second.c_str(), sel))
                {
                    if (_vs.set_string)
                    {
                        _vs.set_string(_reg, _node.cvar_key, o.first);
                    }
                    changed = true;
                }

                if (sel)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        return changed;
    }

    // imgui_render_radio_group
    template<typename _Registry>
    bool
    imgui_render_radio_group(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        if (!_vs.get_string)
        {
            imgui_render_field_unbridged(_node, "string");
            return false;
        }

        const std::string current =
            _vs.get_string(_reg, _node.cvar_key);

        // Group label first
        if (!_node.metadata.label.empty())
        {
            ImGui::TextUnformatted(_node.metadata.label.c_str());
        }

        bool changed = false;

        for (const auto& o : _node.metadata.options)
        {
            const bool sel = (o.first == current);

            ImGui::PushID(o.first.c_str());

            if (ImGui::RadioButton(o.second.c_str(), sel))
            {
                if (_vs.set_string)
                {
                    _vs.set_string(_reg, _node.cvar_key, o.first);
                }
                changed = true;
            }

            ImGui::PopID();
        }

        return changed;
    }

    // imgui_render_slider
    //   helper: slider with min / max / step from metadata.
    // Defaults to [0, 100] when range is unspecified so the
    // control is at least usable.
    template<typename _Registry>
    bool
    imgui_render_slider(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        if (!_vs.get_double)
        {
            imgui_render_field_unbridged(_node, "double");
            return false;
        }

        float v = static_cast<float>(
            _vs.get_double(_reg, _node.cvar_key));

        const float minv = _node.metadata.min_value.has_value()
                               ? static_cast<float>(*_node.metadata.min_value)
                               : 0.0f;
        const float maxv = _node.metadata.max_value.has_value()
                               ? static_cast<float>(*_node.metadata.max_value)
                               : 100.0f;

        const char* fmt = _node.metadata.format_hint.empty()
                              ? "%.3f"
                              : _node.metadata.format_hint.c_str();

        const bool changed = ImGui::SliderFloat(
            imgui_field_label(_node), &v, minv, maxv, fmt);

        if (changed && _vs.set_double)
        {
            _vs.set_double(_reg, _node.cvar_key,
                           static_cast<double>(v));
        }

        return changed;
    }

    // imgui_render_spin_box
    //   helper: spin_box uses an integer or floating step
    // determined by metadata.step.  When step is integer-aligned
    // and min / max are set integer-like, integer InputInt is
    // used; otherwise InputFloat with the configured step.
    template<typename _Registry>
    bool
    imgui_render_spin_box(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        // Decide integer vs float based on whether step is set
        // and integer-aligned.  Heuristic: integer if step has
        // value and the step is integral.
        const bool is_int =
            _node.metadata.step.has_value() &&
            (*_node.metadata.step ==
                static_cast<int>(*_node.metadata.step));

        if (is_int)
        {
            if (!_vs.get_int)
            {
                imgui_render_field_unbridged(_node, "int");
                return false;
            }

            int v = _vs.get_int(_reg, _node.cvar_key);

            const int step_i = static_cast<int>(*_node.metadata.step);

            const bool changed = ImGui::InputInt(
                imgui_field_label(_node), &v, step_i, step_i * 10);

            // Clamp to range if set
            if (changed)
            {
                if (_node.metadata.min_value.has_value())
                {
                    v = std::max(v,
                            static_cast<int>(*_node.metadata.min_value));
                }
                if (_node.metadata.max_value.has_value())
                {
                    v = std::min(v,
                            static_cast<int>(*_node.metadata.max_value));
                }

                if (_vs.set_int)
                {
                    _vs.set_int(_reg, _node.cvar_key, v);
                }
            }

            return changed;
        }
        else
        {
            if (!_vs.get_double)
            {
                imgui_render_field_unbridged(_node, "double");
                return false;
            }

            float v = static_cast<float>(
                _vs.get_double(_reg, _node.cvar_key));

            const float step_f =
                _node.metadata.step.has_value()
                    ? static_cast<float>(*_node.metadata.step)
                    : 0.1f;

            const char* fmt = _node.metadata.format_hint.empty()
                                  ? "%.3f"
                                  : _node.metadata.format_hint.c_str();

            const bool changed = ImGui::InputFloat(
                imgui_field_label(_node),
                &v, step_f, step_f * 10.0f, fmt);

            if (changed)
            {
                if (_node.metadata.min_value.has_value())
                {
                    v = std::max(v,
                            static_cast<float>(*_node.metadata.min_value));
                }
                if (_node.metadata.max_value.has_value())
                {
                    v = std::min(v,
                            static_cast<float>(*_node.metadata.max_value));
                }

                if (_vs.set_double)
                {
                    _vs.set_double(_reg, _node.cvar_key,
                                   static_cast<double>(v));
                }
            }

            return changed;
        }
    }

    // imgui_render_color_picker
    template<typename _Registry>
    bool
    imgui_render_color_picker(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        // ImGui ColorEdit operates on float[3] / float[4].  The
        // bridge here treats the cvar as a packed double encoding
        // the colour - typical wiring is the user converts to /
        // from their colour type in get_double / set_double
        // (e.g. by packing RGBA into 32-bit and casting).  When
        // that round-trip is awkward, users should switch the
        // field to control_kind::custom and supply render_cb.
        if (!_vs.get_double || !_vs.set_double)
        {
            imgui_render_field_unbridged(_node, "double (RGBA pack)");
            return false;
        }

        double packed = _vs.get_double(_reg, _node.cvar_key);

        union { float f[4]; std::uint32_t u; } u{};
        u.u = static_cast<std::uint32_t>(packed);

        float c[4] = {
            ((u.u >> 24) & 0xFF) / 255.0f,
            ((u.u >> 16) & 0xFF) / 255.0f,
            ((u.u >>  8) & 0xFF) / 255.0f,
            ( u.u        & 0xFF) / 255.0f,
        };

        const bool changed = ImGui::ColorEdit4(
            imgui_field_label(_node), c);

        if (changed)
        {
            const std::uint32_t r =
                static_cast<std::uint32_t>(c[0] * 255.0f) & 0xFF;
            const std::uint32_t g =
                static_cast<std::uint32_t>(c[1] * 255.0f) & 0xFF;
            const std::uint32_t b =
                static_cast<std::uint32_t>(c[2] * 255.0f) & 0xFF;
            const std::uint32_t a =
                static_cast<std::uint32_t>(c[3] * 255.0f) & 0xFF;

            const std::uint32_t packed_out =
                (r << 24) | (g << 16) | (b << 8) | a;

            _vs.set_double(_reg, _node.cvar_key,
                           static_cast<double>(packed_out));
        }

        return changed;
    }

    // imgui_render_field
    //   helper: dispatch on _node.control to the matching
    // per-control-kind renderer.  Returns true if the user
    // mutated the underlying value.
    template<typename _Registry>
    bool
    imgui_render_field(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        using component::control_kind;

        if (!_node.metadata.visible)
        {
            return false;
        }

        const bool en = _node.metadata.enabled;

        if (!en)
        {
            ImGui::BeginDisabled();
        }

        ImGui::PushID(_node.id.c_str());

        bool changed = false;

        switch (_node.control)
        {
            case control_kind::checkbox:
            case control_kind::toggle:
                changed = imgui_render_checkbox(_reg, _node, _vs);
                break;

            case control_kind::text_input:
                changed = imgui_render_text_input(
                    _reg, _node, _vs,
                    /*multiline=*/false,
                    /*password=*/ false);
                break;

            case control_kind::multiline_text:
                changed = imgui_render_text_input(
                    _reg, _node, _vs,
                    /*multiline=*/true,
                    /*password=*/ false);
                break;

            case control_kind::password_input:
                changed = imgui_render_text_input(
                    _reg, _node, _vs,
                    /*multiline=*/false,
                    /*password=*/ true);
                break;

            case control_kind::combo_box:
                changed = imgui_render_combo_box(_reg, _node, _vs);
                break;

            case control_kind::radio_group:
                changed = imgui_render_radio_group(_reg, _node, _vs);
                break;

            case control_kind::slider:
                changed = imgui_render_slider(_reg, _node, _vs);
                break;

            case control_kind::spin_box:
                changed = imgui_render_spin_box(_reg, _node, _vs);
                break;

            case control_kind::color_picker:
                changed = imgui_render_color_picker(_reg, _node, _vs);
                break;

            case control_kind::date_picker:
            case control_kind::time_picker:
            case control_kind::datetime_picker:
            case control_kind::file_picker:
            case control_kind::folder_picker:
            case control_kind::list_box:
                // Fall back to user override if provided, else
                // text_input as a degraded representation.
                if (_vs.fallback_render)
                {
                    changed = _vs.fallback_render(_reg, _node, _vs);
                }
                else
                {
                    changed = imgui_render_text_input(
                        _reg, _node, _vs,
                        /*multiline=*/false,
                        /*password=*/ false);
                }
                break;

            case control_kind::custom:
                if (_node.render_cb)
                {
                    _node.render_cb(_reg, _node,
                        static_cast<void*>(&_vs));
                }
                break;

            case control_kind::none:
                // Field with no control; nothing to render.
                break;
        }

        // Tooltip
        if ( !_node.metadata.tooltip.empty() &&
             ImGui::IsItemHovered() )
        {
            ImGui::SetTooltip("%s",
                _node.metadata.tooltip.c_str());
        }

        // Description below
        imgui_render_field_description(_node, _vs);

        ImGui::PopID();

        if (!en)
        {
            ImGui::EndDisabled();
        }

        return changed;
    }


// ===========================================================================
//  IV.  COMPOSITE RENDERING
// ===========================================================================

    // forward decl - composite renders may recurse into detail nodes
    // (static composites contain field children).
    template<typename _Registry>
    bool
    imgui_render_detail_node(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    );

    // imgui_render_composite
    //   helper: dispatch on _node.composite_archetype.  Common
    // chrome (header label, frame, action toolbar at the bottom)
    // is shared across archetypes; the body either iterates
    // children (field_group) or invokes render_cb (everything
    // else).  Returns true if any contained interaction mutated
    // the registry.
    template<typename _Registry>
    bool
    imgui_render_composite(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        using component::composite_kind;

        if (!_node.metadata.visible)
        {
            return false;
        }

        bool changed = false;

        ImGui::PushID(_node.id.c_str());

        // Header
        if (!_node.metadata.label.empty())
        {
            ImGui::SeparatorText(_node.metadata.label.c_str());
        }
        else
        {
            ImGui::Separator();
        }

        // Description (if any)
        if ( _vs.show_descriptions &&
             !_node.metadata.description.empty() )
        {
            ImGui::TextDisabled("%s",
                _node.metadata.description.c_str());
        }

        // Body - dispatch on archetype
        switch (_node.composite_archetype)
        {
            case composite_kind::field_group:
            {
                // Static composite: walk children as fields.
                for (const auto& child : _node.children)
                {
                    if (imgui_render_detail_node(_reg, child, _vs))
                    {
                        changed = true;
                    }
                }
                break;
            }

            case composite_kind::action_bar:
            {
                // Toolbar-only composite; no body.  The action
                // toolbar is rendered below.
                break;
            }

            case composite_kind::string_list:
            case composite_kind::key_value_list:
            case composite_kind::single_col_table:
            case composite_kind::multi_col_table:
            case composite_kind::tag_set:
            case composite_kind::custom:
            {
                // Dynamic composites: data lives in the registry,
                // user-supplied render_cb draws the inner widget.
                if (_node.render_cb)
                {
                    _node.render_cb(_reg, _node,
                        static_cast<void*>(&_vs));
                }
                else
                {
                    ImGui::TextDisabled(
                        "(composite [%s] has no render_cb)",
                        _node.id.c_str());
                }
                break;
            }

            case composite_kind::none:
            default:
                // Treated as a generic group; walk children.
                for (const auto& child : _node.children)
                {
                    if (imgui_render_detail_node(_reg, child, _vs))
                    {
                        changed = true;
                    }
                }
                break;
        }

        // Action toolbar at the bottom of the composite
        imgui_render_action_toolbar(_node, _vs);

        ImGui::PopID();

        return changed;
    }


// ===========================================================================
//  V.   DETAIL / MASTER PANE RENDERING
// ===========================================================================

    // imgui_render_detail_node
    //   helper: dispatches a single node in the detail pane based
    // on its kind.  Sections / groups produce headers and recurse
    // into their children; fields and composites produce widgets;
    // categories are skipped (they live in the master pane).
    template<typename _Registry>
    bool
    imgui_render_detail_node(
        _Registry&                                   _reg,
        const component::cvar_form_node<_Registry>&  _node,
        imgui_cvar_form_view_state<_Registry>&       _vs
    )
    {
        using component::settings_kind;

        if (!_node.metadata.visible)
        {
            return false;
        }

        bool changed = false;

        switch (_node.kind)
        {
            case settings_kind::field:
            {
                changed = imgui_render_field(_reg, _node, _vs);
                break;
            }

            case settings_kind::composite:
            {
                changed = imgui_render_composite(_reg, _node, _vs);
                break;
            }

            case settings_kind::section:
            {
                // Header + indented children
                if (!_node.metadata.label.empty())
                {
                    ImGui::SeparatorText(
                        _node.metadata.label.c_str());
                }
                else
                {
                    ImGui::Separator();
                }

                if ( _vs.show_descriptions &&
                     !_node.metadata.description.empty() )
                {
                    ImGui::TextDisabled("%s",
                        _node.metadata.description.c_str());
                }

                ImGui::Indent(_vs.section_indent);

                for (const auto& child : _node.children)
                {
                    if (imgui_render_detail_node(_reg, child, _vs))
                    {
                        changed = true;
                    }
                }

                // Section-level action toolbar
                imgui_render_action_toolbar(_node, _vs);

                ImGui::Unindent(_vs.section_indent);
                break;
            }

            case settings_kind::group:
            {
                // Lighter-weight than section: no separator
                if (!_node.metadata.label.empty())
                {
                    ImGui::TextUnformatted(
                        _node.metadata.label.c_str());
                }

                ImGui::Indent(_vs.section_indent);

                for (const auto& child : _node.children)
                {
                    if (imgui_render_detail_node(_reg, child, _vs))
                    {
                        changed = true;
                    }
                }

                imgui_render_action_toolbar(_node, _vs);

                ImGui::Unindent(_vs.section_indent);
                break;
            }

            case settings_kind::label:
            {
                ImGui::TextUnformatted(
                    _node.metadata.label.c_str());
                break;
            }

            case settings_kind::spacer:
            {
                ImGui::Spacing();
                break;
            }

            case settings_kind::category:
            case settings_kind::root:
            default:
                // Categories belong in the master pane; the
                // detail walker should not see them, but if it
                // does, skip silently.
                break;
        }

        return changed;
    }

    // imgui_render_master_node
    //   helper: walks _parent_node, rendering each
    // settings_kind::category child as a tree-node row.  A
    // category that itself contains nested categories is
    // expandable; clicking a row sets _form.select_path to that
    // row's full path.  Nested categories recurse.
    template<typename _Registry>
    void
    imgui_render_master_node(
        component::cvar_form<_Registry>&             _form,
        imgui_cvar_form_view_state<_Registry>&       _vs,
        const component::cvar_form_node<_Registry>&  _parent_node,
        const std::string&                           _parent_path
    )
    {
        for (const auto& child : _parent_node.children)
        {
            if (child.kind != component::settings_kind::category)
            {
                continue;
            }

            const std::string child_path =
                _parent_path.empty()
                    ? child.id
                    : _parent_path + "/" + child.id;

            // Is there a nested category?
            bool has_nested_cat = false;
            for (const auto& gc : child.children)
            {
                if (gc.kind == component::settings_kind::category)
                {
                    has_nested_cat = true;
                    break;
                }
            }

            const bool is_selected =
                (_form.selected_path() == child_path);

            const char* label = child.metadata.label.empty()
                                    ? child.id.c_str()
                                    : child.metadata.label.c_str();

            ImGui::PushID(child.id.c_str());

            if (has_nested_cat)
            {
                ImGuiTreeNodeFlags flags =
                    ImGuiTreeNodeFlags_OpenOnArrow      |
                    ImGuiTreeNodeFlags_OpenOnDoubleClick;

                if (is_selected)
                {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                const bool open =
                    ImGui::TreeNodeEx(label, flags);

                // Click on the node label (not on the arrow)
                // selects the category.
                if ( ImGui::IsItemClicked() &&
                     !ImGui::IsItemToggledOpen() )
                {
                    _form.select_path(child_path);
                }

                if (open)
                {
                    imgui_render_master_node(
                        _form, _vs, child, child_path);
                    ImGui::TreePop();
                }
            }
            else
            {
                if (ImGui::Selectable(label, is_selected))
                {
                    _form.select_path(child_path);
                }
            }

            ImGui::PopID();
        }

        return;
    }

}  // namespace detail


// ===========================================================================
//  VI.  IMGUI_DRAW_CVAR_FORM (TOP-LEVEL)
// ===========================================================================

// imgui_draw_cvar_form_master_pane
//   function: emits the master pane (category tree) of _form into
// the current ImGui scope.  Caller is responsible for placing
// this inside a BeginChild / EndChild or other layout container
// of suitable width.  Returns true when the user changed the
// selection during this call (the form's selected_path is also
// updated in-place).
template<typename _Registry>
bool
imgui_draw_cvar_form_master_pane(
    component::cvar_form<_Registry>&         _form,
    imgui_cvar_form_view_state<_Registry>&   _vs
)
{
    if (!_form.is_attached())
    {
        return false;
    }

    const std::string before = _form.selected_path();

    detail::imgui_render_master_node(
        _form, _vs, _form.root(), std::string{});

    return (before != _form.selected_path());
}


// imgui_draw_cvar_form_detail_pane
//   function: emits the detail pane for the currently-selected
// node of _form.  When nothing is selected (or the selection
// does not resolve), renders a disabled placeholder.  Caller is
// responsible for placing this inside an appropriate scope.
// Returns true if any contained interaction mutated the registry.
template<typename _Registry>
bool
imgui_draw_cvar_form_detail_pane(
    component::cvar_form<_Registry>&         _form,
    imgui_cvar_form_view_state<_Registry>&   _vs
)
{
    if (!_form.is_attached())
    {
        return false;
    }

    auto* sel = _form.selected_node();

    if (sel == nullptr)
    {
        ImGui::TextDisabled("(no category selected)");
        return false;
    }

    bool changed = false;

    // Optional category-level header (some users like a big
    // banner above the detail content).
    if (!sel->metadata.label.empty())
    {
        ImGui::TextUnformatted(sel->metadata.label.c_str());

        if ( _vs.show_descriptions &&
             !sel->metadata.description.empty() )
        {
            ImGui::TextDisabled("%s",
                sel->metadata.description.c_str());
        }

        ImGui::Separator();
    }

    // Walk the selected node's children.  Categories nested
    // inside this category are skipped (they live in the master
    // pane).
    for (const auto& child : sel->children)
    {
        if (child.kind == component::settings_kind::category)
        {
            continue;
        }

        if (detail::imgui_render_detail_node(
                _form.registry(), child, _vs))
        {
            changed = true;
        }
    }

    // Category-level action toolbar at the very bottom.
    detail::imgui_render_action_toolbar(*sel, _vs);

    if (changed)
    {
        _form.mark_dirty();
    }

    return changed;
}


// imgui_draw_cvar_form
//   function: emits the full master / detail layout for _form.
// Splits the available content region between the master pane
// (vs.master_pane_ratio fraction) and the detail pane.  Both
// panes are wrapped in BeginChild scopes so the user can place
// this inside any container window.  Returns true if any
// interaction (selection change or value mutation) occurred.
template<typename _Registry>
bool
imgui_draw_cvar_form(
    component::cvar_form<_Registry>&         _form,
    imgui_cvar_form_view_state<_Registry>&   _vs
)
{
    if (!_form.is_attached())
    {
        return false;
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();

    float ratio = _vs.master_pane_ratio;

    if (ratio < 0.10f) { ratio = 0.10f; }
    if (ratio > 0.90f) { ratio = 0.90f; }

    const float master_w = avail.x * ratio;

    bool changed = false;

    // Master pane
    ImGui::BeginChild(
        "##cvar_form_master",
        ImVec2(master_w, 0.0f),
        0);

    if (imgui_draw_cvar_form_master_pane(_form, _vs))
    {
        changed = true;
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Detail pane
    ImGui::BeginChild(
        "##cvar_form_detail",
        ImVec2(0.0f, 0.0f),
        0);

    if (imgui_draw_cvar_form_detail_pane(_form, _vs))
    {
        changed = true;
    }

    ImGui::EndChild();

    return changed;
}


NS_END  // imgui
NS_END  // uxoxo


#endif  // UXOXO_IMGUI_CVAR_FORM_
