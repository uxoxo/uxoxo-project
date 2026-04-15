/******************************************************************************
* uxoxo [component]                                        suggest_adapter.hpp
*
* autosuggest/autocomplete adapter interface:
*   Provides an abstract, zero-overhead foundation for adapting arbitrary
* data structures into autosuggest or autocomplete sources. Built on the
* CRTP interface_adapter pattern, this module defines the greatest common
* subset of functionality shared by all suggestion providers — the pure
* bridge between a backing data source and its suggestion output.
*
*   The adapter is fully agnostic to:
*     - Input type     (_Input)     : query strings, prefixes, tokens, etc.
*     - Suggestion type (_Suggest)  : strings, rich objects, scored pairs, etc.
*     - Container type (_Container) : vectors, lists, sets, custom types, etc.
*
*   Derived classes implement the concrete lookup strategy (trie traversal,
* fuzzy matching, substring search, etc.) via a small set of well-defined
* CRTP hooks. The base provides the common public interface, type machinery,
* and compile-time conformance detection.
*
*   CRTP HOOKS (required in _Derived):
*     _Container do_suggest(const _Input&) const
*       — return all matching suggestions for the given input.
*
*   CRTP HOOKS (optional in _Derived):
*     bool       do_has_suggestions(const _Input&) const
*       — return true if at least one suggestion exists.
*         Default: !do_suggest(input).empty()
*     size_type  do_count(const _Input&) const
*       — return the number of matching suggestions.
*         Default: do_suggest(input).size()
*     void       do_clear()
*       — reset/invalidate any cached state.
*         Default: no-op.
*
*   DESIGN:
*   The module is organized in four layers:
*     1. CONFIGURATION — feature gate macros for progressive enhancement.
*     2. TRAITS — SFINAE-based detection of conforming suggest adapters
*        and their capabilities (has do_has_suggestions, do_count, etc.).
*     3. CORE — the suggest_adapter CRTP base class itself.
*     4. CONCEPTS — C++20 concept constraints (when available).
*
*   PORTABILITY:
*   - C++11  : full functionality via CRTP + trailing return types
*   - C++14  : auto return type deduction in defaults
*   - C++17  : if constexpr dispatch, CTAD
*   - C++20  : concept-constrained suggest_adapter, suggest_source concept
*
*
* path:      /inc/uxoxo/templates/adapter/suggest_adapter.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.09
******************************************************************************/

/*
TABLE OF CONTENTS
=================
I.    CONFIGURATION & FEATURE GATES
      --------------------------------
      i.    D_SUGGEST_HAS_IF_CONSTEXPR
      ii.   D_SUGGEST_HAS_CONCEPTS

II.   TRAITS
      --------
      i.    has_do_suggest (internal)
      ii.   has_do_has_suggestions (internal)
      iii.  has_do_count (internal)
      iv.   has_do_clear (internal)
      v.    has_empty_method (internal)
      vi.   has_size_method (internal)
      vii.  is_suggest_adapter

III.  SUGGEST ADAPTER — CRTP (C++11+)
      ----------------------------------
      i.    suggest_adapter

IV.   CONCEPTS (C++20+)
      --------------------
      i.    suggest_source
      ii.   scored_suggest_source
*/

#ifndef UXOXO_ADAPTER_SUGGEST_ADAPTER_
#define UXOXO_ADAPTER_SUGGEST_ADAPTER_ 1

// std
#include <cstddef>
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../uxoxo.hpp"
#include "../../meta/type_traits.hpp"

#if D_ENV_LANG_IS_CPP11_OR_HIGHER
    #include <utility>
#endif


///////////////////////////////////////////////////////////////////////////////
///           I.    CONFIGURATION & FEATURE GATES                           ///
///////////////////////////////////////////////////////////////////////////////

// D_SUGGEST_HAS_IF_CONSTEXPR
//   macro: 1 if if-constexpr dispatch is available (C++17+).
#if D_ENV_LANG_IS_CPP17_OR_HIGHER
    #define D_SUGGEST_HAS_IF_CONSTEXPR 1
