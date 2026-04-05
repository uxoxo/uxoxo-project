/*******************************************************************************
* uxoxo [ui/qt]                                            qt_tree_view.hpp
*
*   Bridge template that adapts the uxoxo tree_view model (tree_node.hpp,
* tree_view.hpp) to Qt's QTreeWidget / QTreeWidgetItem hierarchy.
*
*   Structure:
*     §1–§2  Qt-agnostic callback/trait infrastructure (always compiles)
*     §3–§8  Qt-specific widget bridge (guarded by D_ENV_QT_* macros)
*
*   Feature mapping:
*     tree_node._Data         → QTreeWidgetItem::text(0)     via adapter
*     tree_node.children      → QTreeWidgetItem::addChild()  (recursive)
*     vf_checkable + checked  → Qt::ItemIsUserCheckable + checkState
*     vf_icons + icon         → QTreeWidgetItem::setIcon()   via adapter
*     vf_collapsible+expanded → QTreeWidgetItem::setExpanded()
*     vf_renamable            → Qt::ItemIsEditable
*     vf_context              → QTreeWidget::customContextMenuRequested
*     tree_view.selected      → QTreeWidget selection
*
*   Bidirectional sync:
*     sync()         model → widget  (full rebuild)
*     sync_state()   model → widget  (lightweight: check/expand/enable only)
*     read_back()    widget → model  (captures user changes)
*
*   Portability:
*     Qt 4/5/6 via D_ENV_QT_IS_QT4 / _QT5 / _QT6
*     C++11/14/17 via D_ENV_LANG_IS_CPP*_OR_HIGHER
*
*
* path:      /inc/uxoxo/ui/qt/qt_tree_view.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.28
*******************************************************************************/

#ifndef  UXOXO_UI_QT_TREE_VIEW_
#define  UXOXO_UI_QT_TREE_VIEW_ 1

#include <uxoxo>
#include <component/tree/tree_node.hpp>
#include <component/tree/tree_view.hpp>
#include <ui/qt/qt_adapter.hpp>

// ── Qt includes (conditional) ────────────────────────────────────────────
#if D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS
    #include <QTreeWidget>
    #include <QTreeWidgetItem>
    #include <QHeaderView>
    #include <QMenu>
    #include <QAction>
    #include <QString>
    #include <QIcon>
    #include <QFont>
    #include <QStringList>
    #include <QPoint>
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
//  §1  CALLBACK TYPES  (always compiles — no Qt dependency)
// ═══════════════════════════════════════════════════════════════════════════════

// tree_action_callback
//   type: called when a tree item is activated (double-click / Enter).
template <typename _Data,
          unsigned _Feat,
          typename _Icon>
using tree_action_callback = std::function<void(
    const component::tree_node<_Data, _Feat, _Icon>& /*node*/,
    std::size_t /*depth*/
)>;

// tree_check_callback
//   type: called when a node's check state changes.
template <typename _Data,
          unsigned _Feat,
          typename _Icon>
using tree_check_callback = std::function<void(
    component::tree_node<_Data, _Feat, _Icon>& /*node*/,
    component::check_state /*new_state*/
)>;

// tree_context_callback
//   type: called when a context menu is requested on a node.
template <typename _Data,
          unsigned _Feat,
          typename _Icon>
using tree_context_callback = std::function<void(
    component::tree_node<_Data, _Feat, _Icon>& /*node*/,
    unsigned /*context_actions*/,
    int /*global_x*/, int /*global_y*/
)>;

// tree_rename_callback
//   type: called when a node is renamed by the user.
template <typename _Data,
          unsigned _Feat,
          typename _Icon>
using tree_rename_callback = std::function<void(
    component::tree_node<_Data, _Feat, _Icon>& /*node*/,
    const _Data& /*old_label*/,
    const std::string& /*new_text*/
)>;


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  QT TREE VIEW TRAITS  (always compiles)
// ═══════════════════════════════════════════════════════════════════════════════

