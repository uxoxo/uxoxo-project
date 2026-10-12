/*******************************************************************************
* uxoxo [core]                                                element_window.hpp
*
* The window element: a column of children in a window of its own, over the
* window it is drawn from.
*   A "floating" window has a title bar and can be moved, resized and
* closed: "width" and "height" size it and "x" and "y" place it the first
* time it is shown -- after that it is where the user left it, and the
* backend remembers that as it remembers its own windows. Its close button
* reports "close_action". "focus" brings it to the front in the frame it is
* drawn with the attribute set.
*   An "overlay" window has no frame of its own and cannot be moved: it is
* drawn upward from where the layout has reached, as wide as the space
* there unless "width" says otherwise, and "height" high -- a drawer that
* slides up over what lies above it. It reports "leave_action" when the
* mouse has been away from it for "linger" seconds; "keep" pixels below it
* count as part of it, so the strip a drawer opens from can hold it open.
* While one of its own popups is open, or one of its items is held, the
* mouse is never away.
*   A window takes no room in the layout it is drawn from.
*
* path:      /inc/uxoxo/element_window.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.10
*                                                            revised: 2026.10.11
*******************************************************************************/

#ifndef UXOXO_ELEMENT_WINDOW_HPP
#define UXOXO_ELEMENT_WINDOW_HPP 1

// std
#include <string>   // std::string
#include <utility>  // std::move
#include <vector>   // std::vector
// uxoxo
#include "./element.hpp"  // element_type, make_element, option_set

NS_UXOXO

// archetype_window
//   constant: the render archetype for a window over the window it is
// drawn from.
inline constexpr const char* archetype_window = "window";


NS_COMPONENT

    // window_type
    //   the descriptor for the window element, owned by this module.
    // Unbounded arity: the children are the window's content.
    D_NODISCARD
    inline const element_type* window_type()
    {
        static const element_type descriptor =
            element_type{
                "window",
                archetype_window,
                option_set{ { { "id",     ::djinterp::option_value(
                                              std::string("window")) },
                              { "title",  ::djinterp::option_value(
                                              std::string()) },
                              { "mode",   ::djinterp::option_value(
                                              std::string("floating")) },
                              { "width",  ::djinterp::option_value(420.0) },
                              { "height", ::djinterp::option_value(280.0) },
                              { "linger", ::djinterp::option_value(0.4) },
                              { "keep",   ::djinterp::option_value(0.0) } } },
                0,
                -1,
                option_set{},   // no state
                handler_fn{}    // inert
            };

        return &descriptor;
    }

    /*
    window
      Builds a `window` template over its content.

    Parameter(s):
      _id:    the window's name ("id"), unique among the windows of the
              application; the backend remembers a floating window's place
              by it.
      _items: its content, top to bottom.
      _attrs: optional extra attributes (title, mode, width, height, x, y,
              focus, close_action, linger, keep, leave_action), overlaid on
              top (caller wins).
    Return:
      An element_template wrapping a window node over _items.
    */
    D_NODISCARD
    inline element_template window(
        const std::string&            _id,
        std::vector<element_template> _items,
        option_set                    _attrs = option_set{}
    )
    {
        option_set resolved = window_type()->defaults;
        resolved.set("id", _id);
        resolved = ::djinterp::overlay(resolved, _attrs);

        return make_element(window_type(), resolved, std::move(_items));
    }

NS_END  // component

NS_END  // uxoxo


#endif  // UXOXO_ELEMENT_WINDOW_HPP
