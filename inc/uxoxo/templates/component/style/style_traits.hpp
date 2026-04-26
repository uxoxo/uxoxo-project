/*******************************************************************************
* uxoxo [component]                                            style_traits.hpp
*
* Structural style detection for UI elements
*   A UI element is stylable if it exposes a defined set of presentation
* parameters that can be externally modified without altering its structure
* or behavior.
*
*   This header provides SFINAE-based detection of the style protocol on
* arbitrary UI types. No base class, no tags, no registration — expose
* the right members and the trait system classifies you automatically.
* All detection uses clean_t<T>.
*
*   The style protocol is framework-agnostic: it detects the ability to
* get, set, apply, and export style — not what that style contains. The
* concrete property tags and value types are defined by the consumer.
*
* STRUCTURAL DETECTION CHEAT SHEET
* ================================
* Type Aliases You Provide           Traits Activated
* ----------------------------------------------------------------
* style_type                         has_style_type
*
* Methods You Provide                Traits Activated
* ----------------------------------------------------------------
* apply_style(style_type)            has_apply_style
* get_style() const                  has_get_style
* get<_Tag>() const                  has_style_property<T, _Tag>
* set<_Tag>(value)                   has_mutable_style_property<T, _Tag>
*
*
* path:      /inc/uxoxo/templates/component/style/style_traits.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.04.18
*******************************************************************************/

#ifndef UXOXO_COMPONENT_STYLE_TRAITS_
#define UXOXO_COMPONENT_STYLE_TRAITS_

// std
#include <cstddef>
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
#include <djinterp/core/meta/type_traits.hpp>
// uxoxo
#include "../../../uxoxo.hpp"


NS_UXOXO
NS_COMPONENT

using djinterp::clean_t;


// =============================================================================
// 0.   STYLE CAPABILITY CLASSIFICATION
// =============================================================================

// DStyleCapability
//   enum: aggregate style capability levels (cheapest to most capable).
enum class DStyleCapability
{
    none,           // no style interface detected
    readable,       // can export style (get_style only)
    writable,       // can receive style (apply_style only)
    configurable    // supports round-trip (both apply and get)
};


// =============================================================================
// 1.   STYLE PROTOCOL DETECTION
// =============================================================================

// -------------------------------------------------------------------------
// has_style_type
// -------------------------------------------------------------------------

// has_style_type
//   trait: detects whether _Type exposes a public `style_type` alias.
// A type that is self-aware of its own style representation.
template<typename _Type,
         typename = void>
struct has_style_type : std::false_type
{};

// has_style_type (success case)
//   trait: specialization when `style_type` is well-formed.
template<typename _Type>
struct has_style_type<_Type, void_t<
    typename clean_t<_Type>::style_type
>> : std::true_type
{};

// has_style_type_v
//   variable template: value of has_style_type<_Type>.
template<typename _Type>
inline constexpr bool has_style_type_v = has_style_type<_Type>::value;

// -------------------------------------------------------------------------
// has_apply_style
// -------------------------------------------------------------------------

// has_apply_style
//   trait: detects an `apply_style(style_type)` method, indicating the
// type supports external style application.
template<typename _Type,
         typename = void>
struct has_apply_style : std::false_type
{};

// has_apply_style (success case)
//   trait: specialization when `apply_style(style_type)` is well-formed.
template<typename _Type>
struct has_apply_style<_Type, void_t<
    typename clean_t<_Type>::style_type,
    decltype(std::declval<clean_t<_Type>&>().apply_style(
        std::declval<
            const typename clean_t<_Type>::style_type&>()))
>> : std::true_type
{};

// has_apply_style_v
//   variable template: value of has_apply_style<_Type>.
template<typename _Type>
inline constexpr bool has_apply_style_v =
    has_apply_style<_Type>::value;

// -------------------------------------------------------------------------
// has_get_style
// -------------------------------------------------------------------------

// has_get_style
//   trait: detects a const-qualified `get_style()` accessor returning
// a style_type, indicating the type can export its current style.
template<typename _Type,
         typename = void>
struct has_get_style : std::false_type
{};

// has_get_style (success case)
//   trait: specialization when `get_style()` is well-formed.
template<typename _Type>
struct has_get_style<_Type, void_t<
    typename clean_t<_Type>::style_type,
    decltype(std::declval<const clean_t<_Type>&>().get_style())
>> : std::true_type
{};

// has_get_style_v
//   variable template: value of has_get_style<_Type>.
template<typename _Type>
inline constexpr bool has_get_style_v = has_get_style<_Type>::value;


// =============================================================================
// 2.   PER-PROPERTY DETECTION
// =============================================================================

// -------------------------------------------------------------------------
// has_style_property
// -------------------------------------------------------------------------

// has_style_property
//   trait: detects whether _Type provides a const `get<_Tag>()` accessor
// for a given property tag. This is tag-driven: the consumer defines the
// tags, and this trait checks whether a type exposes them.
template<typename _Type,
         typename _Tag,
         typename = void>
