/*******************************************************************************
* uxoxo [core]                                                      ui_tree.hpp
*
*   Core semantic tree for the uxoxo meta-framework, backed by the djinterp
* arena_tree for O(1) node management, free-list recycling, and identity
* tracking.
*
*   The ui_tree is the single source of truth for program UI state.  All
* interfaces — console, WYSIWYG editor, DOM tree view, fairy — interact
* with the tree exclusively through mutations, and observe changes through
* the observer pattern.  The constraint system inside tree::apply() is the
* firewall that prevents any interface from breaking structural invariants.
*
*   Integration with djinterp patterns:
*
*   observer.hpp — mutation notifications use the three-tier observer
*     signal system.  Listeners connect to `tree::on_mutation()` and
*     receive RAII connection handles for automatic cleanup.  No
*     abstract base class coupling — any callable works.
*
*   memento.hpp — undo/redo stacks are managed by memento_caretaker
*     with bounded_history for automatic trimming.  The undo_entry
*     struct is the "snapshot" type — a targeted reversal record, not
*     a full state copy.  Arena detach-without-destroy provides the
*     underlying mechanism; the memento system provides stack
*     management, sequencing, and history policy.
*
*   arena_tree — O(1) node lookup, append, detach, reparent.  Free-list
*     recycling means detached nodes stay alive for undo.  Stable
*     identity via arena stable_id + version counters.
*
*   Structure:
*     1.   property_value          variant for node properties
*     2.   DConstraintKind         constraint severity enum
*     3.   constraint              per-node/per-property constraint
*     4.   ui_payload              per-node semantic data (arena payload)
*     5.   DMutationKind           mutation operation enum
*     6.   mutation                a single tree operation
*     7.   DMutationResult         outcome of applying a mutation
*     8.   mutation_signal         observer signal type (replaces class)
*     9.   undo_entry              reversal record (memento snapshot type)
*     10.  tree                    owns arena, applies mutations, notifies
*     11.  free function API       node/tree queries and mutation factories
*     12.  traits                  SFINAE detection
*
*   REQUIRES: C++17 or later.
*
*
* path:      /inc/uxoxo/core/ui_tree/ui_tree.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.04.11
*******************************************************************************/

#ifndef UXOXO_COMPONENT_UI_TREE_
#define UXOXO_COMPONENT_UI_TREE_ 1

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "../../uxoxo.hpp"

// djinterp infrastructure
#include "djinterp/container/arena/arena_tree.hpp"
#include "djinterp/paradigm/pattern/observer.hpp"
#include "djinterp/paradigm/memento/memento.hpp"


NS_UXOXO
NS_COMPONENT


// -- arena type imports ------------------------------------------------------
using djinterp::container::node_id;
using djinterp::container::null_node;
using djinterp::container::full_nary_link_policy;
using djinterp::container::arena_tree;
using djinterp::container::arena_node;


// =============================================================================
//  1.  PROPERTY VALUE
// =============================================================================

// property_value
//   type: variant holding a single property value.
using property_value = std::variant<
    std::monostate,         // unset / null
    bool,                   // boolean flag
    std::int64_t,           // integer
    double,                 // floating point
    std::string,            // text
    std::uint32_t,          // colour (RGBA packed)
    std::size_t             // node reference (id)
>;

// DPropertyType
//   enum: discriminator tags mirroring the variant alternatives.
enum class DPropertyType : std::uint8_t
{
    unset    = 0,
    boolean  = 1,
    integer  = 2,
    floating = 3,
    text     = 4,
    colour   = 5,
    node_ref = 6
};

// property_type_of
//   function: returns the DPropertyType for a property_value.
D_INLINE DPropertyType
property_type_of(
    const property_value& _val
) noexcept
{
    return static_cast<DPropertyType>(_val.index());
}


// =============================================================================
//  2.  CONSTRAINT KIND
// =============================================================================

// DConstraintKind
//   enum: governs what operations are permitted on a node.
enum class DConstraintKind : std::uint8_t
{
    required  = 0,
    fixed     = 1,
    movable   = 2,
    styleable = 3,
    optional  = 4,
    generated = 5
};


// =============================================================================
//  3.  CONSTRAINT
// =============================================================================

// property_validator_fn
//   type: callable that validates a proposed property value.
using property_validator_fn =
    std::function<bool(const property_value& _proposed)>;

