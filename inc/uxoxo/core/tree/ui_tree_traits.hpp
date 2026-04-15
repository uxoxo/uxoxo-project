/*******************************************************************************
* uxoxo [core]                                              ui_tree_traits.hpp
*
*   SFINAE-based structural detection traits for the uxoxo ui_tree module.
* All detection is purely structural — if the expression compiles in a
* void_t sink, the trait fires.  No base class, no tags, no registration.
*
*   The traits are organised in three tiers:
*
*   (a)  Internal detectors — one per method/field, hidden in
*        namespace internal.  These are the atomic building blocks.
*
*   (b)  Composite tree traits — conjunctions of detectors that
*        classify a type by its ui_tree facet: mutable, navigable,
*        observable, undoable, queryable, and complete.
*
*   (c)  Component type detectors — structural matching for the
*        supporting value types: mutation, constraint, ui_payload.
*
*   All traits produce `static constexpr bool` with `_v` suffixes.
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/core/tree/ui_tree_traits.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.14
*******************************************************************************/

#ifndef UXOXO_UI_TREE_TRAITS_
#define UXOXO_UI_TREE_TRAITS_ 1

// std
#include <cstddef>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
// uxoxo
#include "../../uxoxo.hpp"
#include "ui_tree.hpp"


NS_UXOXO
NS_UI_TREE


// =============================================================================
//  A.  INTERNAL DETECTORS
// =============================================================================

