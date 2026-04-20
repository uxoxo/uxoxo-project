/*******************************************************************************
* uxoxo [component]                                       mysql_login_imgui.hpp
*
* Dear ImGui renderer for mysql_login:
*   Vendor-specialized imgui renderer layered on top of
* database_login_imgui.hpp.  The base (dbl_*) slot surface is delegated
* to dbl_imgui_render_base_slots(); this header adds the mysql_login
* mylf_* surface on top:
*     - MySQL-family common extras (unix_socket, init_command,
*       compression, multi_statement, local_infile, connect_attrs).
*     - MySQL-specific slots: ssl_mode (combo over mysql_ssl_mode
*       enum), auth_plugin, X Protocol endpoint, session tracking,
*       cleartext plugin, statement-cache size.
*
*   Zero duplication with database_login_imgui: the shared sub-widget
* helpers (dbl_imgui_render_connect_attrs, dbl_imgui_render_port) are
* invoked directly by their canonical names.
*
*   Zero overhead: every mylf_* block is guarded by `if constexpr
* (has_...)` against mysql_login<_F>::has_*, so disabled slots emit no
* code.
*
* Contents:
*   1  vendor-widget helpers (mysql_ssl_settings, x_protocol_settings)
*   2  vendor-slot renderer (mylf_* surface)
*   3  render() overload for mysql_login
*   4  render_form() overload for mysql_login
*
*
* path:      /inc/uxoxo/component/mysql_login_imgui.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_MYSQL_LOGIN_IMGUI_
#define  UXOXO_COMPONENT_MYSQL_LOGIN_IMGUI_ 1

// std
#include <cstddef>
#include <cstdint>
#include <string>
// imgui
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
// djinterp
#include <djinterp/database/mysql.hpp>
// uxoxo
#include <uxoxo>
#include <uxoxo/component/mysql_login.hpp>
#include <uxoxo/component/database_login_imgui.hpp>


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  VENDOR-WIDGET HELPERS
// ===============================================================================

// mysql_ssl_mode_labels
//   type: label array for the mysql_ssl_mode combo.  Five entries in
// canonical order (disabled, preferred, required, verify_ca,
// verify_identity) — if the underlying djinterp enum ever diverges,
// override via mysql_login_imgui_config::ssl_mode_labels.
struct mysql_ssl_mode_labels
{
    const char* disabled        = "disabled";
    const char* preferred       = "preferred";
    const char* required        = "required";
    const char* verify_ca       = "verify_ca";
    const char* verify_identity = "verify_identity";
};

// mysql_login_imgui_config
//   struct: customization knobs specific to the mysql_login surface.
// Composes a database_login_imgui_config (base slot labels) with
// vendor-slot labels and the ssl_mode combo dictionary.
struct mysql_login_imgui_config
{
    database_login_imgui_config base {};

    // -- mysql-family common extras ----------------------------------
    const char*            unix_socket_label        = "Unix socket";
    const char*            init_command_label       = "Init command";
    const char*            compression_label        = "Enable compression";
    const char*            multi_stmt_label         = "Allow multiple statements";
    const char*            local_infile_label       = "Allow LOAD DATA LOCAL INFILE";
    const char*            connect_attrs_header     = "Connect attributes";

    // -- mysql-specific ----------------------------------------------
    const char*            ssl_mode_header_label    = "SSL mode";
    const char*            ssl_mode_label           = "Mode";
    mysql_ssl_mode_labels  ssl_mode_labels          {};
    const char*            ssl_mode_ca_label        = "CA certificate path";
    const char*            ssl_mode_cert_label      = "Client certificate path";
    const char*            ssl_mode_key_label       = "Client key path";
    const char*            ssl_mode_tls_ver_label   = "TLS versions";
    const char*            ssl_mode_ciphers_label   = "TLS cipher suites";

    const char*            auth_plugin_label        = "Auth plugin";
    const char*            x_protocol_header_label  = "X Protocol";
    const char*            session_tracking_label   = "Session state tracking";
    const char*            cleartext_plugin_label   = "Allow cleartext auth plugin";
    const char*            stmt_cache_label         = "Statement cache size";
};

// ml_imgui_render_mysql_ssl_settings
//   function: renders a mysql_ssl_settings aggregate inside a
// collapsing header.  The mode is presented as a combo backed by the
// label array in _cfg.  Returns true if any field was edited this
// frame.
inline bool
ml_imgui_render_mysql_ssl_settings(
    mysql_ssl_settings&             _s,
    const mysql_login_imgui_config& _cfg           = {},
    const char*                     _header_label  = "SSL mode"
)
{
    bool edited = false;

    if (ImGui::CollapsingHeader(_header_label))
    {
        // -- ssl mode combo -------------------------------------------
        const char* const items[] = {
            _cfg.ssl_mode_labels.disabled,
            _cfg.ssl_mode_labels.preferred,
            _cfg.ssl_mode_labels.required,
            _cfg.ssl_mode_labels.verify_ca,
            _cfg.ssl_mode_labels.verify_identity
        };

        int current = static_cast<int>(_s.mode);

        if (current < 0)
        {
            current = 0;
        }
        else if (current > 4)
        {
            current = 4;
        }

        if (ImGui::Combo(_cfg.ssl_mode_label,
                         &current,
                         items,
                         IM_ARRAYSIZE(items)))
        {
            _s.mode  = static_cast<djinterp::db::mysql_ssl_mode>(current);
            edited   = true;
        }

        // -- paths & tls strings --------------------------------------
        const bool paths_enabled =
            ( _s.mode != djinterp::db::mysql_ssl_mode::disabled );

        ImGui::BeginDisabled(!paths_enabled);

        edited |= ImGui::InputText(_cfg.ssl_mode_ca_label,      &_s.ca);
        edited |= ImGui::InputText(_cfg.ssl_mode_cert_label,    &_s.cert);
        edited |= ImGui::InputText(_cfg.ssl_mode_key_label,     &_s.key);
        edited |= ImGui::InputText(_cfg.ssl_mode_tls_ver_label, &_s.tls_version);
        edited |= ImGui::InputText(_cfg.ssl_mode_ciphers_label, &_s.tls_ciphersuites);

        ImGui::EndDisabled();
    }

    return edited;
}

// ml_imgui_render_x_protocol_settings
//   function: renders an x_protocol_settings aggregate (enable toggle
// plus endpoint port) inside a collapsing header.  Returns true if
// any field was edited this frame.
inline bool
ml_imgui_render_x_protocol_settings(
    x_protocol_settings& _x,
    const char*          _header_label = "X Protocol"
)
{
    bool edited = false;

    if (ImGui::CollapsingHeader(_header_label))
    {
        edited |= ImGui::Checkbox("Enable X Protocol", &_x.enabled);

        ImGui::BeginDisabled(!_x.enabled);
        edited |= dbl_imgui_render_port(_x.port, "X Protocol port");
        ImGui::EndDisabled();
    }

    return edited;
}




// ===============================================================================
//  2  VENDOR-SLOT RENDERER
// ===============================================================================

// ml_imgui_render_vendor_slots
//   function: renders every enabled mylf_* slot of _ml.  Disabled slots
// elide at compile time.  Returns true if any slot was edited this
// frame.
template <unsigned _F>
bool
ml_imgui_render_vendor_slots(
    mysql_login<_F>&                _ml,
    const mysql_login_imgui_config& _cfg = {}
)
{
    using login_t = mysql_login<_F>;

    bool edited = false;

    // -- mysql-family common extras ----------------------------------
    if constexpr ( login_t::template has_field<mylf_tag::unix_socket_tag>()   ||
                   login_t::template has_field<mylf_tag::init_command_tag>()  ||
                   login_t::template has_field<mylf_tag::compression_tag>()   ||
                   login_t::template has_field<mylf_tag::multi_stmt_tag>()    ||
                   login_t::template has_field<mylf_tag::local_infile_tag>()  ||
                   login_t::template has_field<mylf_tag::connect_attrs_tag>() )
    {
        ImGui::Separator();
        ImGui::TextDisabled("MySQL-family options");
    }

    if constexpr (login_t::template has_field<mylf_tag::unix_socket_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.unix_socket_label,
            &tc_get<mylf_tag::unix_socket_tag>(_ml).value);
    }

    if constexpr (login_t::template has_field<mylf_tag::init_command_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.init_command_label,
            &tc_get<mylf_tag::init_command_tag>(_ml).value);
    }

    if constexpr (login_t::template has_field<mylf_tag::compression_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.compression_label,
            &tc_get<mylf_tag::compression_tag>(_ml));
    }

    if constexpr (login_t::template has_field<mylf_tag::multi_stmt_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.multi_stmt_label,
            &tc_get<mylf_tag::multi_stmt_tag>(_ml));
    }

    if constexpr (login_t::template has_field<mylf_tag::local_infile_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.local_infile_label,
            &tc_get<mylf_tag::local_infile_tag>(_ml));
    }

    if constexpr (login_t::template has_field<mylf_tag::connect_attrs_tag>())
    {
        edited |= dbl_imgui_render_connect_attrs(
            tc_get<mylf_tag::connect_attrs_tag>(_ml),
            _cfg.connect_attrs_header);
    }

    // -- mysql-specific ----------------------------------------------
    if constexpr ( login_t::template has_field<mylf_tag::ssl_mode_tag>()         ||
                   login_t::template has_field<mylf_tag::auth_plugin_tag>()      ||
                   login_t::template has_field<mylf_tag::x_protocol_tag>()       ||
                   login_t::template has_field<mylf_tag::session_tracking_tag>() ||
                   login_t::template has_field<mylf_tag::cleartext_plugin_tag>() ||
                   login_t::template has_field<mylf_tag::stmt_cache_tag>() )
    {
        ImGui::Separator();
        ImGui::TextDisabled("MySQL options");
    }

    if constexpr (login_t::template has_field<mylf_tag::ssl_mode_tag>())
    {
        edited |= ml_imgui_render_mysql_ssl_settings(
            tc_get<mylf_tag::ssl_mode_tag>(_ml),
            _cfg,
            _cfg.ssl_mode_header_label);
    }

    if constexpr (login_t::template has_field<mylf_tag::auth_plugin_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.auth_plugin_label,
            &tc_get<mylf_tag::auth_plugin_tag>(_ml).value);
    }

    if constexpr (login_t::template has_field<mylf_tag::x_protocol_tag>())
    {
        edited |= ml_imgui_render_x_protocol_settings(
            tc_get<mylf_tag::x_protocol_tag>(_ml),
            _cfg.x_protocol_header_label);
    }

    if constexpr (login_t::template has_field<mylf_tag::session_tracking_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.session_tracking_label,
            &tc_get<mylf_tag::session_tracking_tag>(_ml));
    }

    if constexpr (login_t::template has_field<mylf_tag::cleartext_plugin_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.cleartext_plugin_label,
            &tc_get<mylf_tag::cleartext_plugin_tag>(_ml));
    }

    if constexpr (login_t::template has_field<mylf_tag::stmt_cache_tag>())
    {
        auto&           slot = tc_get<mylf_tag::stmt_cache_tag>(_ml);
        std::uint64_t   temp = static_cast<std::uint64_t>(slot);

        if (ImGui::InputScalar(_cfg.stmt_cache_label,
                               ImGuiDataType_U64,
                               &temp))
        {
            slot   = static_cast<std::size_t>(temp);
            edited = true;
        }
    }

    return edited;
}




// ===============================================================================
//  3  RENDER OVERLOAD FOR mysql_login
// ===============================================================================

// ml_imgui_render
//   function: renders every enabled field (base + vendor) of a
// mysql_login<_F>.  Returns true if any field was edited this frame.
// Does not draw a submit button or error area; use
// ml_imgui_render_form() for the full shell.
template <unsigned _F>
bool
ml_imgui_render(
    mysql_login<_F>&                _ml,
    const mysql_login_imgui_config& _cfg = {}
)
{
    bool edited = false;

    ImGui::PushID(&_ml);

    edited |= dbl_imgui_render_base_slots(_ml, _cfg.base);
    edited |= ml_imgui_render_vendor_slots(_ml, _cfg);

    ImGui::PopID();

    return edited;
}




// ===============================================================================
//  4  FORM SHELL
// ===============================================================================

/*
ml_imgui_render_form
  Renders a full mysql_login form: base fields, vendor fields, error
banner, submit and reset buttons.  Returns true if the form was
successfully submitted this frame (validation passed via ml_validate
and on_submit fired).

  Respects the form-level state surface inherited from form_builder
(submitting, enabled, error_message) exactly the same way
dbl_imgui_render_form does for the generic database_login.

Parameter(s):
  _ml:  the mysql login form to render.
  _cfg: customization knobs; defaults suit English-language UIs.
Return:
  true  if the submit button was clicked AND ml_validate returned
        true AND on_submit was invoked this frame;
  false otherwise.
*/
template <unsigned _F>
bool
ml_imgui_render_form(
    mysql_login<_F>&                _ml,
    const mysql_login_imgui_config& _cfg = {}
)
{
    bool submitted = false;

    ImGui::PushID(&_ml);

    const bool controls_disabled = ( _ml.submitting || !_ml.enabled );

    ImGui::BeginDisabled(controls_disabled);

    dbl_imgui_render_base_slots  (_ml, _cfg.base);
    ml_imgui_render_vendor_slots (_ml, _cfg);

    ImGui::EndDisabled();

    // -- error banner -------------------------------------------------
    if (!_ml.error_message.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4{1.0f, 0.35f, 0.35f, 1.0f},
                           "%s",
                           _ml.error_message.c_str());
    }

    ImGui::Spacing();

    // -- submit button ------------------------------------------------
    {
        ImGui::BeginDisabled(controls_disabled);

        const char* label = ( _ml.submitting
                              ? _cfg.base.submitting_label
                              : _cfg.base.submit_button_label );

        if (ImGui::Button(label))
        {
            const bool ok = ( !_cfg.base.validate_on_submit ||
                              ml_validate(_ml) );

            if (ok)
            {
                if (_ml.on_submit)
                {
                    _ml.on_submit(_ml);
                }

                submitted = true;
            }
        }

        ImGui::EndDisabled();
    }

    // -- reset button -------------------------------------------------
    if (_cfg.base.show_reset_button)
    {
        ImGui::SameLine();
        ImGui::BeginDisabled(controls_disabled);

        if (ImGui::Button(_cfg.base.reset_button_label))
        {
            ml_reset_to_defaults(_ml);
        }

        ImGui::EndDisabled();
    }

    ImGui::PopID();

    return submitted;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MYSQL_LOGIN_IMGUI_
