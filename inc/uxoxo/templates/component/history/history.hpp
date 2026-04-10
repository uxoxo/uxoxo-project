/******************************************************************************
* uxoxo [templates]                                                history.hpp
*
* Bounded history container:
*   A history is an ordered, bounded collection of elements that
* evicts the oldest entry when the configured maximum capacity
* is reached.  It is the natural container for recording ordered
* events with finite memory: command histories, navigation
* stacks, address trails, undo buffers, and so on.
*
*   The template is fully container-agnostic.  The backing store
* is a template parameter (_Container) that must be a sequential
* container satisfying is_history_compatible.  The default is
* sequence<_Type>.  Any conforming sequential container may be
* substituted — including std::deque, std::list, std::vector,
* or user-defined types.
*
*   Eviction strategy is selected at compile time via a strategy
* enum and `if constexpr`: containers with pop_front() (deque,
* list) use O(1) removal; all others fall back to erase(begin()).
* No tag types are involved.
*
*   The maximum capacity is a template non-type parameter with a
* default of the largest representable value of _SizeType,
* making an effectively unbounded history the zero-configuration
* default.
*
* Contents:
*   - history              bounded, ordered collection of T
*   - make_history         factory functions
*
* Usage:
*   // default: sequence-backed, effectively unbounded
*   history<std::string> cmd_history;
*   cmd_history.record("ls -la");
*   cmd_history.record("cd /home");
*
*   // bounded to 100 entries
*   history<url, sequence<url>, std::size_t, 100> nav;
*   nav.record(url("https://example.com"));
*
*   // deque-backed for O(1) eviction
*   history<int, std::deque<int>, std::size_t, 50> recent;
*   recent.record(42);
*
*
* path:      /inc/uxoxo/templates/util/history/history.hpp
* link(s):   TBA
* author(s): Sam 'teer' Neal-Blim                             date: 2026.04.09
******************************************************************************/

#ifndef UXOXO_TEMPLATES_HISTORY_
#define UXOXO_TEMPLATES_HISTORY_ 1

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <djinterp/util/sequence.hpp>
#include "../../../uxoxo.hpp"
#include "./history_traits.hpp"


NS_UXOXO
NS_TEMPLATES


// ================================================================
//  history
// ================================================================

// history
//   class: a bounded, ordered collection that evicts the oldest
// element when the maximum capacity _MaxSize is reached.
// Elements are recorded in insertion order; the front of the
// container is the oldest entry, and the back is the newest.
//
//   _Type must be copy-constructible.  _Container must be a
// sequential container satisfying is_history_compatible
// (push_back, size, begin/end, clear, and at least one of
// pop_front or erase(iterator)).
//
//   The eviction strategy is resolved at compile time via
// history_eviction_strategy and `if constexpr`: containers
// offering pop_front() use it (O(1) for deque/list), while
// all others fall back to erase(begin()).
template<typename  _Type,
         typename  _Container = sequence<_Type>,
         typename  _SizeType  = std::size_t,
         _SizeType _MaxSize   = std::numeric_limits<_SizeType>::max()>
class history
{
    static_assert(is_history_compatible<_Container>::value,
                  "Template parameter `_Container` must satisfy "
                  "is_history_compatible: it must provide push_back, "
                  "size, begin/end, clear, and front removal "
                  "(pop_front or erase(iterator)).");

    static_assert(_MaxSize > 0,
                  "Non-type parameter `_MaxSize` must be greater "
                  "than zero.");

    // eviction strategy, resolved once at class scope.
    static constexpr history_eviction_strategy m_strategy =
        history_eviction<_Container>::value;

public:
    using value_type      = _Type;
    using container_type  = _Container;
    using size_type       = _SizeType;
    using reference       = _Type&;
    using const_reference = const _Type&;
    using iterator        = typename _Container::iterator;
    using const_iterator  = typename _Container::const_iterator;

    // --------------------------------------------------------
    //  construction
    // --------------------------------------------------------

    // history (default)
    //   constructs an empty history with the compile-time
    // maximum capacity.
    history()
        {}

