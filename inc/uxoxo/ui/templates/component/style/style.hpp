// [uxoxo] ui_style.hpp — Tag-driven style template for UI elements
//
//   Provides a generalized, framework-agnostic style representation
// built on tagged properties. The consumer defines the property tags
// and value types; this header provides the machinery to compose them
// into a style, look them up at compile time, and satisfy the style
// protocol detected by ui_style_traits.hpp.
//
//   No presentation semantics are prescribed — this header has no
// concept of color, font, padding, or any other domain-specific axis.
// Those are defined by the consumer via tag types.
//
// EXAMPLE USAGE
// =============
//   struct fg_tag  {};
//   struct pad_tag {};
//
//   using my_style = uxoxo::ui::style<
//       uxoxo::ui::property<fg_tag,  rgba>,
//       uxoxo::ui::property<pad_tag, int>
//   >;
//
//   my_style s;
//   s.set<fg_tag>(rgba{255, 0, 0, 255});
//   auto c = s.get<fg_tag>();
//
// PORTABILITY
// ===========
// C++20  : defaulted operator==, constexpr member functions
// C++17  : full constexpr, fold expressions, inline variables
// C++14  : constexpr constructors, variable templates
// C++11  : basic struct, no constexpr member functions
//
// path:      /inc/uxoxo/ui_style.hpp
// link(s):   TBA
// author(s): teer                                          date: 2025.06.08

#ifndef UXOXO_UI_STYLE_HPP
#define UXOXO_UI_STYLE_HPP

#include <cstddef>
#include <type_traits>

#include "uxoxo.hpp"
#include "ui_style_traits.hpp"


NS_UXOXO
NS_UI


// =============================================================================
// 0.   PROPERTY
// =============================================================================

// property
//   struct: a tagged style property associating a compile-time tag type
// with a value type. The tag carries no data — it exists only to
// distinguish properties at the type level.
template<typename _Tag,
         typename _Value>
struct property
{
    using tag_type   = _Tag;
    using value_type = _Value;

    _Value value {};
};


// =============================================================================
// 1.   PROPERTY INTROSPECTION
// =============================================================================

// -------------------------------------------------------------------------
// is_property
// -------------------------------------------------------------------------

// is_property
//   trait: detects whether _Type is a property<Tag, Value> instantiation.
template<typename _Type>
struct is_property : std::false_type
{};

// is_property (success case)
//   trait: specialization for property<_Tag, _Value>.
template<typename _Tag,
         typename _Value>
struct is_property<property<_Tag, _Value>> : std::true_type
{};

// is_property_v
//   variable template: value of is_property<_Type>.
template<typename _Type>
inline constexpr bool is_property_v = is_property<_Type>::value;

// -------------------------------------------------------------------------
// has_property
// -------------------------------------------------------------------------

// has_property
//   trait: compile-time check for whether a property pack contains a
// property with the given tag.
template<typename    _Tag,
         typename... _Properties>
struct has_property : std::disjunction<
    std::is_same<_Tag, typename _Properties::tag_type>...
>
{};

// has_property_v
//   variable template: value of has_property<_Tag, _Properties...>.
template<typename    _Tag,
         typename... _Properties>
inline constexpr bool has_property_v =
    has_property<_Tag, _Properties...>::value;

// -------------------------------------------------------------------------
// property_count
// -------------------------------------------------------------------------

// property_count
//   trait: returns the number of properties in a property pack.
template<typename... _Properties>
struct property_count
    : std::integral_constant<std::size_t, sizeof...(_Properties)>
{};

// property_count_v
//   variable template: value of property_count<_Properties...>.
template<typename... _Properties>
inline constexpr std::size_t property_count_v =
    property_count<_Properties...>::value;


// =============================================================================
// 2.   INTERNAL HELPERS
// =============================================================================

NS_INTERNAL

    // property_for_tag
    //   trait: finds the property with matching tag in a property pack
    // (recursive case).
    template<typename    _Tag,
             typename    _Head,
             typename... _Tail>
    struct property_for_tag
    {
        using type = std::conditional_t<
            std::is_same_v<_Tag, typename _Head::tag_type>,
            _Head,
            typename property_for_tag<_Tag, _Tail...>::type
        >;
    };

    // property_for_tag (base case)
    //   trait: single-property termination. Static assertion fires if
    // the tag was not found in the entire pack.
    template<typename _Tag,
             typename _Last>
    struct property_for_tag<_Tag, _Last>
    {
        static_assert(
            std::is_same_v<_Tag, typename _Last::tag_type>,
            "Style property tag `_Tag` not found in property pack.");

        using type = _Last;
    };

    // property_for_tag_t
    //   type: convenience alias for property_for_tag<...>::type.
    template<typename    _Tag,
             typename... _Properties>
    using property_for_tag_t =
        typename property_for_tag<_Tag, _Properties...>::type;

NS_END  // internal


// =============================================================================
// 3.   STYLE TEMPLATE
// =============================================================================

