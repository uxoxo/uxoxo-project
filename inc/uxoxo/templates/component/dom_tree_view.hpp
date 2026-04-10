/*******************************************************************************
* uxoxo [component]                                         dom_tree_view.hpp
*
*   A framework-agnostic, presentation-agnostic DOM tree component built
* on tree_node and tree_view.  The DOM layer provides:
*
*   - dom_element<_Tag>      — the per-node payload carrying a type tag,
*                              id, class list, text content, properties,
*                              event bindings, visibility, and revision
*
*   - dom_node<_Tag, _Feat>  — alias for tree_node<dom_element<_Tag>, _Feat>
*
*   - dom_view<_Tag, _Feat>  — alias for tree_view<dom_element<_Tag>, _Feat>
*
*   - DOM-specific free functions: find_by_id, query_by_tag,
*     collect_by_tag, query_by_class, set_property, get_property,
*     add_event_listener, remove_event_listener, set_text_content,
*     validate_child_insert, count_by_tag, walk_visible_elements
*
*   - dom_tree_traits:: — SFINAE detection for DOM payloads
*
*   The tag enum is a template parameter, making this module usable
* with any domain taxonomy (Qt widgets, HTML elements, scene graphs,
* custom schemas).  The tree_node feature flags control which EBO
* mixins are active — collapsible, checkable, icons, etc.
*
*   This module has no dependency on any presentation framework.
* Renderers discover capabilities via dom_tree_traits:: and dispatch
* with if constexpr.
*
* Template parameters:
*   _Tag:   element type enum     (e.g. DQtDomElementType, DHtmlTag)
*   _Feat:  tree_feat / vf_ flags (default: vf_collapsible | vf_context)
*   _Icon:  icon type             (default: int)
*
*
* TABLE OF CONTENTS
* =================
* I.    dom_property             — key-value attribute
* II.   dom_event_binding        — signal-to-handler connection
* III.  dom_element<_Tag>        — per-node DOM payload
* IV.   dom_node / dom_view      — tree_node / tree_view aliases
* V.    DOM Element Free Fns     — find, query, mutate
* VI.   DOM Structural Free Fns  — validate, reparent, subtree ops
* VII.  dom_tree_traits::        — SFINAE detection
*
*
* path:      /inc/uxoxo/component/dom_tree_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.09
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_DOM_TREE_VIEW_
#define  UXOXO_COMPONENT_DOM_TREE_VIEW_ 1

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "../uxoxo.hpp"
#include "./tree_node.hpp"
#include "./tree_view.hpp"


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  DOM PROPERTY
// ═══════════════════════════════════════════════════════════════════════════════

// dom_property
//   struct: a single key-value attribute on a DOM element.
// Keys are short strings (attribute names); values are strings
// parsed on demand by the consumer.
struct dom_property
{
    std::string key;
    std::string value;
};


// ═══════════════════════════════════════════════════════════════════════════════
//  §2  DOM EVENT BINDING
// ═══════════════════════════════════════════════════════════════════════════════

// dom_event_binding
//   struct: one signal-to-handler connection on a DOM element.
// The handler_id is an opaque string resolved at runtime by the
// event dispatch layer — no std::function, keeping the payload
// serialisable and value-semantic.
struct dom_event_binding
{
    std::string signal_name;
    std::string handler_id;
    bool        once    = false;
    bool        blocked = false;
};


// ═══════════════════════════════════════════════════════════════════════════════
//  §3  DOM ELEMENT
// ═══════════════════════════════════════════════════════════════════════════════

// dom_element
//   struct: the per-node DOM payload.  Parameterised on the tag
// enum so any domain (Qt widgets, HTML tags, scene graphs) can
// plug in its own taxonomy.  This struct is the _Data template
// argument to tree_node<>.
//
//   tree_node handles: children, collapse, checkbox, icons, etc.
//   dom_element handles: tag, id, class, text, properties, events,
//                        visibility, enabled, revision.
template<typename _Tag>
struct dom_element
{
    using tag_type = _Tag;

    // -----------------------------------------------------------------
    //  type identity
    // -----------------------------------------------------------------

    // tag
    //   field: the element type tag.
    _Tag tag {};

    // id
    //   field: unique identifier (analogous to HTML id or
    // Qt objectName).  Empty means anonymous.
    std::string id;

    // class_list
    //   field: space-separated class names for stylesheet
    // matching.
    std::string class_list;


    // -----------------------------------------------------------------
    //  content
    // -----------------------------------------------------------------

    // text_content
    //   field: primary text payload.  Meaning varies by tag:
    // label text, window title, menu item text, etc.
    std::string text_content;


    // -----------------------------------------------------------------
    //  visual state
    // -----------------------------------------------------------------

    // visible
    //   field: whether the element participates in rendering.
    bool visible = true;

    // enabled
    //   field: whether the element accepts input.
    bool enabled = true;


    // -----------------------------------------------------------------
    //  properties (extensible attributes)
    // -----------------------------------------------------------------

    // properties
    //   field: key-value pairs for element-specific config.
    std::vector<dom_property> properties;


    // -----------------------------------------------------------------
    //  event bindings
    // -----------------------------------------------------------------

    // events
    //   field: signal-to-handler connections.
    std::vector<dom_event_binding> events;


    // -----------------------------------------------------------------
    //  dirty tracking
    // -----------------------------------------------------------------

    // revision
    //   field: monotonic counter bumped on every mutation.
    std::uint64_t revision = 0;


    // -----------------------------------------------------------------
    //  construction helpers
    // -----------------------------------------------------------------

    dom_element() = default;

    explicit
    dom_element(_Tag _tag)
        : tag(_tag)
    {
    }

    dom_element(_Tag               _tag,
                const std::string& _id)
        : tag(_tag)
        , id(_id)
    {
    }

    dom_element(_Tag               _tag,
                const std::string& _id,
                const std::string& _text)
        : tag(_tag)
        , id(_id)
        , text_content(_text)
    {
    }
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  TYPE ALIASES
// ═══════════════════════════════════════════════════════════════════════════════

// dom_node
//   alias: tree_node with dom_element payload.  The tree_node
// handles hierarchy (children), and optionally collapse, checkbox,
// icons, rename, context via _Feat flags.
//
// Default features: collapsible + context menu.  Override with any
// combination of vf_none, vf_checkable, vf_icons, vf_collapsible,
// vf_renamable, vf_context, vf_tree_all.
template<typename _Tag,
         unsigned _Feat = (vf_collapsible | vf_context),
         typename _Icon = int>
using dom_node = tree_node<dom_element<_Tag>, _Feat, _Icon>;

// dom_view
//   alias: tree_view with dom_element payload.  The tree_view
// handles cursor, scroll, selection, visible entries, and
// feature-gated edit/context state.
template<typename _Tag,
         unsigned _Feat = (vf_collapsible | vf_context),
         typename _Icon = int>
using dom_view = tree_view<dom_element<_Tag>, _Feat, _Icon>;


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  DOM ELEMENT FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════
//   Free functions operating on individual dom_element payloads.
// Keep the element a pure aggregate.

// set_property
//   Sets key to value, replacing any existing entry.
template<typename _Tag>
void set_property(dom_element<_Tag>& _elem,
                  const std::string& _key,
                  const std::string& _value)
{
    for (auto& p : _elem.properties)
    {
        if (p.key == _key)
        {
            p.value = _value;
            ++_elem.revision;

            return;
        }
    }

    _elem.properties.push_back({_key, _value});
    ++_elem.revision;

    return;
}

// get_property
//   Returns pointer to value for key, or nullptr.
template<typename _Tag>
const std::string*
get_property(const dom_element<_Tag>& _elem,
             const std::string&       _key) noexcept
{
    for (const auto& p : _elem.properties)
    {
        if (p.key == _key)
        {
            return &p.value;
        }
    }

    return nullptr;
}

// has_property
//   Returns true if key exists.
template<typename _Tag>
bool has_property(const dom_element<_Tag>& _elem,
                  const std::string&       _key) noexcept
{
    return (get_property(_elem, _key) != nullptr);
}

// remove_property
//   Removes key.  Returns true if found.
template<typename _Tag>
bool remove_property(dom_element<_Tag>&  _elem,
                     const std::string&  _key)
{
    for (auto it = _elem.properties.begin();
         it != _elem.properties.end();
         ++it)
    {
        if (it->key == _key)
        {
            _elem.properties.erase(it);
            ++_elem.revision;

            return true;
        }
    }

    return false;
}

// set_text_content
//   Sets the primary text payload.
template<typename _Tag>
void set_text_content(dom_element<_Tag>& _elem,
                      const std::string& _text)
{
    _elem.text_content = _text;
    ++_elem.revision;

    return;
}

// add_event_listener
//   Registers a signal binding.
template<typename _Tag>
void add_event_listener(dom_element<_Tag>& _elem,
                        const std::string& _signal,
                        const std::string& _handler,
                        bool               _once = false)
{
    _elem.events.push_back({_signal, _handler, _once, false});
    ++_elem.revision;

    return;
}

// remove_event_listener
//   Removes the first binding matching signal + handler.
// Returns true if found.
template<typename _Tag>
bool remove_event_listener(dom_element<_Tag>& _elem,
                           const std::string& _signal,
                           const std::string& _handler)
{
    for (auto it = _elem.events.begin();
         it != _elem.events.end();
         ++it)
    {
        if ( it->signal_name == _signal &&
             it->handler_id  == _handler )
        {
            _elem.events.erase(it);
            ++_elem.revision;

            return true;
        }
    }

    return false;
}

// has_event_listener
//   Returns true if any binding exists for signal.
template<typename _Tag>
bool has_event_listener(const dom_element<_Tag>& _elem,
                        const std::string&       _signal) noexcept
{
    for (const auto& b : _elem.events)
    {
        if (b.signal_name == _signal)
        {
            return true;
        }
    }

    return false;
}

// has_class
//   Returns true if class_list contains _cls (space-separated).
template<typename _Tag>
bool has_class(const dom_element<_Tag>& _elem,
               const std::string&       _cls) noexcept
{
    const auto& cl = _elem.class_list;

    if (cl.empty() || _cls.empty())
    {
        return false;
    }

    std::size_t pos = 0;

    while (pos < cl.size())
    {
        std::size_t end = cl.find(' ', pos);

        if (end == std::string::npos)
        {
            end = cl.size();
        }

        if ( (end - pos) == _cls.size() &&
             cl.compare(pos, _cls.size(), _cls) == 0 )
        {
            return true;
        }

        pos = end + 1;
    }

    return false;
}

// add_class
//   Appends _cls to class_list if not already present.
template<typename _Tag>
void add_class(dom_element<_Tag>&  _elem,
               const std::string& _cls)
{
    if (!has_class(_elem, _cls))
    {
        if (!_elem.class_list.empty())
        {
            _elem.class_list += ' ';
        }

        _elem.class_list += _cls;
        ++_elem.revision;
    }

    return;
}

// remove_class
//   Removes _cls from class_list.  Returns true if found.
template<typename _Tag>
bool remove_class(dom_element<_Tag>&  _elem,
                  const std::string& _cls)
{
    auto& cl = _elem.class_list;
    std::size_t pos = 0;

    while (pos < cl.size())
    {
        std::size_t end = cl.find(' ', pos);

        if (end == std::string::npos)
        {
            end = cl.size();
        }

        if ( (end - pos) == _cls.size() &&
             cl.compare(pos, _cls.size(), _cls) == 0 )
        {
            // remove the class and any trailing space
            std::size_t erase_end = (end < cl.size()) ? end + 1 : end;
            std::size_t erase_start = pos;

            // if removing from middle/end, also eat leading space
            if ( erase_start > 0 &&
                 erase_end == cl.size() )
            {
                --erase_start;
            }

            cl.erase(erase_start, erase_end - erase_start);
            ++_elem.revision;

            return true;
        }

        pos = end + 1;
    }

    return false;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §6  DOM TREE FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════
//   Free functions operating on dom_node trees.  These compose with
// tree_node's existing walk/find_if/count_nodes/flatten.

// find_by_id
//   Returns pointer to the first node in the subtree whose element
// id matches _id, or nullptr if not found.  Depth-first.
template<typename _Tag,
         unsigned _F,
         typename _I>
dom_node<_Tag, _F, _I>*
find_by_id(dom_node<_Tag, _F, _I>& _root,
           const std::string&      _id)
{
    return find_if(_root, [&_id](const auto& n)
    {
        return (n.data.id == _id);
    });
}

// find_by_id (const)
template<typename _Tag,
         unsigned _F,
         typename _I>
const dom_node<_Tag, _F, _I>*
find_by_id(const dom_node<_Tag, _F, _I>& _root,
           const std::string&            _id)
{
    return find_if(_root, [&_id](const auto& n)
    {
        return (n.data.id == _id);
    });
}

// find_by_tag
//   Returns pointer to the first node with matching tag.
template<typename _Tag,
         unsigned _F,
         typename _I>
dom_node<_Tag, _F, _I>*
find_by_tag(dom_node<_Tag, _F, _I>& _root,
            _Tag                    _tag)
{
    return find_if(_root, [_tag](const auto& n)
    {
        return (n.data.tag == _tag);
    });
}

// find_by_tag (const)
template<typename _Tag,
         unsigned _F,
         typename _I>
const dom_node<_Tag, _F, _I>*
find_by_tag(const dom_node<_Tag, _F, _I>& _root,
            _Tag                          _tag)
{
    return find_if(_root, [_tag](const auto& n)
    {
        return (n.data.tag == _tag);
    });
}

// collect_by_tag
//   Collects pointers to all nodes in the subtree with matching tag.
template<typename _Tag,
         unsigned _F,
         typename _I>
std::vector<dom_node<_Tag, _F, _I>*>
collect_by_tag(dom_node<_Tag, _F, _I>& _root,
               _Tag                    _tag)
{
    std::vector<dom_node<_Tag, _F, _I>*> out;

    walk(_root, [&out, _tag](auto& n, std::size_t)
    {
        if (n.data.tag == _tag)
        {
            out.push_back(&n);
        }
    });

    return out;
}

// collect_by_tag (const)
template<typename _Tag,
         unsigned _F,
         typename _I>
std::vector<const dom_node<_Tag, _F, _I>*>
collect_by_tag(const dom_node<_Tag, _F, _I>& _root,
               _Tag                          _tag)
{
    std::vector<const dom_node<_Tag, _F, _I>*> out;

    walk(_root, [&out, _tag](const auto& n, std::size_t)
    {
        if (n.data.tag == _tag)
        {
            out.push_back(&n);
        }
    });

    return out;
}

// query_by_class
//   Collects pointers to all nodes whose class_list contains _cls.
template<typename _Tag,
         unsigned _F,
         typename _I>
std::vector<dom_node<_Tag, _F, _I>*>
query_by_class(dom_node<_Tag, _F, _I>& _root,
               const std::string&      _cls)
{
    std::vector<dom_node<_Tag, _F, _I>*> out;

    walk(_root, [&out, &_cls](auto& n, std::size_t)
    {
        if (has_class(n.data, _cls))
        {
            out.push_back(&n);
        }
    });

    return out;
}

// count_by_tag
//   Returns the number of nodes in the subtree with matching tag.
template<typename _Tag,
         unsigned _F,
         typename _I>
std::size_t
count_by_tag(const dom_node<_Tag, _F, _I>& _root,
             _Tag                          _tag)
{
    std::size_t n = 0;

    walk(_root, [&n, _tag](const auto& node, std::size_t)
    {
        if (node.data.tag == _tag)
        {
            ++n;
        }
    });

    return n;
}

// walk_visible_elements
//   Walks only visible elements (respects collapse state AND
// element.visible flag).
template<typename _Tag,
         unsigned _F,
         typename _I,
         typename _Fn>
void walk_visible_elements(dom_node<_Tag, _F, _I>& _root,
                           _Fn&&                   _fn,
                           std::size_t             _depth = 0)
{
    if (!_root.data.visible)
    {
        return;
    }

    _fn(_root, _depth);

    if (_root.children_visible())
    {
        for (auto& child : _root.children)
        {
            walk_visible_elements(child, _fn, _depth + 1);
        }
    }

    return;
}

// walk_visible_elements (const)
template<typename _Tag,
         unsigned _F,
         typename _I,
         typename _Fn>
void walk_visible_elements(const dom_node<_Tag, _F, _I>& _root,
                           _Fn&&                         _fn,
                           std::size_t                   _depth = 0)
{
    if (!_root.data.visible)
    {
        return;
    }

    _fn(_root, _depth);

    if (_root.children_visible())
    {
        for (const auto& child : _root.children)
        {
            walk_visible_elements(child, _fn, _depth + 1);
        }
    }

    return;
}

// set_visible_recursive
//   Sets visible on _root and all descendants.
template<typename _Tag,
         unsigned _F,
         typename _I>
void set_visible_recursive(dom_node<_Tag, _F, _I>& _root,
                           bool                    _visible)
{
    walk(_root, [_visible](auto& n, std::size_t)
    {
        n.data.visible = _visible;
        ++n.data.revision;
    });

    return;
}

// set_enabled_recursive
//   Sets enabled on _root and all descendants.
template<typename _Tag,
         unsigned _F,
         typename _I>
void set_enabled_recursive(dom_node<_Tag, _F, _I>& _root,
                           bool                    _enabled)
{
    walk(_root, [_enabled](auto& n, std::size_t)
    {
        n.data.enabled = _enabled;
        ++n.data.revision;
    });

    return;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §7  DOM TREE VIEW OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════════
//   Free functions operating on dom_view (tree_view<dom_element>).
// These compose with tree_view's existing navigation and selection.

// dom_view_find_by_id
//   Searches all roots in the view for the given id.
// Returns pointer to the node, or nullptr.
template<typename _Tag,
         unsigned _F,
         typename _I>
dom_node<_Tag, _F, _I>*
dom_view_find_by_id(dom_view<_Tag, _F, _I>& _view,
                    const std::string&      _id)
{
    for (auto& root : _view.roots)
    {
        auto* found = find_by_id(root, _id);

        if (found)
        {
            return found;
        }
    }

    return nullptr;
}

// dom_view_collect_by_tag
//   Collects from all roots.
template<typename _Tag,
         unsigned _F,
         typename _I>
std::vector<dom_node<_Tag, _F, _I>*>
dom_view_collect_by_tag(dom_view<_Tag, _F, _I>& _view,
                        _Tag                    _tag)
{
    std::vector<dom_node<_Tag, _F, _I>*> out;

    for (auto& root : _view.roots)
    {
        auto batch = collect_by_tag(root, _tag);

        out.insert(out.end(), batch.begin(), batch.end());
    }

    return out;
}

// dom_view_total_nodes
//   Total node count across all roots.
template<typename _Tag,
         unsigned _F,
         typename _I>
std::size_t
dom_view_total_nodes(const dom_view<_Tag, _F, _I>& _view)
{
    std::size_t total = 0;

    for (const auto& root : _view.roots)
    {
        total += count_nodes(root);
    }

    return total;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §8  DOM TREE TRAITS
// ═══════════════════════════════════════════════════════════════════════════════
//   SFINAE detection for DOM element payloads within tree_node.
// Renderers use these to discover capabilities via if constexpr.

namespace dom_tree_traits {

namespace detail {

    // ── dom_element member detection ─────────────────────────────────

    template<typename, typename = void>
    struct has_tag_member : std::false_type {};
    template<typename _T>
    struct has_tag_member<_T, std::void_t<
        decltype(std::declval<_T>().tag)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_id_member : std::false_type {};
    template<typename _T>
    struct has_id_member<_T, std::void_t<
        decltype(std::declval<_T>().id)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_class_list_member : std::false_type {};
    template<typename _T>
    struct has_class_list_member<_T, std::void_t<
        decltype(std::declval<_T>().class_list)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_text_content_member : std::false_type {};
    template<typename _T>
    struct has_text_content_member<_T, std::void_t<
        decltype(std::declval<_T>().text_content)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_visible_member : std::false_type {};
    template<typename _T>
    struct has_visible_member<_T, std::void_t<
        decltype(std::declval<_T>().visible)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_enabled_member : std::false_type {};
    template<typename _T>
    struct has_enabled_member<_T, std::void_t<
        decltype(std::declval<_T>().enabled)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_properties_member : std::false_type {};
    template<typename _T>
    struct has_properties_member<_T, std::void_t<
        decltype(std::declval<_T>().properties)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_events_member : std::false_type {};
    template<typename _T>
    struct has_events_member<_T, std::void_t<
        decltype(std::declval<_T>().events)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_revision_member : std::false_type {};
    template<typename _T>
    struct has_revision_member<_T, std::void_t<
        decltype(std::declval<_T>().revision)
    >> : std::true_type {};

    template<typename, typename = void>
    struct has_tag_type_alias : std::false_type {};
    template<typename _T>
    struct has_tag_type_alias<_T, std::void_t<
        typename _T::tag_type
    >> : std::true_type {};

}   // namespace detail


// ── convenience aliases ──────────────────────────────────────────────

template<typename _T> inline constexpr bool has_tag_v          = detail::has_tag_member<_T>::value;
template<typename _T> inline constexpr bool has_id_v           = detail::has_id_member<_T>::value;
template<typename _T> inline constexpr bool has_class_list_v   = detail::has_class_list_member<_T>::value;
template<typename _T> inline constexpr bool has_text_content_v = detail::has_text_content_member<_T>::value;
template<typename _T> inline constexpr bool has_visible_v      = detail::has_visible_member<_T>::value;
template<typename _T> inline constexpr bool has_enabled_v      = detail::has_enabled_member<_T>::value;
template<typename _T> inline constexpr bool has_properties_v   = detail::has_properties_member<_T>::value;
template<typename _T> inline constexpr bool has_events_v       = detail::has_events_member<_T>::value;
template<typename _T> inline constexpr bool has_revision_v     = detail::has_revision_member<_T>::value;
template<typename _T> inline constexpr bool has_tag_type_v     = detail::has_tag_type_alias<_T>::value;


/*****************************************************************************/

