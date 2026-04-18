/*******************************************************************************
* uxoxo [ui]                                                   frame_patch.hpp
*
*   Additions to components.hpp for the new `frame` container component.
* This file is not meant to be included directly — it is a patch that
* collects, in one place, every site in components.hpp that the `frame`
* addition touches.  Each section is labelled with its insertion point.
*
*   Once merged into components.hpp, this file can be discarded.
*
*
* path:      /inc/uxoxo/ui/frame_patch.hpp   (TRANSIENT)
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.17
*******************************************************************************/


/*═════════════════════════════════════════════════════════════════════════════*/
/*  §A.  ENUMERATIONS                                                          */
/*                                                                             */
/*        Insertion point: components.hpp, §ENUMERATIONS section,              */
/*        immediately after `sort_order`.                                      */
/*═════════════════════════════════════════════════════════════════════════════*/

// title_position
//   enum: which edge of a frame the title sits on.
enum class title_position : std::uint8_t
{
    top,
    bottom,
    left,
    right
};


/*═════════════════════════════════════════════════════════════════════════════*/
/*  §B.  FRAME STRUCT                                                          */
/*                                                                             */
/*        Insertion point: components.hpp, §4 CONTAINERS,                      */
/*        immediately after `panel`.                                           */
/*═════════════════════════════════════════════════════════════════════════════*/

// frame
//   Bordered container with a title that can sit on any of the four edges
// and align anywhere along that edge.  Generalizes `panel` (panel ≡ frame
// with title_position::top and text_alignment::left).
//
//   Title alignment semantics per edge:
//     top / bottom: left = flush-left, center, right = flush-right
//     left / right: left = flush-top,  center, right = flush-bottom
//                   (renderers typically stack or rotate the title along
//                    the vertical edge)
//
//   Edge cases:
//     - title empty         → plain bordered box, unbroken border line
//     - show_border false   → bare title, no surrounding line
//     - both                → invisible layout grouping (semantic only)
struct frame
{
    static constexpr bool focusable = false;

    std::string    title;
    node_list      children;
    title_position title_pos   = title_position::top;
    text_alignment title_align = text_alignment::left;
    bool           show_border = true;
    emphasis       emph        = emphasis::normal;
};


/*═════════════════════════════════════════════════════════════════════════════*/
/*  §C.  COMPONENT_VAR                                                         */
/*                                                                             */
/*        Insertion point: components.hpp, §7 COMPONENT VARIANT & NODE,        */
/*        add `frame` alternative after `panel`.                               */
/*═════════════════════════════════════════════════════════════════════════════*/

/*
using component_var = std::variant<
    // static
    label,
    heading,
    separator,
    // interactive
    button,
    textbox,
    checkbox,
    radio_group,
    // data
    list_view,
    progress_bar,
    scrollbar,
    // containers
    container,
    panel,
    frame,              // ←── NEW
    split_view,
    tab_bar,
    stacked_view,
    dialog,
    // MC composites
    menu_bar,
    function_bar,
    status_bar,
    command_line
>;
*/


/*═════════════════════════════════════════════════════════════════════════════*/
/*  §D.  NODE::CHILDREN_PTR OVERLOADS                                          */
/*                                                                             */
/*        Insertion point: components.hpp, §7 COMPONENT VARIANT & NODE,        */
/*        add one lambda arm to each of the two overloads.                     */
/*═════════════════════════════════════════════════════════════════════════════*/

/*
    // non-const overload — add this arm in the std::visit lambda set:
    [](frame& c) -> node_list* { return &c.children; },

    // const overload — add this arm in the std::visit lambda set:
    [](const frame& c) -> const node_list* { return &c.children; },
*/


/*═════════════════════════════════════════════════════════════════════════════*/
/*  §E.  BUILDER HELPER                                                        */
/*                                                                             */
/*        Insertion point: components.hpp, §9 BUILDER HELPERS,                 */
/*        inside `namespace build`, in the CONTAINERS subsection.              */
/*═════════════════════════════════════════════════════════════════════════════*/

/*
namespace build {

    // fr
    //   function: constructs a node wrapping a frame component with
    // configurable title, edge placement, alignment, and emphasis.
    // Border is enabled by default.
    inline node_ptr
    fr(
        std::string    _title_text,
        title_position _pos   = title_position::top,
        text_alignment _align = text_alignment::left,
        emphasis       _e     = emphasis::normal
    )
    {
        return make(frame{
            std::move(_title_text),
            {},
            _pos,
            _align,
            true,
            _e
        });
    }

}   // namespace build
*/