// constraint
//   struct: constraint attached to a node or to a specific property.
struct constraint
{
    DConstraintKind       kind = DConstraintKind::optional;
    property_validator_fn validator;

    bool
    has_validator() const noexcept
    {
        return static_cast<bool>(validator);
    }

    bool
    validate(
        const property_value& _proposed
    ) const
    {
        if (!validator)
        {
            return true;
        }

        return validator(_proposed);
    }
};


// =============================================================================
//  4.  UI PAYLOAD
// =============================================================================

// ui_payload
//   struct: semantic data stored per arena node.
struct ui_payload
{
    std::string     tag;
    std::string     label;

    std::unordered_map<std::string,
                       property_value>  properties;

    constraint      node_constraint;
    std::unordered_map<std::string,
                       constraint>      prop_constraints;

    bool            can_receive_children = true;

    ui_payload() = default;

    explicit ui_payload(
            std::string _tag
        )
            : tag(std::move(_tag))
        {
        }

    ui_payload(
            std::string _tag,
            std::string _label
        )
            : tag(std::move(_tag)),
              label(std::move(_label))
        {
        }
};


// =============================================================================
//  5.  MUTATION KIND
// =============================================================================

enum class DMutationKind : std::uint8_t
{
    insert          = 0,
    remove          = 1,
    move            = 2,
    set_property    = 3,
    remove_property = 4
};


// =============================================================================
//  6.  MUTATION
// =============================================================================

// mutation
//   struct: a single proposed tree operation.
struct mutation
{
    DMutationKind   kind        = DMutationKind::set_property;
    node_id         target_id   = null_node;
    node_id         parent_id   = null_node;
    std::size_t     position    = static_cast<std::size_t>(-1);
    std::string     property_key;
    property_value  property_val;
    std::string     insert_tag;
    std::unordered_map<std::string,
                       property_value> insert_properties;

    static mutation
    make_insert(
        std::string _tag,
        node_id     _parent_id,
        std::size_t _position = static_cast<std::size_t>(-1)
    )
    {
        mutation m;
        m.kind       = DMutationKind::insert;
        m.insert_tag = std::move(_tag);
        m.parent_id  = _parent_id;
        m.position   = _position;
        return m;
    }

    static mutation
    make_remove(
        node_id _target_id
    )
    {
        mutation m;
        m.kind      = DMutationKind::remove;
        m.target_id = _target_id;
        return m;
    }

    static mutation
    make_move(
        node_id     _target_id,
        node_id     _new_parent_id,
        std::size_t _position = static_cast<std::size_t>(-1)
    )
    {
        mutation m;
        m.kind      = DMutationKind::move;
        m.target_id = _target_id;
        m.parent_id = _new_parent_id;
        m.position  = _position;
        return m;
    }

    static mutation
    make_set_property(
        node_id        _target_id,
        std::string    _key,
        property_value _value
    )
    {
        mutation m;
        m.kind         = DMutationKind::set_property;
        m.target_id    = _target_id;
        m.property_key = std::move(_key);
        m.property_val = std::move(_value);
        return m;
    }

    static mutation
    make_remove_property(
        node_id     _target_id,
        std::string _key
    )
    {
        mutation m;
        m.kind         = DMutationKind::remove_property;
        m.target_id    = _target_id;
        m.property_key = std::move(_key);
        return m;
    }
};


// =============================================================================
//  7.  MUTATION RESULT
// =============================================================================

enum class DMutationResult : std::uint8_t
{
    accepted = 0,
    rejected = 1,
    coerced  = 2
};


// =============================================================================
//  8.  MUTATION SIGNAL
// =============================================================================
//   Replaces the hand-rolled abstract observer base class with the
// djinterp three-tier observer pattern.  Listeners connect any
// callable and receive RAII connection handles.
//
// Usage:
//   auto conn = tree.on_mutation().connect(
//       [](const mutation& m, DMutationResult r, node_id id)
//       {
//           // handle
//       });
//
//   scoped_connection guard(std::move(conn));   // auto-disconnect
//
//   For single-listener cases (e.g. a fairy that observes one tree),
// use delegate instead:
//   delegate<void(const mutation&, DMutationResult, node_id)>

