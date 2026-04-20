/*******************************************************************************
* uxoxo [component]                                    database_login_imgui.hpp
*
* Dear ImGui renderer for database_login:
*   Header-only imgui renderer for the generic database_login<_Vendor, _Feat>
* form.  Every slot is rendered only when its feature bit is set in _Feat;
* disabled slots elide at compile time via if constexpr and contribute no
* code, matching the zero-overhead contract of the underlying form.
*
*   This header also exposes a small library of re-usable sub-widget
* renderers (ssl_settings, uri_settings, connect_attrs map) so that the
* vendor-specialized headers (mariadb_login_imgui.hpp,
* mysql_login_imgui.hpp) can share them without duplication.
*
*   No persistent state is kept outside the form itself: ImGui's own
* id-scoped state storage is used for the connect-attrs "add entry"
* scratch buffers, keyed by map address, so that multiple logins
* rendered in the same window stay independent.
*
*   Dependencies:
*     - dear imgui (<imgui.h>)
*     - imgui std::string binding (<misc/cpp/imgui_stdlib.h>)
*     - uxoxo::component::database_login
*
* Contents:
*   1  render configuration
*   2  sub-widget helpers (ssl, uri, connect_attrs, port, charset)
*   3  base-slot renderer (shared across all database_login variants)
*   4  render() overload for database_login<_V, _F>
*   5  render_form() wrapper with submit / error / submitting surface
*
*
* path:      /inc/uxoxo/component/database_login_imgui.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_DATABASE_LOGIN_IMGUI_
#define  UXOXO_COMPONENT_DATABASE_LOGIN_IMGUI_ 1

// std
#include <cfloat>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
// imgui
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include <uxoxo>
#include <uxoxo/component/database_login.hpp>


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  RENDER CONFIGURATION
// ===============================================================================
//   Per-call customization of labels, widths, and layout.  All fields
// default to sensible English labels and ImGui-default sizing; callers
// override only the pieces they need.  Passed by const reference so that
// a default-constructed instance can be supplied as a temporary.

// database_login_imgui_config
//   struct: customization knobs for imgui rendering of a database_login.
struct database_login_imgui_config
{
    // -- field labels (base slots) ------------------------------------
    const char* username_label       = "Username";
    const char* password_label       = "Password";
    const char* remember_me_label    = "Remember me";
    const char* show_password_label  = "Show password";
    const char* host_label           = "Host";
    const char* port_label           = "Port";
    const char* database_name_label  = "Database";
    const char* schema_label         = "Schema";
    const char* charset_label        = "Charset";

    // -- section labels (composite slots) -----------------------------
    const char* ssl_header_label     = "SSL / TLS";
    const char* uri_header_label     = "Connection URI";

    // -- form-level labels --------------------------------------------
    const char* submit_button_label  = "Connect";
    const char* reset_button_label   = "Reset";
    const char* submitting_label     = "Connecting...";

    // -- layout -------------------------------------------------------
    float       field_width          = 0.0f;    // 0.0 => ImGui default
    bool        show_reset_button    = true;
    bool        validate_on_submit   = true;
};




// ===============================================================================
//  2  SUB-WIDGET HELPERS
// ===============================================================================
//   Each helper renders one composite-typed slot value.  They are
// deliberately independent of database_login so the vendor headers can
// reuse them for identically-shaped sub-structs.

// dbl_imgui_render_ssl_settings
//   function: renders an ssl_settings aggregate inside a collapsing
// header.  Returns true if any field was edited this frame.
inline bool
dbl_imgui_render_ssl_settings(
    ssl_settings& _s,
    const char*   _header_label = "SSL / TLS"
)
{
    bool edited = false;

    if (ImGui::CollapsingHeader(_header_label))
    {
        edited |= ImGui::Checkbox("Enable",                   &_s.enable);

        ImGui::BeginDisabled(!_s.enable);
        edited |= ImGui::Checkbox("Verify peer certificate",  &_s.verify);
        edited |= ImGui::InputText("CA certificate path",     &_s.ca);
        edited |= ImGui::InputText("Client certificate path", &_s.cert);
        edited |= ImGui::InputText("Client key path",         &_s.key);
        ImGui::EndDisabled();
    }

    return edited;
}

// dbl_imgui_render_uri_settings
//   function: renders a uri_settings aggregate (active toggle plus the
// URI text_input) inside a collapsing header.  Returns true if any
// field was edited.
inline bool
dbl_imgui_render_uri_settings(
    uri_settings& _u,
    const char*   _header_label = "Connection URI"
)
{
    bool edited = false;

    if (ImGui::CollapsingHeader(_header_label))
    {
        edited |= ImGui::Checkbox("Use URI (overrides host/port/db)",
                                  &_u.active);

        ImGui::BeginDisabled(!_u.active);
        edited |= ImGui::InputText("URI", &_u.uri.value);
        ImGui::EndDisabled();
    }

    return edited;
}

// dbl_imgui_render_connect_attrs
//   function: renders a std::map<string,string> as an editable table
// inside a collapsing header.  Existing entries expose an inline edit
// field and a remove button; a persistent "add entry" row at the
// bottom supports appending new keys.  Returns true if the map was
// mutated this frame.
//
//   Scratch state for the add-entry row is keyed off the map's
// address via ImGui's id scope, so rendering two different maps in
// the same window does not cross-contaminate the buffers.
inline bool
dbl_imgui_render_connect_attrs(
    std::map<std::string, std::string>& _attrs,
    const char*                         _header_label = "Connect attributes"
)
{
    bool mutated = false;

    if (ImGui::CollapsingHeader(_header_label))
    {
        ImGui::PushID(&_attrs);

        // -- existing entries -----------------------------------------
        if (ImGui::BeginTable("##attrs_table", 3,
                              ( ImGuiTableFlags_Borders          |
                                ImGuiTableFlags_SizingStretchProp )))
        {
            ImGui::TableSetupColumn("Key");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("",
                                    ImGuiTableColumnFlags_WidthFixed,
                                    24.0f);
            ImGui::TableHeadersRow();

            std::string to_remove;

            for (auto& [k, v] : _attrs)
            {
                ImGui::TableNextRow();

                ImGui::PushID(k.c_str());

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(k.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);

                if (ImGui::InputText("##val", &v))
                {
                    mutated = true;
                }

                ImGui::TableSetColumnIndex(2);

                if (ImGui::SmallButton("x"))
                {
                    to_remove = k;
                }

                ImGui::PopID();
            }

            ImGui::EndTable();

            if (!to_remove.empty())
            {
                _attrs.erase(to_remove);
                mutated = true;
            }
        }

        // -- add-entry row --------------------------------------------
        //   Scratch strings live in ImGui's storage keyed under this
        // push-id scope.  GetStateStorage returns the current window's
        // storage; we use the pointer-to-string trick of stashing a
        // heap-allocated std::string behind a GetVoidPtr slot.  Much
        // simpler alternative: per-process static keyed by map address.
        // We choose the static because the add-entry buffers are
        // transient UI scratch and do not warrant custom allocation.

        static thread_local
        std::map<const void*, std::pair<std::string, std::string>>
            s_scratch;

        auto& scratch = s_scratch[&_attrs];

        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputTextWithHint("##new_key",   "key",   &scratch.first);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##new_value", "value", &scratch.second);

        ImGui::SameLine();

        const bool can_add =
            ( !scratch.first.empty() &&
              (_attrs.find(scratch.first) == _attrs.end()) );

        ImGui::BeginDisabled(!can_add);

        if (ImGui::Button("Add"))
        {
            _attrs.emplace(std::move(scratch.first),
                           std::move(scratch.second));
            scratch.first.clear();
            scratch.second.clear();
            mutated = true;
        }

        ImGui::EndDisabled();

        ImGui::PopID();
    }

    return mutated;
}

// dbl_imgui_render_port
//   function: renders a std::uint16_t port as an InputScalar (clamped
// to the full u16 range).  Returns true if the value was edited.
inline bool
dbl_imgui_render_port(
    std::uint16_t& _port,
    const char*    _label = "Port"
)
{
    int temp = static_cast<int>(_port);

    const bool edited =
        ImGui::InputInt(_label, &temp, 1, 100);

    if (edited)
    {
        if (temp < 0)
        {
            temp = 0;
        }
        else if (temp > 65535)
        {
            temp = 65535;
        }

        _port = static_cast<std::uint16_t>(temp);
    }

    return edited;
}




// ===============================================================================
//  3  BASE-SLOT RENDERER
// ===============================================================================
//   Renders the subset of slots defined in database_login.hpp (the
// dbl_* surface) on any type that exposes them via tc_get<dbl_tag::*>.
// Because mariadb_login<_F> and mysql_login<_F> both inherit a form
// that carries the full dbl_* tag catalogue, this single template can
// drive the shared portion of all three vendor renderers.
//
//   The generic parameter _Login must satisfy
// database_login_traits::is_database_login_v<_Login>, and its feature
// mask is queried through the type's static has_* booleans.

// dbl_imgui_render_base_slots
//   function: renders every enabled base (dbl_*) slot of _login.
// Disabled slots elide at compile time.  Returns true if any slot
// was edited this frame.
//
//   Uses the uniform has_field<Tag>() accessor inherited from the
// form<> base rather than the vendor-specific has_* static members:
// mariadb_login and mysql_login deliberately expose only a subset of
// those (omitting has_remember_me / has_show_password), but all three
// login types inherit has_field<Tag>() intact, so routing through it
// keeps this template portable across every vendor.
template <typename _Login>
bool
dbl_imgui_render_base_slots(
    _Login&                            _login,
    const database_login_imgui_config& _cfg = {}
)
{
    bool edited = false;

    if (_cfg.field_width != 0.0f)
    {
        ImGui::PushItemWidth(_cfg.field_width);
    }

    // -- identity -----------------------------------------------------
    if constexpr (_Login::template has_field<dbl_tag::username_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.username_label,
            &tc_get<dbl_tag::username_tag>(_login).value);
    }

    if constexpr (_Login::template has_field<dbl_tag::password_tag>())
    {
        auto& pw = tc_get<dbl_tag::password_tag>(_login);

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;

        if (pw.masked)
        {
            flags |= ImGuiInputTextFlags_Password;
        }

        edited |= ImGui::InputText(_cfg.password_label,
                                   &pw.value,
                                   flags);
    }

    if constexpr (_Login::template has_field<dbl_tag::show_password_tag>())
    {
        auto& show = tc_get<dbl_tag::show_password_tag>(_login);

        if (ImGui::Checkbox(_cfg.show_password_label, &show))
        {
            edited = true;

            // keep the paired password field's masked flag in sync, so
            // the next InputText draw picks up the new visibility.
            if constexpr (_Login::template has_field<dbl_tag::password_tag>())
            {
                tc_get<dbl_tag::password_tag>(_login).masked = !show;
            }
        }
    }

    if constexpr (_Login::template has_field<dbl_tag::remember_me_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.remember_me_label,
            &tc_get<dbl_tag::remember_me_tag>(_login));
    }

    // -- connection target --------------------------------------------
    if constexpr ( _Login::template has_field<dbl_tag::host_tag>()          ||
                   _Login::template has_field<dbl_tag::port_tag>()          ||
                   _Login::template has_field<dbl_tag::database_name_tag>() ||
                   _Login::template has_field<dbl_tag::schema_tag>() )
    {
        ImGui::Separator();
    }

    if constexpr (_Login::template has_field<dbl_tag::host_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.host_label,
            &tc_get<dbl_tag::host_tag>(_login).value);
    }

    if constexpr (_Login::template has_field<dbl_tag::port_tag>())
    {
        edited |= dbl_imgui_render_port(
            tc_get<dbl_tag::port_tag>(_login),
            _cfg.port_label);
    }

    if constexpr (_Login::template has_field<dbl_tag::database_name_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.database_name_label,
            &tc_get<dbl_tag::database_name_tag>(_login).value);
    }

    if constexpr (_Login::template has_field<dbl_tag::schema_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.schema_label,
            &tc_get<dbl_tag::schema_tag>(_login).value);
    }

    // -- charset ------------------------------------------------------
    if constexpr (_Login::template has_field<dbl_tag::charset_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.charset_label,
            &tc_get<dbl_tag::charset_tag>(_login));
    }

    // -- ssl ----------------------------------------------------------
    if constexpr (_Login::template has_field<dbl_tag::ssl_tag>())
    {
        edited |= dbl_imgui_render_ssl_settings(
            tc_get<dbl_tag::ssl_tag>(_login),
            _cfg.ssl_header_label);
    }

    // -- uri ----------------------------------------------------------
    if constexpr (_Login::template has_field<dbl_tag::uri_tag>())
    {
        edited |= dbl_imgui_render_uri_settings(
            tc_get<dbl_tag::uri_tag>(_login),
            _cfg.uri_header_label);
    }

    if (_cfg.field_width != 0.0f)
    {
        ImGui::PopItemWidth();
    }

    return edited;
}




// ===============================================================================
//  4  RENDER OVERLOAD FOR database_login
// ===============================================================================
//   Top-level field renderer for the generic database_login.  Does not
// draw the submit / error / submitting surface; see render_form() for
// the full form shell.

// dbl_imgui_render
//   function: renders every enabled field of a database_login<_V, _F>.
// Returns true if any field was edited this frame.  Does not draw a
// submit button or error area; use dbl_imgui_render_form() for the
// full shell.
template <djinterp::db::database_type _V,
          unsigned                    _F>
bool
dbl_imgui_render(
    database_login<_V, _F>&            _dbl,
    const database_login_imgui_config& _cfg = {}
)
{
    ImGui::PushID(&_dbl);

    const bool edited = dbl_imgui_render_base_slots(_dbl, _cfg);

    ImGui::PopID();

    return edited;
}




// ===============================================================================
//  5  FORM SHELL
// ===============================================================================
//   Wraps the field renderer with the form-level surface inherited
// from form_builder: submitting / enabled state, error_message display,
// and submit / reset buttons.  The on_submit callback is invoked only
// after dbl_validate returns true (unless _cfg.validate_on_submit is
// set to false).
//
//   Template is parameterized on any type that satisfies
// is_database_login_v — the generic database_login, mariadb_login,
// mysql_login, and future vendor logins all dispatch through here via
// the vendor headers' render_form() overloads.

/*
dbl_imgui_render_form
  Renders a full login form: fields, error banner, submit and reset
buttons.  Returns true if the form was successfully submitted this
frame (validation passed and on_submit fired).

  Respects the form-level state surface:
  - `submitting` disables all controls (shows _cfg.submitting_label in
    place of the submit button label).
  - `enabled == false` disables all controls unconditionally.
  - `error_message` is drawn as a red banner above the button row when
    non-empty.

  The caller is expected to clear `submitting` / `error_message` when
its async connection attempt completes.

  This overload binds to database_login<_V, _F> specifically;
mariadb_login and mysql_login have their own render_form overloads
(mdl_imgui_render_form, ml_imgui_render_form) so that the appropriate
vendor validator (mdl_validate, ml_validate) is dispatched.

Parameter(s):
  _dbl: the database_login to render.
  _cfg: customization knobs; defaults suit English-language UIs.
Return:
  true  if the submit button was clicked AND dbl_validate returned
        true AND on_submit was invoked this frame;
  false otherwise.
*/
template <djinterp::db::database_type _V,
          unsigned                    _F>