namespace qt_tree_view_traits {
namespace detail
{
    // has_widget_method
    //   trait: detects a .widget() method.
    template <typename,
              typename = void>
    struct has_widget_method : std::false_type
    {};

    template <typename _T>
    struct has_widget_method<_T, std::void_t<
        decltype(std::declval<_T>().widget())
    >> : std::true_type
    {};

    // has_sync_method
    //   trait: detects a .sync() method.
    template <typename,
              typename = void>
    struct has_sync_method : std::false_type
    {};

    template <typename _T>
    struct has_sync_method<_T, std::void_t<
        decltype(std::declval<_T>().sync())
    >> : std::true_type
    {};

    // has_model_method
    //   trait: detects a .model() method.
    template <typename,
              typename = void>
    struct has_model_method : std::false_type
    {};

    template <typename _T>
    struct has_model_method<_T, std::void_t<
        decltype(std::declval<_T>().model())
    >> : std::true_type
    {};

    // has_read_back_method
    //   trait: detects a .read_back() method.
    template <typename,
              typename = void>
    struct has_read_back_method : std::false_type
    {};

    template <typename _T>
    struct has_read_back_method<_T, std::void_t<
        decltype(std::declval<_T>().read_back())
    >> : std::true_type
    {};
}

template <typename _T>
inline constexpr bool has_widget_v    = detail::has_widget_method<_T>::value;
template <typename _T>
inline constexpr bool has_sync_v      = detail::has_sync_method<_T>::value;
template <typename _T>
inline constexpr bool has_model_v     = detail::has_model_method<_T>::value;
template <typename _T>
inline constexpr bool has_read_back_v = detail::has_read_back_method<_T>::value;

// is_qt_tree_view_bridge
//   trait: composite detection for the qt_tree_view bridge interface.
template <typename _Type>
struct is_qt_tree_view_bridge : std::conjunction<
    detail::has_widget_method<_Type>,
    detail::has_sync_method<_Type>,
    detail::has_model_method<_Type>,
    detail::has_read_back_method<_Type>
>
{};

template <typename _T>
inline constexpr bool is_qt_tree_view_bridge_v = is_qt_tree_view_bridge<_T>::value;

}   // namespace qt_tree_view_traits


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3–§8  QT WIDGET BRIDGE  (only compiles when Qt Widgets are available)
// ═══════════════════════════════════════════════════════════════════════════════

#if D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  QT TREE VIEW BRIDGE
// ═══════════════════════════════════════════════════════════════════════════════

// qt_tree_view
//   class: bridges the uxoxo tree_view model to Qt's QTreeWidget hierarchy.
template <typename _Data    = std::string,
          unsigned _Feat    = component::vf_none,
          typename _Icon    = int,
          typename _Adapter = qt_adapter<_Data, _Icon>>
class qt_tree_view
{
public:
    using model_type      = component::tree_view<_Data, _Feat, _Icon>;
    using node_type       = component::tree_node<_Data, _Feat, _Icon>;
    using adapter_type    = _Adapter;

    using action_cb_type  = tree_action_callback<_Data, _Feat, _Icon>;
    using check_cb_type   = tree_check_callback<_Data, _Feat, _Icon>;
    using context_cb_type = tree_context_callback<_Data, _Feat, _Icon>;
    using rename_cb_type  = tree_rename_callback<_Data, _Feat, _Icon>;

    static_assert(adapter_traits::is_qt_adapter_v<_Adapter>,
        "Adapter must satisfy is_qt_adapter");

    static constexpr bool feat_checkable   = component::has_feat(_Feat, component::vf_checkable);
    static constexpr bool feat_icons       = component::has_feat(_Feat, component::vf_icons);
    static constexpr bool feat_collapsible = component::has_feat(_Feat, component::vf_collapsible);
    static constexpr bool feat_renamable   = component::has_feat(_Feat, component::vf_renamable);
    static constexpr bool feat_context     = component::has_feat(_Feat, component::vf_context);

