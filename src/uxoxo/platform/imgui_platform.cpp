/*******************************************************************************
* uxoxo [platform]                                            imgui_platform.cpp
*
* The Dear ImGui backend's archetype interpreter.
*   Each archetype becomes a draw function over its already-realized children.
* Containers push each child's key -- or, where it has none, its index --
* onto ImGui's ID stack before drawing it, so widget identity follows the
* element's place in the tree and two equal labels in different places never
* collide.
*   Every event an element posts goes through post(), which copies the
* element's "target" attribute into the payload. The archetypes below never
* touch the frame directly.
*
* path:      /src/uxoxo/platform/imgui_platform.cpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.09.22
*                                                            revised: 2026.10.04
*******************************************************************************/
#include <uxoxo/platform/imgui_platform.hpp>  // corresponding header
// std
#include <algorithm>  // std::max, std::min
#include <cfloat>     // FLT_MAX
#include <cstddef>    // std::size_t
#include <cstdint>    // std::intptr_t
#include <cstdlib>    // std::atof
#include <string>     // std::string
#include <utility>    // std::move
#include <vector>     // std::vector
// imgui
#include <imgui.h>                  // ImGui widgets and layout
#include <misc/cpp/imgui_stdlib.h>  // InputText over std::string
// uxoxo
#include "../../../inc/uxoxo/element_color_field.hpp"   // archetype_color_field
#include "../../../inc/uxoxo/element_combo.hpp"         // archetype_combo
#include "../../../inc/uxoxo/element_menu.hpp"          // archetype_menu*
#include "../../../inc/uxoxo/element_number_field.hpp"  // archetype_slider, ...
#include "../../../inc/uxoxo/element_panel.hpp"         // archetype_panel
#include "../../../inc/uxoxo/element_section.hpp"       // archetype_section
#include "../../../inc/uxoxo/element_selectable.hpp"    // archetype_selectable
#include "../../../inc/uxoxo/element_separator.hpp"     // archetype_separator
#include "../../../inc/uxoxo/element_splitter.hpp"      // archetype_splitter
#include "../../../inc/uxoxo/element_table.hpp"         // archetype_table*
#include "../../../inc/uxoxo/element_tabs.hpp"          // archetype_tab_bar/tab
#include "../../../inc/uxoxo/element_text_field.hpp"    // archetype_text_field
#include "../../../inc/uxoxo/element_toggle.hpp"        // archetype_toggle
#include "../../../inc/uxoxo/element_tree_node.hpp"     // archetype_tree_node


NS_UXOXO
NS_PLATFORM