NS_INTERNAL

    // -- mutation gate --------------------------------------------------

    // has_apply_method
    //   trait: detects _Type::apply(const mutation&).
    template<typename _Type,
             typename = void>
    struct has_apply_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_apply_method<
        _Type,
        std::void_t<decltype(
            std::declval<_Type>().apply(
                std::declval<const mutation&>()))>>
        : std::true_type
    {
    };

    // has_apply_batch_method
    //   trait: detects _Type::apply_batch(const std::vector<mutation>&).
    template<typename _Type,
             typename = void>
    struct has_apply_batch_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_apply_batch_method<
        _Type,
        std::void_t<decltype(
            std::declval<_Type>().apply_batch(
                std::declval<const std::vector<mutation>&>()))>>
        : std::true_type
    {
    };

    // -- arena access ---------------------------------------------------

    // has_arena_method
    //   trait: detects _Type::arena() const.
    template<typename _Type,
             typename = void>
    struct has_arena_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_arena_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().arena())>>
        : std::true_type
    {
    };

    // -- observer -------------------------------------------------------

    // has_on_mutation_method
    //   trait: detects _Type::on_mutation().
    template<typename _Type,
             typename = void>
    struct has_on_mutation_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_on_mutation_method<
        _Type,
        std::void_t<decltype(
            std::declval<_Type>().on_mutation())>>
        : std::true_type
    {
    };

    // -- root access ----------------------------------------------------

    // has_root_method
    //   trait: detects _Type::root() const.
    template<typename _Type,
             typename = void>
    struct has_root_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_root_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().root())>>
        : std::true_type
    {
    };

    // has_has_root_method
    //   trait: detects _Type::has_root() const.
    template<typename _Type,
             typename = void>
    struct has_has_root_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_has_root_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().has_root())>>
        : std::true_type
    {
    };

    // has_set_root_method
    //   trait: detects _Type::set_root(std::string).
    template<typename _Type,
             typename = void>
    struct has_set_root_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_set_root_method<
        _Type,
        std::void_t<decltype(
            std::declval<_Type>().set_root(
                std::declval<std::string>()))>>
        : std::true_type
    {
    };

    // -- node access ----------------------------------------------------

    // has_valid_method
    //   trait: detects _Type::valid(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_valid_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_valid_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().valid(
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // has_payload_method
    //   trait: detects _Type::payload(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_payload_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_payload_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().payload(
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // -- navigation -----------------------------------------------------

    // has_parent_of_method
    //   trait: detects _Type::parent_of(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_parent_of_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_parent_of_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().parent_of(
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // has_first_child_of_method
    //   trait: detects _Type::first_child_of(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_first_child_of_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_first_child_of_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().first_child_of(
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // has_next_sibling_of_method
    //   trait: detects _Type::next_sibling_of(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_next_sibling_of_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_next_sibling_of_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().next_sibling_of(
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // has_child_count_of_method
    //   trait: detects _Type::child_count_of(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_child_count_of_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_child_count_of_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().child_count_of(
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // has_child_at_method
    //   trait: detects _Type::child_at(node_id, std::size_t) const.
    template<typename _Type,
             typename = void>
    struct has_child_at_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_child_at_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().child_at(
                std::declval<node_id>(),
                std::declval<std::size_t>()))>>
        : std::true_type
    {
    };

    // has_is_descendant_of_method
    //   trait: detects _Type::is_descendant_of(node_id, node_id) const.
    template<typename _Type,
             typename = void>
    struct has_is_descendant_of_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_is_descendant_of_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().is_descendant_of(
                std::declval<node_id>(),
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // has_is_leaf_method
    //   trait: detects _Type::is_leaf(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_is_leaf_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_is_leaf_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().is_leaf(
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // has_node_count_method
    //   trait: detects _Type::node_count() const.
    template<typename _Type,
             typename = void>
    struct has_node_count_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_node_count_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().node_count())>>
        : std::true_type
    {
    };

    // has_depth_of_method
    //   trait: detects _Type::depth_of(node_id) const.
    template<typename _Type,
             typename = void>
    struct has_depth_of_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_depth_of_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().depth_of(
                std::declval<node_id>()))>>
        : std::true_type
    {
    };

    // -- undo / redo ----------------------------------------------------

    // has_undo_method
    //   trait: detects _Type::undo().
    template<typename _Type,
             typename = void>
    struct has_undo_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_undo_method<
        _Type,
        std::void_t<decltype(
            std::declval<_Type>().undo())>>
        : std::true_type
    {
    };

    // has_redo_method
    //   trait: detects _Type::redo().
    template<typename _Type,
             typename = void>
    struct has_redo_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_redo_method<
        _Type,
        std::void_t<decltype(
            std::declval<_Type>().redo())>>
        : std::true_type
    {
    };

    // has_can_undo_method
    //   trait: detects _Type::can_undo() const.
    template<typename _Type,
             typename = void>
    struct has_can_undo_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_can_undo_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().can_undo())>>
        : std::true_type
    {
    };

    // has_can_redo_method
    //   trait: detects _Type::can_redo() const.
    template<typename _Type,
             typename = void>
    struct has_can_redo_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_can_redo_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().can_redo())>>
        : std::true_type
    {
    };

    // has_clear_history_method
    //   trait: detects _Type::clear_history().
    template<typename _Type,
             typename = void>
    struct has_clear_history_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_clear_history_method<
        _Type,
        std::void_t<decltype(
            std::declval<_Type>().clear_history())>>
        : std::true_type
    {
    };

    // -- component field detectors --------------------------------------

    // has_mutation_kind
    //   trait: detects _Type::kind of type DMutationKind.
    template<typename _Type,
             typename = void>
    struct has_mutation_kind : std::false_type
    {
    };

    template<typename _Type>
    struct has_mutation_kind<
        _Type,
        std::void_t<
            std::enable_if_t<std::is_same<
                decltype(_Type::kind),
                DMutationKind>::value>>>
        : std::true_type
    {
    };

    // has_mutation_target_id
    //   trait: detects _Type::target_id of type node_id.
    template<typename _Type,
             typename = void>
    struct has_mutation_target_id : std::false_type
    {
    };

    template<typename _Type>
    struct has_mutation_target_id<
        _Type,
        std::void_t<
            std::enable_if_t<std::is_same<
                decltype(_Type::target_id),
                node_id>::value>>>
        : std::true_type
    {
    };

    // has_mutation_parent_id
    //   trait: detects _Type::parent_id of type node_id.
    template<typename _Type,
             typename = void>
    struct has_mutation_parent_id : std::false_type
    {
    };

    template<typename _Type>
    struct has_mutation_parent_id<
        _Type,
        std::void_t<
            std::enable_if_t<std::is_same<
                decltype(_Type::parent_id),
                node_id>::value>>>
        : std::true_type
    {
    };

    // has_mutation_property_key
    //   trait: detects _Type::property_key of type std::string.
    template<typename _Type,
             typename = void>
    struct has_mutation_property_key : std::false_type
    {
    };

    template<typename _Type>
    struct has_mutation_property_key<
        _Type,
        std::void_t<
            std::enable_if_t<std::is_same<
                decltype(std::declval<_Type>().property_key),
                std::string>::value>>>
        : std::true_type
    {
    };

    // has_constraint_kind
    //   trait: detects _Type::kind of type DConstraintKind.
    template<typename _Type,
             typename = void>
    struct has_constraint_kind : std::false_type
    {
    };

    template<typename _Type>
    struct has_constraint_kind<
        _Type,
        std::void_t<
            std::enable_if_t<std::is_same<
                decltype(std::declval<_Type>().kind),
                DConstraintKind>::value>>>
        : std::true_type
    {
    };

    // has_validate_method
    //   trait: detects _Type::validate(const property_value&) const.
    template<typename _Type,
             typename = void>
    struct has_validate_method : std::false_type
    {
    };

    template<typename _Type>
    struct has_validate_method<
        _Type,
        std::void_t<decltype(
            std::declval<const _Type>().validate(
                std::declval<const property_value&>()))>>
        : std::true_type
    {
    };

    // has_payload_tag
    //   trait: detects _Type::tag of type std::string.
    template<typename _Type,
             typename = void>
    struct has_payload_tag : std::false_type
    {
    };

    template<typename _Type>
    struct has_payload_tag<
        _Type,
        std::void_t<
            std::enable_if_t<std::is_same<
                decltype(std::declval<_Type>().tag),
                std::string>::value>>>
        : std::true_type
    {
    };

    // has_payload_properties
    //   trait: detects _Type::properties as unordered_map<string, property_value>.
    template<typename _Type,
             typename = void>
    struct has_payload_properties : std::false_type
    {
    };

    template<typename _Type>
    struct has_payload_properties<
        _Type,
        std::void_t<
            std::enable_if_t<std::is_same<
                decltype(std::declval<_Type>().properties),
                std::unordered_map<std::string, property_value>>::value>>>
        : std::true_type
    {
    };

    // has_can_receive_children_field
    //   trait: detects _Type::can_receive_children as bool.
    template<typename _Type,
             typename = void>
    struct has_can_receive_children_field : std::false_type
    {
    };

    template<typename _Type>
    struct has_can_receive_children_field<
        _Type,
        std::void_t<
            std::enable_if_t<std::is_same<
                decltype(std::declval<_Type>().can_receive_children),
                bool>::value>>>
        : std::true_type
    {
    };