    // ── construction ─────────────────────────────────────────────────────

    explicit qt_tree_view(
            model_type& _model,
            QWidget*    _parent = nullptr
        )
            : m_model(&_model),
              m_qtree(new QTreeWidget(_parent))
        {
            configure_widget_();
            sync();
        }

    qt_tree_view(
            model_type&  _model,
            QTreeWidget* _tree
        )
            : m_model(&_model),
              m_qtree(_tree)
        {
            if (m_qtree)
            {
                configure_widget_();
                sync();
            }
        }

    // ── access ───────────────────────────────────────────────────────────

    QTreeWidget*         widget()       { return m_qtree; }
    const QTreeWidget*   widget() const { return m_qtree; }
    model_type&          model()        { return *m_model; }
    const model_type&    model()  const { return *m_model; }

    // ── column configuration ─────────────────────────────────────────────

    // set_columns
    //   Sets the header labels for a multi-column tree.  Column 0 is always
    // the primary (node label) column.  Additional columns are populated by
    // the column_extractor callback.
    void set_columns(const std::vector<std::string>& headers)
    {
        if (!m_qtree)
        {
            return;
        }

        QStringList labels;
        for (const auto& h : headers)
        {
            labels << _Adapter::to_qstring(h);
        }

        m_qtree->setHeaderLabels(labels);
        m_column_count = static_cast<int>(headers.size());

        return;
    }

    // set_column_extractor
    //   Callback: (const node_type&, column_index) → display string.
    //   Called during sync to populate columns 1..N.
    using column_extractor = std::function<std::string(
        const node_type&, int /*column*/)>;

    void set_column_extractor(column_extractor fn)
    {
        m_extractor = std::move(fn);

        return;
    }

    // ── callback wiring ──────────────────────────────────────────────────

    void set_action_callback(action_cb_type cb)  { m_on_action = std::move(cb); }
    void set_check_callback(check_cb_type cb)    { m_on_check = std::move(cb); }
    void set_context_callback(context_cb_type cb){ m_on_context = std::move(cb); }
    void set_rename_callback(rename_cb_type cb)  { m_on_rename = std::move(cb); }

    // ── synchronisation: model → widget ──────────────────────────────────

    // sync
    //   Full rebuild of the QTreeWidget from the model.
    void sync()
    {
        if (!m_qtree)
        {
            return;
        }

        // block signals during rebuild to avoid spurious callbacks
        m_qtree->blockSignals(true);
        m_qtree->clear();
        m_item_map.clear();

        for (auto& root : m_model->roots)
        {
            build_item_(nullptr, root, 0);
        }

        m_qtree->blockSignals(false);

        // expand state (after items exist)
        sync_expand_state_();

        return;
    }

    // sync_state
    //   Lightweight: updates check/expand/enabled without rebuilding items.
    void sync_state()
    {
        if (!m_qtree)
        {
            return;
        }

        m_qtree->blockSignals(true);

        for (auto& mapping : m_item_map)
        {
            auto* item = mapping.qt_item;
            auto* node = mapping.node;

        #if D_ENV_LANG_IS_CPP17_OR_HIGHER
            if constexpr (feat_checkable)
            {
                item->setCheckState(0, to_qt_check_state_(node->checked));
            }
            if constexpr (feat_collapsible)
            {
                item->setExpanded(node->expanded);
            }
        #else
            sync_check_dispatch_(item, node,
                std::integral_constant<bool, feat_checkable>{});
            sync_expand_dispatch_(item, node,
                std::integral_constant<bool, feat_collapsible>{});
        #endif
        }

        m_qtree->blockSignals(false);

        return;
    }

    // ── synchronisation: widget → model ──────────────────────────────────

