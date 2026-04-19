/******************************************************************************
* uxoxo [templates]                                         history_traits.hpp
*
* History container traits:
*   Compile-time detection and policy traits for the history<>
* template.  These traits inspect an arbitrary container type
* for the operations history<> requires — push_back, size,
* front-removal — and select the optimal eviction strategy
* based on the container's interface.
*
*   All detection traits use the SFINAE/void_t idiom and are
* compatible with C++11 and later.  The eviction strategy is
* expressed as a strategy enum for use in `if constexpr`
* chains, following the framework dispatch convention.
*
* Contents:
*   Detection traits (namespace internal)
*     - has_push_back           container supports push_back(T)
*     - has_pop_front           container supports pop_front()
*     - has_erase_iterator      container supports erase(iterator)
*     - has_size                container supports size()
*     - has_front               container supports front()
*     - has_back                container supports back()
*     - has_begin_end           container supports begin()/end()
*     - has_clear               container supports clear()
*     - has_reserve             container supports reserve(n)
*
*   Strategy enum
*     - history_eviction_strategy   dispatch path for eviction
*
*   Policy traits
*     - history_can_pop_front       whether eviction uses pop_front
*     - history_eviction            strategy selector
*
*   Aggregate traits
*     - history_has_front_removal   container can remove front
*     - is_history_compatible       container satisfies all
*                                   history requirements
*
*   Convenience aliases (_v suffixed)
*
*
* path:      /inc/uxoxo/templates/util/history/history_traits.hpp
* link(s):   TBA
* author(s): Sam 'teer' Neal-Blim                          created: 2026.04.09
******************************************************************************/

#ifndef UXOXO_TEMPLATES_HISTORY_TRAITS_
#define UXOXO_TEMPLATES_HISTORY_TRAITS_ 1

#include <cstddef>
#include <type_traits>
#include "../../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT


// ================================================================
//  detection traits (internal)
// ================================================================

NS_INTERNAL

    // has_push_back
    //   trait: detects whether _Container supports push_back(const
    // value_type&).  Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_push_back : std::false_type
    {};

    // has_push_back (success case)
    //   trait: partial specialization when push_back is
    // well-formed.
    template<typename _Container>
    struct has_push_back<
        _Container,
        std::void_t<
            decltype(std::declval<_Container&>().push_back(
                std::declval<const typename _Container::value_type&>()))
        >
    > : std::true_type
    {};

    // has_pop_front
    //   trait: detects whether _Container supports pop_front().
    // Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_pop_front : std::false_type
    {};

    // has_pop_front (success case)
    //   trait: partial specialization when pop_front is
    // well-formed.
    template<typename _Container>
    struct has_pop_front<
        _Container,
        std::void_t<
            decltype(std::declval<_Container&>().pop_front())
        >
    > : std::true_type
    {};

    // has_erase_iterator
    //   trait: detects whether _Container supports
    // erase(iterator).  Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_erase_iterator : std::false_type
    {};

    // has_erase_iterator (success case)
    //   trait: partial specialization when erase(begin()) is
    // well-formed.
    template<typename _Container>
    struct has_erase_iterator<
        _Container,
        std::void_t<
            decltype(std::declval<_Container&>().erase(
                std::declval<_Container&>().begin()))
        >
    > : std::true_type
    {};

    // has_size
    //   trait: detects whether _Container supports size().
    // Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_size : std::false_type
    {};

    // has_size (success case)
    //   trait: partial specialization when size() is well-formed.
    template<typename _Container>
    struct has_size<
        _Container,
        std::void_t<
            decltype(std::declval<const _Container&>().size())
        >
    > : std::true_type
    {};

    // has_front
    //   trait: detects whether _Container supports front().
    // Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_front : std::false_type
    {};

    // has_front (success case)
    //   trait: partial specialization when front() is
    // well-formed.
    template<typename _Container>
    struct has_front<
        _Container,
        std::void_t<
            decltype(std::declval<const _Container&>().front())
        >
    > : std::true_type
    {};

    // has_back
    //   trait: detects whether _Container supports back().
    // Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_back : std::false_type
    {};

    // has_back (success case)
    //   trait: partial specialization when back() is well-formed.
    template<typename _Container>
    struct has_back<
        _Container,
        std::void_t<
            decltype(std::declval<const _Container&>().back())
        >
    > : std::true_type
    {};

    // has_begin_end
    //   trait: detects whether _Container supports begin() and
    // end().  Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_begin_end : std::false_type
    {};

    // has_begin_end (success case)
    //   trait: partial specialization when begin() and end() are
    // well-formed.
    template<typename _Container>
    struct has_begin_end<
        _Container,
        std::void_t<
            decltype(std::declval<const _Container&>().begin()),
            decltype(std::declval<const _Container&>().end())
        >
    > : std::true_type
    {};

    // has_clear
    //   trait: detects whether _Container supports clear().
    // Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_clear : std::false_type
    {};

    // has_clear (success case)
    //   trait: partial specialization when clear() is
    // well-formed.
    template<typename _Container>
    struct has_clear<
        _Container,
        std::void_t<
            decltype(std::declval<_Container&>().clear())
        >
    > : std::true_type
    {};

    // has_reserve
    //   trait: detects whether _Container supports reserve(n).
    // Primary template (failure case).
    template<typename _Container,
             typename = void>
    struct has_reserve : std::false_type
    {};

    // has_reserve (success case)
    //   trait: partial specialization when reserve(n) is
    // well-formed.
    template<typename _Container>
    struct has_reserve<
        _Container,
        std::void_t<
            decltype(std::declval<_Container&>().reserve(
                std::declval<std::size_t>()))
        >
    > : std::true_type
    {};

