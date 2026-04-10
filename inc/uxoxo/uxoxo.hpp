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

// D_KEYWORD_TEMPLATES
//   keyword: resolves to `templates`.
// Used to specify that a unit of code deals with templates.
#define U_KEYWORD_TEMPLATES             templates

// U_KEYWORD_UI
//   keyword: resolves to `ui`.
// Used to specify that a unit of code deals with templates.
#define U_KEYWORD_UI                    ui

// ii.  namespace macros
//////////////////////////////////////////

// NS_UXOXO
//   namespace: the root uxoxo framework namespace.
#define NS_UXOXO                        D_NAMESPACE(U_UXOXO_FRAMEWORK_NAME)

// NS_COMPONENT
//   namespace: the `component` namespace containing vendor-agnostic UI 
// component templates.
#define NS_COMPONENT                    D_NAMESPACE(U_KEYWORD_COMPONENT)

// NS_TEMPLATES
//   namespace: the `templates` namespace containing templates for various 
// individual UI components, UI layouts, and more.
#define NS_TEMPLATES                    D_NAMESPACE(U_KEYWORD_TEMPLATES)

// NS_UI
//   namespace: vendor-specific UI elements, layouts, widgets, and more.
#define NS_UI                           D_NAMESPACE(U_KEYWORD_UI)




#endif  // UXOXO_