    // read_back
    //   Captures user changes from the QTreeWidget back into the model.
    //   Handles: check state, expand state.
    void read_back()
    {
        for (auto& mapping : m_item_map)
        {
            auto* item = mapping.qt_item;
            auto* node = mapping.node;

        #if D_ENV_LANG_IS_CPP17_OR_HIGHER
            if constexpr (feat_checkable)
            {
                node->checked = from_qt_check_state_(item->checkState(0));
            }
            if constexpr (feat_collapsible)
            {
                node->expanded = item->isExpanded();
            }
        #else
            read_check_dispatch_(item, node,
                std::integral_constant<bool, feat_checkable>{});
            read_expand_dispatch_(item, node,
                std::integral_constant<bool, feat_collapsible>{});
        #endif
        }

        // rebuild model's visible cache since expand state may have changed
        m_model->visible_dirty = true;

        return;
    }

    // ── expand helpers ───────────────────────────────────────────────────

    void expand_all()
    {
        if (m_qtree)
        {
            m_qtree->expandAll();
        }

        return;
    }

    void collapse_all()
    {
        if (m_qtree)
        {
            m_qtree->collapseAll();
        }

        return;
    }

    void expand_to_depth(int depth)
    {
        if (!m_qtree)
        {
            return;
        }

    #if D_ENV_QT_IS_QT5 || D_ENV_QT_IS_QT6
        m_qtree->expandToDepth(depth);
    #else
        // Qt 4: manual implementation
        expand_to_depth_manual_(m_qtree->invisibleRootItem(), 0, depth);
    #endif

        return;
    }

    // ── find ─────────────────────────────────────────────────────────────

    // find_qt_item
    //   Returns the QTreeWidgetItem associated with a model node, or nullptr.
    QTreeWidgetItem* find_qt_item(const node_type* node)
    {
        for (auto& m : m_item_map)
        {
            if (m.node == node)
            {
                return m.qt_item;
            }
        }

        return nullptr;
    }

    // find_node
    //   Returns the model node associated with a QTreeWidgetItem, or nullptr.
    node_type* find_node(const QTreeWidgetItem* item)
    {
        for (auto& m : m_item_map)
        {
            if (m.qt_item == item)
            {
                return m.node;
            }
        }

        return nullptr;
    }

    // ── selection sync ───────────────────────────────────────────────────

    // sync_selection_to_widget
    //   Updates QTreeWidget selection from model's selected indices.
    void sync_selection_to_widget()
    {
        if (!m_qtree)
        {
            return;
        }

        m_qtree->blockSignals(true);
        m_qtree->clearSelection();

        auto& vis = m_model->entries();
        for (auto idx : m_model->selected)
        {
            if (idx < vis.size())
            {
                auto* qitem = find_qt_item(vis[idx].node);
                if (qitem)
                {
                    qitem->setSelected(true);
                }
            }
        }

        m_qtree->blockSignals(false);

        return;
    }

    // read_selection_from_widget
    //   Captures QTreeWidget selection back into the model.
    void read_selection_from_widget()
    {
        m_model->selected.clear();
        auto selected = m_qtree->selectedItems();
        auto& vis = m_model->entries();

        for (auto* qitem : selected)
        {
            auto* node = find_node(qitem);
            if (!node)
            {
                continue;
            }

            // find flat index in visible entries
            for (std::size_t i = 0; i < vis.size(); ++i)
            {
                if (vis[i].node == node)
                {
                    m_model->selected.push_back(i);
                    break;
                }
            }
        }

        return;
    }

private:

    // ── item ↔ node mapping ──────────────────────────────────────────────

    // item_mapping
    //   struct: associates a QTreeWidgetItem with its model node and depth.
    struct item_mapping
    {
        QTreeWidgetItem* qt_item;
        node_type*       node;
        std::size_t      depth;
    };

    // ── widget configuration ─────────────────────────────────────────────