struct has_style_property : std::false_type
{};

// has_style_property (success case)
//   trait: specialization when `get<_Tag>()` is well-formed.
template<typename _Type,
         typename _Tag>
struct has_style_property<_Type, _Tag, void_t<
    decltype(std::declval<const clean_t<_Type>&>()
        .template get<_Tag>())
>> : std::true_type
{};

// has_style_property_v
//   variable template: value of has_style_property<_Type, _Tag>.
template<typename _Type,
         typename _Tag>
inline constexpr bool has_style_property_v =
    has_style_property<_Type, _Tag>::value;

// -------------------------------------------------------------------------
// has_mutable_style_property
// -------------------------------------------------------------------------

// has_mutable_style_property
//   trait: detects whether _Type provides a mutable `set<_Tag>(value)`
// mutator for a given property tag. The value argument is inferred from
// the return type of `get<_Tag>()`.
template<typename _Type,
         typename _Tag,
         typename = void>
struct has_mutable_style_property : std::false_type
{};

// has_mutable_style_property (success case)
//   trait: specialization when `set<_Tag>(get<_Tag>())` is well-formed.
template<typename _Type,
         typename _Tag>
struct has_mutable_style_property<_Type, _Tag, void_t<
    decltype(std::declval<clean_t<_Type>&>()
        .template set<_Tag>(
            std::declval<const clean_t<_Type>&>()
                .template get<_Tag>()))
>> : std::true_type
{};

// has_mutable_style_property_v
//   variable template: value of has_mutable_style_property<_Type, _Tag>.
template<typename _Type,
         typename _Tag>
inline constexpr bool has_mutable_style_property_v =
    has_mutable_style_property<_Type, _Tag>::value;


// =============================================================================
// 3.   AGGREGATE TRAITS
// =============================================================================

// -------------------------------------------------------------------------
// is_stylable
// -------------------------------------------------------------------------

// is_stylable
//   trait: a UI element is stylable if it exposes a style_type alias.
// This is the entry-point trait — use it to gate style-related template
// code.
template<typename _Type>
struct is_stylable : has_style_type<_Type>
{};

// is_stylable_v
//   variable template: value of is_stylable<_Type>.
template<typename _Type>
inline constexpr bool is_stylable_v = is_stylable<_Type>::value;

// -------------------------------------------------------------------------
// is_style_readable
// -------------------------------------------------------------------------

// is_style_readable
//   trait: true if the type can export its current style via get_style.
template<typename _Type>
struct is_style_readable : has_get_style<_Type>
{};

// is_style_readable_v
//   variable template: value of is_style_readable<_Type>.
template<typename _Type>
inline constexpr bool is_style_readable_v =
    is_style_readable<_Type>::value;

// -------------------------------------------------------------------------
// is_style_writable
// -------------------------------------------------------------------------

// is_style_writable
//   trait: true if the type can receive an external style via
// apply_style.
template<typename _Type>
struct is_style_writable : has_apply_style<_Type>
{};

// is_style_writable_v
//   variable template: value of is_style_writable<_Type>.
template<typename _Type>
inline constexpr bool is_style_writable_v =
    is_style_writable<_Type>::value;

// -------------------------------------------------------------------------
// is_style_configurable
// -------------------------------------------------------------------------

// is_style_configurable
//   trait: true if the type supports the full apply_style / get_style
// protocol, enabling round-trip style serialization.
template<typename _Type>
struct is_style_configurable
    : std::conjunction<
        has_apply_style<_Type>,
        has_get_style<_Type>
    >
{};

// is_style_configurable_v
//   variable template: value of is_style_configurable<_Type>.
template<typename _Type>
inline constexpr bool is_style_configurable_v =
    is_style_configurable<_Type>::value;


// =============================================================================
// 4.   CLASSIFICATION STRUCT
// =============================================================================

// ui_style_class
//   struct: aggregates all style protocol detection results for a given
// type into a single classification.
template<typename _Type>
struct ui_style_class
{
private:
    using T = clean_t<_Type>;

public:
    // protocol detection
    static constexpr bool has_style_t  = has_style_type_v<T>;
    static constexpr bool apply        = has_apply_style_v<T>;
    static constexpr bool get          = has_get_style_v<T>;

    // aggregate classification
    static constexpr bool stylable     = is_stylable_v<T>;
    static constexpr bool readable     = is_style_readable_v<T>;
    static constexpr bool writable     = is_style_writable_v<T>;
    static constexpr bool configurable = is_style_configurable_v<T>;

    // capability level
    static constexpr DStyleCapability capability =
        configurable ? DStyleCapability::configurable :
        writable     ? DStyleCapability::writable     :
        readable     ? DStyleCapability::readable     :
                       DStyleCapability::none;
};


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_STYLE_TRAITS_