NS_END  // internal


// ================================================================
//  strategy enum
// ================================================================

// history_eviction_strategy
//   enum: compile-time dispatch path for evicting the oldest
// element from the backing container.  Used in `if constexpr`
// chains within history<>.
//
//   pop_front   — O(1); preferred for deque, list
//   erase_front — O(n); fallback for vector, sequence
//   unsupported — container cannot evict the front element
enum class history_eviction_strategy
{
    pop_front,
    erase_front,
    unsupported
};


// ================================================================
//  policy traits
// ================================================================

// history_can_pop_front
//   trait: determines whether eviction should use pop_front()
// (true) or erase(begin()) (false) for a given container.
// Containers with pop_front() are preferred because the
// operation is typically O(1) for deque/list versus O(n) for
// vector-like containers.
template<typename _Container>
struct history_can_pop_front
{
    static constexpr bool value =
        internal::has_pop_front<_Container>::value;
};

// history_eviction
//   trait: resolves the eviction strategy for a given container.
// Priority: pop_front -> erase(begin()) -> unsupported.
template<typename _Container>
struct history_eviction
{
    static constexpr history_eviction_strategy value =
        internal::has_pop_front<_Container>::value
            ? history_eviction_strategy::pop_front
        : internal::has_erase_iterator<_Container>::value
            ? history_eviction_strategy::erase_front
        : history_eviction_strategy::unsupported;
};

template<typename _Container>
inline constexpr history_eviction_strategy history_eviction_v =
    history_eviction<_Container>::value;


// ================================================================
//  aggregate traits
// ================================================================

// history_has_front_removal
//   trait: determines whether the container supports at least
// one method of removing the front element (pop_front or
// erase(iterator)).
template<typename _Container>
struct history_has_front_removal
{
    static constexpr bool value =
        ( internal::has_pop_front<_Container>::value      ||
          internal::has_erase_iterator<_Container>::value );
};

// is_history_compatible
//   trait: aggregate check that a container type satisfies all
// requirements for use as the backing store of history<>.
// Requires: push_back, size, begin/end, front removal
// (pop_front or erase(iterator)), and clear.
template<typename _Container>
struct is_history_compatible
{
    static constexpr bool value =
        ( internal::has_push_back<_Container>::value       &&
          internal::has_size<_Container>::value            &&
          internal::has_begin_end<_Container>::value       &&
          internal::has_clear<_Container>::value           &&
          history_has_front_removal<_Container>::value );
};


// ================================================================
//  convenience value aliases
// ================================================================

#if D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES

    // history_can_pop_front_v
    //   value: convenience variable template for
    // history_can_pop_front<_Container>::value.
    template<typename _Container>
    constexpr bool history_can_pop_front_v =
        history_can_pop_front<_Container>::value;

    // history_has_front_removal_v
    //   value: convenience variable template for
    // history_has_front_removal<_Container>::value.
    template<typename _Container>
    constexpr bool history_has_front_removal_v =
        history_has_front_removal<_Container>::value;

    // is_history_compatible_v
    //   value: convenience variable template for
    // is_history_compatible<_Container>::value.
    template<typename _Container>
    constexpr bool is_history_compatible_v =
        is_history_compatible<_Container>::value;

#endif  // D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_TEMPLATES_HISTORY_TRAITS_