namespace
{

using node_t   = element_node<imgui_blueprint>;
using blocks_t = std::vector<imgui_blueprint>;

// ------------------------------------------------------------------- helpers

// make_blueprint
//   a blueprint whose key is the element's own "key" attribute.
imgui_blueprint
make_blueprint(
    imgui_draw         _draw,
    std::string        _text,
    const option_set&  _attrs
)
{
    return imgui_blueprint{ std::move(_draw),
                            std::move(_text),
                            _attrs.as_string("key", "") };
}

// push_child
//   puts one child's identity on the ID stack: its key, else its index.
void
push_child(
    const imgui_blueprint& _child,
    std::size_t            _index
)
{
    if (!_child.key.empty())
    {
        ImGui::PushID(_child.key.c_str());
    }
    else
    {
        ImGui::PushID(static_cast<int>(_index));
    }

    return;
}

// post
//   reports one event, copying the element's "target" into the payload.
void
post(
    imgui_frame&       _frame,
    const option_set&  _attrs,
    const std::string& _action,
    option_set         _payload
)
{
    const ::djinterp::option_value* target = _attrs.find("target");

    if (target != nullptr)
    {
        _payload.set("target", *target);
    }

    _frame.post(_action, std::move(_payload));

    return;
}

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

// split_lines
//   a string of newline-separated entries as a vector of them.
std::vector<std::string>
split_lines(
    const std::string& _joined
)
{
    std::vector<std::string> out;

    // an empty string is no entries, not one empty entry
    if (_joined.empty())
    {
        return out;
    }

    std::size_t start = 0;

    while (true)
    {
        const std::size_t end = _joined.find('\n', start);

        if (end == std::string::npos)
        {
            out.push_back(_joined.substr(start));

            break;
        }

        out.push_back(_joined.substr(start, end - start));
        start = end + 1;
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

        push_child(_children[i], i);
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

// own_color
//   the element's own colour from "r", "g", "b" and "a"; false when it has
// none, or its alpha is zero.
bool
own_color(
    const option_set& _attrs,
    ImVec4&           _color
)
{
    // all four keys are optional; alpha decides whether there is a colour
    if (!_attrs.has("a"))
    {
        return false;
    }

    _color = ImVec4(static_cast<float>(_attrs.as_double("r", 1.0)),
                    static_cast<float>(_attrs.as_double("g", 1.0)),
                    static_cast<float>(_attrs.as_double("b", 1.0)),
                    static_cast<float>(_attrs.as_double("a", 0.0)));

    return (_color.w > 0.0f);
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
//   one axis of a size: 0 is all remaining space, (0, 1] a fraction of
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

// item_width
//   sets the next item's width from a "width" attribute; 0 leaves ImGui's.
void
item_width(
    const option_set& _attrs
)
{
    const double width = _attrs.as_double("width", 0.0);

    if (width != 0.0)
    {
        ImGui::SetNextItemWidth(
            extent(width, ImGui::GetContentRegionAvail().x));
    }

    return;
}

// widget_id
//   the label an ImGui widget is given: the visible "label" if there is one,
// else a hidden name local to the element.
std::string
widget_id(
    const option_set&  _attrs,
    const std::string& _hidden
)
{
    const std::string label = _attrs.as_string("label", "");

    return label.empty() ? _hidden
                         : label;
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

// --------------------------------------------------- archetypes: text, layout

imgui_blueprint
text_leaf(
    const node_t& _node
)
{
    const option_set  attrs = _node.attrs;
    const std::string text  = attrs.as_string("text", "");

    return make_blueprint(
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

            int    colours = push_tone(tone);
            ImVec4 colour;

            // an element's own colour wins over its tone
            if (own_color(attrs, colour))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, colour);
                ++colours;
            }

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
        text,
        attrs);
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

    return make_blueprint(
        [children, vertical, spacing](imgui_frame& _frame)
        {
            const float unit  = ImGui::GetStyle().ItemSpacing.x;
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

                push_child(children[i], i);
                children[i].draw(_frame);
                ImGui::PopID();

                first = false;
            }

            return;
        },
        join_text(children),
        _node.attrs);
}

imgui_blueprint
panel(
    const node_t& _node
)
{
    const blocks_t   children = _node.children;
    const option_set attrs    = _node.attrs;

    return make_blueprint(
        [children, attrs](imgui_frame& _frame)
        {
            const ImVec2      avail  = ImGui::GetContentRegionAvail();
            const std::string id     = attrs.as_string("id", "panel");
            const std::string resize = attrs.as_string("resize", "");
            const ImVec2      size(
                extent(attrs.as_double("width", 0.0), avail.x),
                extent(attrs.as_double("height", 0.0), avail.y));

            ImGuiChildFlags  flags  = ImGuiChildFlags_None;
            ImGuiWindowFlags window = ImGuiWindowFlags_None;

            if (attrs.as_bool("border", false))
            {
                flags |= ImGuiChildFlags_Borders;
            }

            if (attrs.as_bool("scroll_x", false))
            {
                window |= ImGuiWindowFlags_HorizontalScrollbar;
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
            if (ImGui::BeginChild(id.c_str(), size, flags, window))
            {
                draw_stack(children, _frame);
            }

            ImGui::EndChild();

            return;
        },
        join_text(children),
        attrs);
}

imgui_blueprint
section(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const option_set  attrs    = _node.attrs;
    const std::string title    = attrs.as_string("text", "section");

    return make_blueprint(
        [children, attrs, title](imgui_frame& _frame)
        {
            const ImGuiTreeNodeFlags flags =
                attrs.as_bool("open", true) ? ImGuiTreeNodeFlags_DefaultOpen
                                            : ImGuiTreeNodeFlags_None;

            // the header keeps its own open state, keyed by its title
            if (ImGui::CollapsingHeader(title.c_str(), flags))
            {
                ImGui::PushID(title.c_str());
                draw_stack(children, _frame);
                ImGui::PopID();
            }

            tooltip(attrs);

            return;
        },
        title,
        attrs);
}

imgui_blueprint
tab_bar(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const std::string id       = _node.attrs.as_string("id", "tabs");

    return make_blueprint(
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
        join_text(children),
        _node.attrs);
}

imgui_blueprint
tab(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const option_set  attrs    = _node.attrs;
    const std::string title    = attrs.as_string("text", "tab");

    return make_blueprint(
        [children, attrs, title](imgui_frame& _frame)
        {
            const ImGuiTabItemFlags flags =
                attrs.as_bool("selected", false) ? ImGuiTabItemFlags_SetSelected
                                                 : ImGuiTabItemFlags_None;

            // only the front page draws its content
            if (ImGui::BeginTabItem(title.c_str(), nullptr, flags))
            {
                const std::string action = attrs.as_string("action", "");

                // a page coming to the front says so, once
                if ( (!action.empty()) &&
                     (!attrs.as_bool("active", false)) )
                {
                    post(_frame,
                         attrs,
                         action,
                         payload_value(attrs.as_long("value", 0)));
                }

                draw_stack(children, _frame);
                ImGui::EndTabItem();
            }

            return;
        },
        title,
        attrs);
}

imgui_blueprint
separator(
    const node_t& _node
)
{
    const std::string text = _node.attrs.as_string("text", "");

    return make_blueprint(
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
        text,
        _node.attrs);
}

imgui_blueprint
splitter(
    const node_t& _node
)
{
    const option_set attrs = _node.attrs;

    return make_blueprint(
        [attrs](imgui_frame& _frame)
        {
            const bool   vertical =
                (attrs.as_string("orientation", "vertical") == "vertical");
            const float  thickness = std::max(
                1.0f,
                static_cast<float>(attrs.as_double("thickness", 6.0)));
            const double length = attrs.as_double("length", 0.0);
            const ImVec2 avail  = ImGui::GetContentRegionAvail();

            // the bar runs the whole way unless told otherwise; a negative
            // length leaves that many pixels, as a panel's does
            const float room  = vertical ? avail.y : avail.x;
            const float along = std::max(
                1.0f,
                (length == 0.0) ? room
              : (length < 0.0)  ? room + static_cast<float>(length)
                                : extent(length, room));
            const ImVec2 size = vertical ? ImVec2(thickness, along)
                                         : ImVec2(along, thickness);

            ImGui::InvisibleButton("##splitter", size);

            const bool hovered = ImGui::IsItemHovered();
            const bool active  = ImGui::IsItemActive();

            if ( (hovered) ||
                 (active) )
            {
                ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW
                                               : ImGuiMouseCursor_ResizeNS);
            }

            const ImGuiCol colour =
                active  ? ImGuiCol_SeparatorActive
                        : (hovered ? ImGuiCol_SeparatorHovered
                                   : ImGuiCol_Separator);

            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetItemRectMin(),
                ImGui::GetItemRectMax(),
                ImGui::GetColorU32(colour),
                thickness * 0.5f);

            const std::string action = attrs.as_string("action", "");
            const float       delta  =
                vertical ? ImGui::GetIO().MouseDelta.x
                         : ImGui::GetIO().MouseDelta.y;

            // a drag reports how far it moved this frame
            if ( (active)            &&
                 (delta != 0.0f)     &&
                 (!action.empty()) )
            {
                post(_frame,
                     attrs,
                     action,
                     payload_value(static_cast<double>(delta)));
            }

            return;
        },
        std::string(),
        attrs);
}

// ---------------------------------------------------- archetypes: controls

imgui_blueprint
interactive(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const option_set  attrs    = _node.attrs;
    const std::string text     = join_text(children);

    return make_blueprint(
        [attrs, text](imgui_frame& _frame)
        {
            const bool        enabled = attrs.as_bool("enabled", true);
            const std::string action  = attrs.as_string("action", "");
            const bool        accent  =
                (attrs.as_string("tone", "") == "accent");
            const bool        latched = attrs.as_bool("active", false);
            const double      width   = attrs.as_double("width", 0.0);

            if (!enabled)
            {
                ImGui::BeginDisabled();
            }

            int colours = 0;

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
                colours += 3;
            }
            // a latched button looks held down
            else if (latched)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(
                                          ImGuiCol_ButtonActive));
                colours += 1;
            }