    void configure_widget_()
    {
        if (!m_qtree)
        {
            return;
        }

        // header
        if (m_column_count <= 1)
        {
            m_qtree->setHeaderHidden(true);
            m_qtree->setColumnCount(1);
        }

        // selection mode
        switch (m_model->sel_mode)
        {
            case component::selection_mode::none:
            {
                m_qtree->setSelectionMode(QTreeWidget::NoSelection);
                break;
            }
            case component::selection_mode::single:
            {
                m_qtree->setSelectionMode(QTreeWidget::SingleSelection);
                break;
            }
            case component::selection_mode::multi:
            {
                m_qtree->setSelectionMode(QTreeWidget::ExtendedSelection);
                break;
            }
        }

        // ── signal wiring ────────────────────────────────────────────────

    #if D_ENV_QT_IS_QT5 || D_ENV_QT_IS_QT6

        // item activated (double-click / Enter)
        QObject::connect(m_qtree, &QTreeWidget::itemActivated,
            [this](QTreeWidgetItem* item, int)
            {
                auto* node = find_node(item);
                if (node && m_on_action)
                {
                    // find depth
                    std::size_t d = 0;
                    for (auto& m : m_item_map)
                    {
                        if (m.qt_item == item)
                        {
                            d = m.depth;
                            break;
                        }
                    }
                    m_on_action(*node, d);
                }
            });

        // item expanded / collapsed (sync back to model)
        QObject::connect(m_qtree, &QTreeWidget::itemExpanded,
            [this](QTreeWidgetItem* item)
            {
                if constexpr (feat_collapsible)
                {
                    auto* node = find_node(item);
                    if (node)
                    {
                        node->expanded = true;
                    }
                }
            });

        QObject::connect(m_qtree, &QTreeWidget::itemCollapsed,
            [this](QTreeWidgetItem* item)
            {
                if constexpr (feat_collapsible)
                {
                    auto* node = find_node(item);
                    if (node)
                    {
                        node->expanded = false;
                    }
                }
            });

        // item changed (checkbox toggle or rename)
        QObject::connect(m_qtree, &QTreeWidget::itemChanged,
            [this](QTreeWidgetItem* item, int column)
            {
                auto* node = find_node(item);
                if (!node)
                {
                    return;
                }

                // checkbox
                if constexpr (feat_checkable)
                {
                    if (column == 0)
                    {
                        auto new_state = from_qt_check_state_(
                            item->checkState(0));
                        if (node->checked != new_state)
                        {
                            node->checked = new_state;
                            if (m_on_check)
                            {
                                m_on_check(*node, new_state);
                            }
                        }
                    }
                }

                // rename (column 0 text changed)
                if constexpr (feat_renamable)
                {
                    if (column == 0)
                    {
                        auto new_text = item->text(0).toStdString();
                        // only fire if text actually changed
                        // (itemChanged fires for check changes too)
                    }
                }
            });

        // context menu
        if constexpr (feat_context)
        {
            m_qtree->setContextMenuPolicy(
                ::Qt::CustomContextMenu);

            QObject::connect(m_qtree,
                &QTreeWidget::customContextMenuRequested,
                [this](const QPoint& pos)
                {
                    auto* item = m_qtree->itemAt(pos);
                    if (!item)
                    {
                        return;
                    }

                    auto* node = find_node(item);
                    if (!node)
                    {
                        return;
                    }

                    auto global_pos = m_qtree->viewport()->mapToGlobal(pos);
                    if (m_on_context)
                    {
                        m_on_context(*node, node->context_actions,
                                     global_pos.x(), global_pos.y());
                    }
                });
        }

    #endif  // D_ENV_QT_IS_QT5 || D_ENV_QT_IS_QT6

        return;
    }

    // ── recursive item builder ───────────────────────────────────────────

