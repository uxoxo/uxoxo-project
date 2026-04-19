/*******************************************************************************
* uxoxo [platform]                                           glfw_ui_tree.hpp
*
*   GLFW platform tree schema and builder.  Populates a ui_tree with a
* single root node representing the platform window and its OpenGL
* context.  Properties on this node are the source of truth for window
* title, dimensions, vsync, clear colour, and GL version.
*
*   The glfw_app_window reads from this tree at creation time, and
* mutations to these properties (e.g. resizing via a fairy or the
* WYSIWYG editor) propagate back to the GLFW window through the
* observer pattern.
*
*   Structure:
*     1.  tag constants
*     2.  property key constants
*     3.  glfw_layout_ids (returned by builder)
*     4.  build_glfw_tree (builder function)
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/platform/glfw/glfw_ui_tree.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.15
*******************************************************************************/

#ifndef UXOXO_GLFW_UI_TREE_
#define UXOXO_GLFW_UI_TREE_ 1

// std
#include <cstdint>
#include <string>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../core/tree/ui_tree.hpp"


NS_UXOXO
NS_PLATFORM
NS_GLFW


// -- type imports -----------------------------------------------------------
using uxoxo::ui_tree::ui_tree;
using uxoxo::ui_tree::ui_payload;
using uxoxo::ui_tree::DConstraintKind;
using uxoxo::ui_tree::property_value;
using djinterp::node_id;
using djinterp::null_node;


// =============================================================================
//  1.  TAG CONSTANTS
// =============================================================================

namespace glfw_tags
{
    // glfw_tags::window
    //   constant: the platform window root node.
    D_INLINE constexpr const char* window = "glfw_window";

}   // namespace glfw_tags


// =============================================================================
//  2.  PROPERTY KEY CONSTANTS
// =============================================================================

namespace glfw_props
{
    D_INLINE constexpr const char* title    = "title";
    D_INLINE constexpr const char* width    = "width";
    D_INLINE constexpr const char* height   = "height";
    D_INLINE constexpr const char* vsync    = "vsync";
    D_INLINE constexpr const char* gl_major = "gl_major";
    D_INLINE constexpr const char* gl_minor = "gl_minor";
    D_INLINE constexpr const char* clear_r  = "clear_r";
    D_INLINE constexpr const char* clear_g  = "clear_g";
    D_INLINE constexpr const char* clear_b  = "clear_b";
    D_INLINE constexpr const char* clear_a  = "clear_a";

}   // namespace glfw_props


// =============================================================================
//  3.  GLFW LAYOUT IDS
// =============================================================================

// glfw_layout_ids
//   struct: node identifiers returned by the GLFW tree builder.
struct glfw_layout_ids
{
    node_id window = null_node;
};


// =============================================================================
//  4.  BUILD GLFW TREE
// =============================================================================

/*
build_glfw_tree
    Populates a ui_tree with a single root node representing the
platform window.  Sets initial properties for title, dimensions,
vsync, OpenGL version, and clear colour.

Parameter(s):
    _tree:   the tree to populate (must be empty).
    _title:  window title string.
    _width:  initial window width in pixels.
    _height: initial window height in pixels.
Return:
    a glfw_layout_ids struct containing the window node id.
*/
D_INLINE glfw_layout_ids
build_glfw_tree(
    ui_tree&           _tree,
    const std::string& _title,
    std::int64_t       _width,
    std::int64_t       _height
)
{
    glfw_layout_ids ids;

    // root: the platform window
    ids.window = _tree.set_root(glfw_tags::window, _title);

    ui_payload& pl = _tree.payload(ids.window);

    // constraint: the window is required and cannot be removed
    pl.node_constraint.kind = DConstraintKind::required;
    pl.can_receive_children = false;

    // initial properties
    pl.properties[glfw_props::title]    = _title;
    pl.properties[glfw_props::width]    = _width;
    pl.properties[glfw_props::height]   = _height;
    pl.properties[glfw_props::vsync]    = true;
    pl.properties[glfw_props::gl_major] = std::int64_t(3);
    pl.properties[glfw_props::gl_minor] = std::int64_t(3);
    pl.properties[glfw_props::clear_r]  = 0.08;
    pl.properties[glfw_props::clear_g]  = 0.08;
    pl.properties[glfw_props::clear_b]  = 0.10;
    pl.properties[glfw_props::clear_a]  = 1.0;

    // property constraints: title is styleable, dimensions are
    // styleable (resizable), vsync and GL version are fixed
    pl.prop_constraints[glfw_props::title].kind    = DConstraintKind::styleable;
    pl.prop_constraints[glfw_props::width].kind    = DConstraintKind::styleable;
    pl.prop_constraints[glfw_props::height].kind   = DConstraintKind::styleable;
    pl.prop_constraints[glfw_props::vsync].kind    = DConstraintKind::fixed;
    pl.prop_constraints[glfw_props::gl_major].kind = DConstraintKind::fixed;
    pl.prop_constraints[glfw_props::gl_minor].kind = DConstraintKind::fixed;

    // construction is not undoable
    _tree.clear_history();

    return ids;
}


NS_END  // glfw
NS_END  // platform
NS_END  // uxoxo


#endif  // UXOXO_GLFW_UI_TREE_