/*******************************************************************************
* uxoxo [component]                                     mariadb_login_imgui.hpp
*
* Dear ImGui renderer for mariadb_login:
*   Vendor-specialized imgui renderer layered on top of
* database_login_imgui.hpp.  The base (dbl_*) slot surface is delegated
* to dbl_imgui_render_base_slots(); this header adds the mariadb_login
* mdlf_* surface on top:
*     - MySQL-family common extras (unix_socket, init_command,
*       compression, multi_statement, local_infile, connect_attrs).
*     - MariaDB-specific slots (default storage engine, Galera
*       settings).
*
*   Zero duplication with database_login_imgui: the shared sub-widget
* helpers (dbl_imgui_render_ssl_settings, dbl_imgui_render_connect_attrs,
* dbl_imgui_render_port) are invoked directly by their canonical names.
*
*   Zero overhead: every mdlf_* block is guarded by `if constexpr
* (has_...)` against mariadb_login<_F>::has_*, so disabled slots emit
* no code.
*
* Contents:
*   1  vendor-widget helpers (galera_settings)
*   2  vendor-slot renderer (mdlf_* surface)
*   3  render() overload for mariadb_login
*   4  render_form() overload for mariadb_login
*
*
* path:      /inc/uxoxo/component/mariadb_login_imgui.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         date: 2026.04.19
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_MARIADB_LOGIN_IMGUI_
#define  UXOXO_COMPONENT_MARIADB_LOGIN_IMGUI_ 1

// std
#include <string>
// imgui
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
// uxoxo
#include <uxoxo>
#include <uxoxo/component/mariadb_login.hpp>
#include <uxoxo/component/database_login_imgui.hpp>


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  VENDOR-WIDGET HELPERS
// ===============================================================================

// mariadb_login_imgui_config
//   struct: customization knobs specific to the mariadb_login surface.
// Composes a database_login_imgui_config (base slot labels) with
// vendor-slot labels; defaults are English and match the djinterp
// vendor vocabulary.
struct mariadb_login_imgui_config
{
    database_login_imgui_config base {};

    // -- mysql-family common extras ----------------------------------
    const char* unix_socket_label    = "Unix socket";
    const char* init_command_label   = "Init command";
    const char* compression_label    = "Enable compression";
    const char* multi_stmt_label     = "Allow multiple statements";
    const char* local_infile_label   = "Allow LOAD DATA LOCAL INFILE";
    const char* connect_attrs_header = "Connect attributes";

    // -- mariadb-specific --------------------------------------------
    const char* storage_engine_label = "Default storage engine";
    const char* galera_header_label  = "Galera cluster";
};

// mdl_imgui_render_galera_settings
//   function: renders a galera_settings aggregate inside a collapsing
// header.  Returns true if either flag was edited this frame.
inline bool
mdl_imgui_render_galera_settings(
    galera_settings& _g,
    const char*      _header_label = "Galera cluster"
)
{
    bool edited = false;

    if (ImGui::CollapsingHeader(_header_label))
    {
        edited |= ImGui::Checkbox("wsrep_sync_wait",    &_g.wsrep_sync_wait);
        edited |= ImGui::Checkbox("wsrep_causal_reads", &_g.wsrep_causal_reads);
    }

    return edited;
}




// ===============================================================================
//  2  VENDOR-SLOT RENDERER
// ===============================================================================
//   Renders only the mdlf_* surface.  Intended to be called after
// dbl_imgui_render_base_slots when a caller wants explicit control
// over section ordering; the convenience wrappers below handle the
// standard layout.

// mdl_imgui_render_vendor_slots
//   function: renders every enabled mdlf_* slot of _mdl.  Disabled
// slots elide at compile time.  Returns true if any slot was edited
// this frame.
template <unsigned _F>
bool
mdl_imgui_render_vendor_slots(
    mariadb_login<_F>&                _mdl,
    const mariadb_login_imgui_config& _cfg = {}
)
{
    using login_t = mariadb_login<_F>;

    bool edited = false;

    // -- mysql-family common extras ---------------------------------
    if constexpr ( login_t::template has_field<mdlf_tag::unix_socket_tag>()   ||
                   login_t::template has_field<mdlf_tag::init_command_tag>()  ||
                   login_t::template has_field<mdlf_tag::compression_tag>()   ||
                   login_t::template has_field<mdlf_tag::multi_stmt_tag>()    ||
                   login_t::template has_field<mdlf_tag::local_infile_tag>()  ||
                   login_t::template has_field<mdlf_tag::connect_attrs_tag>() )
    {
        ImGui::Separator();
        ImGui::TextDisabled("MySQL-family options");
    }

    if constexpr (login_t::template has_field<mdlf_tag::unix_socket_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.unix_socket_label,
            &tc_get<mdlf_tag::unix_socket_tag>(_mdl).value);
    }

    if constexpr (login_t::template has_field<mdlf_tag::init_command_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.init_command_label,
            &tc_get<mdlf_tag::init_command_tag>(_mdl).value);
    }

    if constexpr (login_t::template has_field<mdlf_tag::compression_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.compression_label,
            &tc_get<mdlf_tag::compression_tag>(_mdl));
    }

    if constexpr (login_t::template has_field<mdlf_tag::multi_stmt_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.multi_stmt_label,
            &tc_get<mdlf_tag::multi_stmt_tag>(_mdl));
    }

    if constexpr (login_t::template has_field<mdlf_tag::local_infile_tag>())
    {
        edited |= ImGui::Checkbox(
            _cfg.local_infile_label,
            &tc_get<mdlf_tag::local_infile_tag>(_mdl));
    }

    if constexpr (login_t::template has_field<mdlf_tag::connect_attrs_tag>())
    {
        edited |= dbl_imgui_render_connect_attrs(
            tc_get<mdlf_tag::connect_attrs_tag>(_mdl),
            _cfg.connect_attrs_header);
    }

    // -- mariadb-specific --------------------------------------------
    if constexpr ( login_t::template has_field<mdlf_tag::storage_engine_tag>() ||
                   login_t::template has_field<mdlf_tag::galera_tag>() )
    {
        ImGui::Separator();
        ImGui::TextDisabled("MariaDB options");
    }

    if constexpr (login_t::template has_field<mdlf_tag::storage_engine_tag>())
    {
        edited |= ImGui::InputText(
            _cfg.storage_engine_label,
            &tc_get<mdlf_tag::storage_engine_tag>(_mdl).value);
    }

    if constexpr (login_t::template has_field<mdlf_tag::galera_tag>())
    {
        edited |= mdl_imgui_render_galera_settings(
            tc_get<mdlf_tag::galera_tag>(_mdl),
            _cfg.galera_header_label);
    }

    return edited;
}




// ===============================================================================
//  3  RENDER OVERLOAD FOR mariadb_login
// ===============================================================================

// mdl_imgui_render
//   function: renders every enabled field (base + vendor) of a
// mariadb_login<_F>.  Returns true if any field was edited this
// frame.  Does not draw a submit button or error area; use
// mdl_imgui_render_form() for the full shell.
template <unsigned _F>
bool
mdl_imgui_render(
    mariadb_login<_F>&                _mdl,
    const mariadb_login_imgui_config& _cfg = {}
)
{
    bool edited = false;

    ImGui::PushID(&_mdl);

    edited |= dbl_imgui_render_base_slots (_mdl, _cfg.base);
    edited |= mdl_imgui_render_vendor_slots(_mdl, _cfg);

    ImGui::PopID();

    return edited;
}




// ===============================================================================
//  4  FORM SHELL
// ===============================================================================

/*
mdl_imgui_render_form
  Renders a full mariadb_login form: base fields, vendor fields,
error banner, submit and reset buttons.  Returns true if the form
was successfully submitted this frame (validation passed via
mdl_validate and on_submit fired).

  Respects the form-level state surface inherited from form_builder
(submitting, enabled, error_message) exactly the same way
dbl_imgui_render_form does for the generic database_login.

Parameter(s):
  _mdl: the mariadb login form to render.
  _cfg: customization knobs; defaults suit English-language UIs.
Return:
  true  if the submit button was clicked AND mdl_validate returned
        true AND on_submit was invoked this frame;
  false otherwise.
*/
template <unsigned _F>
bool
mdl_imgui_render_form(
    mariadb_login<_F>&                _mdl,
    const mariadb_login_imgui_config& _cfg = {}
)
{
    bool submitted = false;

    ImGui::PushID(&_mdl);

    const bool controls_disabled = ( _mdl.submitting || !_mdl.enabled );

    ImGui::BeginDisabled(controls_disabled);

    dbl_imgui_render_base_slots (_mdl, _cfg.base);
    mdl_imgui_render_vendor_slots(_mdl, _cfg);

    ImGui::EndDisabled();

    // -- error banner -------------------------------------------------
    if (!_mdl.error_message.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4{1.0f, 0.35f, 0.35f, 1.0f},
                           "%s",
                           _mdl.error_message.c_str());
    }

    ImGui::Spacing();

    // -- submit button ------------------------------------------------
    {
        ImGui::BeginDisabled(controls_disabled);

        const char* label = ( _mdl.submitting
                              ? _cfg.base.submitting_label
                              : _cfg.base.submit_button_label );

        if (ImGui::Button(label))
        {
            const bool ok = ( !_cfg.base.validate_on_submit ||
                              mdl_validate(_mdl) );

            if (ok)
            {
                if (_mdl.on_submit)
                {
                    _mdl.on_submit(_mdl);
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
            mdl_reset_to_defaults(_mdl);
        }

        ImGui::EndDisabled();
    }

    ImGui::PopID();

    return submitted;
}


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_MARIADB_LOGIN_IMGUI_
