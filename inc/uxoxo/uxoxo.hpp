/******************************************************************************
* uxoxo [core]                                                       uxoxo.hpp
*
*
*
* path:      /inc/uxoxo/uxoxo.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2025.05.19
******************************************************************************/

#ifndef UXOXO_
#define UXOXO_ 1

#include <djinterp/core/djinterp.hpp>


///////////////////////////////////////////////////////////////////////////////
///                  I.   KEYWORDS & NAMESPACE MACROS                       ///
///////////////////////////////////////////////////////////////////////////////

// i.   keywords
//////////////////////////////////////////

// D_KEYWORD_FRAMEWORK_NAME
//   keyword: resolves to `uxoxo`.
//   constant: keyword corresponding to the name of this framework.
#define U_UXOXO_FRAMEWORK_NAME          uxoxo

// D_KEYWORD_COMPONENT
//   keyword: resolves to `component`.
// Used to specify that a unit of code deals individual UI components.
#define U_KEYWORD_COMPONENT             component

// U_KEYWORD_PLATFORM
//   keyword: resolves to `platform`.
// Specifies vendor-specific functionality.
#define U_KEYWORD_PLATFORM              platform

// U_KEYWORD_UI_TREE
//   keyword: resolves to `tree`.
// Used to specify that a unit of code deals with the central UI tree that is
// central to the uxoxo framework.
#define U_KEYWORD_UI_TREE               ui_tree

// D_KEYWORD_TEMPLATES
//   keyword: resolves to `templates`.
// Used to specify that a unit of code deals with templates.
#define U_KEYWORD_TEMPLATES             templates

// U_KEYWORD_UI
//   keyword: resolves to `ui`.
// Used to specify that a unit of code deals with templates.
#define U_KEYWORD_UI                    ui

// U_KEYWORD_WYSIWYG
//   keyword: resolves to `wysiwyg`.
// Used to specify that a unit of code deals the WYSIWYG editor (what you see
// is what you get).
#define U_KEYWORD_WYSIWYG               wysiwyg

// ii.  namespace macros
//////////////////////////////////////////

// NS_UXOXO
//   namespace: the root uxoxo framework namespace.
#define NS_UXOXO                        D_NAMESPACE(U_UXOXO_FRAMEWORK_NAME)

// NS_COMPONENT
//   namespace: the `component` namespace containing vendor-agnostic UI 
// component templates.
#define NS_COMPONENT                    D_NAMESPACE(U_KEYWORD_COMPONENT)

// NS_PLATFORM
//   namespace: vendor-specific UI elements, layouts, widgets, and more.
#define NS_PLATFORM                     D_NAMESPACE(U_KEYWORD_PLATFORM)

// NS_TEMPLATES
//   namespace: the `templates` namespace containing templates for various 
// individual UI components, UI layouts, and more.
#define NS_TEMPLATES                    D_NAMESPACE(U_KEYWORD_TEMPLATES)

// NS_UI
//   namespace: vendor-specific UI elements, layouts, widgets, and more.
#define NS_UI                           D_NAMESPACE(U_KEYWORD_UI)

// NS_UI_TREE
//   namespace: the `tree` namespace, corresponding to the central UI element
// tree.
#define NS_UI_TREE                      D_NAMESPACE(U_KEYWORD_UI_TREE)

// NS_WYSIWYG
//   namespace:  WYSIWYG editor namespace (what you see is what you get).
#define NS_WYSIWYG                      D_NAMESPACE(U_KEYWORD_WYSIWYG)

// NS_GLFW
//   namespace: vendor-specific UI elements, layouts, widgets, and more.
#define NS_GLFW                         D_NAMESPACE(glfw)

// NS_IMGUI
//   namespace: vendor-specific UI elements, layouts, widgets, and more.
#define NS_IMGUI                        D_NAMESPACE(imgui)


#endif  // UXOXO_