NS_END  // internal


// =============================================================================
//  B.  COMPOSITE TREE TRAITS
// =============================================================================

// is_ui_tree
//   trait: true if a type satisfies the core ui_tree interface —
// apply(), arena(), and on_mutation().
template<typename _Type>
struct is_ui_tree : std::conjunction<
    internal::has_apply_method<_Type>,
    internal::has_arena_method<_Type>,
    internal::has_on_mutation_method<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_ui_tree_v = is_ui_tree<_Type>::value;


// is_mutable_tree
//   trait: true if a type supports both single and batch mutation
// application.  Requires apply() and apply_batch().
template<typename _Type>
struct is_mutable_tree : std::conjunction<
    internal::has_apply_method<_Type>,
    internal::has_apply_batch_method<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_mutable_tree_v = is_mutable_tree<_Type>::value;


// is_navigable_tree
//   trait: true if a type provides full tree navigation — root access,
// parent/child traversal, sibling traversal, and child indexing.
template<typename _Type>
struct is_navigable_tree : std::conjunction<
    internal::has_root_method<_Type>,
    internal::has_valid_method<_Type>,
    internal::has_parent_of_method<_Type>,
    internal::has_first_child_of_method<_Type>,
    internal::has_next_sibling_of_method<_Type>,
    internal::has_child_count_of_method<_Type>,
    internal::has_child_at_method<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_navigable_tree_v =
    is_navigable_tree<_Type>::value;