// mutation_signature
//   type: the signal signature for mutation notifications.
using mutation_signature =
    void(const mutation&, DMutationResult, node_id);

// mutation_signal
//   type: dynamic observer with RAII connection tracking.
using mutation_signal =
    djinterp::pattern::observer<mutation_signature>;

// mutation_connection
//   type: connection handle returned by signal.connect().
using mutation_connection =
    djinterp::pattern::connection;

// mutation_scoped_connection
//   type: RAII guard that disconnects on destruction.
using mutation_scoped_connection =
    djinterp::pattern::scoped_connection;

// mutation_delegate
//   type: single-slot zero-overhead listener (8 bytes).
using mutation_delegate =
    djinterp::pattern::delegate<mutation_signature>;

// mutation_event
//   type: fixed-capacity listener set (no heap).
template<std::size_t _N>
using mutation_event =
    djinterp::pattern::event<mutation_signature, _N>;


// =============================================================================
//  9.  UNDO ENTRY
// =============================================================================
//   The "snapshot type" for the memento system.  Not a full state
// copy — a targeted reversal record that exploits arena detach-
// without-destroy for structural mutations.

struct undo_entry
{
    DMutationKind kind;
    node_id       affected_id  = null_node;
    node_id       old_parent   = null_node;
    node_id       old_prev_sib = null_node;
    std::string   prop_key;
    property_valu old_value;
    bool          had_property = false;
};

// -- memento type aliases ----------------------------------------------------
//   undo_entry is the snapshot; memento<undo_entry> adds sequencing
// and metadata.  bounded_history<128> trims old entries automatically.

// undo_memento
//   type: a single undo_entry wrapped with memento metadata.
using undo_memento =
    djinterp::memento<undo_entry>;

// undo_history_policy
//   type: history eviction policy for the undo/redo stacks.
// 128 levels by default — enough for interactive editing
// without unbounded memory growth.
using undo_history_policy =
    djinterp::bounded_history<128>;

// undo_caretaker
//   type: manages a bounded stack of undo mementos.
using undo_caretaker =
    djinterp::memento_caretaker<undo_entry, undo_history_policy>;


// =============================================================================
//  10. TREE
// =============================================================================

// tree
//   class: the semantic UI tree — single source of truth.
class tree
{
public:
    using arena_type =
        arena_tree<ui_payload, full_nary_link_policy>;
    using node_type  =
        arena_node<ui_payload, full_nary_link_policy>;

    tree()
        : m_arena(64)
    {}

    explicit tree(
        std::size_t _reserve
    )
        : m_arena(_reserve)
    {}

    ~tree() = default;

    tree(const tree&) = delete;
    tree& operator=(const tree&) = delete;
    tree(tree&&) noexcept = default;
    tree& operator=(tree&&) noexcept = default;

    // -- root --------------------------------------------------------

    node_id
    root() const noexcept
    {
        return m_arena.root();
    }

    bool
    has_root() const noexcept
    {
        return m_arena.has_root();
    }

    node_id
    set_root(
        std::string _tag
    )
    {
        return m_arena.create_root(
            ui_payload(std::move(_tag)));
    }

    node_id
    set_root(
        std::string _tag,
        std::string _label
    )
    {
        return m_arena.create_root(
            ui_payload(std::move(_tag),
                       std::move(_label)));
    }

    // -- node access -------------------------------------------------

    const ui_payload&
    payload(
        node_id _id
    ) const
    {
        return m_arena.data(_id);
    }

    ui_payload&
    payload(
        node_id _id
    )
    {
        return m_arena.data(_id);
    }

    bool
    valid(
        node_id _id
    ) const noexcept
    {
        return m_arena.valid(_id);
    }

    // -- navigation --------------------------------------------------

    node_id
    parent_of(
        node_id _id
    ) const
    {
        return m_arena[_id].parent();
    }

    node_id
    first_child_of(
        node_id _id
    ) const
    {
        return m_arena[_id].first_child();
    }

    node_id
    next_sibling_of(
        node_id _id
    ) const
    {
        return m_arena[_id].next_sibling();
    }

    node_id
    prev_sibling_of(
        node_id _id
    ) const
    {
        return m_arena[_id].prev_sibling();
    }

    std::size_t
    child_count_of(
        node_id _id
    ) const
    {
        return m_arena.child_count(_id);
    }