#else
    #define D_SUGGEST_HAS_IF_CONSTEXPR 0
#endif

// D_SUGGEST_HAS_CONCEPTS
//   macro: 1 if concepts are available (C++20+).
#if D_ENV_LANG_IS_CPP20_OR_HIGHER
    #define D_SUGGEST_HAS_CONCEPTS 1
#else
    #define D_SUGGEST_HAS_CONCEPTS 0
#endif


NS_UXOXO
NS_ADAPTER

#if D_ENV_LANG_IS_CPP11_OR_HIGHER


///////////////////////////////////////////////////////////////////////////////
///           II.   TRAITS                                                  ///
///////////////////////////////////////////////////////////////////////////////

NS_INTERNAL

    // =====================================================================
    // CRTP hook detection
    // =====================================================================
    // These traits detect whether a derived type provides optional CRTP
    // hooks. Used by suggest_adapter to select between the derived
    // implementation and the default fallback.

    // has_do_suggest
    //   trait: detects _Type::do_suggest(const _Input&) const.
    template<typename _Type,
             typename _Input,
             typename = void>
    struct has_do_suggest : std::false_type
    {};

    template<typename _Type,
             typename _Input>
    struct has_do_suggest<_Type, _Input, void_t<
        decltype(std::declval<const _Type>().do_suggest(
            std::declval<const _Input&>()))
    >> : std::true_type
    {};

    // has_do_has_suggestions
    //   trait: detects _Type::do_has_suggestions(const _Input&) const.
    template<typename _Type,
             typename _Input,
             typename = void>
    struct has_do_has_suggestions : std::false_type
    {};

    template<typename _Type,
             typename _Input>
    struct has_do_has_suggestions<_Type, _Input, void_t<
        decltype(std::declval<const _Type>().do_has_suggestions(
            std::declval<const _Input&>()))
    >> : std::true_type
    {};

    // has_do_count
    //   trait: detects _Type::do_count(const _Input&) const.
    template<typename _Type,
             typename _Input,
             typename = void>
    struct has_do_count : std::false_type
    {};

    template<typename _Type,
             typename _Input>
    struct has_do_count<_Type, _Input, void_t<
        decltype(std::declval<const _Type>().do_count(
            std::declval<const _Input&>()))
    >> : std::true_type
    {};

    // has_do_clear
    //   trait: detects _Type::do_clear().
    template<typename _Type,
             typename = void>
    struct has_do_clear : std::false_type
    {};

    template<typename _Type>
    struct has_do_clear<_Type, void_t<
        decltype(std::declval<_Type>().do_clear())
    >> : std::true_type
    {};

    // =====================================================================
    // Container capability detection
    // =====================================================================

    // has_empty_method
    //   trait: detects _Type::empty() const.
    template<typename _Type,
             typename = void>
    struct has_empty_method : std::false_type
    {};

    template<typename _Type>
    struct has_empty_method<_Type, void_t<
        decltype(std::declval<const _Type>().empty())
    >> : std::true_type
    {};

    // has_size_method
    //   trait: detects _Type::size() const.
    template<typename _Type,
             typename = void>
    struct has_size_method : std::false_type
    {};

    template<typename _Type>
    struct has_size_method<_Type, void_t<
        decltype(std::declval<const _Type>().size())
    >> : std::true_type
    {};

    // =====================================================================
    // Default dispatch helpers
    // =====================================================================
    // These provide the fallback implementations for optional CRTP hooks.
    // Each overload pair is selected via SFINAE on whether the derived
    // class provides its own implementation or not.

    // default_has_suggestions — derived provides do_has_suggestions
    template<typename _Derived,
             typename _Input>
    auto default_has_suggestions(
        const _Derived& _d,
        const _Input&   _input,
        std::true_type
    )
        -> bool
    {
        return _d.do_has_suggestions(_input);
    }

    // default_has_suggestions — fallback: negate container empty()
    template<typename _Derived,
             typename _Input>
    auto default_has_suggestions(
        const _Derived& _d,
        const _Input&   _input,
        std::false_type
    )
        -> bool
    {
        return !(static_cast<const _Derived&>(_d)
            .do_suggest(_input).empty());
    }

    // default_count — derived provides do_count
    template<typename _Derived,
             typename _Input>
    auto default_count(
        const _Derived& _d,
        const _Input&   _input,
        std::true_type
    )
        -> std::size_t
    {
        return static_cast<std::size_t>(_d.do_count(_input));
    }

    // default_count — fallback: container size()
    template<typename _Derived,
             typename _Input>
    auto default_count(
        const _Derived& _d,
        const _Input&   _input,
        std::false_type
    )
        -> std::size_t
    {
        return static_cast<std::size_t>(
            static_cast<const _Derived&>(_d)
                .do_suggest(_input).size());
    }

    // default_clear — derived provides do_clear
    template<typename _Derived>
    void default_clear(
        _Derived&      _d,
        std::true_type
    )
    {
        _d.do_clear();

        return;
    }

    // default_clear — fallback: no-op
    template<typename _Derived>
    void default_clear(
        _Derived&,
        std::false_type
    )
    {
        return;
    }