// ── composite identity ──────────────────────────────────────────────

// is_dom_element
//   True if _T has tag + id + properties — the structural minimum
// for a DOM element payload.
template<typename _T>
struct is_dom_element : std::conjunction<
    detail::has_tag_member<_T>,
    detail::has_id_member<_T>,
    detail::has_properties_member<_T>
> {};

template<typename _T>
inline constexpr bool is_dom_element_v = is_dom_element<_T>::value;


/*****************************************************************************/

// is_identifiable_element
//   True if _T has an id member.
template<typename _T>
struct is_identifiable_element : detail::has_id_member<_T> {};

template<typename _T>
inline constexpr bool is_identifiable_element_v = is_identifiable_element<_T>::value;

// is_classifiable_element
//   True if _T has a class_list member.
template<typename _T>
struct is_classifiable_element : detail::has_class_list_member<_T> {};

template<typename _T>
inline constexpr bool is_classifiable_element_v = is_classifiable_element<_T>::value;

// is_evented_element
//   True if _T has an events member.
template<typename _T>
struct is_evented_element : detail::has_events_member<_T> {};

template<typename _T>
inline constexpr bool is_evented_element_v = is_evented_element<_T>::value;

// is_revisioned_element
//   True if _T has a revision member.
template<typename _T>
struct is_revisioned_element : detail::has_revision_member<_T> {};