    node_id
    child_at(
        node_id     _parent,
        std::size_t _index
    ) const
    {
        node_id cur = m_arena[_parent].first_child();
        std::size_t i = 0;

        while ( (cur != null_node) &&
                (i < _index) )
        {
            cur = m_arena[cur].next_sibling();
            ++i;
        }

        return cur;
    }

    bool
    is_leaf(
        node_id _id
    ) const
    {
        return m_arena[_id].is_leaf();
    }

    bool
    is_descendant_of(
        node_id _child_id,
        node_id _ancestor_id
    ) const
    {
        node_id cur = m_arena[_child_id].parent();

        while (cur != null_node)
        {
            if (cur == _ancestor_id)
            {
                return true;
            }

            cur = m_arena[cur].parent();
        }

        return false;
    }

    std::size_t
    node_count() const noexcept
    {
        return m_arena.size();
    }

    std::size_t
    depth_of(
        node_id _id
    ) const
    {
        return m_arena.depth(_id);
    }

    // -- apply (THE WRITE GATE) --------------------------------------

    DMutationResult
    apply(
        const mutation& _mut
    )
    {
        DMutationResult result = DMutationResult::rejected;
        node_id         target = null_node;

        switch (_mut.kind)
        {
            case DMutationKind::insert:
            {
                result = apply_insert(_mut, target);
                break;
            }

            case DMutationKind::remove:
            {
                target = _mut.target_id;
                result = apply_remove(_mut);

                if (result != DMutationResult::rejected)
                {
                    target = null_node;
                }

                break;
            }

            case DMutationKind::move:
            {
                result = apply_move(_mut);
                target = _mut.target_id;
                break;
            }

            case DMutationKind::set_property:
            {
                result = apply_set_property(_mut);
                target = _mut.target_id;
                break;
            }

            case DMutationKind::remove_property:
            {
                result = apply_remove_property(_mut);
                target = _mut.target_id;
                break;
            }
        }

        m_on_mutation.notify(_mut, result, target);

        return result;
    }

    DMutationResult
    apply_batch(
        const std::vector<mutation>& _mutations
    )
    {
        for (const auto& mut : _mutations)
        {
            DMutationResult r = apply(mut);

            if (r == DMutationResult::rejected)
            {
                return DMutationResult::rejected;
            }
        }

        return DMutationResult::accepted;
    }

    // -- signal access -----------------------------------------------
    //   Listeners connect to on_mutation() and receive a connection
    // handle for lifetime management.  No abstract base class
    // coupling — any callable matching mutation_signature works.

    mutation_signal&
    on_mutation() noexcept
    {
        return m_on_mutation;
    }

    const mutation_signal&
    on_mutation() const noexcept
    {
        return m_on_mutation;
    }

    // -- undo / redo -------------------------------------------------

    bool can_undo() const noexcept { return (!m_undo.empty()); }
    bool can_redo() const noexcept { return (!m_redo.empty()); }

    std::size_t undo_depth() const noexcept { return m_undo.size(); }
    std::size_t redo_depth() const noexcept { return m_redo.size(); }

    DMutationResult
    undo()
    {
        if (m_undo.empty())
        {
            return DMutationResult::rejected;
        }

        undo_memento mem = m_undo.pop();
        undo_entry&  entry = mem.state;

        undo_entry redo_e = capture_for_redo(entry);

        DMutationResult result = apply_inverse(entry);

        if (result != DMutationResult::rejected)
        {
            m_redo.push(undo_memento(
                std::move(redo_e),
                djinterp::memento_metadata(m_undo_seq++)));
        }

        return result;
    }

    DMutationResult
    redo()
    {
        if (m_redo.empty())
        {
            return DMutationResult::rejected;
        }

        undo_memento mem = m_redo.pop();
        undo_entry&  entry = mem.state;

        undo_entry undo_e = capture_for_redo(entry);

        DMutationResult result = apply_inverse(entry);

        if (result != DMutationResult::rejected)
        {
            m_undo.push(undo_memento(
                std::move(undo_e),
                djinterp::memento_metadata(m_undo_seq++)));
        }

        return result;
    }

    // clear_history
    //   discards all undo and redo history.
    // TODO: deallocate arena nodes from discarded remove entries.
    void
    clear_history()
    {
        m_undo.clear();
        m_redo.clear();
        m_undo_seq = 0;

        return;
    }