    // history (runtime capacity)
    //   constructs an empty history with a runtime-specified
    // maximum capacity.  The effective capacity is the lesser
    // of _max_size and the compile-time _MaxSize.
    explicit history(
            size_type _max_size
        )
            : m_runtime_max(_max_size < _MaxSize
                            ? _max_size
                            : _MaxSize)
        {}

    // history (copy)
    //   constructs a history by copying another history.
    history(
            const history& _other
        )
            : m_data(_other.m_data),
              m_runtime_max(_other.m_runtime_max)
        {}

    // history (move)
    //   constructs a history by moving another history.
    history(
            history&& _other
        )
            : m_data(static_cast<_Container&&>(_other.m_data)),
              m_runtime_max(_other.m_runtime_max)
        {}

    // --------------------------------------------------------
    //  assignment
    // --------------------------------------------------------

    // operator= (copy)
    //   copy-assigns from another history.
    history&
    operator=
    (
        const history& _other
    )
    {
        if (this != &_other)
        {
            m_data        = _other.m_data;
            m_runtime_max = _other.m_runtime_max;
        }

        return *this;
    }

    // operator= (move)
    //   move-assigns from another history.
    history&
    operator=
    (
        history&& _other
    )
    {
        if (this != &_other)
        {
            m_data        = static_cast<_Container&&>(_other.m_data);
            m_runtime_max = _other.m_runtime_max;
        }

        return *this;
    }

    // --------------------------------------------------------
    //  recording
    // --------------------------------------------------------

    // record
    //   appends an element to the history.  If the history is
    // at capacity, the oldest element (front) is evicted before
    // the new element is appended.
    void
    record
    (
        const _Type& _value
    )
    {
        // evict oldest if at capacity.
        if (m_data.size() >= effective_max())
        {
            evict_oldest();
        }

        m_data.push_back(_value);

        return;
    }

    // record (move)
    //   appends an element by move to the history.  Evicts the
    // oldest entry if at capacity.
    void
    record
    (
        _Type&& _value
    )
    {
        // evict oldest if at capacity.
        if (m_data.size() >= effective_max())
        {
            evict_oldest();
        }

        m_data.push_back(static_cast<_Type&&>(_value));

        return;
    }

    // --------------------------------------------------------
    //  element access
    // --------------------------------------------------------

    // operator[]
    //   returns a reference to the element at _index.
    reference
    operator[]
    (
        size_type _index
    )
    {
        return m_data[_index];
    }

    // operator[] (const)
    //   returns a const reference to the element at _index.
    const_reference
    operator[]
    (
        size_type _index
    ) const
    {
        return m_data[_index];
    }

    // oldest
    //   returns a reference to the oldest (front) element.
    reference
    oldest()
    {
        return m_data.front();
    }

    // oldest (const)
    //   returns a const reference to the oldest element.
    const_reference
    oldest() const
    {
        return m_data.front();
    }

    // newest
    //   returns a reference to the newest (back) element.
    reference
    newest()
    {
        return m_data.back();
    }

    // newest (const)
    //   returns a const reference to the newest element.
    const_reference
    newest() const
    {
        return m_data.back();
    }

    // --------------------------------------------------------
    //  capacity
    // --------------------------------------------------------

    // size
    //   returns the current number of recorded elements.
    size_type
    size() const
    {
        return static_cast<size_type>(m_data.size());
    }

    // empty
    //   returns true if the history contains no elements.
    bool
    empty() const
    {
        return m_data.size() == 0;
    }

    // full
    //   returns true if the history has reached its effective
    // maximum capacity.
    bool
    full() const
    {
        return m_data.size() >= effective_max();
    }

    // max_size
    //   returns the compile-time maximum capacity.
    static constexpr size_type
    max_size()
    {
        return _MaxSize;
    }

    // effective_max
    //   returns the effective maximum capacity, which is the
    // lesser of the compile-time _MaxSize and any runtime
    // override.
    size_type
    effective_max() const
    {
        return m_runtime_max;
    }

    // --------------------------------------------------------
    //  iteration
    // --------------------------------------------------------

