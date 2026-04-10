/*******************************************************************************
* uxoxo [ui/qt]                                               qt_menu_bar.hpp
*
*   Bridge template that adapts the uxoxo menu model (menu.hpp, menu_bar.hpp)
*   to Qt's QMenuBar / QMenu / QAction.
*
*   Uses the shared qt_adapter from qt_adapter.hpp for type conversions.
*
* path:      /inc/uxoxo/ui/qt/qt_menu_bar.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.28
*******************************************************************************/

#ifndef  UXOXO_UI_QT_MENU_BAR_
#define  UXOXO_UI_QT_MENU_BAR_ 1

#include <uxoxo>
#include <component/menu/menu.hpp>
#include <component/menu/menu_bar.hpp>
#include <ui/qt/qt_adapter.hpp>

#if D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS
    #include <QMenuBar>
    #include <QMenu>
    #include <QAction>
    #include <QFont>
    #if D_ENV_QT_IS_QT6
        #include <QActionGroup>
    #endif
#endif

#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


NS_UXOXO
NS_UI
NS_QT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  CALLBACK TYPES
// ═══════════════════════════════════════════════════════════════════════════════

// menu_action_callback
//   type: callback invoked when a menu action is triggered.
template <typename _Data,
          unsigned _Feat,
          typename _Icon>
using menu_action_callback = std::function<void(
    std::size_t, std::size_t,
    const component::menu_item<_Data, _Feat, _Icon>&)>;


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  QT MENU BAR TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace qt_menu_bar_traits {
namespace detail {
    // has_widget_method
    //   trait: detects a .widget() method.
    template <typename,
              typename = void>
    struct has_widget_method : std::false_type
    {};

    template <typename _T>
    struct has_widget_method<_T, std::void_t<decltype(std::declval<_T>().widget())>>
        : std::true_type
    {};

    // has_sync_method
    //   trait: detects a .sync() method.
    template <typename,
              typename = void>
    struct has_sync_method : std::false_type
    {};

    template <typename _T>
    struct has_sync_method<_T, std::void_t<decltype(std::declval<_T>().sync())>>
        : std::true_type
    {};

    // has_model_method
    //   trait: detects a .model() method.
    template <typename,
              typename = void>
    struct has_model_method : std::false_type
    {};

    template <typename _T>
    struct has_model_method<_T, std::void_t<decltype(std::declval<_T>().model())>>
        : std::true_type
    {};
}

template <typename _T> inline constexpr bool has_widget_v = detail::has_widget_method<_T>::value;
template <typename _T> inline constexpr bool has_sync_v   = detail::has_sync_method<_T>::value;
template <typename _T> inline constexpr bool has_model_v  = detail::has_model_method<_T>::value;

// is_qt_menu_bar_bridge
//   trait: composite detection for the qt_menu_bar bridge interface.
template <typename _Type>
struct is_qt_menu_bar_bridge : std::conjunction<
    detail::has_widget_method<_Type>,
    detail::has_sync_method<_Type>,
    detail::has_model_method<_Type>>
{};

template <typename _T>
inline constexpr bool is_qt_menu_bar_bridge_v = is_qt_menu_bar_bridge<_T>::value;

}   // namespace qt_menu_bar_traits


/*****************************************************************************/

#if D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  QT MENU BAR BRIDGE
// ═══════════════════════════════════════════════════════════════════════════════

// qt_menu_bar
//   class: bridges the uxoxo menu_bar model to Qt's QMenuBar / QMenu / QAction.
template <typename _Data    = std::string,
          unsigned _Feat    = component::mf_none,
          typename _Icon    = int,
          typename _Adapter = qt_adapter<_Data, _Icon>>
class qt_menu_bar
{
public:
    using model_type    = component::menu_bar<_Data, _Feat, _Icon>;
    using menu_type     = component::menu<_Data, _Feat, _Icon>;
    using item_type     = component::menu_item<_Data, _Feat, _Icon>;
    using entry_type    = component::menu_bar_entry<_Data, _Feat, _Icon>;
    using callback_type = menu_action_callback<_Data, _Feat, _Icon>;

    static_assert(adapter_traits::is_qt_adapter_v<_Adapter>,
        "Adapter must satisfy is_qt_adapter");

    explicit qt_menu_bar(
            model_type& _model,
            QWidget*    _parent = nullptr
        )
            : m_model(&_model),
              m_qmenubar(new QMenuBar(_parent))
        {
            sync();
        }

    qt_menu_bar(
            model_type& _model,
            QMenuBar*   _bar
        )
            : m_model(&_model),
              m_qmenubar(_bar)
        {
            if (m_qmenubar)
            {
                sync();
            }
        }

    QMenuBar*         widget()       { return m_qmenubar; }
    const QMenuBar*   widget() const { return m_qmenubar; }
    model_type&       model()        { return *m_model; }
    const model_type& model()  const { return *m_model; }

    void set_callback(callback_type cb)
    {
        m_callback = std::move(cb);

        return;
    }

    void set_simple_callback(simple_callback<_Data> cb)
    {
        m_callback = [cb = std::move(cb)](std::size_t, std::size_t,
            const item_type& item) { cb(item.label); };

        return;
    }