    // -- direct arena access -----------------------------------------

    const arena_type& arena() const noexcept { return m_arena; }
    arena_type&       arena()       noexcept { return m_arena; }

private:
    // -- position helpers --------------------------------------------

    void
    capture_position(
        node_id  _id,
        node_id& _out_parent,
        node_id& _out_prev_sib
    ) const
    {
        _out_parent   = m_arena[_id].parent();
        _out_prev_sib = m_arena[_id].prev_sibling();

        return;
    }

    void
    insert_at_position(
        node_id     _parent,
        node_id     _child,
        std::size_t _position
    )
    {
        if (_position == 0)
        {
            m_arena.prepend_child(_parent, _child);
        }
        else
        {
            node_id prev = m_arena[_parent].first_child();

            for (std::size_t i = 1;
                 (i < _position) && (prev != null_node);
                 ++i)
            {
                node_id next = m_arena[prev].next_sibling();

                if (next == null_node)
                {
                    break;
                }

                prev = next;
            }

            if (prev != null_node)
            {
                m_arena.insert_after(prev, _child);
            }
            else
            {
                m_arena.append_child(_parent, _child);
            }
        }

        return;
    }

    void
    reattach_at(
        node_id _child,
        node_id _parent,
        node_id _prev_sib
    )
    {
        if (_parent == null_node)
        {
            m_arena.set_root(_child);

            return;
        }

        if (_prev_sib == null_node)
        {
            m_arena.prepend_child(_parent, _child);
        }
        else
        {
            m_arena.insert_after(_prev_sib, _child);
        }

        return;
    }

    // -- undo stack push helper --------------------------------------

    void
    push_undo(
        undo_entry _entry
    )
    {
        m_undo.push(undo_memento(
            std::move(_entry),
            djinterp::memento_metadata(m_undo_seq++)));

        m_redo.clear();

        return;
    }

    // -- constraint checking -----------------------------------------

    bool
    check_remove_allowed(
        node_id _id
    ) const
    {
        if (!m_arena.valid(_id))
        {
            return false;
        }

        DConstraintKind k = m_arena.data(_id).node_constraint.kind;

        return ( (k == DConstraintKind::optional) ||
                 (k == DConstraintKind::generated) );
    }

    bool
    check_move_allowed(
        node_id _id
    ) const
    {
        if (!m_arena.valid(_id))
        {
            return false;
        }

        DConstraintKind k = m_arena.data(_id).node_constraint.kind;

        return ( (k == DConstraintKind::movable)  ||
                 (k == DConstraintKind::optional)  ||
                 (k == DConstraintKind::generated) );
    }

    bool
    check_set_property_allowed(
        node_id               _id,
        const std::string&    _key,
        const property_value& _value
    ) const
    {
        if (!m_arena.valid(_id))
        {
            return false;
        }

        const ui_payload& p = m_arena.data(_id);
        DConstraintKind nk = p.node_constraint.kind;

        if ( (nk == DConstraintKind::required) ||
             (nk == DConstraintKind::fixed) )
        {
            return false;
        }

        auto pc_it = p.prop_constraints.find(_key);

        if (pc_it != p.prop_constraints.end())
        {
            const constraint& pc = pc_it->second;

            if ( (pc.kind == DConstraintKind::required) ||
                 (pc.kind == DConstraintKind::fixed) )
            {
                return false;
            }

            if (!pc.validate(_value))
            {
                return false;
            }
        }

        if (!p.node_constraint.validate(_value))
        {
            return false;
        }

        return true;
    }

    // -- mutation implementation --------------------------------------

