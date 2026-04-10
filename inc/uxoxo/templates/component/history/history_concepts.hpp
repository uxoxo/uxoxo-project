/******************************************************************************
* uxoxo [templates]                                      history_concepts.hpp
*
* History container concepts:
*   C++20 concepts layered over history_traits.hpp.  These concepts provide
* readable constraints for history-compatible backing containers without
* replacing the existing SFINAE trait surface.
*
*   The concepts mirror the detection and policy axes from history_traits.hpp:
*   - primitive backing-container capabilities
*   - eviction strategy selection
*   - aggregate history compatibility
*
*
* path:      /inc/uxoxo/templates/util/history/history_concepts.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.09
******************************************************************************/

#ifndef UXOXO_TEMPLATES_HISTORY_CONCEPTS_
#define UXOXO_TEMPLATES_HISTORY_CONCEPTS_ 1

#include "../../../uxoxo.hpp"
#include "./history_traits.hpp"


NS_UXOXO
NS_TEMPLATES

#if D_ENV_CPP_FEATURE_LANG_CONCEPTS

// ================================================================
//  primitive backing-container capability concepts
// ================================================================

// history_push_backable_container
//   concept: the container supports push_back(const value_type&).
template<typename _Container>
concept history_push_backable_container =
    internal::has_push_back<_Container>::value;

// history_sized_container
//   concept: the container supports size().
template<typename _Container>
concept history_sized_container =
    internal::has_size<_Container>::value;

// history_front_readable_container
//   concept: the container supports front().
template<typename _Container>
concept history_front_readable_container =
    internal::has_front<_Container>::value;

// history_back_readable_container
//   concept: the container supports back().
template<typename _Container>
concept history_back_readable_container =
    internal::has_back<_Container>::value;

// history_iterable_container
//   concept: the container supports begin() and end().
template<typename _Container>
concept history_iterable_container =
    internal::has_begin_end<_Container>::value;

// history_clearable_container
//   concept: the container supports clear().
template<typename _Container>
concept history_clearable_container =
    internal::has_clear<_Container>::value;

// history_reservable_container
//   concept: the container supports reserve(size_t).
template<typename _Container>
concept history_reservable_container =
    internal::has_reserve<_Container>::value;

// history_pop_frontable_container
//   concept: the container supports pop_front().
template<typename _Container>
concept history_pop_frontable_container =
    internal::has_pop_front<_Container>::value;

// history_erase_frontable_container
//   concept: the container supports erase(begin()).
template<typename _Container>
concept history_erase_frontable_container =
    internal::has_erase_iterator<_Container>::value;


// ================================================================
//  eviction strategy concepts
// ================================================================

// history_front_evictable_container
//   concept: the container supports at least one valid
// front-eviction path.
template<typename _Container>
concept history_front_evictable_container =
    history_has_front_removal<_Container>::value;

// pop_front_evictable_history_container
//   concept: the container's eviction strategy resolves to
// pop_front (O(1) path).
template<typename _Container>
concept pop_front_evictable_history_container =
    (history_eviction<_Container>::value ==
     history_eviction_strategy::pop_front);

// erase_front_evictable_history_container
//   concept: the container's eviction strategy resolves to
// erase(begin()) (O(n) fallback path).
template<typename _Container>
concept erase_front_evictable_history_container =
    (history_eviction<_Container>::value ==
     history_eviction_strategy::erase_front);


// ================================================================
//  aggregate compatibility concepts
// ================================================================

// history_compatible_container
//   concept: the container satisfies all structural requirements
// for use as the backing store of history<>.
template<typename _Container>
concept history_compatible_container =
    is_history_compatible<_Container>::value;

// reservable_history_container
//   concept: a history-compatible container that also supports
// reserve().
template<typename _Container>
concept reservable_history_container =
    history_compatible_container<_Container> &&
    internal::has_reserve<_Container>::value;

// readable_history_container
//   concept: a history-compatible container that exposes both
// front() and back() accessors.
template<typename _Container>
concept readable_history_container =
    history_compatible_container<_Container> &&
    internal::has_front<_Container>::value   &&
    internal::has_back<_Container>::value;

#endif  // D_ENV_CPP_FEATURE_LANG_CONCEPTS


NS_END  // templates
NS_END  // uxoxo


#endif  // UXOXO_TEMPLATES_HISTORY_CONCEPTS_