template<typename _T>
inline constexpr bool is_revisioned_element_v = is_revisioned_element<_T>::value;

// is_visibility_aware
//   True if _T has visible and enabled members.
template<typename _T>
struct is_visibility_aware : std::conjunction<
    detail::has_visible_member<_T>,
    detail::has_enabled_member<_T>
> {};

template<typename _T>
inline constexpr bool is_visibility_aware_v = is_visibility_aware<_T>::value;


/*****************************************************************************/

// is_dom_tree_node
//   True if _T is a tree_node whose data type satisfies is_dom_element.
// Checks tree_traits::is_tree_node (data + children + is_leaf) AND
// that the payload is a DOM element.
template<typename _T,
         typename = void>
struct is_dom_tree_node : std::false_type {};

template<typename _T>
struct is_dom_tree_node<_T, std::enable_if_t<
    tree_traits::is_tree_node_v<_T> &&
    is_dom_element<typename _T::data_type>::value
>> : std::true_type {};

template<typename _T>
inline constexpr bool is_dom_tree_node_v = is_dom_tree_node<_T>::value;


/*****************************************************************************/

// is_dom_tree_view
//   True if _T is a tree_view whose node's data type is a DOM element.
template<typename _T,
         typename = void>
struct is_dom_tree_view : std::false_type {};