    DMutationResult
    apply_insert(
        const mutation& _mut,
        node_id&        _out_id
    )
    {
        if (!m_arena.valid(_mut.parent_id))
        {
            return DMutationResult::rejected;
        }

        if (!m_arena.data(_mut.parent_id).can_receive_children)
        {
            return DMutationResult::rejected;
        }

        ui_payload p(_mut.insert_tag);

        for (const auto& [key, val] : _mut.insert_properties)
        {
            p.properties[key] = val;
        }

        p.node_constraint.kind = DConstraintKind::generated;

        node_id new_id;

        if ( (_mut.position == static_cast<std::size_t>(-1)) ||
             (_mut.position >= m_arena.child_count(
                  _mut.parent_id)) )
        {
            new_id = m_arena.add_child(
                _mut.parent_id, std::move(p));
        }
        else
        {
            new_id = m_arena.allocate(std::move(p));
            insert_at_position(_mut.parent_id,
                                new_id,
                                _mut.position);
        }

        _out_id = new_id;

        undo_entry undo;
        undo.kind        = DMutationKind::insert;
        undo.affected_id = new_id;

        capture_position(new_id,
                          undo.old_parent,
                          undo.old_prev_sib);

        push_undo(std::move(undo));

        m_arena.bump_version(new_id);

        return DMutationResult::accepted;
    }

    DMutationResult
    apply_remove(
        const mutation& _mut
    )
    {
        if (!check_remove_allowed(_mut.target_id))
        {
            return DMutationResult::rejected;
        }

        undo_entry undo;
        undo.kind        = DMutationKind::remove;
        undo.affected_id = _mut.target_id;

        capture_position(_mut.target_id,
                          undo.old_parent,
                          undo.old_prev_sib);

        m_arena.detach(_mut.target_id);

        if (m_arena.root() == _mut.target_id)
        {
            m_arena.set_root(null_node);
        }

        push_undo(std::move(undo));

        return DMutationResult::accepted;
    }

    DMutationResult
    apply_move(
        const mutation& _mut
    )
    {
        if (!check_move_allowed(_mut.target_id))
        {
            return DMutationResult::rejected;
        }

        if (!m_arena.valid(_mut.parent_id))
        {
            return DMutationResult::rejected;
        }

        if (!m_arena.data(_mut.parent_id).can_receive_children)
        {
            return DMutationResult::rejected;
        }

        if (is_descendant_of(_mut.parent_id, _mut.target_id))
        {
            return DMutationResult::rejected;
        }

        undo_entry undo;
        undo.kind        = DMutationKind::move;
        undo.affected_id = _mut.target_id;

        capture_position(_mut.target_id,
                          undo.old_parent,
                          undo.old_prev_sib);

        m_arena.detach(_mut.target_id);

        if ( (_mut.position == static_cast<std::size_t>(-1)) ||
             (_mut.position >= m_arena.child_count(
                  _mut.parent_id)) )
        {
            m_arena.append_child(_mut.parent_id,
                                  _mut.target_id);
        }
        else
        {
            insert_at_position(_mut.parent_id,
                                _mut.target_id,
                                _mut.position);
        }

        m_arena.bump_version(_mut.target_id);

        push_undo(std::move(undo));

        return DMutationResult::accepted;
    }

    DMutationResult
    apply_set_property(
        const mutation& _mut
    )
    {
        if (!check_set_property_allowed(_mut.target_id,
                                         _mut.property_key,
                                         _mut.property_val))
        {
            return DMutationResult::rejected;
        }

        ui_payload& p = m_arena.data(_mut.target_id);

        undo_entry undo;
        undo.kind        = DMutationKind::set_property;
        undo.affected_id = _mut.target_id;
        undo.prop_key    = _mut.property_key;

        auto it = p.properties.find(_mut.property_key);

        if (it != p.properties.end())
        {
            undo.old_value    = it->second;
            undo.had_property = true;
        }
        else
        {
            undo.had_property = false;
        }

        p.properties[_mut.property_key] = _mut.property_val;
        m_arena.bump_version(_mut.target_id);

        push_undo(std::move(undo));

        return DMutationResult::accepted;
    }

    DMutationResult
    apply_remove_property(
        const mutation& _mut
    )
    {
        if (!m_arena.valid(_mut.target_id))
        {
            return DMutationResult::rejected;
        }

        ui_payload& p = m_arena.data(_mut.target_id);
        auto it = p.properties.find(_mut.property_key);

        if (it == p.properties.end())
        {
            return DMutationResult::rejected;
        }

        undo_entry undo;
        undo.kind         = DMutationKind::remove_property;
        undo.affected_id  = _mut.target_id;
        undo.prop_key     = _mut.property_key;
        undo.old_value    = it->second;
        undo.had_property = true;

        p.properties.erase(it);
        m_arena.bump_version(_mut.target_id);

        push_undo(std::move(undo));

        return DMutationResult::accepted;
    }

