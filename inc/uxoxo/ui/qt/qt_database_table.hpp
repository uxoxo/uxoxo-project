/*******************************************************************************
* uxoxo [qt]                                              qt_database_table.hpp
*
*   Qt integration for database-backed tables.  Bridges the framework-agnostic
* database_table_view (uxoxo) and database_table (djinterp) to Qt's model/view
* architecture, producing a ready-to-use widget with:
*
*   - QAbstractTableModel backed by any database_table variant
*   - QTableView with schema-driven column headers
*   - Inline cell editing (gated on mutability and connection)
*   - Column sorting via the view's sort callback
*   - Status bar showing connection, dirty, stale, and dimension info
*   - Refresh / commit actions wired to toolbar, context menu, and shortcuts
*   - Signals for database lifecycle events (refreshed, committed, error)
*   - Timer-driven periodic refresh (mirrors sync_policy::periodic)
*
*   The module is templated on _DbTable — any type satisfying the
* database_table structural interface (rows, cols, cell, get_schema,
* is_connected, refresh, commit).  This includes database_table itself,
* mysql_table, mariadb_table, sqlite_table, and any future vendor subclass.
*
*   LAYER DIAGRAM:
*     qt_database_table_widget<_DbTable>       (this header — QWidget)
*       ├── qt_database_table_model<_DbTable>  (this header — QAbstractTableModel)
*       │     └── database_table_view          (database_table_view.hpp)
*       │           ├── table_view             (table_view.hpp)
*       │           └── database_table_state
*       └── QTableView + QLabel (status)
*
*   USAGE:
*   ```cpp
*   mysql_table<>  tbl(conn, "employees");
*   tbl.fetch_schema();
*   tbl.refresh();
*
*   auto* widget = new qt_database_table_widget<mysql_table<>>(tbl);
*   layout->addWidget(widget);
*   ```
*
*   REQUIRES:
*   - C++17 or later
*   - Qt 5.x or Qt 6.x (QtWidgets module)
*   - database_table_view.hpp, table_view.hpp
*
* 
* path:      /inc/uxoxo/qt/qt_database_table.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                          date: 2026.04.05
*******************************************************************************/

#ifndef UXOXO_QT_DATABASE_TABLE_
#define UXOXO_QT_DATABASE_TABLE_ 1

#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
// Qt
#include <QAbstractTableModel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QShortcut>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
// uxoxo / djinterp
#include "../uxoxo.hpp"
#include "../component/database_table_view.hpp"


NS_UXOXO
NS_QT


// =============================================================================
//  §1  QT TABLE MODEL
// =============================================================================
//   QAbstractTableModel subclass that reads from a database_table through
// the framework-agnostic database_table_view layer.  The model does not
// own the database_table — it holds a reference and a pair of uxoxo view
// structs (table_view + database_table_state).
//
//   Template parameter _DbTable:  any type satisfying the database_table
// structural interface.  The model is instantiated once per concrete
// database_table type, but the Qt view code is identical regardless of
// the underlying vendor.

template<typename _DbTable>
class qt_database_table_model : public QAbstractTableModel
{
public:
    using db_table_type = _DbTable;
    using size_type     = typename _DbTable::size_type;
    using value_type    = typename _DbTable::value_type;

    // -----------------------------------------------------------------
    //  user-supplied formatters (optional overrides)
    // -----------------------------------------------------------------
    using cell_formatter    = std::function<QString(const value_type&)>;
    using cell_parser       = std::function<bool(value_type&, const QString&)>;
    using alignment_mapper  = std::function<Qt::AlignmentFlag(size_type col)>;


    // =================================================================
    //  constructor
    // =================================================================

    // qt_database_table_model(parent, db_table)
    //   constructor: binds the model to a live database_table.
    explicit qt_database_table_model(
            _DbTable*    _db_table,
            QObject*     _parent = nullptr
        )
            : QAbstractTableModel(_parent)
            , m_db_table(_db_table)
    {
        rebind();
    }


    // =================================================================
    //  QAbstractTableModel interface
    // =================================================================

    int rowCount(
            const QModelIndex& _parent = QModelIndex()
        ) const override
    {
        if (_parent.isValid())
        {
            return 0;
        }

        return static_cast<int>(m_view.num_rows);
    }

    int 
    columnCount(
        const QModelIndex& _parent = QModelIndex()
    ) const override
    {
        if (_parent.isValid())
        {
            return 0;
        }

        return static_cast<int>(m_view.num_cols);
    }