template<typename _T>
struct is_dom_tree_view<_T, std::enable_if_t<
    tree_traits::is_tree_view_v<_T> &&
    is_dom_element<typename _T::data_type>::value
>> : std::true_type {};

template<typename _T>
inline constexpr bool is_dom_tree_view_v = is_dom_tree_view<_T>::value;


/*****************************************************************************/

// dom_element_class
//   struct: aggregate classification of a DOM element type.
template<typename _T>
struct dom_element_class
{
    static constexpr bool is_element      = is_dom_element_v<_T>;
    static constexpr bool has_tag         = has_tag_v<_T>;
    static constexpr bool has_id          = has_id_v<_T>;
    static constexpr bool has_class_list  = has_class_list_v<_T>;
    static constexpr bool has_text        = has_text_content_v<_T>;
    static constexpr bool has_visible     = has_visible_v<_T>;
    static constexpr bool has_enabled     = has_enabled_v<_T>;
    static constexpr bool has_properties  = has_properties_v<_T>;
    static constexpr bool has_events      = has_events_v<_T>;
    static constexpr bool has_revision    = has_revision_v<_T>;
    static constexpr bool has_tag_type    = has_tag_type_v<_T>;
    static constexpr bool is_identifiable = is_identifiable_element_v<_T>;
    static constexpr bool is_classifiable = is_classifiable_element_v<_T>;
    static constexpr bool is_evented      = is_evented_element_v<_T>;
    static constexpr bool is_revisioned   = is_revisioned_element_v<_T>;
    static constexpr bool is_visibility_aware = is_visibility_aware_v<_T>;
};


}   // namespace dom_tree_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_DOM_TREE_VIEW_