NS_END  // internal


// is_suggest_adapter
//   trait: detects whether _Type conforms to the suggest_adapter interface.
// A conforming type must expose input_type, suggest_type, container_type,
// and a const suggest() method accepting input_type.
template<typename _Type,
         typename = void>
struct is_suggest_adapter : std::false_type
{};

template<typename _Type>
struct is_suggest_adapter<_Type, void_t<
    typename _Type::input_type,
    typename _Type::suggest_type,
    typename _Type::container_type,
    decltype(std::declval<const _Type>().suggest(
        std::declval<const typename _Type::input_type&>()))
>> : std::true_type
{};

#if D_ENV_CPP_FEATURE_LANG_VARIABLE_TEMPLATES
    // is_suggest_adapter_v
    //   value: convenience alias for is_suggest_adapter<_Type>::value.
    template<typename _Type>
    constexpr bool is_suggest_adapter_v =
        is_suggest_adapter<_Type>::value;
#endif


///////////////////////////////////////////////////////////////////////////////
///        III.  SUGGEST ADAPTER — CRTP (C++11+)                           ///
///////////////////////////////////////////////////////////////////////////////

// suggest_adapter
//   class: abstract CRTP base providing a zero-overhead interface for
// autosuggest / autocomplete adapters. Parameterized on the derived
// implementation, input type, suggestion element type, and result
// container type.
//
//   _Derived must implement at minimum:
//     _Container do_suggest(const _Input&) const;
//
//   Optional overrides in _Derived:
//     bool       do_has_suggestions(const _Input&) const;
//     size_type  do_count(const _Input&) const;
//     void       do_clear();
//
// Usage:
//   class prefix_suggest
//       : public suggest_adapter<prefix_suggest,
//                                std::string,
//                                std::string,
//                                std::vector<std::string>>
//   {
//   public:
//       std::vector<std::string>
//       do_suggest(const std::string& _prefix) const
//       {
//           // ... trie traversal, etc.
//       }
//   };
//
//   prefix_suggest ps;
//   auto results = ps.suggest("hel");    // -> {"hello", "help", ...}
//   bool any     = ps.has_suggestions("hel");
//   auto n       = ps.count("hel");
template<typename _Derived,
         typename _Input,
         typename _Suggest,
         typename _Container>
class suggest_adapter
{
public:
    using input_type     = _Input;
    using suggest_type   = _Suggest;
    using container_type = _Container;
    using size_type      = std::size_t;

    // suggest
    //   returns all matching suggestions for the given input.
    // Delegates to _Derived::do_suggest().
    D_CONSTEXPR_INLINE container_type
    suggest(
        const input_type& _input
    ) const
    {
        return derived_ref().do_suggest(_input);
    }