    QVariant 
    data(
        const QModelIndex& _index,
        int                _role = Qt::DisplayRole
    ) const override
    {
        if (!_index.isValid())
        {
            return {};
        }

        auto row = static_cast<size_type>(_index.row());
        auto col = static_cast<size_type>(_index.column());

        if (_role == Qt::DisplayRole || _role == Qt::EditRole)
        {
            if (m_formatter)
            {
                return m_formatter(m_db_table->cell(row, col));
            }

            // fall back to the uxoxo view's cell extractor
            return QString::fromStdString(
                component::tv_cell_text(m_view, row, col));
        }

        if (_role == Qt::TextAlignmentRole)
        {
            if (m_alignment_mapper)
            {
                return QVariant(static_cast<int>(
                    m_alignment_mapper(col)));
            }

            return QVariant(static_cast<int>(
                Qt::AlignLeft | Qt::AlignVCenter));
        }

        return {};
    }

    QVariant
    headerData(
        int             _section,
        Qt::Orientation _orientation,
        int             _role = Qt::DisplayRole
    ) const override
    {
        if (_role != Qt::DisplayRole)
        {
            return {};
        }

        if (_orientation == Qt::Horizontal)
        {
            auto col = static_cast<size_type>(_section);

            if (col < m_view.columns.size())
            {
                return QString::fromStdString(
                    m_view.columns[col].name);
            }

            return QString("Col %1").arg(_section);
        }

        // vertical header: row number (1-based)
        return _section + 1;
    }

    Qt::ItemFlags flags(
        const QModelIndex& _index
    ) const override
    {
        auto base = QAbstractTableModel::flags(_index);

        if ( _index.isValid() &&
             component::dtv_is_editable(m_state) )
        {
            base |= Qt::ItemIsEditable;
        }

        return base;
    }

    bool setData(
        const QModelIndex& _index,
        const QVariant&    _value,
        int                _role = Qt::EditRole
    ) override
    {
        if ( (!_index.isValid()) ||
             (_role != Qt::EditRole) )
        {
            return false;
        }

        if (!component::dtv_is_editable(m_state))
        {
            return false;
        }

        auto row = static_cast<size_type>(_index.row());
        auto col = static_cast<size_type>(_index.column());

        // if a custom parser is provided, use it for type-safe conversion
        if (m_parser)
        {
            value_type cell_val = m_db_table->cell(row, col);

            if (!m_parser(cell_val, _value.toString()))
            {
                return false;
            }

            m_db_table->cell(row, col) = cell_val;
            m_state.dirty = true;
        }
        else
        {
            // string-based fallback through the uxoxo view
            if ( m_view.set_cell &&
                 !m_view.set_cell(row, col, _value.toString().toStdString()) )
            {
                return false;
            }
        }

        emit dataChanged(_index, _index, { _role });

        return true;
    }


    // =================================================================
    //  database lifecycle
    // =================================================================

    // refresh
    //   function: refreshes the underlying database_table and resets
    // the model.
    bool refresh()
    {
        beginResetModel();

        bool ok = component::dtv_refresh(m_state);

        if (ok)
        {
            // re-sync view dimensions (already done in the callback,
            // but ensure model consistency)
            m_view.num_rows = m_db_table->rows();
            m_view.num_cols = m_db_table->cols();
        }

        endResetModel();

        return ok;
    }

    // commit
    //   function: commits local modifications to the database.
    bool commit()
    {
        bool ok = component::dtv_commit(m_state);

        if (ok)
        {
            // notify the view that dirty state changed
            emit dataChanged(
                index(0, 0),
                index(rowCount() - 1, columnCount() - 1));
        }

        return ok;
    }

    // rebind
    //   function: re-runs the full binding between the database_table
    // and the uxoxo view structs.  Call after replacing the database_table
    // or after schema changes.
    void rebind()
    {
        beginResetModel();

        component::dtv_bind(m_view, m_state, *m_db_table);

        endResetModel();

        return;
    }

    // sync_state
    //   function: re-reads connection/dirty/stale flags without a full
    // refresh.
    void sync_state()
    {
        component::dtv_sync_state(m_state, *m_db_table);

        return;
    }


    // =================================================================
    //  accessors
    // =================================================================

    // view
    //   function: returns a const reference to the uxoxo table_view.
    const component::table_view& view() const noexcept
    {
        return m_view;
    }

    // state
    //   function: returns a const reference to the database_table_state.
    const component::database_table_state& state() const noexcept
    {
        return m_state;
    }

    // db_table
    //   function: returns a pointer to the underlying database_table.
    _DbTable* db_table() noexcept
    {
        return m_db_table;
    }