    void build_item_(QTreeWidgetItem* _parent,
                     node_type&       _node,
                     std::size_t      _depth)
    {
        QTreeWidgetItem* item;
        if (_parent)
        {
            item = new QTreeWidgetItem(_parent);
        }
        else
        {
            item = new QTreeWidgetItem(m_qtree);
        }

        // primary label (column 0)
        item->setText(0, _Adapter::to_qstring(_node.data));

        // additional columns via extractor
        if (m_extractor)
        {
            for (int col = 1; col < m_column_count; ++col)
            {
                item->setText(col, _Adapter::to_qstring(
                    m_extractor(_node, col)));
            }
        }

        // ── feature application ──────────────────────────────────────────

    #if D_ENV_LANG_IS_CPP17_OR_HIGHER

        if constexpr (feat_checkable)
        {
            item->setFlags(item->flags() | ::Qt::ItemIsUserCheckable);
            item->setCheckState(0, to_qt_check_state_(_node.checked));
        }

        if constexpr (feat_icons)
        {
            item->setIcon(0, _Adapter::to_qicon(
                component::effective_icon(_node)));
        }

        if constexpr (feat_renamable)
        {
            if (_node.renamable)
            {
                item->setFlags(item->flags() | ::Qt::ItemIsEditable);
            }
        }

    #else   // C++11/14 tag dispatch

        apply_check_dispatch_(item, _node,
            std::integral_constant<bool, feat_checkable>{});
        apply_icon_dispatch_(item, _node,
            std::integral_constant<bool, feat_icons>{});
        apply_rename_dispatch_(item, _node,
            std::integral_constant<bool, feat_renamable>{});

    #endif

        // record mapping
        m_item_map.push_back({ item, &_node, _depth });

        // recurse into children
        for (auto& child : _node.children)
        {
            build_item_(item, child, _depth + 1);
        }

        return;
    }

    // ── expand state sync ────────────────────────────────────────────────

    void sync_expand_state_()
    {
    #if D_ENV_LANG_IS_CPP17_OR_HIGHER
        if constexpr (feat_collapsible)
        {
            for (auto& m : m_item_map)
            {
                m.qt_item->setExpanded(m.node->expanded);
            }
        }
        else
        {
            // not collapsible → expand everything (flat display)
            if (m_qtree)
            {
                m_qtree->expandAll();
            }
        }
    #else
        sync_expand_all_dispatch_(
            std::integral_constant<bool, feat_collapsible>{});
    #endif

        return;
    }

    // ── check state conversion ───────────────────────────────────────────

    static Qt::CheckState to_qt_check_state_(component::check_state cs)
    {
        switch (cs)
        {
            case component::check_state::checked:       return Qt::Checked;
            case component::check_state::indeterminate: return Qt::PartiallyChecked;
            default:                                    return Qt::Unchecked;
        }
    }

    static component::check_state from_qt_check_state_(Qt::CheckState cs)
    {
        switch (cs)
        {
            case Qt::Checked:          return component::check_state::checked;
            case Qt::PartiallyChecked: return component::check_state::indeterminate;
            default:                   return component::check_state::unchecked;
        }
    }

    // ── C++11/14 tag-dispatch fallbacks ──────────────────────────────────

#if !D_ENV_LANG_IS_CPP17_OR_HIGHER

    void apply_check_dispatch_(QTreeWidgetItem*, const node_type&,
                               std::false_type)
    {}

    void apply_check_dispatch_(QTreeWidgetItem* item, const node_type& node,
                               std::true_type)
    {
        item->setFlags(item->flags() | ::Qt::ItemIsUserCheckable);
        item->setCheckState(0, to_qt_check_state_(node.checked));

        return;
    }

    void apply_icon_dispatch_(QTreeWidgetItem*, const node_type&,
                              std::false_type)
    {}

    void apply_icon_dispatch_(QTreeWidgetItem* item, const node_type& node,
                              std::true_type)
    {
        item->setIcon(0, _Adapter::to_qicon(
            component::effective_icon(node)));

        return;
    }

    void apply_rename_dispatch_(QTreeWidgetItem*, const node_type&,
                                std::false_type)
    {}

