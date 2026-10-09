/******************************************************************************
* uxoxo [component]                                                    menu.hpp
*
* The `menu` component family: a container of items, with submenus.
*   A self-contained module owning the menu container and its entry types --
* item, separator, submenu -- and ignorant of every other component. A menu is
* an ordered list of entry children; a submenu is an entry whose single child
* is a nested menu, so hierarchy is just the tree nesting the free monad
* already gives us. No unique_ptr, no navigation state: those were properties
* of the old mutable menu, and here structure is the template and behaviour
* rides a later layer.
*
* ON TRAITS AND CONCEPTS (the fold).
*   The old menu shipped as three files -- menu.hpp, menu_traits.hpp,
* menu_concepts.hpp -- because a menu was a distinct *struct type* per feature
* set, and menu_traits / menu_concepts were compile-time introspection over
* those types (is_menu<T>, items_have_labels<T>, supports_submenus<T>, ...).
* In the declarative model there are no per-component types: a menu is a
* component_template, its identity a runtime descriptor pointer. So those
* structural traits and concepts have no T to bind to -- they do not fold into
* this module, they *dissolve*. What replaces "does this type support submenus?"
* is a runtime question about a node (its descriptor, its attributes, its
* children), asked where it is needed, not a static trait. This file is the
* whole menu surface.
*
*   Render archetypes (declared here): menu, menu_item, menu_separator, submenu.
* Backends that do not know them degrade; real controls are per-(type,
* platform) overrides.
*
*   Attribute keys:
*     menu_item      : "text" (string), "enabled" (bool),
*                      "shortcut" (string), "checked" (bool).     Arity: 0.
*     menu_separator : (none).                                    Arity: 0.
*     submenu        : "text" (string), "enabled" (bool).         Arity: 1 (a menu).
*     menu           : "title" (string).                          Arity: 0..*.
*
* USAGE:
*   using namespace uxoxo;
*   component_template file = component::menu(
*       { component::menu_item("New",  { { { "shortcut",
*                                            ::djinterp::option_value(std::string("Ctrl+N")) } } }),
*         component::menu_item("Open"),
*         component::menu_separator(),
*         component::submenu("Recent",
*             component::menu({ component::menu_item("a.txt"),
*                               component::menu_item("b.txt") })) },
*       "File");
*
* path:      /inc/uxoxo/menu.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                       created: 2026.07.02
******************************************************************************/

#ifndef UXOXO_MENU_
#define UXOXO_MENU_ 1

// std
#include <string>
#include <utility>
#include <vector>
// uxoxo
#include "./component_template.hpp"


NS_UXOXO


// archetype_menu / archetype_menu_item / archetype_menu_separator /
// archetype_submenu
//   constants: the render archetypes for the menu family. Declared here (the
// open-archetype pattern -- a new archetype is a new string, no core edit), at
// uxoxo:: root, so modules and backends spell them identically.
inline constexpr const char* archetype_menu           = "menu";
inline constexpr const char* archetype_menu_item      = "menu_item";
inline constexpr const char* archetype_menu_separator = "menu_separator";
inline constexpr const char* archetype_submenu        = "submenu";


NS_COMPONENT

    // menu_item_type
    //   the descriptor for a normal menu entry, owned by this module.
    D_NODISCARD
    inline const component_type* menu_item_type()
    {
        static const component_type descriptor =
            component_type{
                "menu_item",
                archetype_menu_item,
                option_set{ { { "text",     ::djinterp::option_value(std::string()) },
                              { "enabled",  ::djinterp::option_value(true) },
                              { "shortcut", ::djinterp::option_value(std::string()) },
                              { "checked",  ::djinterp::option_value(false) } } },
                0,
                0
            };

        return &descriptor;
    }

    /*
    menu_item
      Builds a leaf `menu_item` template carrying a label.

    Parameter(s):
      _label: the entry's display text (stored under "text").
      _attrs: optional extra attributes -- "shortcut", "checked", "enabled" --
              overlaid on top (caller wins).
    Return:
      A closed component_template wrapping a single menu_item node.
    */
    D_NODISCARD
    inline component_template menu_item(
        const std::string& _label,
        option_set         _attrs = option_set{}
    )
    {
        option_set resolved = menu_item_type()->defaults;
        resolved.set("text", _label);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(menu_item_type(), resolved, {});
    }


    // menu_separator_type
    //   the descriptor for a menu divider, owned by this module.
    D_NODISCARD
    inline const component_type* menu_separator_type()
    {
        static const component_type descriptor =
            component_type{
                "menu_separator",
                archetype_menu_separator,
                option_set{},   // no attributes
                0,
                0
            };

        return &descriptor;
    }

    /*
    menu_separator
      Builds a leaf `menu_separator` template -- a non-selectable divider.

    Return:
      A closed component_template wrapping a single menu_separator node.
    */
    D_NODISCARD
    inline component_template menu_separator()
    {
        return make_component(menu_separator_type(),
                              menu_separator_type()->defaults,
                              {});
    }


    // submenu_type
    //   the descriptor for a submenu entry -- an item whose one child is a
    // nested menu. Owned by this module.
    D_NODISCARD
    inline const component_type* submenu_type()
    {
        static const component_type descriptor =
            component_type{
                "submenu",
                archetype_submenu,
                option_set{ { { "text",    ::djinterp::option_value(std::string()) },
                              { "enabled", ::djinterp::option_value(true) } } },
                1,
                1
            };

        return &descriptor;
    }

    /*
    submenu
      Builds a `submenu` template -- a labelled entry that opens a nested menu.

    Parameter(s):
      _label:    the entry's display text (stored under "text").
      _contents: the nested menu template opened by this entry.
      _attrs:    optional extra attributes, overlaid on top (caller wins).
    Return:
      A component_template wrapping a submenu node over _contents. Closed iff
      _contents is closed.
    */
    D_NODISCARD
    inline component_template submenu(
        const std::string&        _label,
        const component_template& _contents,
        option_set                _attrs = option_set{}
    )
    {
        option_set resolved = submenu_type()->defaults;
        resolved.set("text", _label);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(submenu_type(), resolved, { _contents });
    }


    // menu_type
    //   the descriptor for the menu container, owned by this module.
    D_NODISCARD
    inline const component_type* menu_type()
    {
        static const component_type descriptor =
            component_type{
                "menu",
                archetype_menu,
                option_set{ { { "title", ::djinterp::option_value(std::string()) } } },
                0,
                -1
            };

        return &descriptor;
    }

    /*
    menu
      Builds a `menu` template laying out its entry children in order.

    Parameter(s):
      _items: the entry templates, in order (menu_item / menu_separator /
              submenu nodes, or any component).
      _title: the menu's title ("title"); defaults to empty.
      _attrs: optional extra attributes, overlaid on top (caller wins).
    Return:
      A component_template wrapping a menu node over _items. Closed iff every
      entry is closed.
    */
    D_NODISCARD
    inline component_template menu(
        std::vector<component_template> _items,
        const std::string&              _title = std::string(),
        option_set                      _attrs = option_set{}
    )
    {
        option_set resolved = menu_type()->defaults;
        resolved.set("title", _title);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_component(menu_type(), resolved, std::move(_items));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_MENU_
