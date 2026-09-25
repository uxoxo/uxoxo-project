/*******************************************************************************
* uxoxo [platform]                                            imgui_platform.cpp
*
* The Dear ImGui backend's archetype interpreter.
*   Each archetype becomes a draw function over its already-realized children.
* Containers push each child's index onto ImGui's ID stack before drawing it,
* so widget identity follows the element's position in the tree and two equal
* labels in different places never collide.
*
* path:      /src/uxoxo/platform/imgui_platform.cpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.09.22
*******************************************************************************/
#include <uxoxo/platform/imgui_platform.hpp>  // corresponding header
// std
#include <cfloat>   // FLT_MAX
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// imgui
#include <imgui.h>                // ImGui widgets and layout
#include <misc/cpp/imgui_stdlib.h>  // InputText over std::string
// uxoxo
#include "../../../inc/uxoxo/element_panel.hpp"       // archetype_panel
#include "../../../inc/uxoxo/element_selectable.hpp"  // archetype_selectable
#include "../../../inc/uxoxo/element_separator.hpp"   // archetype_separator
#include "../../../inc/uxoxo/element_tabs.hpp"        // archetype_tab_bar/tab
#include "../../../inc/uxoxo/element_text_field.hpp"  // archetype_text_field
#include "../../../inc/uxoxo/element_toggle.hpp"      // archetype_toggle


NS_UXOXO
NS_PLATFORM