            const ImVec2 size(
                (width == 0.0)
                    ? 0.0f
                    : extent(width, ImGui::GetContentRegionAvail().x),
                0.0f);
            const bool pressed =
                attrs.as_bool("small", false)
                    ? ImGui::SmallButton(text.empty() ? "button"
                                                      : text.c_str())
                    : ImGui::Button(text.empty() ? "button"
                                                 : text.c_str(),
                                    size);

            ImGui::PopStyleColor(colours);

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

                post(_frame,
                     attrs,
                     action,
                     value ? payload_value(*value) : option_set{});
            }

            return;
        },
        text,
        attrs);
}

imgui_blueprint
text_field(
    const node_t& _node
)
{
    const option_set attrs = _node.attrs;

    return make_blueprint(
        [attrs](imgui_frame& _frame)
        {
            std::string       buffer  = attrs.as_string("value", "");
            const std::string hint    = attrs.as_string("placeholder", "");
            const std::string action  = attrs.as_string("action", "");
            const bool        enabled = attrs.as_bool("enabled", true);
            const std::string id      = widget_id(attrs, "##field");

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
                    id.c_str(),
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

                changed = ImGui::InputTextWithHint(id.c_str(),
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
                post(_frame,
                     attrs,
                     action,
                     payload_value(buffer));
            }

            return;
        },
        _node.attrs.as_string("value", ""),
        attrs);
}