    const _DbTable* db_table() const noexcept
    {
        return m_db_table;
    }


    // =================================================================
    //  formatter / parser overrides
    // =================================================================

    // set_formatter
    //   function: sets a custom cell → QString formatter.
    void set_formatter(cell_formatter _fmt)
    {
        m_formatter = std::move(_fmt);

        return;
    }

    // set_parser
    //   function: sets a custom QString → value_type parser for editing.
    void set_parser(cell_parser _parser)
    {
        m_parser = std::move(_parser);

        return;
    }

    // set_alignment_mapper
    //   function: sets a column → Qt::AlignmentFlag mapper.
    void set_alignment_mapper(alignment_mapper _mapper)
    {
        m_alignment_mapper = std::move(_mapper);

        return;
    }


private:
    _DbTable*                           m_db_table;
    component::table_view               m_view;
    component::database_table_state     m_state;

    cell_formatter                      m_formatter;
    cell_parser                         m_parser;
    alignment_mapper                    m_alignment_mapper;
};


/******************************************************************************/

// =============================================================================
//  §2  QT TABLE WIDGET
// =============================================================================
//   Composite widget: QTableView + toolbar + status label.  Wires keyboard
// shortcuts (F5 = refresh, Ctrl+S = commit, Ctrl+C = copy selection) and
// provides a context menu with database operations.
//
//   The widget does not own the database_table — the caller must ensure the
// database_table outlives the widget.

template<typename _DbTable>
class qt_database_table_widget : public QWidget
{
public:
    using model_type = qt_database_table_model<_DbTable>;


    // =================================================================
    //  constructor
    // =================================================================

    explicit qt_database_table_widget(
            _DbTable& _db_table,
            QWidget*  _parent = nullptr
        )
            : QWidget(_parent)
    {
        // ── model ────────────────────────────────────────────────────
        m_model = new model_type(&_db_table, this);

        // ── table view ───────────────────────────────────────────────
        m_table_view = new QTableView(this);
        m_table_view->setModel(m_model);
        m_table_view->setSelectionBehavior(
            QAbstractItemView::SelectRows);
        m_table_view->setSelectionMode(
            QAbstractItemView::ExtendedSelection);
        m_table_view->setAlternatingRowColors(true);
        m_table_view->setSortingEnabled(true);
        m_table_view->setContextMenuPolicy(Qt::CustomContextMenu);

        // horizontal header: stretch last column
        m_table_view->horizontalHeader()->setStretchLastSection(true);

        // ── status label ─────────────────────────────────────────────
        m_status_label = new QLabel(this);
        m_status_label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
        update_status();

        // ── toolbar ──────────────────────────────────────────────────
        m_toolbar = new QToolBar(this);
        m_toolbar->setIconSize(QSize(16, 16));

        m_action_refresh = m_toolbar->addAction(
            tr("Refresh"), this, [this]() { do_refresh(); });
        m_action_refresh->setShortcut(QKeySequence(Qt::Key_F5));

        m_action_commit = m_toolbar->addAction(
            tr("Commit"), this, [this]() { do_commit(); });
        m_action_commit->setShortcut(
            QKeySequence(Qt::CTRL | Qt::Key_S));

        m_toolbar->addSeparator();

        m_action_copy = m_toolbar->addAction(
            tr("Copy"), this, [this]() { do_copy(); });
        m_action_copy->setShortcut(
            QKeySequence(Qt::CTRL | Qt::Key_C));

        // ── layout ───────────────────────────────────────────────────
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        layout->addWidget(m_toolbar);
        layout->addWidget(m_table_view, 1);
        layout->addWidget(m_status_label);
        setLayout(layout);

        // ── context menu ─────────────────────────────────────────────
        connect(m_table_view, &QTableView::customContextMenuRequested,
                this, &qt_database_table_widget::show_context_menu);

        // ── periodic refresh timer ───────────────────────────────────
        m_refresh_timer = new QTimer(this);
        connect(m_refresh_timer, &QTimer::timeout,
                this, [this]() { do_periodic_refresh(); });

        // ── initial state ────────────────────────────────────────────
        update_actions();
    }


    // =================================================================
    //  public interface
    // =================================================================

    // model
    //   function: returns the underlying Qt model.
    model_type* model() noexcept
    {
        return m_model;
    }

    const model_type* model() const noexcept
    {
        return m_model;
    }

    // table_view_widget
    //   function: returns the underlying QTableView.
    QTableView* table_view_widget() noexcept
    {
        return m_table_view;
    }