namespace
{

using node_t   = element_node<imgui_blueprint>;
using blocks_t = std::vector<imgui_blueprint>;

// join_text
//   the text projection of a set of children: their texts, space-separated.
std::string
join_text(
    const blocks_t& _children
)
{
    std::string out;

    for (const imgui_blueprint& child : _children)
    {
        // skip children that contribute no text
        if (child.text.empty())
        {
            continue;
        }

        if (!out.empty())
        {
            out += ' ';
        }

        out += child.text;
    }

    return out;
}

// draw_stack
//   draws children top to bottom, each under its own ID.
void
draw_stack(
    const blocks_t& _children,
    imgui_frame&    _frame
)
{
    for (std::size_t i = 0; i < _children.size(); ++i)
    {
        // an empty draw is a child that renders nothing
        if (!_children[i].draw)
        {
            continue;
        }

        ImGui::PushID(static_cast<int>(i));
        _children[i].draw(_frame);
        ImGui::PopID();
    }

    return;
}

// to_color
//   an RGBA array as an ImGui colour.
ImVec4
to_color(
    const float (&_rgba)[4]
)
{
    return ImVec4(_rgba[0],
                  _rgba[1],
                  _rgba[2],
                  _rgba[3]);
}

// push_tone
//   pushes the text colour for a tone; returns how many colours to pop.
int
push_tone(
    const std::string& _tone
)
{
    const imgui_tones& tones = imgui_platform::tones();

    if (_tone == "muted")
    {
        ImGui::PushStyleColor(ImGuiCol_Text, to_color(tones.muted));

        return 1;
    }

    if (_tone == "accent")
    {
        ImGui::PushStyleColor(ImGuiCol_Text, to_color(tones.accent));

        return 1;
    }

    if (_tone == "error")
    {
        ImGui::PushStyleColor(ImGuiCol_Text, to_color(tones.error));

        return 1;
    }

    if (_tone == "ok")
    {
        ImGui::PushStyleColor(ImGuiCol_Text, to_color(tones.ok));

        return 1;
    }

    return 0;
}

// tooltip
//   shows the element's "tooltip" attribute over the last item, if any.
void
tooltip(
    const option_set& _attrs
)
{
    const std::string tip = _attrs.as_string("tooltip", "");

    // only a hovered item with a tip shows one
    if ( (!tip.empty()) &&
         (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) )
    {
        ImGui::SetTooltip("%s", tip.c_str());
    }

    return;
}

// extent
//   one axis of a panel size: 0 is all remaining space, (0, 1] a fraction of
// _avail, a negative value leaves that many pixels, anything else is pixels.
float
extent(
    double _value,
    float  _avail
)
{
    if ( (_value > 0.0) &&
         (_value <= 1.0) )
    {
        return static_cast<float>(_value) * _avail;
    }

    return static_cast<float>(_value);
}

// payload_value
//   the {value: ...} payload an interactive element reports.
template<typename Value>
option_set
payload_value(
    Value&& _value
)
{
    option_set payload;
    payload.set("value", std::forward<Value>(_value));

    return payload;
}

// ---------------------------------------------------------------- archetypes

imgui_blueprint
text_leaf(
    const node_t& _node
)
{
    const option_set  attrs = _node.attrs;
    const std::string text  = attrs.as_string("text", "");

    return imgui_blueprint{
        [attrs, text](imgui_frame&)
        {
            const std::string tone    = attrs.as_string("tone", "");
            const bool        heading = (tone == "heading");

            // a heading is larger body text
            if (heading)
            {
                ImGui::PushFont(nullptr,
                                ImGui::GetStyle().FontSizeBase *
                                    imgui_platform::tones().heading);
            }

            const int colours = push_tone(tone);

            if (attrs.as_bool("wrap", false))
            {
                ImGui::TextWrapped("%s", text.c_str());
            }
            else
            {
                ImGui::TextUnformatted(text.c_str());
            }

            ImGui::PopStyleColor(colours);

            if (heading)
            {
                ImGui::PopFont();
            }

            tooltip(attrs);

            return;
        },
        text };
}

imgui_blueprint
container(
    const node_t& _node
)
{
    const blocks_t children = _node.children;
    const bool     vertical =
        (_node.attrs.as_string("orientation", "") == "vertical");
    const long     spacing  = _node.attrs.as_long("spacing", 1);

    return imgui_blueprint{
        [children, vertical, spacing](imgui_frame& _frame)
        {
            const float unit = ImGui::GetStyle().ItemSpacing.x;
            bool        first = true;

            for (std::size_t i = 0; i < children.size(); ++i)
            {
                // an empty draw is a child that renders nothing
                if (!children[i].draw)
                {
                    continue;
                }

                // a row keeps each child on the previous child's line
                if ( (!vertical) &&
                     (!first) )
                {
                    ImGui::SameLine(0.0f,
                                    static_cast<float>(spacing) * unit);
                }

                ImGui::PushID(static_cast<int>(i));
                children[i].draw(_frame);
                ImGui::PopID();

                first = false;
            }

            return;
        },
        join_text(children) };
}

imgui_blueprint
interactive(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const option_set  attrs    = _node.attrs;
    const std::string text     = join_text(children);

    return imgui_blueprint{
        [attrs, text](imgui_frame& _frame)
        {
            const bool        enabled = attrs.as_bool("enabled", true);
            const std::string action  = attrs.as_string("action", "");
            const bool        accent  =
                (attrs.as_string("tone", "") == "accent");

            if (!enabled)
            {
                ImGui::BeginDisabled();
            }

            // an accented button wears the accent colour
            if (accent)
            {
                const ImVec4 colour =
                    to_color(imgui_platform::tones().accent);
                ImGui::PushStyleColor(ImGuiCol_Button, colour);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      ImVec4(colour.x * 1.15f,
                                             colour.y * 1.15f,
                                             colour.z * 1.15f,
                                             1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(
                                          ImGuiCol_WindowBg));
            }

            const bool pressed =
                ImGui::Button(text.empty() ? "button" : text.c_str());

            if (accent)
            {
                ImGui::PopStyleColor(3);
            }

            if (!enabled)
            {
                ImGui::EndDisabled();
            }

            tooltip(attrs);

            // a press reports the action, passing any value through
            if ( (pressed) &&
                 (!action.empty()) )
            {
                const ::djinterp::option_value* value =
                    attrs.find("value");

                _frame.post(action,
                            value ? payload_value(*value) : option_set{});
            }

            return;
        },
        text };
}

imgui_blueprint
panel(
    const node_t& _node
)
{
    const blocks_t   children = _node.children;
    const option_set attrs    = _node.attrs;

    return imgui_blueprint{
        [children, attrs](imgui_frame& _frame)
        {
            const ImVec2      avail  = ImGui::GetContentRegionAvail();
            const std::string id     = attrs.as_string("id", "panel");
            const std::string resize = attrs.as_string("resize", "");
            const ImVec2      size(
                extent(attrs.as_double("width", 0.0), avail.x),
                extent(attrs.as_double("height", 0.0), avail.y));

            ImGuiChildFlags flags = ImGuiChildFlags_None;

            if (attrs.as_bool("border", false))
            {
                flags |= ImGuiChildFlags_Borders;
            }

            if (resize == "x")
            {
                flags |= ImGuiChildFlags_ResizeX;
            }
            else if (resize == "y")
            {
                flags |= ImGuiChildFlags_ResizeY;
            }

            // BeginChild is always paired with EndChild, drawn or not
            if (ImGui::BeginChild(id.c_str(), size, flags))
            {
                draw_stack(children, _frame);
            }

            ImGui::EndChild();

            return;
        },
        join_text(children) };
}

imgui_blueprint
tab_bar(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const std::string id       = _node.attrs.as_string("id", "tabs");

    return imgui_blueprint{
        [children, id](imgui_frame& _frame)
        {
            // pages are identified by title within the bar
            if (ImGui::BeginTabBar(id.c_str()))
            {
                for (const imgui_blueprint& page : children)
                {
                    if (page.draw)
                    {
                        page.draw(_frame);
                    }
                }

                ImGui::EndTabBar();
            }

            return;
        },
        join_text(children) };
}

imgui_blueprint
tab(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const std::string title    = _node.attrs.as_string("text", "tab");
    const bool        selected = _node.attrs.as_bool("selected", false);

    return imgui_blueprint{
        [children, title, selected](imgui_frame& _frame)
        {
            const ImGuiTabItemFlags flags =
                selected ? ImGuiTabItemFlags_SetSelected
                         : ImGuiTabItemFlags_None;

            // only the front page draws its content
            if (ImGui::BeginTabItem(title.c_str(), nullptr, flags))
            {
                draw_stack(children, _frame);
                ImGui::EndTabItem();
            }

            return;
        },
        title };
}

imgui_blueprint
text_field(
    const node_t& _node
)
{
    const option_set attrs = _node.attrs;

    return imgui_blueprint{
        [attrs](imgui_frame& _frame)
        {
            std::string       buffer = attrs.as_string("value", "");
            const std::string hint   = attrs.as_string("placeholder", "");
            const std::string action = attrs.as_string("action", "");
            const bool        enabled = attrs.as_bool("enabled", true);

            ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;

            if (attrs.as_bool("read_only", false))
            {
                flags |= ImGuiInputTextFlags_ReadOnly;
            }

            if (!enabled)
            {
                ImGui::BeginDisabled();
            }

            bool changed = false;

            // many lines fill the width; one line takes the rest of its row
            if (attrs.as_bool("multiline", false))
            {
                const float avail  = ImGui::GetContentRegionAvail().y;
                const float height =
                    extent(attrs.as_double("height", 0.0), avail);

                changed = ImGui::InputTextMultiline(
                    "##field",
                    &buffer,
                    ImVec2(-FLT_MIN,
                           (height == 0.0f) ? avail : height),
                    flags | ImGuiInputTextFlags_AllowTabInput);
            }
            else
            {
                const double width = attrs.as_double("width", 0.0);

                ImGui::SetNextItemWidth(
                    (width == 0.0)
                        ? -FLT_MIN
                        : extent(width, ImGui::GetContentRegionAvail().x));

                changed = ImGui::InputTextWithHint("##field",
                                                   hint.c_str(),
                                                   &buffer,
                                                   flags);
            }

            if (!enabled)
            {
                ImGui::EndDisabled();
            }

            tooltip(attrs);

            // an edit reports the new text
            if ( (changed) &&
                 (!action.empty()) )
            {
                _frame.post(action, payload_value(buffer));
            }

            return;
        },
        _node.attrs.as_string("value", "") };
}

imgui_blueprint
toggle(
    const node_t& _node
)
{
    const option_set  attrs = _node.attrs;
    const std::string text  = attrs.as_string("text", "");

    return imgui_blueprint{
        [attrs, text](imgui_frame& _frame)
        {
            bool              checked = attrs.as_bool("checked", false);
            const std::string action  = attrs.as_string("action", "");

            // a flip reports the new state
            if ( (ImGui::Checkbox(text.c_str(), &checked)) &&
                 (!action.empty()) )
            {
                _frame.post(action, payload_value(checked));
            }

            tooltip(attrs);

            return;
        },
        text };
}

imgui_blueprint
selectable(
    const node_t& _node
)
{
    const option_set  attrs = _node.attrs;
    const std::string text  = attrs.as_string("text", "");

    return imgui_blueprint{
        [attrs, text](imgui_frame& _frame)
        {
            const std::string action   = attrs.as_string("action", "");
            const bool        selected = attrs.as_bool("selected", false);
            const bool        enabled  = attrs.as_bool("enabled", true);
            const double      width    = attrs.as_double("width", 0.0);
            const int         colours  =
                push_tone(attrs.as_string("tone", ""));

            const ImGuiSelectableFlags flags =
                enabled ? ImGuiSelectableFlags_None
                        : ImGuiSelectableFlags_Disabled;
            const ImVec2 size(
                (width == 0.0)
                    ? 0.0f
                    : extent(width, ImGui::GetContentRegionAvail().x),
                0.0f);

            const bool picked =
                ImGui::Selectable(text.c_str(), selected, flags, size);

            ImGui::PopStyleColor(colours);
            tooltip(attrs);

            // a pick reports the row's value
            if ( (picked) &&
                 (!action.empty()) )
            {
                _frame.post(action,
                            payload_value(attrs.as_long("value", 0)));
            }

            return;
        },
        text };
}

imgui_blueprint
separator(
    const node_t& _node
)
{
    const std::string text = _node.attrs.as_string("text", "");

    return imgui_blueprint{
        [text](imgui_frame&)
        {
            if (text.empty())
            {
                ImGui::Separator();
            }
            else
            {
                ImGui::SeparatorText(text.c_str());
            }

            return;
        },
        text };
}

imgui_blueprint
degraded(
    const node_t& _node
)
{
    // children, if any, top to bottom; otherwise the text attribute
    if (!_node.children.empty())
    {
        const blocks_t children = _node.children;

        return imgui_blueprint{
            [children](imgui_frame& _frame)
            {
                draw_stack(children, _frame);

                return;
            },
            join_text(children) };
    }

    return text_leaf(_node);
}

}  // anonymous namespace