imgui_blueprint
toggle(
    const node_t& _node
)
{
    const option_set  attrs = _node.attrs;
    const std::string text  = attrs.as_string("text", "");

    return make_blueprint(
        [attrs, text](imgui_frame& _frame)
        {
            bool              checked = attrs.as_bool("checked", false);
            const std::string action  = attrs.as_string("action", "");
            const bool        enabled = attrs.as_bool("enabled", true);

            if (!enabled)
            {
                ImGui::BeginDisabled();
            }

            // a flip reports the new state
            if ( (ImGui::Checkbox(text.c_str(), &checked)) &&
                 (!action.empty()) )
            {
                post(_frame,
                     attrs,
                     action,
                     payload_value(checked));
            }

            if (!enabled)
            {
                ImGui::EndDisabled();
            }

            tooltip(attrs);

            return;
        },
        text,
        attrs);
}

imgui_blueprint
selectable(
    const node_t& _node
)
{
    const option_set  attrs = _node.attrs;
    const std::string text  = attrs.as_string("text", "");

    return make_blueprint(
        [attrs, text](imgui_frame& _frame)
        {
            const std::string action   = attrs.as_string("action", "");
            const bool        selected = attrs.as_bool("selected", false);
            const bool        enabled  = attrs.as_bool("enabled", true);
            const double      width    = attrs.as_double("width", 0.0);
            int               colours  =
                push_tone(attrs.as_string("tone", ""));
            ImVec4            colour;

            if (own_color(attrs, colour))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, colour);
                ++colours;
            }

            ImGuiSelectableFlags flags =
                enabled ? ImGuiSelectableFlags_None
                        : ImGuiSelectableFlags_Disabled;

            // a row in a table can span every column
            if (attrs.as_bool("span", false))
            {
                flags |= ImGuiSelectableFlags_SpanAllColumns;
            }

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
                post(_frame,
                     attrs,
                     action,
                     payload_value(attrs.as_long("value", 0)));
            }

            return;
        },
        text,
        attrs);
}

imgui_blueprint
number_field(
    const node_t& _node
)
{
    const option_set attrs = _node.attrs;

    return make_blueprint(
        [attrs](imgui_frame& _frame)
        {
            const std::string action  = attrs.as_string("action", "");
            const bool        enabled = attrs.as_bool("enabled", true);
            const bool        drag    = attrs.as_bool("drag", false);
            const std::string id      = widget_id(attrs, "##number");

            if (!enabled)
            {
                ImGui::BeginDisabled();
            }

            item_width(attrs);

            bool        changed = false;
            option_set  payload;

            // an integer edits as 64 bits and reports as a long
            if (attrs.as_string("kind", "int") == "int")
            {
                ImS64       value  = attrs.as_long("value", 0);
                const bool  hex    = attrs.as_bool("hex", false);
                const ImS64 low    = attrs.as_long("min", 0);
                const ImS64 high   = attrs.as_long("max", 0);
                const ImS64 step   = attrs.as_long("step", 1);
                const ImS64 fast   = attrs.as_long("step_fast", 8);
                const bool  bounds = ( attrs.has("min") &&
                                       attrs.has("max") );
                const char* format = hex ? "%llX" : nullptr;

                changed =
                    drag ? ImGui::DragScalar(
                               id.c_str(),
                               ImGuiDataType_S64,
                               &value,
                               static_cast<float>(
                                   attrs.as_double("speed", 0.2)),
                               bounds ? &low : nullptr,
                               bounds ? &high : nullptr,
                               format)
                         : ImGui::InputScalar(
                               id.c_str(),
                               ImGuiDataType_S64,
                               &value,
                               (step > 0) ? &step : nullptr,
                               (step > 0) ? &fast : nullptr,
                               format,
                               hex ? ImGuiInputTextFlags_CharsHexadecimal
                                   : ImGuiInputTextFlags_None);

                // typed values are held to the bounds as dragged ones are
                if (attrs.has("min"))
                {
                    value = std::max(value, low);
                }

                if (attrs.has("max"))
                {
                    value = std::min(value, high);
                }

                payload.set("value", static_cast<long>(value));
            }
            else
            {
                double            value  = attrs.as_double("value", 0.0);
                const double      low    = attrs.as_double("min", 0.0);
                const double      high   = attrs.as_double("max", 0.0);
                const double      step   = attrs.as_double("step", 0.0);
                const bool        bounds = ( attrs.has("min") &&
                                             attrs.has("max") );
                const std::string format =
                    attrs.as_string("format", "%.3f");

                changed =
                    drag ? ImGui::DragScalar(
                               id.c_str(),
                               ImGuiDataType_Double,
                               &value,
                               static_cast<float>(
                                   attrs.as_double("speed", 0.01)),
                               bounds ? &low : nullptr,
                               bounds ? &high : nullptr,
                               format.c_str())
                         : ImGui::InputScalar(
                               id.c_str(),
                               ImGuiDataType_Double,
                               &value,
                               (step > 0.0) ? &step : nullptr,
                               nullptr,
                               format.c_str());

                if (attrs.has("min"))
                {
                    value = std::max(value, low);
                }

                if (attrs.has("max"))
                {
                    value = std::min(value, high);
                }

                payload.set("value", value);
            }

            if (!enabled)
            {
                ImGui::EndDisabled();
            }

            tooltip(attrs);

            // an edit reports the new value, of the field's own kind
            if ( (changed) &&
                 (!action.empty()) )
            {
                post(_frame,
                     attrs,
                     action,
                     std::move(payload));
            }

            return;
        },
        std::string(),
        attrs);
}