bool
dbl_imgui_render_form(
    database_login<_V, _F>&            _dbl,
    const database_login_imgui_config& _cfg = {}
)
{
    bool submitted = false;

    ImGui::PushID(&_dbl);

    // whole form disabled when submitting or the enabled flag is off.
    const bool controls_disabled = ( _dbl.submitting || !_dbl.enabled );

    ImGui::BeginDisabled(controls_disabled);

    dbl_imgui_render_base_slots(_dbl, _cfg);

    ImGui::EndDisabled();

    // -- error banner -------------------------------------------------
    if (!_dbl.error_message.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4{1.0f, 0.35f, 0.35f, 1.0f},
                           "%s",
                           _dbl.error_message.c_str());
    }

    ImGui::Spacing();

    // -- submit button ------------------------------------------------
    {
        ImGui::BeginDisabled(controls_disabled);

        const char* label = ( _dbl.submitting
                              ? _cfg.submitting_label
                              : _cfg.submit_button_label );

        if (ImGui::Button(label))
        {
            const bool ok = ( !_cfg.validate_on_submit ||
                              dbl_validate(_dbl) );

            if (ok)
            {
                if (_dbl.on_submit)
                {
                    _dbl.on_submit(_dbl);
                }

                submitted = true;
            }
        }

        ImGui::EndDisabled();
    }

    // -- reset button -------------------------------------------------
    if (_cfg.show_reset_button)
    {
        ImGui::SameLine();
        ImGui::BeginDisabled(controls_disabled);

        if (ImGui::Button(_cfg.reset_button_label))
        {
            dbl_reset_to_defaults(_dbl);
        }

        ImGui::EndDisabled();
    }

    ImGui::PopID();

    return submitted;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DATABASE_LOGIN_IMGUI_