    void sync()
    {
        if (!m_qmenubar)
        {
            return;
        }

        m_qmenubar->clear();
        m_qmenus.clear();
        m_qactions.clear();

        for (std::size_t bi = 0; bi < m_model->entries.size(); ++bi)
        {
            auto& entry = m_model->entries[bi];
            if (entry.dropdown && !entry.dropdown->empty())
            {
                QMenu* qm = m_qmenubar->addMenu(_Adapter::to_qstring(entry.label));
                m_qmenus.push_back(qm);
                build_menu_(qm, *entry.dropdown, bi);
            }
            else
            {
                QAction* act = m_qmenubar->addAction(_Adapter::to_qstring(entry.label));
                act->setEnabled(entry.enabled);
                wire_(act, bi, 0, item_type(entry.label));
            }
        }

        return;
    }

    void sync_enabled()
    {
        std::size_t ai = 0;
        for (std::size_t bi = 0; bi < m_model->entries.size(); ++bi)
        {
            auto& entry = m_model->entries[bi];
            if (!entry.dropdown)
            {
                continue;
            }

            for (std::size_t ii = 0; ii < entry.dropdown->size(); ++ii)
            {
                if (ai >= m_qactions.size())
                {
                    return;
                }

                auto& item = (*entry.dropdown)[ii];
                m_qactions[ai]->setEnabled(item.enabled);
            #if D_ENV_LANG_IS_CPP17_OR_HIGHER
                if constexpr (component::has_mf(_Feat, component::mf_checkable))
                {
                    m_qactions[ai]->setChecked(item.checked);
                }
            #endif
                ++ai;
            }
        }

        return;
    }

private:
    void build_menu_(QMenu*      _qm,
                     menu_type&  _menu,
                     std::size_t _bi)
    {
        for (std::size_t ii = 0; ii < _menu.size(); ++ii)
        {
            auto& item = _menu[ii];
            switch (item.type)
            {
                case component::menu_item_type::separator:
                {
                    _qm->addSeparator();
                    break;
                }
                case component::menu_item_type::header:
                {
                    QAction* a = _qm->addAction(_Adapter::to_qstring(item.label));
                    a->setEnabled(false);
                #if D_ENV_QT_IS_QT5 || D_ENV_QT_IS_QT6
                    QFont f = a->font();
                    f.setBold(true);
                    a->setFont(f);
                #endif
                    break;
                }
                case component::menu_item_type::normal:
                {
                #if D_ENV_LANG_IS_CPP17_OR_HIGHER
                    if constexpr (component::has_mf(_Feat, component::mf_submenu))
                    {
                        if (item.submenu && !item.submenu->empty())
                        {
                            build_menu_(_qm->addMenu(_Adapter::to_qstring(item.label)),
                                        *item.submenu, _bi);
                            break;
                        }
                    }
                #endif
                    QAction* a = _qm->addAction(_Adapter::to_qstring(item.label));
                    a->setEnabled(item.enabled);
                #if D_ENV_LANG_IS_CPP17_OR_HIGHER
                    if constexpr (component::has_mf(_Feat, component::mf_shortcuts))
                    {
                        if (!item.shortcut.empty())
                        {
                            a->setShortcut(_Adapter::to_qkeysequence(item.shortcut));
                        }
                    }
                    if constexpr (component::has_mf(_Feat, component::mf_icons))
                    {
                        a->setIcon(_Adapter::to_qicon(item.icon));
                    }
                    if constexpr (component::has_mf(_Feat, component::mf_checkable))
                    {
                        a->setCheckable(true);
                        a->setChecked(item.checked);
                    }
                #endif
                    wire_(a, _bi, ii, item);
                    m_qactions.push_back(a);
                    break;
                }
            }
        }

        return;
    }

    void wire_(QAction*        _a,
               std::size_t     _bi,
               std::size_t     _ii,
               const item_type& _item)
    {
    #if D_ENV_QT_IS_QT5 || D_ENV_QT_IS_QT6
        QObject::connect(_a, &QAction::triggered,
            [this, _bi, _ii, _item](bool)
            {
                m_model->focused = _bi;
                if (m_callback)
                {
                    m_callback(_bi, _ii, _item);
                }
            });
    #else
        Q_UNUSED(_a); Q_UNUSED(_bi); Q_UNUSED(_ii); Q_UNUSED(_item);
    #endif

        return;
    }

    model_type*           m_model    = nullptr;
    QMenuBar*             m_qmenubar = nullptr;
    std::vector<QMenu*>   m_qmenus;
    std::vector<QAction*> m_qactions;
    callback_type         m_callback;
};

// make_qt_menu_bar
//   convenience factory for qt_menu_bar.
template <typename _D,
          unsigned _F,
          typename _I,
          typename _A = qt_adapter<_D, _I>>
qt_menu_bar<_D, _F, _I, _A> make_qt_menu_bar(
    component::menu_bar<_D, _F, _I>& model,
    QWidget*                         parent = nullptr)
{
    return qt_menu_bar<_D, _F, _I, _A>(model, parent);
}

#endif  // D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS


NS_END  // qt
NS_END  // ui
NS_END  // uxoxo

#endif  // UXOXO_UI_QT_MENU_BAR_