// is_observable_tree
//   trait: true if a type exposes a mutation observer signal.
// Requires on_mutation().
template<typename _Type>
struct is_observable_tree : internal::has_on_mutation_method<_Type>
{
};

template<typename _Type>
D_INLINE constexpr bool is_observable_tree_v =
    is_observable_tree<_Type>::value;


// is_undoable_tree
//   trait: true if a type supports undo/redo operations.
// Requires undo(), redo(), can_undo(), can_redo(), and
// clear_history().
template<typename _Type>
struct is_undoable_tree : std::conjunction<
    internal::has_undo_method<_Type>,
    internal::has_redo_method<_Type>,
    internal::has_can_undo_method<_Type>,
    internal::has_can_redo_method<_Type>,
    internal::has_clear_history_method<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_undoable_tree_v =
    is_undoable_tree<_Type>::value;


// is_queryable_tree
//   trait: true if a type supports structural queries —
// is_leaf(), is_descendant_of(), node_count(), and depth_of().
template<typename _Type>
struct is_queryable_tree : std::conjunction<
    internal::has_is_leaf_method<_Type>,
    internal::has_is_descendant_of_method<_Type>,
    internal::has_node_count_method<_Type>,
    internal::has_depth_of_method<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_queryable_tree_v =
    is_queryable_tree<_Type>::value;


// is_complete_ui_tree
//   trait: true if a type satisfies all ui_tree facets — mutable,
// navigable, observable, undoable, queryable, with arena and
// payload access.
template<typename _Type>
struct is_complete_ui_tree : std::conjunction<
    is_mutable_tree<_Type>,
    is_navigable_tree<_Type>,
    is_observable_tree<_Type>,
    is_undoable_tree<_Type>,
    is_queryable_tree<_Type>,
    internal::has_arena_method<_Type>,
    internal::has_payload_method<_Type>,
    internal::has_set_root_method<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_complete_ui_tree_v =
    is_complete_ui_tree<_Type>::value;


// =============================================================================
//  C.  COMPONENT TYPE DETECTORS
// =============================================================================

// is_mutation_type
//   trait: true if a type structurally matches the mutation interface —
// kind, target_id, parent_id, and property_key fields.
template<typename _Type>
struct is_mutation_type : std::conjunction<
    internal::has_mutation_kind<_Type>,
    internal::has_mutation_target_id<_Type>,
    internal::has_mutation_parent_id<_Type>,
    internal::has_mutation_property_key<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_mutation_type_v =
    is_mutation_type<_Type>::value;


// is_constraint_type
//   trait: true if a type structurally matches the constraint
// interface — kind field of DConstraintKind and validate() method.
template<typename _Type>
struct is_constraint_type : std::conjunction<
    internal::has_constraint_kind<_Type>,
    internal::has_validate_method<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_constraint_type_v =
    is_constraint_type<_Type>::value;


// is_ui_payload_type
//   trait: true if a type structurally matches the ui_payload
// interface — tag, properties map, and can_receive_children flag.
template<typename _Type>
struct is_ui_payload_type : std::conjunction<
    internal::has_payload_tag<_Type>,
    internal::has_payload_properties<_Type>,
    internal::has_can_receive_children_field<_Type>>
{
};

template<typename _Type>
D_INLINE constexpr bool is_ui_payload_type_v =
    is_ui_payload_type<_Type>::value;


NS_END  // ui_tree
NS_END  // uxoxo


#endif  // UXOXO_UI_TREE_TRAITS_