    // has_suggestions
    //   returns true if at least one suggestion exists for the
    // given input. Delegates to _Derived::do_has_suggestions()
    // if provided; otherwise falls back to checking whether the
    // result of do_suggest() is empty.
    D_CONSTEXPR_INLINE bool
    has_suggestions(
        const input_type& _input
    ) const
    {
        return internal::default_has_suggestions(
            derived_ref(),
            _input,
            typename internal::has_do_has_suggestions<
                _Derived, _Input>());
    }

    // count
    //   returns the number of matching suggestions for the given
    // input. Delegates to _Derived::do_count() if provided;
    // otherwise falls back to the size of do_suggest().
    D_CONSTEXPR_INLINE size_type
    count(
        const input_type& _input
    ) const
    {
        return internal::default_count(
            derived_ref(),
            _input,
            typename internal::has_do_count<_Derived, _Input>());
    }

    // empty
    //   returns true if the given input produces no suggestions.
    // Logical complement of has_suggestions().
    D_CONSTEXPR_INLINE bool
    empty(
        const input_type& _input
    ) const
    {
        return !(has_suggestions(_input));
    }

    // clear
    //   resets or invalidates any cached state in the adapter.
    // Delegates to _Derived::do_clear() if provided; otherwise
    // is a no-op.
    D_INLINE void
    clear()
    {
        internal::default_clear(
            derived_mut(),
            typename internal::has_do_clear<_Derived>());

        return;
    }

    // operator()
    //   callable interface — equivalent to suggest(). Allows the
    // adapter to be used as a function object.
    D_CONSTEXPR_INLINE container_type
    operator()(
        const input_type& _input
    ) const
    {
        return suggest(_input);
    }

protected:
    suggest_adapter()                                = default;
    suggest_adapter(const suggest_adapter&)          = default;
    suggest_adapter(suggest_adapter&&)               = default;
    suggest_adapter& operator=(const suggest_adapter&) = default;
    suggest_adapter& operator=(suggest_adapter&&)    = default;
    ~suggest_adapter()                               = default;

private:
    // derived_ref
    //   function: const CRTP downcast.
    D_CONSTEXPR_INLINE const _Derived&
    derived_ref() const noexcept
    {
        return static_cast<const _Derived&>(*this);
    }

    // derived_mut
    //   function: mutable CRTP downcast.
    D_CONSTEXPR_INLINE _Derived&
    derived_mut() noexcept
    {
        return static_cast<_Derived&>(*this);
    }
};


///////////////////////////////////////////////////////////////////////////////
///           IV.   CONCEPTS (C++20+)                                       ///
///////////////////////////////////////////////////////////////////////////////

#if D_SUGGEST_HAS_CONCEPTS

// suggest_source
//   concept: constrains types that provide the minimal suggest_adapter
// interface — a const suggest() method returning a range of suggestions.
template<typename _Type>
concept suggest_source = requires(const _Type _t,
                                  const typename _Type::input_type& _input)
{
    typename _Type::input_type;
    typename _Type::suggest_type;
    typename _Type::container_type;
    { _t.suggest(_input) }          -> std::convertible_to<
                                           typename _Type::container_type>;
    { _t.has_suggestions(_input) }  -> std::convertible_to<bool>;
    { _t.count(_input) }            -> std::convertible_to<std::size_t>;
};

// scored_suggest_source
//   concept: constrains suggest sources whose suggestion type exposes
// a score() method. Useful for ranking-aware autocomplete providers.
template<typename _Type>
concept scored_suggest_source =
    ( suggest_source<_Type> &&
      requires(const typename _Type::suggest_type& _s)
      {
          { _s.score() } -> std::convertible_to<double>;
      } );

#endif  // D_SUGGEST_HAS_CONCEPTS


#endif  // D_ENV_LANG_IS_CPP11_OR_HIGHER


NS_END  // adapter
NS_END  // uxoxo


#endif  // UXOXO_ADAPTER_SUGGEST_ADAPTER_
