/*******************************************************************************
* uxoxo [core]                                            ui_tree_concepts.hpp
*
*   C++20 concept definitions for the uxoxo ui_tree module.  These concepts
* mirror the SFINAE traits in ui_tree_traits.hpp and provide first-class
* constraint syntax for template parameters, terse `requires` clauses, and
* improved compiler diagnostics.
*
*   The concepts are organised in three tiers matching the trait header:
*
*   (a)  Atomic concepts — one per interface facet, each testing a
*        single method or field via a `requires` expression.
*
*   (b)  Composite tree concepts — conjunctions of atomic concepts
*        that classify a type by its ui_tree role.
*
*   (c)  Component type concepts — structural matching for the
*        supporting value types: mutation, constraint, ui_payload.
*
*   All concepts are defined in namespace `uxoxo::ui_tree::ui_tree_concepts`.
*
*   REQUIRES: C++20 or later (gated on D_ENV_CPP_FEATURE_LANG_CONCEPTS).
*
*
* path:      /inc/uxoxo/core/tree/ui_tree_concepts.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.14
*******************************************************************************/

#ifndef UXOXO_UI_TREE_CONCEPTS_
#define UXOXO_UI_TREE_CONCEPTS_ 1

// std
#include <cstddef>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
// uxoxo
#include "../../uxoxo.hpp"
#include "ui_tree.hpp"


#if D_ENV_CPP_FEATURE_LANG_CONCEPTS


NS_UXOXO
NS_UI_TREE


// =============================================================================
//  A.  ATOMIC CONCEPTS — single interface facets
// =============================================================================

// -- mutation gate ----------------------------------------------------------

// applicable
//   concept: constrains types that accept a single mutation via apply().
template<typename _Type>
concept applicable = requires(_Type _t, const mutation& _m)
{
    { _t.apply(_m) } -> std::same_as<DMutationResult>;
};

// batch_applicable
//   concept: constrains types that accept a batch of mutations via
// apply_batch().
template<typename _Type>
concept batch_applicable = requires(_Type _t,
                                    const std::vector<mutation>& _ms)
{
    { _t.apply_batch(_ms) } -> std::same_as<DMutationResult>;
};

// -- arena access -----------------------------------------------------------

// arena_accessible
//   concept: constrains types that expose an arena via arena() const.
template<typename _Type>
concept arena_accessible = requires(const _Type _t)
{
    _t.arena();
};

// -- observer ---------------------------------------------------------------

// mutation_observable
//   concept: constrains types that expose a mutation signal via
// on_mutation().
template<typename _Type>
concept mutation_observable = requires(_Type _t)
{
    _t.on_mutation();
};

// -- root access ------------------------------------------------------------

// rooted
//   concept: constrains types with root() and has_root() const methods.
template<typename _Type>
concept rooted = requires(const _Type _t)
{
    { _t.root() }     -> std::same_as<node_id>;
    { _t.has_root() } -> std::same_as<bool>;
};

// root_settable
//   concept: constrains types that can set a root node from a tag string.
template<typename _Type>
concept root_settable = requires(_Type _t, std::string _tag)
{
    { _t.set_root(std::move(_tag)) } -> std::same_as<node_id>;
};

// -- node access ------------------------------------------------------------

// node_validatable
//   concept: constrains types that can validate a node_id.
template<typename _Type>
concept node_validatable = requires(const _Type _t, node_id _id)
{
    { _t.valid(_id) } -> std::same_as<bool>;
};

// payload_accessible
//   concept: constrains types that expose per-node payload by node_id.
template<typename _Type>
concept payload_accessible = requires(const _Type _t, node_id _id)
{
    _t.payload(_id);
};

// -- navigation -------------------------------------------------------------

// parent_navigable
//   concept: constrains types that can navigate to a node's parent.
template<typename _Type>
concept parent_navigable = requires(const _Type _t, node_id _id)
{
    { _t.parent_of(_id) } -> std::same_as<node_id>;
};

// child_navigable
//   concept: constrains types that support first-child traversal.
template<typename _Type>
concept child_navigable = requires(const _Type _t, node_id _id)
{
    { _t.first_child_of(_id) } -> std::same_as<node_id>;
};

// sibling_navigable
//   concept: constrains types that support next-sibling traversal.
template<typename _Type>
concept sibling_navigable = requires(const _Type _t, node_id _id)
{
    { _t.next_sibling_of(_id) } -> std::same_as<node_id>;
};

// child_countable
//   concept: constrains types that report child count by node_id.
template<typename _Type>
concept child_countable = requires(const _Type _t, node_id _id)
{
    { _t.child_count_of(_id) } -> std::same_as<std::size_t>;
};

// child_indexable
//   concept: constrains types that support indexed child access.
template<typename _Type>
concept child_indexable = requires(const _Type _t,
                                   node_id     _parent,
                                   std::size_t _index)
{
    { _t.child_at(_parent, _index) } -> std::same_as<node_id>;
};

// descendant_testable
//   concept: constrains types that can test ancestor/descendant
// relationships.
template<typename _Type>
concept descendant_testable = requires(const _Type _t,
                                       node_id    _child,
                                       node_id    _ancestor)
{
    { _t.is_descendant_of(_child, _ancestor) } -> std::same_as<bool>;
};