    // -- undo/redo engine --------------------------------------------

    DMutationResult
    apply_inverse(
        const undo_entry& _e
    )
    {
        switch (_e.kind)
        {
            case DMutationKind::insert:
            {
                if (!m_arena.valid(_e.affected_id))
                {
                    return DMutationResult::rejected;
                }

                m_arena.detach(_e.affected_id);

                return DMutationResult::accepted;
            }

            case DMutationKind::remove:
            {
                reattach_at(_e.affected_id,
                             _e.old_parent,
                             _e.old_prev_sib);

                return DMutationResult::accepted;
            }

            case DMutationKind::move:
            {
                m_arena.detach(_e.affected_id);

                reattach_at(_e.affected_id,
                             _e.old_parent,
                             _e.old_prev_sib);

                return DMutationResult::accepted;
            }

            case DMutationKind::set_property:
            {
                if (!m_arena.valid(_e.affected_id))
                {
                    return DMutationResult::rejected;
                }

                ui_payload& p = m_arena.data(_e.affected_id);

                if (_e.had_property)
                {
                    p.properties[_e.prop_key] = _e.old_value;
                }
                else
                {
                    p.properties.erase(_e.prop_key);
                }

                m_arena.bump_version(_e.affected_id);

                return DMutationResult::accepted;
            }

            case DMutationKind::remove_property:
            {
                if (!m_arena.valid(_e.affected_id))
                {
                    return DMutationResult::rejected;
                }

                m_arena.data(_e.affected_id)
                    .properties[_e.prop_key] = _e.old_value;

                m_arena.bump_version(_e.affected_id);

                return DMutationResult::accepted;
            }
        }

        return DMutationResult::rejected;
    }

    undo_entry
    capture_for_redo(
        const undo_entry& _e
    ) const
    {
        undo_entry redo;
        redo.kind        = _e.kind;
        redo.affected_id = _e.affected_id;

        switch (_e.kind)
        {
            case DMutationKind::insert:
            case DMutationKind::move:
            {
                if (m_arena.valid(_e.affected_id))
                {
                    capture_position(_e.affected_id,
                                      redo.old_parent,
                                      redo.old_prev_sib);
                }

                break;
            }

            case DMutationKind::remove:
            {
                break;
            }

            case DMutationKind::set_property:
            case DMutationKind::remove_property:
            {
                redo.prop_key = _e.prop_key;

                if (m_arena.valid(_e.affected_id))
                {
                    const auto& props =
                        m_arena.data(_e.affected_id).properties;

                    auto it = props.find(_e.prop_key);

                    if (it != props.end())
                    {
                        redo.old_value    = it->second;
                        redo.had_property = true;
                    }
                    else
                    {
                        redo.had_property = false;
                    }
                }

                break;
            }
        }

        return redo;
    }

    arena_type        m_arena;
    mutation_signal   m_on_mutation;
    undo_caretaker    m_undo;
    undo_caretaker    m_redo;
    std::size_t       m_undo_seq = 0;
};


// =============================================================================
//  11. FREE FUNCTION API
// =============================================================================

D_INLINE node_id
tree_root(
    const tree& _tree
) noexcept
{
    return _tree.root();
}

D_INLINE bool
tree_valid(
    const tree& _tree,
    node_id     _id
) noexcept
{
    return _tree.valid(_id);
}

D_INLINE DMutationResult
tree_apply(
    tree&           _tree,
    const mutation& _mut
)
{
    return _tree.apply(_mut);
}

D_INLINE DMutationResult
tree_apply_batch(
    tree&                        _tree,
    const std::vector<mutation>& _mutations
)
{
    return _tree.apply_batch(_mutations);
}

D_INLINE const std::string&
node_tag(
    const tree& _tree,
    node_id     _id
)
{
    return _tree.payload(_id).tag;
}

D_INLINE const std::string&
node_label(
    const tree& _tree,
    node_id     _id
)
{
    return _tree.payload(_id).label;
}

D_INLINE node_id
node_parent(
    const tree& _tree,
    node_id     _id
)
{
    return _tree.parent_of(_id);
}

D_INLINE std::size_t
node_child_count(
    const tree& _tree,
    node_id     _id
)
{
    return _tree.child_count_of(_id);
}