    void apply_rename_dispatch_(QTreeWidgetItem* item, const node_type& node,
                                std::true_type)
    {
        if (node.renamable)
        {
            item->setFlags(item->flags() | ::Qt::ItemIsEditable);
        }

        return;
    }

    void sync_check_dispatch_(QTreeWidgetItem*, node_type*,
                              std::false_type)
    {}

    void sync_check_dispatch_(QTreeWidgetItem* item, node_type* node,
                              std::true_type)
    {
        item->setCheckState(0, to_qt_check_state_(node->checked));

        return;
    }

    void sync_expand_dispatch_(QTreeWidgetItem*, node_type*,
                               std::false_type)
    {}

    void sync_expand_dispatch_(QTreeWidgetItem* item, node_type* node,
                               std::true_type)
    {
        item->setExpanded(node->expanded);

        return;
    }

    void read_check_dispatch_(QTreeWidgetItem*, node_type*,
                              std::false_type)
    {}

    void read_check_dispatch_(QTreeWidgetItem* item, node_type* node,
                              std::true_type)
    {
        node->checked = from_qt_check_state_(item->checkState(0));

        return;
    }

    void read_expand_dispatch_(QTreeWidgetItem*, node_type*,
                               std::false_type)
    {}

    void read_expand_dispatch_(QTreeWidgetItem* item, node_type* node,
                               std::true_type)
    {
        node->expanded = item->isExpanded();

        return;
    }

    void sync_expand_all_dispatch_(std::false_type)
    {
        if (m_qtree)
        {
            m_qtree->expandAll();
        }

        return;
    }

    void sync_expand_all_dispatch_(std::true_type)
    {
        for (auto& m : m_item_map)
        {
            m.qt_item->setExpanded(m.node->expanded);
        }

        return;
    }

    // Qt 4 expand-to-depth
    void expand_to_depth_manual_(QTreeWidgetItem* _item,
                                 int              _d,
                                 int              _max_d)
    {
        if (!_item)
        {
            return;
        }

        _item->setExpanded(_d < _max_d);
        for (int i = 0; i < _item->childCount(); ++i)
        {
            expand_to_depth_manual_(_item->child(i), _d + 1, _max_d);
        }

        return;
    }

#endif  // !D_ENV_LANG_IS_CPP17_OR_HIGHER

    // ── members ──────────────────────────────────────────────────────────

    model_type*                 m_model        = nullptr;
    QTreeWidget*                m_qtree        = nullptr;
    std::vector<item_mapping>   m_item_map;
    int                         m_column_count = 1;
    column_extractor            m_extractor;