/*
imgui_frame::post
  Appends one event; the host drains them after the frame.
*/
void
imgui_frame::post(
    const std::string& _kind,
    option_set         _payload
)
{
    events.push_back(event{ _kind, std::move(_payload) });

    return;
}

/*
imgui_platform::tones
  One table per process, so the host themes every element at once.
*/
imgui_tones&
imgui_platform::tones()
{
    static imgui_tones table;

    return table;
}

/*
imgui_platform::on_pure::operator()
  A hole draws as its slot name in angle brackets, as the ascii platform does,
so an unclosed template is visibly unclosed rather than silently blank.
*/
imgui_blueprint
imgui_platform::on_pure::operator()(
    const hole& _hole
) const
{
    const std::string text = "<" + _hole.slot + ">";

    return imgui_blueprint{
        [text](imgui_frame&)
        {
            ImGui::TextDisabled("%s", text.c_str());

            return;
        },
        text };
}

/*
imgui_platform::on_archetype::operator()
  Dispatches on the archetype token. The comparison chain is the backend's own
closed switch over the tokens it knows; an unknown token degrades rather than
failing, which is the contract element.hpp states for open archetypes.
*/
imgui_blueprint
imgui_platform::on_archetype::operator()(
    const element_node<imgui_blueprint>& _node
) const
{
    const std::string& archetype = _node.type->archetype;

    if (archetype == archetype_text_leaf)   { return text_leaf(_node);   }
    if (archetype == archetype_container)   { return container(_node);   }
    if (archetype == archetype_interactive) { return interactive(_node); }
    if (archetype == archetype_panel)       { return panel(_node);       }
    if (archetype == archetype_tab_bar)     { return tab_bar(_node);     }
    if (archetype == archetype_tab)         { return tab(_node);         }
    if (archetype == archetype_text_field)  { return text_field(_node);  }
    if (archetype == archetype_toggle)      { return toggle(_node);      }
    if (archetype == archetype_selectable)  { return selectable(_node);  }
    if (archetype == archetype_separator)   { return separator(_node);   }

    return degraded(_node);
}


NS_END  // platform
NS_END  // uxoxo