D_INLINE node_id
node_child_at(
    const tree& _tree,
    node_id     _parent,
    std::size_t _index
)
{
    return _tree.child_at(_parent, _index);
}

D_INLINE DConstraintKind
node_constraint_kind(
    const tree& _tree,
    node_id     _id
)
{
    return _tree.payload(_id).node_constraint.kind;
}

D_INLINE bool
node_can_receive_children(
    const tree& _tree,
    node_id     _id
)
{
    return _tree.payload(_id).can_receive_children;
}

D_INLINE bool
node_is_descendant_of(
    const tree& _tree,
    node_id     _child_id,
    node_id     _ancestor_id
)
{
    return _tree.is_descendant_of(_child_id, _ancestor_id);
}

D_INLINE std::int64_t
node_property_int(
    const tree&        _tree,
    node_id            _id,
    const std::string& _key
)
{
    const auto& props = _tree.payload(_id).properties;
    auto it = props.find(_key);

    if ( (it != props.end()) &&
         (std::holds_alternative<std::int64_t>(it->second)) )
    {
        return std::get<std::int64_t>(it->second);
    }

    return 0;
}

D_INLINE double
node_property_float(
    const tree&        _tree,
    node_id            _id,
    const std::string& _key
)
{
    const auto& props = _tree.payload(_id).properties;
    auto it = props.find(_key);

    if ( (it != props.end()) &&
         (std::holds_alternative<double>(it->second)) )
    {
        return std::get<double>(it->second);
    }

    return 0.0;
}

D_INLINE std::string
node_property_str(
    const tree&        _tree,
    node_id            _id,
    const std::string& _key
)
{
    const auto& props = _tree.payload(_id).properties;
    auto it = props.find(_key);

    if ( (it != props.end()) &&
         (std::holds_alternative<std::string>(it->second)) )
    {
        return std::get<std::string>(it->second);
    }

    return std::string{};
}

D_INLINE bool
node_property_bool(
    const tree&        _tree,
    node_id            _id,
    const std::string& _key
)
{
    const auto& props = _tree.payload(_id).properties;
    auto it = props.find(_key);

    if ( (it != props.end()) &&
         (std::holds_alternative<bool>(it->second)) )
    {
        return std::get<bool>(it->second);
    }

    return false;
}

D_INLINE mutation
mutation_make_remove(
    node_id _target_id
)
{
    return mutation::make_remove(_target_id);
}

D_INLINE mutation
mutation_make_move(
    node_id     _target_id,
    node_id     _new_parent_id,
    std::size_t _position = static_cast<std::size_t>(-1)
)
{
    return mutation::make_move(_target_id,
                               _new_parent_id,
                               _position);
}

D_INLINE mutation
mutation_make_set_property(
    node_id        _target_id,
    std::string    _key,
    property_value _value
)
{
    return mutation::make_set_property(_target_id,
                                       std::move(_key),
                                       std::move(_value));
}


// =============================================================================
//  12. TRAITS
// =============================================================================

namespace ui_tree_traits
{
NS_INTERNAL

    template<typename _T, typename = void>
    struct has_apply_method : std::false_type
    {
    };

    template<typename _T>
    struct has_apply_method<_T,
        std::void_t<decltype(
            std::declval<_T>().apply(
                std::declval<const mutation&>()))>>
        : std::true_type
    {
    };

    template<typename _T, typename = void>
    struct has_arena_method : std::false_type
    {
    };

    template<typename _T>
    struct has_arena_method<_T,
        std::void_t<decltype(
            std::declval<const _T>().arena())>>
        : std::true_type
    {
    };

    template<typename _T, typename = void>
    struct has_on_mutation_method : std::false_type
    {
    };

    template<typename _T>
    struct has_on_mutation_method<_T,
        std::void_t<decltype(
            std::declval<_T>().on_mutation())>>
        : std::true_type
    {
    };

NS_END  // internal

// is_ui_tree
//   trait: true if a type has apply(), arena(), and on_mutation().
template<typename _T>
struct is_ui_tree : std::conjunction<
    internal::has_apply_method<_T>,
    internal::has_arena_method<_T>,
    internal::has_on_mutation_method<_T>
>
{
};

template<typename _T>
D_INLINE constexpr bool is_ui_tree_v = is_ui_tree<_T>::value;

}   // namespace ui_tree_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_UI_TREE_
