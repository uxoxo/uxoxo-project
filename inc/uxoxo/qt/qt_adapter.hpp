/*******************************************************************************
* uxoxo [ui/qt]                                               qt_adapter.hpp
*
*   Shared customization-point adapter for bridging uxoxo data types to
* Qt types.  Used by qt_menu_bar and qt_tree_view (and any future Qt bridge).
*
*   Provides default conversions:
*     std::string  → QString          (via fromStdString)
*     const char*  → QString          (via fromUtf8)
*     int          → QIcon            (via QStyle::StandardPixmap)
*     std::string  → QIcon            (from file/resource path)
*     std::string  → QKeySequence     (via fromString)
*
*   Users specialise qt_adapter<_Data, _Icon> for custom types.
*
*   Structure:
*     §1  Environment defaults (safe fallbacks when env.h absent)
*     §2  Adapter traits (SFINAE detection — always compiles)
*     §3  Default adapter (Qt-conditional conversions)
*     §4  Callback types
*
*
* path:      /inc/uxoxo/ui/qt/qt_adapter.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.28
*******************************************************************************/

#ifndef  UXOXO_UI_QT_ADAPTER_
#define  UXOXO_UI_QT_ADAPTER_ 1

#include <uxoxo>

// ═══════════════════════════════════════════════════════════════════════════════
//  §1  ENVIRONMENT DEFAULTS
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef D_ENV_QT_AVAILABLE
    #define D_ENV_QT_AVAILABLE      0
#endif
#ifndef D_ENV_QT_HAS_WIDGETS
    #define D_ENV_QT_HAS_WIDGETS    0
#endif
#ifndef D_ENV_QT_IS_QT6
    #define D_ENV_QT_IS_QT6         0
#endif
#ifndef D_ENV_QT_IS_QT5
    #define D_ENV_QT_IS_QT5         0
#endif
#ifndef D_ENV_QT_IS_QT4
    #define D_ENV_QT_IS_QT4         0
#endif
#ifndef D_ENV_LANG_IS_CPP17_OR_HIGHER
    #if __cplusplus >= 201703L
        #define D_ENV_LANG_IS_CPP17_OR_HIGHER 1
    #else
        #define D_ENV_LANG_IS_CPP17_OR_HIGHER 0
    #endif
#endif
#ifndef D_ENV_LANG_IS_CPP14_OR_HIGHER
    #if __cplusplus >= 201402L
        #define D_ENV_LANG_IS_CPP14_OR_HIGHER 1
    #else
        #define D_ENV_LANG_IS_CPP14_OR_HIGHER 0
    #endif
#endif

// ── Qt includes (conditional) ────────────────────────────────────────────
#if D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS
    #include <QIcon>
    #include <QKeySequence>
    #include <QString>
    #include <QStyle>
    #include <QApplication>
#endif

#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>


NS_UXOXO
NS_UI
NS_QT


// ═══════════════════════════════════════════════════════════════════════════════
//  §2  ADAPTER TRAITS  (always compiles — no Qt dependency)
// ═══════════════════════════════════════════════════════════════════════════════

namespace adapter_traits {
namespace detail
{
    // has_data_type
    //   trait: detects a public data_type alias.
    template <typename,
              typename = void>
    struct has_data_type : std::false_type
    {};

    template <typename _T>
    struct has_data_type<_T, std::void_t<typename _T::data_type>>
        : std::true_type
    {};

    // has_icon_type
    //   trait: detects a public icon_type alias.
    template <typename,
              typename = void>
    struct has_icon_type : std::false_type
    {};

    template <typename _T>
    struct has_icon_type<_T, std::void_t<typename _T::icon_type>>
        : std::true_type
    {};

#if D_ENV_QT_AVAILABLE
    // has_to_qstring_method
    //   trait: detects a static to_qstring conversion method.
    template <typename,
              typename = void>
    struct has_to_qstring_method : std::false_type
    {};