    // refresh
    //   function: triggers a database refresh.
    bool refresh()
    {
        return do_refresh();
    }

    // commit
    //   function: commits local changes.
    bool commit()
    {
        return do_commit();
    }

    // rebind
    //   function: re-binds after schema or connection changes.
    void rebind()
    {
        m_model->rebind();
        update_status();
        update_actions();

        return;
    }


    // =================================================================
    //  periodic refresh
    // =================================================================

    // set_periodic_refresh
    //   function: enables or disables timer-driven periodic refresh.
    // Mirrors sync_policy::periodic at the widget level.
    void set_periodic_refresh(int _interval_ms)
    {
        if (_interval_ms > 0)
        {
            m_refresh_timer->start(_interval_ms);
        }
        else
        {
            m_refresh_timer->stop();
        }

        return;
    }


    // =================================================================
    //  formatter / parser passthrough
    // =================================================================

    void set_formatter(typename model_type::cell_formatter _fmt)
    {
        m_model->set_formatter(std::move(_fmt));

        return;
    }

    void set_parser(typename model_type::cell_parser _parser)
    {
        m_model->set_parser(std::move(_parser));

        return;
    }

    void set_alignment_mapper(typename model_type::alignment_mapper _mapper)
    {
        m_model->set_alignment_mapper(std::move(_mapper));

        return;
    }


protected:

    // =================================================================
    //  internal operations
    // =================================================================

    bool do_refresh()
    {
        bool ok = m_model->refresh();

        update_status();
        update_actions();

        return ok;
    }

    bool do_commit()
    {
        bool ok = m_model->commit();

        update_status();
        update_actions();

        return ok;
    }

    void do_periodic_refresh()
    {
        m_model->sync_state();

        if (m_model->state().stale)
        {
            do_refresh();
        }

        return;
    }

    void do_copy()
    {
        auto selection = m_table_view->selectionModel()->selectedIndexes();

        if (selection.isEmpty())
        {
            return;
        }

        // sort by row then column
        std::sort(selection.begin(), selection.end(),
            [](const QModelIndex& a, const QModelIndex& b)
            {
                if (a.row() != b.row())
                {
                    return a.row() < b.row();
                }

                return a.column() < b.column();
            });

        QString text;
        int prev_row = selection.first().row();

        for (const auto& idx : selection)
        {
            if (idx.row() != prev_row)
            {
                text += '\n';
                prev_row = idx.row();
            }
            else if (&idx != &selection.first())
            {
                text += '\t';
            }

            text += idx.data().toString();
        }

        QApplication::clipboard()->setText(text);

        return;
    }


    // =================================================================
    //  status and action updates
    // =================================================================

    void update_status()
    {
        auto status = component::dtv_status_text(
            m_model->state(), m_model->view());
        m_status_label->setText(QString::fromStdString(status));

        return;
    }

    void update_actions()
    {
        const auto& state = m_model->state();

        m_action_refresh->setEnabled(state.connected);
        m_action_commit->setEnabled(
            state.connected && state.mutable_table && state.dirty);

        return;
    }

    void show_context_menu(const QPoint& _pos)
    {
        QMenu menu(this);

        menu.addAction(m_action_refresh);
        menu.addAction(m_action_commit);
        menu.addSeparator();
        menu.addAction(m_action_copy);

        // column visibility submenu
        auto* col_menu = menu.addMenu(tr("Columns"));

        for (int c = 0; c < m_model->columnCount(); ++c)
        {
            auto name = m_model->headerData(
                c, Qt::Horizontal).toString();
            auto* act = col_menu->addAction(name);
            act->setCheckable(true);
            act->setChecked(
                !m_table_view->isColumnHidden(c));

            connect(act, &QAction::toggled, this,
                [this, c](bool _visible)
                {
                    m_table_view->setColumnHidden(c, !_visible);
                });
        }

        menu.exec(m_table_view->viewport()->mapToGlobal(_pos));

        return;
    }


    // =================================================================
    //  members
    // =================================================================

    model_type*  m_model        = nullptr;
    QTableView*  m_table_view   = nullptr;
    QLabel*      m_status_label = nullptr;
    QToolBar*    m_toolbar      = nullptr;
    QTimer*      m_refresh_timer = nullptr;

    QAction*     m_action_refresh = nullptr;
    QAction*     m_action_commit  = nullptr;
    QAction*     m_action_copy    = nullptr;
};


NS_END  // qt
NS_END  // uxoxo


#endif  // UXOXO_QT_DATABASE_TABLE_