imgui_blueprint
slider(
    const node_t& _node
)
{
    const option_set attrs = _node.attrs;

    return make_blueprint(
        [attrs](imgui_frame& _frame)
        {
            double            value   = attrs.as_double("value", 0.0);
            const double      low     = attrs.as_double("min", 0.0);
            const double      high    = attrs.as_double("max", 1.0);
            const std::string format  = attrs.as_string("format", "%.2f");
            const std::string action  = attrs.as_string("action", "");
            const bool        enabled = attrs.as_bool("enabled", true);
            const std::string id      = widget_id(attrs, "##slider");

            if (!enabled)
            {
                ImGui::BeginDisabled();
            }

            item_width(attrs);

            const bool changed = ImGui::SliderScalar(id.c_str(),
                                                     ImGuiDataType_Double,
                                                     &value,
                                                     &low,
                                                     &high,
                                                     format.c_str());

            if (!enabled)
            {
                ImGui::EndDisabled();
            }

            tooltip(attrs);

            // a move reports the new position
            if ( (changed) &&
                 (!action.empty()) )
            {
                post(_frame,
                     attrs,
                     action,
                     payload_value(value));
            }

            return;
        },
        std::string(),
        attrs);
}

imgui_blueprint
combo(
    const node_t& _node
)
{
    const option_set               attrs = _node.attrs;
    const std::vector<std::string> items =
        split_lines(attrs.as_string("items", ""));

    return make_blueprint(
        [attrs, items](imgui_frame& _frame)
        {
            const long        selected = attrs.as_long("selected", 0);
            const std::string action   = attrs.as_string("action", "");
            const bool        enabled  = attrs.as_bool("enabled", true);
            const std::string id       = widget_id(attrs, "##combo");
            const bool        known    =
                ( (selected >= 0) &&
                  (static_cast<std::size_t>(selected) < items.size()) );
            const std::string preview  =
                known ? items[static_cast<std::size_t>(selected)]
                      : std::string();

            if (!enabled)
            {
                ImGui::BeginDisabled();
            }

            item_width(attrs);

            if (ImGui::BeginCombo(id.c_str(), preview.c_str()))
            {
                for (std::size_t i = 0; i < items.size(); ++i)
                {
                    const bool current =
                        (static_cast<long>(i) == selected);

                    ImGui::PushID(static_cast<int>(i));

                    // a pick of another choice reports its index
                    if ( (ImGui::Selectable(items[i].c_str(), current)) &&
                         (!current)                                    &&
                         (!action.empty()) )
                    {
                        post(_frame,
                             attrs,
                             action,
                             payload_value(static_cast<long>(i)));
                    }

                    if (current)
                    {
                        ImGui::SetItemDefaultFocus();
                    }

                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }

            if (!enabled)
            {
                ImGui::EndDisabled();
            }

            tooltip(attrs);

            return;
        },
        attrs.as_string("items", ""),
        attrs);
}

imgui_blueprint
color_field(
    const node_t& _node
)
{
    const option_set attrs = _node.attrs;

    return make_blueprint(
        [attrs](imgui_frame& _frame)
        {
            float colour[4] = {
                static_cast<float>(attrs.as_double("r", 1.0)),
                static_cast<float>(attrs.as_double("g", 1.0)),
                static_cast<float>(attrs.as_double("b", 1.0)),
                static_cast<float>(attrs.as_double("a", 1.0)) };

            const std::string action = attrs.as_string("action", "");
            const std::string label  = attrs.as_string("label", "");
            const std::string id     = widget_id(attrs, "##colour");

            ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaPreviewHalf;

            if (!attrs.as_bool("alpha", true))
            {
                flags |= ImGuiColorEditFlags_NoAlpha;
            }

            if (!attrs.as_bool("inputs", false))
            {
                flags |= ImGuiColorEditFlags_NoInputs;
            }

            if (label.empty())
            {
                flags |= ImGuiColorEditFlags_NoLabel;
            }

            const bool enabled = attrs.as_bool("enabled", true);

            if (!enabled)
            {
                ImGui::BeginDisabled();
            }

            const bool changed = ImGui::ColorEdit4(id.c_str(), colour, flags);

            if (!enabled)
            {
                ImGui::EndDisabled();
            }

            tooltip(attrs);

            // an edit reports all four channels
            if ( (changed) &&
                 (!action.empty()) )
            {
                option_set payload;
                payload.set("r", static_cast<double>(colour[0]));
                payload.set("g", static_cast<double>(colour[1]));
                payload.set("b", static_cast<double>(colour[2]));
                payload.set("a", static_cast<double>(colour[3]));

                post(_frame,
                     attrs,
                     action,
                     std::move(payload));
            }

            return;
        },
        std::string(),
        attrs);
}

// -------------------------------------------------------- archetypes: trees

// swatch_label
//   the label a tree node is drawn with: its text, after enough spaces to
// leave room for a swatch when it has one.
std::string
swatch_label(
    const std::string& _text,
    bool               _swatch,
    float              _side
)
{
    // no swatch, no room to leave
    if (!_swatch)
    {
        return _text;
    }

    const float space  = std::max(1.0f, ImGui::CalcTextSize(" ").x);
    const int   spaces = static_cast<int>((_side + space * 1.5f) / space) + 1;

    return std::string(static_cast<std::size_t>(spaces), ' ') + _text;
}

// tree_drag_and_drop
//   makes the last item a drag source and a drop target for its type.
void
tree_drag_and_drop(
    const option_set&  _attrs,
    const std::string& _text,
    imgui_frame&       _frame
)
{
    const std::string type  = _attrs.as_string("drag_type", "");
    const long        value = _attrs.as_long("value", 0);

    // without a type the node takes part in no drag
    if (type.empty())
    {
        return;
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
        ImGui::SetDragDropPayload(type.c_str(), &value, sizeof(value));
        ImGui::TextUnformatted(_text.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        const ImGuiPayload* dropped =
            ImGui::AcceptDragDropPayload(type.c_str());
        const std::string   action  =
            _attrs.as_string("drop_action", "");

        // a drop names what was dragged and where it landed
        if ( (dropped != nullptr)                         &&
             (dropped->DataSize == sizeof(long))          &&
             (!action.empty()) )
        {
            option_set payload;
            payload.set("value", *static_cast<const long*>(dropped->Data));
            payload.set("target", value);
            _frame.post(action, std::move(payload));
        }

        ImGui::EndDragDropTarget();
    }

    return;
}

imgui_blueprint
tree_node(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const option_set  attrs    = _node.attrs;
    const std::string text     = attrs.as_string("text", "");

    return make_blueprint(
        [children, attrs, text](imgui_frame& _frame)
        {
            const long  value  = attrs.as_long("value", 0);
            const bool  leaf   = attrs.as_bool("leaf", false);
            ImVec4      colour;
            const bool  swatch = own_color(attrs, colour);
            const float side   = ImGui::GetFontSize() * 0.75f;

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow       |
                                       ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;

            if (leaf)
            {
                flags |= ImGuiTreeNodeFlags_Leaf |
                         ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }
            else
            {
                // the application owns the open state
                ImGui::SetNextItemOpen(attrs.as_bool("open", false),
                                       ImGuiCond_Always);
            }

            if (attrs.as_bool("selected", false))
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            const std::string label = swatch_label(text, swatch, side);
            const bool        open  = ImGui::TreeNodeEx(
                reinterpret_cast<void*>(static_cast<std::intptr_t>(value)),
                flags,
                "%s",
                label.c_str());

            const bool toggled = ImGui::IsItemToggledOpen();
            const bool clicked =
                ( (ImGui::IsItemClicked(ImGuiMouseButton_Left)) &&
                  (!toggled) );

            // the swatch sits where the label begins
            if (swatch)
            {
                const ImVec2 min   = ImGui::GetItemRectMin();
                const float  x     = min.x + ImGui::GetTreeNodeToLabelSpacing();
                const float  y     = min.y +
                                     (ImGui::GetItemRectSize().y - side) * 0.5f;

                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(x, y),
                    ImVec2(x + side, y + side),
                    ImGui::GetColorU32(colour),
                    side * 0.25f);
            }

            tooltip(attrs);
            tree_drag_and_drop(attrs, text, _frame);

            const std::string action        = attrs.as_string("action", "");
            const std::string toggle_action =
                attrs.as_string("toggle_action", "");

            if ( (clicked) &&
                 (!action.empty()) )
            {
                post(_frame,
                     attrs,
                     action,
                     payload_value(value));
            }

            // opening or closing reports the new state
            if ( (toggled) &&
                 (!toggle_action.empty()) )
            {
                option_set payload = payload_value(value);
                payload.set("open", open);

                post(_frame,
                     attrs,
                     toggle_action,
                     std::move(payload));
            }

            // an open inner node draws its subtree, and pops what it pushed
            if ( (open) &&
                 (!leaf) )
            {
                draw_stack(children, _frame);
                ImGui::TreePop();
            }

            return;
        },
        text,
        attrs);
}

// -------------------------------------------------------- archetypes: menus

imgui_blueprint
menu_bar(
    const node_t& _node
)
{
    const blocks_t children = _node.children;
    const bool     main     = _node.attrs.as_bool("main", false);

    return make_blueprint(
        [children, main](imgui_frame& _frame)
        {
            const bool open = main ? ImGui::BeginMainMenuBar()
                                   : ImGui::BeginMenuBar();

            // each Begin that succeeded has its own End
            if (open)
            {
                draw_stack(children, _frame);

                if (main)
                {
                    ImGui::EndMainMenuBar();
                }
                else
                {
                    ImGui::EndMenuBar();
                }
            }

            return;
        },
        join_text(children),
        _node.attrs);
}

imgui_blueprint
menu(
    const node_t& _node
)
{
    const blocks_t    children = _node.children;
    const std::string text     = _node.attrs.as_string("text", "menu");
    const bool        enabled  = _node.attrs.as_bool("enabled", true);

    return make_blueprint(
        [children, text, enabled](imgui_frame& _frame)
        {
            if (ImGui::BeginMenu(text.c_str(), enabled))
            {
                draw_stack(children, _frame);
                ImGui::EndMenu();
            }

            return;
        },
        text,
        _node.attrs);
}

imgui_blueprint
menu_item(
    const node_t& _node
)
{
    const option_set  attrs = _node.attrs;
    const std::string text  = attrs.as_string("text", "");

    return make_blueprint(
        [attrs, text](imgui_frame& _frame)
        {
            const std::string shortcut  = attrs.as_string("shortcut", "");
            const std::string action    = attrs.as_string("action", "");
            const bool        checkable = attrs.as_bool("checkable", false);
            const bool        checked   = attrs.as_bool("checked", false);
            const bool        enabled   = attrs.as_bool("enabled", true);

            const bool chosen = ImGui::MenuItem(
                text.c_str(),
                shortcut.empty() ? nullptr : shortcut.c_str(),
                checkable && checked,
                enabled);

            // a choice reports the action: the item's value, or a checkable
            // item's new state
            if ( (chosen) &&
                 (!action.empty()) )
            {
                const ::djinterp::option_value* value = attrs.find("value");

                if (value != nullptr)
                {
                    post(_frame,
                         attrs,
                         action,
                         payload_value(*value));
                }
                else if (checkable)
                {
                    post(_frame,
                         attrs,
                         action,
                         payload_value(!checked));
                }
                else
                {
                    post(_frame,
                         attrs,
                         action,
                         option_set{});
                }
            }

            return;
        },
        text,
        attrs);
}

// -------------------------------------------------------- archetypes: tables

// table_flags
//   the ImGui flags a table's attributes ask for.
ImGuiTableFlags
table_flags(
    const option_set& _attrs
)
{
    const std::string borders = _attrs.as_string("borders", "inner");
    ImGuiTableFlags   flags   = ImGuiTableFlags_SizingStretchProp;

    if (borders == "all")
    {
        flags |= ImGuiTableFlags_Borders;
    }
    else if (borders == "inner")
    {
        flags |= ImGuiTableFlags_BordersInner;
    }
    else if (borders == "outer")
    {
        flags |= ImGuiTableFlags_BordersOuter;
    }
    else if (borders == "rows")
    {
        flags |= ImGuiTableFlags_BordersInnerH |
                 ImGuiTableFlags_BordersOuterH;
    }
    else if (borders == "columns")
    {
        flags |= ImGuiTableFlags_BordersInnerV |
                 ImGuiTableFlags_BordersOuterV;
    }

    if (_attrs.as_bool("row_bg", true))
    {
        flags |= ImGuiTableFlags_RowBg;
    }

    if (_attrs.as_bool("resizable", true))
    {
        flags |= ImGuiTableFlags_Resizable;
    }

    if (_attrs.as_double("height", 0.0) > 0.0)
    {
        flags |= ImGuiTableFlags_ScrollY;
    }

    return flags;
}

imgui_blueprint
table(
    const node_t& _node
)
{
    const blocks_t                 children = _node.children;
    const option_set               attrs    = _node.attrs;
    const std::vector<std::string> headers  =
        split_lines(attrs.as_string("headers", ""));

    return make_blueprint(
        [children, attrs, headers](imgui_frame& _frame)
        {
            const std::string id      = attrs.as_string("id", "table");
            const int         columns =
                std::max(1, static_cast<int>(headers.size()));
            const float       height  =
                static_cast<float>(attrs.as_double("height", 0.0));
            bool              titled  = false;

            if (!ImGui::BeginTable(id.c_str(),
                                   columns,
                                   table_flags(attrs),
                                   ImVec2(0.0f, height)))
            {
                return;
            }

            const std::vector<std::string> widths =
                split_lines(attrs.as_string("widths", ""));

            for (std::size_t i = 0; i < headers.size(); ++i)
            {
                const float width =
                    (i < widths.size())
                        ? static_cast<float>(std::atof(widths[i].c_str()))
                        : 0.0f;

                // a width above zero fixes the column; the rest share
                ImGui::TableSetupColumn(
                    headers[i].c_str(),
                    (width > 0.0f) ? ImGuiTableColumnFlags_WidthFixed
                                   : ImGuiTableColumnFlags_WidthStretch,
                    width);
                titled = ( (titled) ||
                           (!headers[i].empty()) );
            }

            // the header stays in view over a scrolling body
            if (titled)
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
            }

            draw_stack(children, _frame);
            ImGui::EndTable();

            return;
        },
        join_text(children),
        attrs);
}