    template <typename _T>
    struct has_to_qstring_method<_T, std::void_t<
        decltype(_T::to_qstring(std::declval<const typename _T::data_type&>()))
    >> : std::true_type
    {};

    // has_to_qicon_method
    //   trait: detects a static to_qicon conversion method.
    template <typename,
              typename = void>
    struct has_to_qicon_method : std::false_type
    {};

    template <typename _T>
    struct has_to_qicon_method<_T, std::void_t<
        decltype(_T::to_qicon(std::declval<const typename _T::icon_type&>()))
    >> : std::true_type
    {};
#endif

}   // namespace detail

template <typename _T>
inline constexpr bool has_data_type_v = detail::has_data_type<_T>::value;

template <typename _T>
inline constexpr bool has_icon_type_v = detail::has_icon_type<_T>::value;

// is_qt_adapter
//   trait: T provides the minimum adapter interface.
template <typename _Type>
struct is_qt_adapter : std::conjunction<
    detail::has_data_type<_Type>,
    detail::has_icon_type<_Type>
>
{};

template <typename _T>
inline constexpr bool is_qt_adapter_v = is_qt_adapter<_T>::value;

}   // namespace adapter_traits


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  DEFAULT ADAPTER
// ═══════════════════════════════════════════════════════════════════════════════

// qt_adapter
//   struct: default data-type → Qt-type conversion adapter.  Users specialise
// for custom types.
template <typename _Data = std::string,
          typename _Icon = int>
struct qt_adapter
{
    using data_type = _Data;
    using icon_type = _Icon;

#if D_ENV_QT_AVAILABLE
    static QString to_qstring(const _Data& data)
    {
        if constexpr (std::is_same_v<_Data, std::string>)
        {
            return QString::fromStdString(data);
        }
        else if constexpr (std::is_convertible_v<_Data, const char*>)
        {
            return QString::fromUtf8(data);
        }
        else
        {
            return QString::fromStdString(std::to_string(data));
        }
    }

    static QIcon to_qicon(const _Icon& icon)
    {
        if constexpr (std::is_integral_v<_Icon>)
        {
            if (auto* app = qApp)
            {
                if (auto* style = app->style())
                {
                    return style->standardIcon(
                        static_cast<QStyle::StandardPixmap>(icon));
                }
            }

            return QIcon();
        }
        else if constexpr (std::is_same_v<_Icon, std::string>)
        {
            return QIcon(QString::fromStdString(icon));
        }
        else if constexpr (std::is_same_v<_Icon, QIcon>)
        {
            return icon;
        }
        else
        {
            static_assert(sizeof(_Icon) == 0,
                "No default icon conversion; specialise qt_adapter");

            return QIcon();
        }
    }

    static QKeySequence to_qkeysequence(const std::string& shortcut)
    {
        if (shortcut.empty())
        {
            return QKeySequence();
        }

        return QKeySequence(QString::fromStdString(shortcut));
    }
#endif
};

// qt_menu_adapter
//   type: backward compatibility alias for qt_adapter.
template <typename _Data = std::string,
          typename _Icon = int>
using qt_menu_adapter = qt_adapter<_Data, _Icon>;


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  CALLBACK TYPES
// ═══════════════════════════════════════════════════════════════════════════════

// simple_callback
//   type: basic callback accepting a const reference to the data type.
template <typename _Data>
using simple_callback = std::function<void(const _Data&)>;


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  ADAPTER VERIFICATION
// ═══════════════════════════════════════════════════════════════════════════════

namespace adapter_verify {
    static_assert(
        adapter_traits::is_qt_adapter_v<qt_adapter<std::string, int>>,
        "Default adapter must satisfy is_qt_adapter");
    static_assert(
        adapter_traits::is_qt_adapter_v<qt_adapter<std::string, std::string>>,
        "string/string adapter must satisfy is_qt_adapter");
}


NS_END  // qt
NS_END  // ui
NS_END  // uxoxo

#endif  // UXOXO_UI_QT_ADAPTER_