// style
//   class: a compile-time parameterized collection of tagged style
// properties. Each template argument must be a property<Tag, Value>
// instantiation. Properties are stored via private inheritance (empty
// base optimization applies to empty tag types). Access is exclusively
// through the tag-based get<Tag>() and set<Tag>() interface.
//
//   Satisfies the style protocol: exposes style_type, apply_style,
// and get_style, making it detectable by ui_style_traits.hpp.
template<typename... _Properties>
class style : private _Properties...
{
    static_assert(
        ( is_property_v<_Properties> && ... ),
        "All template arguments to style<> must be property<Tag, Value> "
        "instantiations.");

public:
    // -----------------------------------------------------------------
    // public type aliases
    // -----------------------------------------------------------------

    using style_type = style;

    // -----------------------------------------------------------------
    // compile-time introspection
    // -----------------------------------------------------------------

    static constexpr std::size_t count = sizeof...(_Properties);

    // has
    //   function: compile-time check for tag presence.
    template<typename _Tag>
    static constexpr bool
    has() noexcept
    {
        return has_property_v<_Tag, _Properties...>;
    }

    // -----------------------------------------------------------------
    // constructors
    // -----------------------------------------------------------------

    constexpr style() = default;

    constexpr style(
            const style& _other
        ) = default;

    constexpr style(
            style&& _other
        ) = default;

    // value-initialization constructor
    constexpr explicit style(
            const _Properties&... _props
        ) noexcept
            : _Properties(_props)...
        {}

    // -----------------------------------------------------------------
    // assignment
    // -----------------------------------------------------------------

    style& operator=(const style& _other) = default;
    style& operator=(style&&      _other) = default;

    // -----------------------------------------------------------------
    // tag-based property access
    // -----------------------------------------------------------------

    // get
    //   accessor: returns the value of the property identified by _Tag.
    template<typename _Tag>
    constexpr const
        typename internal::property_for_tag_t<
            _Tag, _Properties...>::value_type&
    get() const noexcept
    {
        using prop_type =
            internal::property_for_tag_t<_Tag, _Properties...>;

        return static_cast<const prop_type&>(*this).value;
    }

    // set
    //   mutator: sets the value of the property identified by _Tag.
    template<typename _Tag>
    void
    set(
        const typename internal::property_for_tag_t<
            _Tag, _Properties...>::value_type& _v
    ) noexcept
    {
        using prop_type =
            internal::property_for_tag_t<_Tag, _Properties...>;

        static_cast<prop_type&>(*this).value = _v;

        return;
    }

    // -----------------------------------------------------------------
    // style protocol
    // -----------------------------------------------------------------

    // apply_style
    //   method: applies another style to this one (overwrites all
    // properties).
    void
    apply_style(
        const style& _s
    ) noexcept
    {
        *this = _s;

        return;
    }

    // get_style
    //   accessor: returns a copy of this style.
    constexpr style
    get_style() const noexcept
    {
        return *this;
    }

    // -----------------------------------------------------------------
    // comparison
    // -----------------------------------------------------------------

#if D_ENV_LANG_IS_CPP20_OR_HIGHER

    constexpr bool
    operator==(
        const style& _other
    ) const = default;

#else

    // operator==
    //   comparison: member-wise property equality via fold.
    constexpr bool
    operator==(
        const style& _other
    ) const noexcept
    {
        return ( (static_cast<const _Properties&>(*this).value ==
                  static_cast<const _Properties&>(_other).value) && ... );
    }

    // operator!=
    //   comparison: negation of operator==.
    constexpr bool
    operator!=(
        const style& _other
    ) const noexcept
    {
        return !(*this == _other);
    }

#endif  // D_ENV_LANG_IS_CPP20_OR_HIGHER
};

// style<> (empty specialization)
//   class: zero-property style. Satisfies the protocol but carries no
// presentation parameters.
template<>
class style<>
{
public:
    using style_type = style;

    static constexpr std::size_t count = 0;

    template<typename _Tag>
    static constexpr bool
    has() noexcept
    {
        return false;
    }

    constexpr style()                          = default;
    constexpr style(const style&)              = default;
    constexpr style(style&&)                   = default;
    style& operator=(const style&)             = default;
    style& operator=(style&&)                  = default;

    void
    apply_style(
        const style& _s
    ) noexcept
    {
        (void)_s;

        return;
    }

    constexpr style
    get_style() const noexcept
    {
        return *this;
    }

    constexpr bool
    operator==(
        const style&
    ) const noexcept
    {
        return true;
    }

    constexpr bool
    operator!=(
        const style&
    ) const noexcept
    {
        return false;
    }
};


// =============================================================================
// 4.   STYLE PROPERTY TYPE EXTRACTION
// =============================================================================

// style_property_value_t
//   type: extracts the value type of a property identified by _Tag
// within a style<Properties...>. Produces a compile error if the tag
// is not present.
template<typename _Style,
         typename _Tag>
struct style_property_value
{};

// style_property_value (style<...> specialization)
//   type: delegates to internal::property_for_tag for tag lookup.
template<typename... _Properties,
         typename    _Tag>
struct style_property_value<style<_Properties...>, _Tag>
{
    using type =
        typename internal::property_for_tag_t<
            _Tag, _Properties...>::value_type;
};

// style_property_value_t
//   type: convenience alias for style_property_value<...>::type.
template<typename _Style,
         typename _Tag>
using style_property_value_t =
    typename style_property_value<_Style, _Tag>::type;


NS_END  // ui
NS_END  // uxoxo


#endif  // UXOXO_UI_STYLE_HPP
