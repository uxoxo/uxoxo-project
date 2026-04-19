/*******************************************************************************
* uxoxo [component]                                          component_types.hpp
*
* Shared component type vocabulary:
*   Small enums and value types shared across more than one component
* template.  When a single-component header defines a type that a second
* component subsequently wants, it graduates here rather than being
* duplicated or cross-included.
*
*   This header deliberately contains no traits, no mixins, no functions —
* only plain value types.  Component-specific state machines that remain
* scoped to one component (DProgressStatus for progress_bar, any future
* selector-specific enums) stay in their own headers until they accrue
* a second consumer.
*
* Contents:
*   1  DOrientation    — layout axis for axis-sensitive components
*   2  DEmphasis       — semantic emphasis level for styling
*   3  DTextAlignment  — horizontal text alignment
*   4  DCheckState     — tri-state binary value (checkbox, future parent
*                        selectors)
*
*
* path:      /inc/uxoxo/templates/component/component_types.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.18
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_TYPES_
#define  UXOXO_COMPONENT_TYPES_ 1

// std
#include <cstdint>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ===============================================================================
//  1  ORIENTATION
// ===============================================================================

// DOrientation
//   enum: layout axis for axis-sensitive components.  Horizontal variants
// grow left-to-right; vertical variants grow bottom-to-top by convention,
// though renderers may invert either axis independently.
enum class DOrientation : std::uint8_t
{
    horizontal = 0,
    vertical   = 1
};


/*****************************************************************************/

// ===============================================================================
//  2  EMPHASIS
// ===============================================================================

// DEmphasis
//   enum: semantic emphasis level.  Renderers map this to concrete style
// (color in TUI, font weight / hue in GUI, pitch or pace in VUI).  Shared
// across labels, headings, buttons, badges, and anything else that needs
// to express semantic importance rather than literal styling.
enum class DEmphasis : std::uint8_t
{
    normal    = 0,  // default styling
    muted     = 1,  // dimmed / de-emphasized
    primary   = 2,  // main action / brand color
    secondary = 3,  // supporting action
    success   = 4,  // positive / completed
    warning   = 5,  // caution
    danger    = 6,  // destructive / failed
    info      = 7   // informational
};


/*****************************************************************************/

// ===============================================================================
//  3  TEXT ALIGNMENT
// ===============================================================================

// DTextAlignment
//   enum: horizontal text alignment within a cell, region, or component.
// Consumed by label_control, and by any future component that displays
// a text run within a bounded region (table cells, menu items,
// status bars).
enum class DTextAlignment : std::uint8_t
{
    left   = 0,
    center = 1,
    right  = 2
};


/*****************************************************************************/

// ===============================================================================
//  4  CHECK STATE
// ===============================================================================

// DCheckState
//   enum: tri-state binary value.  `mixed` represents the indeterminate
// state used by "check all" master controls whose governed children are
// in heterogeneous states.  Consumed primarily by checkbox; lives here
// rather than in checkbox.hpp so that toggleable_common.hpp can provide
// generic is_on / turn_on / toggle overloads without a circular include.
enum class DCheckState : std::uint8_t
{
    unchecked = 0,
    checked   = 1,
    mixed     = 2
};


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_TYPES_