    action_cb_type              m_on_action;
    check_cb_type               m_on_check;
    context_cb_type             m_on_context;
    rename_cb_type              m_on_rename;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §7  CONVENIENCE FACTORY
// ═══════════════════════════════════════════════════════════════════════════════

// make_qt_tree_view
//   convenience factory for qt_tree_view.
template <typename _D,
          unsigned _F,
          typename _I,
          typename _Adapter = qt_adapter<_D, _I>>
qt_tree_view<_D, _F, _I, _Adapter> make_qt_tree_view(
    component::tree_view<_D, _F, _I>& model,
    QWidget*                          parent = nullptr)
{
    return qt_tree_view<_D, _F, _I, _Adapter>(model, parent);
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §8  QT MODEL ADAPTER (for QTreeView + QAbstractItemModel)
// ═══════════════════════════════════════════════════════════════════════════════
//   For users who need the full QAbstractItemModel interface (lazy loading,
// proxy models, drag & drop), this section provides the foundation.  A
// complete implementation requires a QObject-derived class, which cannot be
// a template in the Qt MOC system.  The recommended approach:
//
//   1. Create a QObject-derived class in a .cpp file
//   2. Store a void* to the model and use qt_tree_view_model_ops<> for
//      type-safe operations
//   3. Wire rowCount, data, index, parent via the ops struct
//
// This keeps the template header MOC-free.

// qt_tree_view_model_ops
//   struct: stateless helper providing type-safe operations for bridging
// tree_view data into QAbstractItemModel.
template <typename _Data,
          unsigned _Feat,
          typename _Icon,
          typename _Adapter = qt_adapter<_Data, _Icon>>
struct qt_tree_view_model_ops
{
    using node_type = component::tree_node<_Data, _Feat, _Icon>;
    using view_type = component::tree_view<_Data, _Feat, _Icon>;

    // row_count
    //   Number of children of a node (or root count for the invisible root).
    static int row_count(const view_type& view,
                         const node_type* parent)
    {
        if (!parent)
        {
            return static_cast<int>(view.roots.size());
        }

        return static_cast<int>(parent->children.size());
    }

    // column_count
    static int column_count(int configured_cols)
    {
        return configured_cols > 0 ? configured_cols : 1;
    }

    // node_at
    //   Returns the node at (row, parent).
    static node_type* node_at(view_type&       view,
                              int              row,
                              const node_type* parent)
    {
        if (!parent)
        {
            if ( (row < 0) ||
                 (static_cast<std::size_t>(row) >= view.roots.size()) )
            {
                return nullptr;
            }

            return &view.roots[static_cast<std::size_t>(row)];
        }

        if ( (row < 0) ||
             (static_cast<std::size_t>(row) >= parent->children.size()) )
        {
            return nullptr;
        }

        return const_cast<node_type*>(
            &parent->children[static_cast<std::size_t>(row)]);
    }

    // display_text
    //   Returns the display string for column 0.
    static QString display_text(const node_type& node)
    {
        return _Adapter::to_qstring(node.data);
    }

    // decoration
    //   Returns the icon for a node (if icons enabled).
    static QIcon decoration(const node_type& node)
    {
    #if D_ENV_LANG_IS_CPP17_OR_HIGHER
        if constexpr (component::has_feat(_Feat, component::vf_icons))
        {
            return _Adapter::to_qicon(component::effective_icon(node));
        }
        else
        {
            return QIcon();
        }
    #else
        return decoration_dispatch_(node,
            std::integral_constant<bool,
                component::has_feat(_Feat, component::vf_icons)>{});
    #endif
    }

    // check_state_role
    //   Returns the Qt check state for a node (if checkable).
    static QVariant check_state_role(const node_type& node)
    {
    #if D_ENV_LANG_IS_CPP17_OR_HIGHER
        if constexpr (component::has_feat(_Feat, component::vf_checkable))
        {
            switch (node.checked)
            {
                case component::check_state::checked:
                    return Qt::Checked;
                case component::check_state::indeterminate:
                    return Qt::PartiallyChecked;
                default:
                    return Qt::Unchecked;
            }
        }
    #endif

        return QVariant();
    }

    // flags
    //   Returns Qt::ItemFlags for a node.
    static Qt::ItemFlags flags(const node_type& node)
    {
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    #if D_ENV_LANG_IS_CPP17_OR_HIGHER
        if constexpr (component::has_feat(_Feat, component::vf_checkable))
        {
            f |= Qt::ItemIsUserCheckable;
        }
        if constexpr (component::has_feat(_Feat, component::vf_renamable))
        {
            if (node.renamable)
            {
                f |= Qt::ItemIsEditable;
            }
        }
    #endif

        return f;
    }

private:
#if !D_ENV_LANG_IS_CPP17_OR_HIGHER
    static QIcon decoration_dispatch_(const node_type&, std::false_type)
    {
        return QIcon();
    }

    static QIcon decoration_dispatch_(const node_type& node, std::true_type)
    {
        return _Adapter::to_qicon(component::effective_icon(node));
    }
#endif
};


#endif  // D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS


NS_END  // qt
NS_END  // ui
NS_END  // uxoxo

#endif  // UXOXO_UI_QT_TREE_VIEW_