imgui_blueprint
table_row(
    const node_t& _node
)
{
    const blocks_t children = _node.children;

    return make_blueprint(
        [children](imgui_frame& _frame)
        {
            ImGui::TableNextRow();

            for (std::size_t i = 0; i < children.size(); ++i)
            {
                // a cell is taken even when it draws nothing, so the
                // columns stay aligned
                ImGui::TableNextColumn();

                if (!children[i].draw)
                {
                    continue;
                }

                push_child(children[i], i);
                children[i].draw(_frame);
                ImGui::PopID();
            }

            return;
        },
        join_text(children),
        _node.attrs);
}

// ---------------------------------------------------------- the last resort

imgui_blueprint
degraded(
    const node_t& _node
)
{
    // children, if any, top to bottom; otherwise the text attribute
    if (!_node.children.empty())
    {
        const blocks_t children = _node.children;

        return make_blueprint(
            [children](imgui_frame& _frame)
            {
                draw_stack(children, _frame);

                return;
            },
            join_text(children),
            _node.attrs);
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
        text,
        std::string() };
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

    if (archetype == archetype_text_leaf)    { return text_leaf(_node);    }
    if (archetype == archetype_container)    { return container(_node);    }
    if (archetype == archetype_interactive)  { return interactive(_node);  }
    if (archetype == archetype_panel)        { return panel(_node);        }
    if (archetype == archetype_tab_bar)      { return tab_bar(_node);      }
    if (archetype == archetype_tab)          { return tab(_node);          }
    if (archetype == archetype_text_field)   { return text_field(_node);   }
    if (archetype == archetype_toggle)       { return toggle(_node);       }
    if (archetype == archetype_selectable)   { return selectable(_node);   }
    if (archetype == archetype_separator)    { return separator(_node);    }
    if (archetype == archetype_number_field) { return number_field(_node); }
    if (archetype == archetype_slider)       { return slider(_node);       }
    if (archetype == archetype_combo)        { return combo(_node);        }
    if (archetype == archetype_color_field)  { return color_field(_node);  }
    if (archetype == archetype_tree_node)    { return tree_node(_node);    }
    if (archetype == archetype_menu_bar)     { return menu_bar(_node);     }
    if (archetype == archetype_menu)         { return menu(_node);         }
    if (archetype == archetype_menu_item)    { return menu_item(_node);    }
    if (archetype == archetype_section)      { return section(_node);      }
    if (archetype == archetype_table)        { return table(_node);        }
    if (archetype == archetype_table_row)    { return table_row(_node);    }
    if (archetype == archetype_splitter)     { return splitter(_node);     }

    return degraded(_node);
}


NS_END  // platform
NS_END  // uxoxo