    // begin
    //   returns an iterator to the oldest element.
    iterator
    begin()
    {
        return m_data.begin();
    }

    // begin (const)
    //   returns a const iterator to the oldest element.
    const_iterator
    begin() const
    {
        return m_data.begin();
    }

    // end
    //   returns an iterator past the newest element.
    iterator
    end()
    {
        return m_data.end();
    }

    // end (const)
    //   returns a const iterator past the newest element.
    const_iterator
    end() const
    {
        return m_data.end();
    }

    // --------------------------------------------------------
    //  modification
    // --------------------------------------------------------

    // clear
    //   removes all recorded elements.
    void
    clear()
    {
        m_data.clear();

        return;
    }

    // set_max
    //   updates the runtime maximum capacity.  If the current
    // size exceeds the new maximum, the oldest elements are
    // evicted until the size is within bounds.
    void
    set_max
    (
        size_type _new_max
    )
    {
        // clamp to compile-time maximum.
        m_runtime_max = (_new_max < _MaxSize)
                        ? _new_max
                        : _MaxSize;

        // evict excess entries.
        while (m_data.size() > m_runtime_max)
        {
            evict_oldest();
        }

        return;
    }

    // --------------------------------------------------------
    //  comparison
    // --------------------------------------------------------

    // operator==
    //   returns true if both histories have the same effective
    // maximum and identical recorded elements.
    friend bool
    operator==
    (
        const history& _a,
        const history& _b
    )
    {
        if ( (_a.m_runtime_max != _b.m_runtime_max) ||
             (_a.m_data.size() != _b.m_data.size()) )
        {
            return false;
        }

        // element-wise comparison via iterators for
        // container-agnostic traversal.
        auto it_a = _a.m_data.begin();
        auto it_b = _b.m_data.begin();

        while (it_a != _a.m_data.end())
        {
            if (!(*it_a == *it_b))
            {
                return false;
            }

            ++it_a;
            ++it_b;
        }

        return true;
    }

    // operator!=
    //   returns true if the histories differ.
    friend bool
    operator!=
    (
        const history& _a,
        const history& _b
    )
    {
        return !(_a == _b);
    }

    // --------------------------------------------------------
    //  conversion
    // --------------------------------------------------------

    // container
    //   returns a const reference to the underlying container.
    const _Container&
    container() const
    {
        return m_data;
    }

private:

    // evict_oldest
    //   removes the oldest element from the backing container.
    // The dispatch path is selected at compile time via the
    // history_eviction_strategy enum and `if constexpr`.
    void
    evict_oldest()
    {
        if constexpr (m_strategy ==
                      history_eviction_strategy::pop_front)
        {
            m_data.pop_front();
        }
        else if constexpr (m_strategy ==
                           history_eviction_strategy::erase_front)
        {
            m_data.erase(m_data.begin());
        }

        return;
    }

    _Container m_data;
    size_type  m_runtime_max = _MaxSize;
};


// ================================================================
//  make_history
// ================================================================

// make_history (compile-time capacity)
//   factory: creates a history with a compile-time maximum
// capacity, using the default container (sequence<_Type>).
template<typename    _Type,
         std::size_t _MaxSize>
history<_Type, sequence<_Type>, std::size_t, _MaxSize>
make_history()
{
    return history<_Type, sequence<_Type>, std::size_t, _MaxSize>();
}

// make_history (runtime capacity)
//   factory: creates a history with a runtime maximum capacity,
// using the default container (sequence<_Type>).
template<typename _Type>
history<_Type>
make_history
(
    std::size_t _max_size
)
{
    return history<_Type>(_max_size);
}

// make_history (custom container, compile-time capacity)
//   factory: creates a history with a specified container type
// and compile-time maximum capacity.
template<typename    _Type,
         typename    _Container,
         std::size_t _MaxSize>
history<_Type, _Container, std::size_t, _MaxSize>
make_history()
{
    return history<_Type, _Container, std::size_t, _MaxSize>();
}


NS_END  // templates
NS_END  // uxoxo


#endif  // UXOXO_TEMPLATES_HISTORY_