// leaf_testable
//   concept: constrains types that can test whether a node is a leaf.
template<typename _Type>
concept leaf_testable = requires(const _Type _t, node_id _id)
{
    { _t.is_leaf(_id) } -> std::same_as<bool>;
};

// node_countable
//   concept: constrains types that report total node count.
template<typename _Type>
concept node_countable = requires(const _Type _t)
{
    { _t.node_count() } -> std::same_as<std::size_t>;
};

// depth_queryable
//   concept: constrains types that report depth of a node.
template<typename _Type>
concept depth_queryable = requires(const _Type _t, node_id _id)
{
    { _t.depth_of(_id) } -> std::same_as<std::size_t>;
};

// -- undo / redo ------------------------------------------------------------

// undoable
//   concept: constrains types that support a single undo step.
template<typename _Type>
concept undoable = requires(_Type _t, const _Type _ct)
{
    { _t.undo() }       -> std::same_as<DMutationResult>;
    { _ct.can_undo() }  -> std::same_as<bool>;
};

// redoable
//   concept: constrains types that support a single redo step.
template<typename _Type>
concept redoable = requires(_Type _t, const _Type _ct)
{
    { _t.redo() }       -> std::same_as<DMutationResult>;
    { _ct.can_redo() }  -> std::same_as<bool>;
};

// history_clearable
//   concept: constrains types that can discard undo/redo history.
template<typename _Type>
concept history_clearable = requires(_Type _t)
{
    _t.clear_history();
};


// =============================================================================
//  B.  COMPOSITE TREE CONCEPTS
// =============================================================================

// ui_tree_type
//   concept: constrains types that satisfy the core ui_tree interface —
// apply(), arena(), and on_mutation().
template<typename _Type>
concept ui_tree_type =
    applicable<_Type>       &&
    arena_accessible<_Type> &&
    mutation_observable<_Type>;

// mutable_tree
//   concept: constrains types that support both single and batch
// mutation application.
template<typename _Type>
concept mutable_tree =
    applicable<_Type>       &&
    batch_applicable<_Type>;

// navigable_tree
//   concept: constrains types that provide full tree navigation —
// root access, parent/child traversal, sibling traversal, and
// child indexing.
template<typename _Type>
concept navigable_tree =
    rooted<_Type>            &&
    node_validatable<_Type>  &&
    parent_navigable<_Type>  &&
    child_navigable<_Type>   &&
    sibling_navigable<_Type> &&
    child_countable<_Type>   &&
    child_indexable<_Type>;

// observable_tree
//   concept: constrains types that expose a mutation observer signal.
template<typename _Type>
concept observable_tree = mutation_observable<_Type>;

// undoable_tree
//   concept: constrains types that support full undo/redo with
// history management.
template<typename _Type>
concept undoable_tree =
    undoable<_Type>          &&
    redoable<_Type>          &&
    history_clearable<_Type>;

// queryable_tree
//   concept: constrains types that support structural queries —
// leaf testing, descendant testing, node counting, and depth.
template<typename _Type>
concept queryable_tree =
    leaf_testable<_Type>       &&
    descendant_testable<_Type> &&
    node_countable<_Type>      &&
    depth_queryable<_Type>;

// complete_ui_tree
//   concept: constrains types that satisfy all ui_tree facets —
// mutable, navigable, observable, undoable, queryable, with arena
// and payload access.
template<typename _Type>
concept complete_ui_tree =
    mutable_tree<_Type>       &&
    navigable_tree<_Type>     &&
    observable_tree<_Type>    &&
    undoable_tree<_Type>      &&
    queryable_tree<_Type>     &&
    arena_accessible<_Type>   &&
    payload_accessible<_Type> &&
    root_settable<_Type>;


// =============================================================================
//  C.  COMPONENT TYPE CONCEPTS
// =============================================================================

// mutation_type
//   concept: constrains types that structurally match the mutation
// interface — kind, target_id, parent_id, and property_key fields.
template<typename _Type>
concept mutation_type = requires(_Type _m)
{
    { _Type::kind }         -> std::same_as<const DMutationKind&>;
    { _Type::target_id }    -> std::same_as<const node_id&>;
    { _Type::parent_id }    -> std::same_as<const node_id&>;
    { _m.property_key }     -> std::convertible_to<std::string>;
};

// constraint_type
//   concept: constrains types that structurally match the constraint
// interface — kind field of DConstraintKind and validate() method.
template<typename _Type>
concept constraint_type = requires(const _Type        _c,
                                   const property_value& _val)
{
    { _c.kind }            -> std::same_as<const DConstraintKind&>;
    { _c.validate(_val) }  -> std::same_as<bool>;
};

// ui_payload_type
//   concept: constrains types that structurally match the ui_payload
// interface — tag, properties map, and can_receive_children flag.
template<typename _Type>
concept ui_payload_type = requires(_Type _p)
{
    { _p.tag }                   -> std::convertible_to<std::string>;
    { _p.properties }            -> std::same_as<std::unordered_map<
                                        std::string, property_value>&>;
    { _p.can_receive_children }  -> std::same_as<bool&>;
};


NS_END  // ui_tree
NS_END  // uxoxo

#endif  // D_ENV_CPP_FEATURE_LANG_CONCEPTS


#endif  // UXOXO_UI_TREE_CONCEPTS_