/*******************************************************************************
* uxoxo [ui][qt]                                             qt_dev_console.hpp
*
*   Qt widget bridge for the framework-agnostic dev_console model.  Provides
* five presentation modes covering every way a console/interpreter prompt
* might appear in an application:
*
*
*    Mode       Description                     Qt container
*
*    panel      Embedded in layout, collapsible  QWidget in parent layout
*    overlay    Quake-style drop-down            QWidget, FramelessWindow
*    toolbar    Compact input strip              QToolBar
*    dock       User-repositionable panel        QDockWidget
*    popup      Context-menulike appearance     QWidget, Popup flags
*
*
*   All modes share a common core:
*     - Output display  (QPlainTextEdit, read-only, colored by entry_kind)
*     - Input line      (QLineEdit with prompt prefix)
*     - Suggestion popup (QListWidget overlay, dcf_autosuggest)
*     - Toggle button   (optional QToolButton for show/hide)
*
*   Configuration is via the console_config struct (always compiles, no Qt).
*   Color formatting is via the console_colors struct (RGB triples  QColor).
*
*   Portability:
*     Qt 5/6 via D_ENV_QT_IS_QT5 / _QT6
*     C++11/14/17 via D_ENV_LANG_IS_CPP*_OR_HIGHER
*
*   Structure:
*     1.   presentation mode enum   (always compiles)
*     2.   color scheme             (always compiles  RGB, not QColor)
*     3.   configuration struct     (always compiles)
*     4.   traits                   (always compiles)
*     5.   qt widget bridge         (guarded by D_ENV_QT_*)
*     6.   convenience factory
*
*
* path:      /inc/uxoxo/ui/qt/qt_dev_console.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                           date: 2026.03.28
*******************************************************************************/

#ifndef  UXOXO_UI_QT_DEV_CONSOLE_
#define  UXOXO_UI_QT_DEV_CONSOLE_ 1

#include <uxoxo>
#include <component/dev_console.hpp>
#include <ui/qt/qt_adapter.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

//  Qt includes (conditional) 
#if ( D_ENV_QT_AVAILABLE &&                                                   \
      D_ENV_QT_HAS_WIDGETS )
    #include <QAction>
    #include <QApplication>
    #include <QBoxLayout>
    #include <QColor>
    #include <QDockWidget>
    #include <QFont>
    #include <QKeySequence>
    #include <QLineEdit>
    #include <QListWidget>
    #include <QMainWindow>
    #include <QPlainTextEdit>
    #include <QShortcut>
    #include <QSplitter>
    #include <QTextCharFormat>
    #include <QToolBar>
    #include <QToolButton>
    #include <QTimer>
    #include <QWidget>
#endif  // ( D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS )


NS_UXOXO
NS_UI
NS_QT

//  1.  PRESENTATION MODE  (always compiles)

// console_mode
//   enum: presentation mode for the dev console widget.
enum class console_mode : std::uint8_t
{
    panel,          // embedded in parent layout, vertical collapse
    overlay,        // frameless drop-down over content (Quake-style)
    toolbar,        // compact horizontal strip, expandable
    dock,           // QDockWidget  user can reposition, float, close
    popup           // appears at a position, disappears on blur
};

// dock_area
//   Maps to Qt::DockWidgetArea but without requiring Qt headers.
enum class dock_area : std::uint8_t
{
    bottom = 0,
    top,
    left,
    right
};

// overlay_edge
//   Which screen edge the overlay slides from.
enum class overlay_edge : std::uint8_t
{
    top = 0,
    bottom
};


/*****************************************************************************/

// 
//  2  COLOR SCHEME  (always compiles  RGB, not QColor)
// 

// rgb
//   struct: simple 8-bit RGB colour triple.
struct rgb { std::uint8_t r, g, b; };

// console_colors
//   struct: colour scheme for console widget elements.
struct console_colors
{
    // entry_kind  text color
    rgb command = { 220, 220, 170 };    // yellowish  user input echo
    rgb output = { 200, 200, 200 };    // light grey  normal output
    rgb error = { 255,  80,  80 };    // red
    rgb warning = { 255, 200,  60 };    // amber
    rgb info = { 100, 180, 255 };    // blue
    rgb debug = { 140, 140, 140 };    // dim grey
    rgb separator = { 80,  80,  80 };    // dark grey

    // UI element colors
    rgb background = { 30,  30,  30 };    // output pane bg
    rgb input_bg = { 40,  40,  40 };    // input line bg
    rgb input_fg = { 230, 230, 230 };    // input line text
    rgb prompt_fg = { 100, 200, 100 };    // prompt prefix
    rgb suggest_bg = { 50,  50,  50 };    // suggestion popup bg
    rgb suggest_sel = { 70,  90, 130 };    // selected suggestion
    rgb suggest_fg = { 200, 200, 200 };    // suggestion text
    rgb badge_fg = { 160, 120, 220 };    // badge text
    rgb timestamp_fg = { 90,  90,  90 };    // timestamp text
    rgb button_fg = { 180, 180, 180 };    // toggle button

    // preset: dark (default)
    static console_colors dark() { return {}; }

    // preset: light
    static console_colors light()
    {
        return {
            { 100, 100,  40 },   // command
            {  40,  40,  40 },   // output
            { 200,  30,  30 },   // error
            { 180, 120,   0 },   // warning
            {  30,  90, 180 },   // info
            { 140, 140, 140 },   // debug
            { 190, 190, 190 },   // separator
            { 245, 245, 245 },   // background
            { 255, 255, 255 },   // input_bg
            {  30,  30,  30 },   // input_fg
            {  40, 140,  40 },   // prompt_fg
            { 235, 235, 235 },   // suggest_bg
            { 200, 210, 230 },   // suggest_sel
            {  40,  40,  40 },   // suggest_fg
            { 120,  80, 180 },   // badge_fg
            { 160, 160, 160 },   // timestamp_fg
            {  80,  80,  80 },   // button_fg
        };
    }

    // preset: solarized dark
    static console_colors solarized()
    {
        return {
            { 181, 137,   0 },   // command (yellow)
            { 131, 148, 150 },   // output (base0)
            { 220,  50,  47 },   // error (red)
            { 203,  75,  22 },   // warning (orange)
            {  38, 139, 210 },   // info (blue)
            {  88, 110, 117 },   // debug (base01)
            {   7,  54,  66 },   // separator (base02)
            {   0,  43,  54 },   // background (base03)
            {   7,  54,  66 },   // input_bg (base02)
            { 253, 246, 227 },   // input_fg (base3)
            { 133, 153,   0 },   // prompt_fg (green)
            {   7,  54,  66 },   // suggest_bg
            {  88, 110, 117 },   // suggest_sel
            { 147, 161, 161 },   // suggest_fg
            { 108, 113, 196 },   // badge_fg (violet)
            {  88, 110, 117 },   // timestamp_fg
            { 131, 148, 150 },   // button_fg
        };
    }
};


/*****************************************************************************/

// 
//  3  CONFIGURATION STRUCT  (always compiles)
// 

// console_config
//   struct: framework-agnostic configuration for the console widget.
struct console_config
{
    //  presentation 
    console_mode   mode = console_mode::panel;
    console_colors colors;

    //  toggle button 
    bool           show_button = true;      // show the toggle button
    std::string    button_text = "Console"; // button label
    std::string    button_icon;             // icon path (empty = text only)

    //  keyboard shortcut 
    std::string    toggle_key = "`";       // tilde key by default
    bool           focus_on_show = true;    // auto-focus input on show

    //  auto-hide 
    bool           auto_hide = false; // hide on blur/Escape
    int            auto_hide_ms = 0;     // 0 = no timer, >0 = ms delay
    bool           hide_on_escape = true;  // Escape closes console
    bool           hide_on_submit = false; // hide after submit (popup mode)

    //  sizing 
    float          height_ratio = 0.35f;    // fraction of parent (overlay)
    int            min_height = 120;      // pixels
    int            max_height = 0;        // 0 = unlimited
    int            toolbar_height = 32;     // toolbar mode input height

    //  overlay 
    overlay_edge   slide_from = overlay_edge::top;
    int            slide_ms = 150;       // animation duration
    float          opacity = 0.95f;     // window opacity (0.01.0)

    //  dock 
    dock_area      dock_pos = dock_area::bottom;
    bool           dock_float = false;     // start floating
    std::string    dock_title = "Console";

    //  font 
    std::string    font_family = "Consolas, 'Courier New', monospace";
    int            font_size = 10;

    //  input 
    std::string    placeholder = "Type a command...";
    int            suggest_popup_max_height = 200;  // pixels

    //  output 
    bool           show_timestamps = false;    // render timestamps
    bool           show_badges = false;    // render badges
    std::string    separator_text = "";
    int            output_max_block_count = 5000;   // QPlainTextEdit limit
};



// 
//  4  CALLBACK TYPES  (always compiles)
// 

// command_handler
//   type: called when the user submits a command.  The handler should execute
// the command and call dc_print / dc_print_error on the model to show results.
template <unsigned _Feat>
using command_handler = std::function<void(
    component::dev_console<_Feat>& /*model*/,
    const std::string& /*command*/
    )>;



// 
//  5  TRAITS  (always compiles)
// 

namespace qt_console_traits {
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

        // has_toggle_method
        //   trait: detects a .toggle() method.
        template <typename,
                  typename = void>
        struct has_toggle_method : std::false_type
        {};

        template <typename _T>
        struct has_toggle_method<_T, std::void_t<
            decltype(std::declval<_T>().toggle())
        >> : std::true_type
        {};

        // has_config_method
        //   trait: detects a .config() method.
        template <typename,
                  typename = void>
        struct has_config_method : std::false_type
        {};

        template <typename _T>
        struct has_config_method<_T, std::void_t<
            decltype(std::declval<const _T>().config())
        >> : std::true_type
        {};
    }

    template <typename _T> inline constexpr bool has_widget_v = detail::has_widget_method<_T>::value;
    template <typename _T> inline constexpr bool has_sync_v   = detail::has_sync_method<_T>::value;
    template <typename _T> inline constexpr bool has_model_v  = detail::has_model_method<_T>::value;
    template <typename _T> inline constexpr bool has_toggle_v = detail::has_toggle_method<_T>::value;
    template <typename _T> inline constexpr bool has_config_v = detail::has_config_method<_T>::value;

    // is_qt_console_bridge
    //   trait: composite detection for the qt_dev_console bridge interface.
    template <typename _Type>
    struct is_qt_console_bridge : std::conjunction<
        detail::has_widget_method<_Type>,
        detail::has_sync_method<_Type>,
        detail::has_model_method<_Type>,
        detail::has_toggle_method<_Type>,
        detail::has_config_method<_Type>
    >
    {};

    template <typename _T>
    inline constexpr bool is_qt_console_bridge_v = is_qt_console_bridge<_T>::value;

}   // namespace qt_console_traits



// 
//  610  QT WIDGET BRIDGE  (only compiles when Qt Widgets are available)
// 

#if D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS

// 
//  6  QT DEV CONSOLE
// 

// qt_dev_console
//   class: Qt widget bridge for the framework-agnostic dev_console model.
template <unsigned _Feat = component::dcf_none>
class qt_dev_console
{
public:
    using model_type   = component::dev_console<_Feat>;
    using entry_type   = component::log_entry<_Feat>;
    using handler_type = command_handler<_Feat>;

    static constexpr bool feat_history     = component::has_dcf(_Feat, component::dcf_history);
    static constexpr bool feat_autosuggest = component::has_dcf(_Feat, component::dcf_autosuggest);
    static constexpr bool feat_timestamps  = component::has_dcf(_Feat, component::dcf_timestamps);
    static constexpr bool feat_log_levels  = component::has_dcf(_Feat, component::dcf_log_levels);
    static constexpr bool feat_badges      = component::has_dcf(_Feat, component::dcf_badges);

    //  construction 

    qt_dev_console(
            model_type&    _model,
            QWidget*       _parent,
            console_config _cfg = {}
        )
            : m_model(&_model),
              m_cfg(std::move(_cfg)),
              m_parent(_parent)
        {
            build_();
        }

    //  access 

    QWidget* widget() { return m_container; }
    const QWidget* widget() const { return m_container; }
    model_type& model() { return *m_model; }
    const model_type& model()  const { return *m_model; }
    const console_config& config() const { return m_cfg; }

    //  callback wiring 

    void set_command_handler(handler_type handler)
    {
        m_handler = std::move(handler);

        return;
    }

    //  visibility 

    void show()
    {
        if (!m_container)
        {
            return;
        }

        m_model->visible = true;

        if ( (m_cfg.mode == console_mode::dock) &&
             (m_dock_widget) )
        {
            m_dock_widget->show();
            m_dock_widget->raise();
        }
        else
        {
            m_container->show();
            m_container->raise();
        }

        if (m_cfg.focus_on_show && m_input_line)
        {
            m_input_line->setFocus();
        }

        start_auto_hide_timer_();

        return;
    }

    void hide()
    {
        if (!m_container)
        {
            return;
        }

        m_model->visible = false;

        if ( (m_cfg.mode == console_mode::dock) &&
             (m_dock_widget) )
        {
            m_dock_widget->hide();
        }
        else
        {
            m_container->hide();
        }

        return;
    }

    void toggle()
    {
        if (m_model->visible)
        {
            hide();
        }
        else
        {
            show();
        }

        return;
    }

    [[nodiscard]] bool is_visible() const { return m_model->visible; }

    //  sync: model  widget 

    // sync
    //   Full rebuild of the output display from the model.
    void sync()
    {
        if (!m_output)
        {
            return;
        }

        m_output->blockSignals(true);
        m_output->clear();

        auto vis = m_model->visible_entries();
        for (auto idx : vis)
        {
            append_entry_(m_model->log[idx]);
        }

        m_output->blockSignals(false);

        if (m_model->auto_scroll)
        {
            m_output->verticalScrollBar()->setValue(
                m_output->verticalScrollBar()->maximum());
        }

        // sync input
        if ( (m_input_line) &&
             (m_input_line->text().toStdString() != m_model->input) )
        {
            m_input_line->blockSignals(true);
            m_input_line->setText(
                QString::fromStdString(m_model->input));
            m_input_line->blockSignals(false);
        }

        return;
    }

    // sync_append
    //   Appends only the latest entry (faster than full sync).
    void sync_append()
    {
        if ( (!m_output) ||
             (m_model->log.empty()) )
        {
            return;
        }

        auto& entry = m_model->log.back();

        // check filter
        if constexpr (feat_log_levels)
        {
            if ( (entry.kind != component::entry_kind::command) &&
                 (entry.kind != component::entry_kind::separator) )
            {
                auto lvl = component::entry_kind_to_level(entry.kind);
                if (static_cast<int>(lvl) <
                    static_cast<int>(m_model->active_level))
                {
                    return;
                }
            }
        }

        append_entry_(entry);

        if (m_model->auto_scroll)
        {
            m_output->verticalScrollBar()->setValue(
                m_output->verticalScrollBar()->maximum());
        }

        return;
    }

    //  suggestion popup 

    void show_suggestions()
    {
        if constexpr (feat_autosuggest)
        {
            if ( (!m_suggest_list) ||
                 (m_model->suggestions.empty()) )
            {
                return;
            }

            m_suggest_list->clear();
            for (const auto& s : m_model->suggestions)
            {
                m_suggest_list->addItem(
                    QString::fromStdString(s));
            }

            if (m_model->suggest_selected <
                static_cast<std::size_t>(m_suggest_list->count()))
            {
                m_suggest_list->setCurrentRow(
                    static_cast<int>(m_model->suggest_selected));
            }

            position_suggest_popup_();
            m_suggest_list->show();
        }

        return;
    }

    void hide_suggestions()
    {
        if constexpr (feat_autosuggest)
        {
            if (m_suggest_list)
            {
                m_suggest_list->hide();
            }
        }

        return;
    }

    //  log level indicator 

    void update_level_indicator()
    {
        if constexpr (feat_log_levels)
        {
            if (!m_level_button)
            {
                return;
            }

            const char* labels[] = {
                "ALL", "DBG", "INF", "WRN", "ERR", "---"
            };
            int idx = static_cast<int>(m_model->active_level);
            if (idx > 4)
            {
                idx = 5;
            }

            m_level_button->setText(labels[idx]);
        }

        return;
    }

    //  toolbar mode: add custom actions 

    QToolButton* add_tool_button(const std::string&    text,
                                 std::function<void()> callback)
    {
        if (!m_toolbar_area)
        {
            return nullptr;
        }

        auto* btn = new QToolButton(m_toolbar_area);
        btn->setText(QString::fromStdString(text));
        m_toolbar_area->layout()->addWidget(btn);
        if (callback)
        {
            QObject::connect(btn, &QToolButton::clicked,
                [cb = std::move(callback)]() { cb(); });
        }

        return btn;
    }

    //  toggle button access 

    QToolButton* toggle_button() { return m_toggle_btn; }

private:

    // 
    //  7  WIDGET CONSTRUCTION
    // 

    void build_()
    {
        //  core widgets (shared by all modes) 
        build_output_();
        build_input_();

        if constexpr (feat_autosuggest)
        {
            build_suggest_popup_();
        }

        //  mode-specific container 
        switch (m_cfg.mode)
        {
            case console_mode::panel:   { build_panel_();   break; }
            case console_mode::overlay: { build_overlay_(); break; }
            case console_mode::toolbar: { build_toolbar_(); break; }
            case console_mode::dock:    { build_dock_();    break; }
            case console_mode::popup:   { build_popup_();   break; }
        }

        //  toggle button 
        if (m_cfg.show_button)
        {
            build_toggle_button_();
        }

        //  keyboard shortcut 
        if ( (!m_cfg.toggle_key.empty()) &&
             (m_parent) )
        {
            auto* shortcut = new QShortcut(
                QKeySequence(QString::fromStdString(m_cfg.toggle_key)),
                m_parent);
            QObject::connect(shortcut, &QShortcut::activated,
                [this]() { toggle(); });
        }

        //  initial visibility 
        if (!m_model->visible)
        {
            hide();
        }

        return;
    }

    void build_output_()
    {
        m_output = new QPlainTextEdit();
        m_output->setReadOnly(true);
        m_output->setMaximumBlockCount(m_cfg.output_max_block_count);

        // font
        QFont font(QString::fromStdString(m_cfg.font_family));
        font.setPointSize(m_cfg.font_size);
        font.setStyleHint(QFont::Monospace);
        m_output->setFont(font);

        // colors
        auto pal = m_output->palette();
        pal.setColor(QPalette::Base, to_qcolor_(m_cfg.colors.background));
        pal.setColor(QPalette::Text, to_qcolor_(m_cfg.colors.output));
        m_output->setPalette(pal);

        m_output->setFrameStyle(0);

        return;
    }

    void build_input_()
    {
        m_input_line = new QLineEdit();
        m_input_line->setPlaceholderText(
            QString::fromStdString(m_cfg.placeholder));

        QFont font(QString::fromStdString(m_cfg.font_family));
        font.setPointSize(m_cfg.font_size);
        font.setStyleHint(QFont::Monospace);
        m_input_line->setFont(font);

        // colors
        auto pal = m_input_line->palette();
        pal.setColor(QPalette::Base, to_qcolor_(m_cfg.colors.input_bg));
        pal.setColor(QPalette::Text, to_qcolor_(m_cfg.colors.input_fg));
        m_input_line->setPalette(pal);

        //  signal wiring 

        // enter → submit
        QObject::connect(m_input_line, &QLineEdit::returnPressed,
            [this]()
            {
                // accept suggestion if visible
                if constexpr (feat_autosuggest)
                {
                    if (m_model->suggest_visible)
                    {
                        component::dc_accept_suggestion(*m_model);
                        m_input_line->setText(
                            QString::fromStdString(m_model->input));
                        hide_suggestions();

                        return;
                    }
                }

                m_model->input = m_input_line->text().toStdString();
                m_model->input_cursor = m_model->input.size();
                auto cmd = component::dc_submit(*m_model);

                if (!cmd.empty())
                {
                    sync_append();  // show command echo

                    if (m_handler)
                    {
                        m_handler(*m_model, cmd);
                        sync();     // show results
                    }

                    m_input_line->clear();

                    if (m_cfg.hide_on_submit)
                    {
                        hide();
                    }
                }

                start_auto_hide_timer_();
            });

        // text changed → update suggestions
        QObject::connect(m_input_line, &QLineEdit::textChanged,
            [this](const QString& text)
            {
                m_model->input = text.toStdString();
                m_model->input_cursor =
                    static_cast<std::size_t>(m_input_line->cursorPosition());

                if constexpr (feat_autosuggest)
                {
                    component::dc_update_suggestions(*m_model);
                    if (m_model->suggest_visible)
                    {
                        show_suggestions();
                    }
                    else
                    {
                        hide_suggestions();
                    }
                }

                start_auto_hide_timer_();
            });

        // install event filter for special keys
        m_input_line->installEventFilter(
            new console_key_filter_(this, m_input_line));

        return;
    }

    void build_suggest_popup_()
    {
        m_suggest_list = new QListWidget(m_parent);
        m_suggest_list->setWindowFlags(
            Qt::ToolTip | Qt::FramelessWindowHint);
        m_suggest_list->setMaximumHeight(
            m_cfg.suggest_popup_max_height);

        auto pal = m_suggest_list->palette();
        pal.setColor(QPalette::Base, to_qcolor_(m_cfg.colors.suggest_bg));
        pal.setColor(QPalette::Text, to_qcolor_(m_cfg.colors.suggest_fg));
        pal.setColor(QPalette::Highlight,
            to_qcolor_(m_cfg.colors.suggest_sel));
        m_suggest_list->setPalette(pal);
        m_suggest_list->hide();

        // click to accept
        QObject::connect(m_suggest_list,
            &QListWidget::itemClicked,
            [this](QListWidgetItem* item)
            {
                m_model->input = item->text().toStdString();
                m_model->input_cursor = m_model->input.size();
                m_input_line->setText(item->text());
                hide_suggestions();
                m_input_line->setFocus();
            });

        return;
    }

    //  mode-specific builders 

    void build_panel_()
    {
        m_container = new QWidget(m_parent);
        auto* layout = new QVBoxLayout(m_container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(m_output, 1);

        auto* input_row = build_input_row_();
        layout->addWidget(input_row);

        if (m_cfg.min_height > 0)
        {
            m_container->setMinimumHeight(m_cfg.min_height);
        }

        if (m_cfg.max_height > 0)
        {
            m_container->setMaximumHeight(m_cfg.max_height);
        }

        return;
    }

    void build_overlay_()
    {
        m_container = new QWidget(m_parent,
            Qt::FramelessWindowHint | Qt::Tool);
        m_container->setWindowOpacity(m_cfg.opacity);

        auto* layout = new QVBoxLayout(m_container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        if (m_cfg.slide_from == overlay_edge::top)
        {
            layout->addWidget(m_output, 1);
            layout->addWidget(build_input_row_());
        }
        else
        {
            layout->addWidget(build_input_row_());
            layout->addWidget(m_output, 1);
        }

        return;
    }

    void build_toolbar_()
    {
        m_container = new QWidget(m_parent);
        m_toolbar_area = m_container;

        auto* layout = new QHBoxLayout(m_container);
        layout->setContentsMargins(2, 0, 2, 0);
        layout->setSpacing(4);

        // prompt label
        auto* prompt = new QToolButton(m_container);
        prompt->setText(
            QString::fromStdString(m_model->prompt));
        prompt->setEnabled(false);
        layout->addWidget(prompt);

        // input line (dominant)
        m_input_line->setFixedHeight(m_cfg.toolbar_height);
        layout->addWidget(m_input_line, 1);

        // log level button
        if constexpr (feat_log_levels)
        {
            m_level_button = new QToolButton(m_container);
            m_level_button->setToolTip("Log level filter");
            update_level_indicator();
            QObject::connect(m_level_button, &QToolButton::clicked,
                [this]()
                {
                    component::dc_cycle_log_level(*m_model);
                    update_level_indicator();
                    sync();
                });
            layout->addWidget(m_level_button);
        }

        m_container->setFixedHeight(m_cfg.toolbar_height + 4);

        return;
    }

    void build_dock_()
    {
        // build a panel first
        build_panel_();

        // wrap in QDockWidget
        if (auto* mw = qobject_cast<QMainWindow*>(m_parent))
        {
            m_dock_widget = new QDockWidget(
                QString::fromStdString(m_cfg.dock_title), mw);
            m_dock_widget->setWidget(m_container);

            Qt::DockWidgetArea area;
            switch (m_cfg.dock_pos)
            {
                case dock_area::top:    { area = Qt::TopDockWidgetArea;    break; }
                case dock_area::left:   { area = Qt::LeftDockWidgetArea;   break; }
                case dock_area::right:  { area = Qt::RightDockWidgetArea;  break; }
                default:                { area = Qt::BottomDockWidgetArea; break; }
            }

            mw->addDockWidget(area, m_dock_widget);

            if (m_cfg.dock_float)
            {
                m_dock_widget->setFloating(true);
            }
        }

        return;
    }

    void build_popup_()
    {
        m_container = new QWidget(m_parent,
            Qt::Popup | Qt::FramelessWindowHint);
        m_container->setWindowOpacity(m_cfg.opacity);

        auto* layout = new QVBoxLayout(m_container);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(2);
        layout->addWidget(m_output, 1);
        layout->addWidget(build_input_row_());

        m_container->resize(
            m_parent ? m_parent->width() / 2 : 400,
            m_cfg.min_height > 0 ? m_cfg.min_height : 300);

        return;
    }

    //  shared input row builder 

    QWidget* build_input_row_()
    {
        auto* row = new QWidget();
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // prompt label
        if (!m_model->prompt.empty())
        {
            auto* prompt = new QLineEdit(row);
            prompt->setText(
                QString::fromStdString(m_model->prompt));
            prompt->setReadOnly(true);
            prompt->setFixedWidth(
                prompt->fontMetrics().horizontalAdvance(
                    prompt->text()) + 8);
            prompt->setFrame(false);
            auto pal = prompt->palette();
            pal.setColor(QPalette::Base,
                to_qcolor_(m_cfg.colors.input_bg));
            pal.setColor(QPalette::Text,
                to_qcolor_(m_cfg.colors.prompt_fg));
            prompt->setPalette(pal);
            prompt->setFont(m_input_line->font());
            layout->addWidget(prompt);
        }

        layout->addWidget(m_input_line, 1);

        // log level button (non-toolbar modes)
        if constexpr (feat_log_levels)
        {
            if (m_cfg.mode != console_mode::toolbar)
            {
                m_level_button = new QToolButton(row);
                m_level_button->setToolTip("Log level filter");
                update_level_indicator();
                QObject::connect(m_level_button, &QToolButton::clicked,
                    [this]()
                    {
                        component::dc_cycle_log_level(*m_model);
                        update_level_indicator();
                        sync();
                    });
                layout->addWidget(m_level_button);
            }
        }

        return row;
    }

    void build_toggle_button_()
    {
        m_toggle_btn = new QToolButton(m_parent);
        if (m_cfg.button_text.empty())
        {
            m_toggle_btn->setText("");
        }
        else
        {
            m_toggle_btn->setText(
                QString::fromStdString(m_cfg.button_text));
        }

        if (!m_cfg.button_icon.empty())
        {
            m_toggle_btn->setIcon(
                QIcon(QString::fromStdString(m_cfg.button_icon)));
        }

        QObject::connect(m_toggle_btn, &QToolButton::clicked,
            [this]() { toggle(); });

        return;
    }

    // 
    //  8  OUTPUT FORMATTING
    // 

    void append_entry_(const entry_type& entry)
    {
        if (entry.kind == component::entry_kind::separator)
        {
            QTextCharFormat fmt;
            fmt.setForeground(to_qcolor_(m_cfg.colors.separator));
            append_formatted_(m_cfg.separator_text, fmt);

            return;
        }

        // build prefix
        std::string prefix;

        // timestamp
        if constexpr (feat_timestamps)
        {
            if (m_cfg.show_timestamps)
            {
                auto elapsed = std::chrono::duration_cast<
                    std::chrono::seconds>(
                        entry.timestamp - m_start_time).count();
                auto min = elapsed / 60;
                auto sec = elapsed % 60;
                char buf[16];
                std::snprintf(buf, sizeof(buf),
                    "[%02ld:%02ld] ", min, sec);
                prefix += buf;
            }
        }

        // badge
        if constexpr (feat_badges)
        {
            if ( (m_cfg.show_badges) &&
                 (!entry.badge.empty()) )
            {
                prefix += "[" + entry.badge + "] ";
            }
        }

        QTextCharFormat fmt;
        fmt.setForeground(color_for_kind_(entry.kind));
        append_formatted_(prefix + entry.text, fmt);

        return;
    }

    void append_formatted_(const std::string&      text,
                           const QTextCharFormat& fmt)
    {
        auto cursor = m_output->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QString::fromStdString(text) + "\n", fmt);

        return;
    }

    QColor color_for_kind_(component::entry_kind kind) const
    {
        switch (kind)
        {
            case component::entry_kind::command:  return to_qcolor_(m_cfg.colors.command);
            case component::entry_kind::error:    return to_qcolor_(m_cfg.colors.error);
            case component::entry_kind::warning:  return to_qcolor_(m_cfg.colors.warning);
            case component::entry_kind::info:     return to_qcolor_(m_cfg.colors.info);
            case component::entry_kind::debug:    return to_qcolor_(m_cfg.colors.debug);
            default:                              return to_qcolor_(m_cfg.colors.output);
        }
    }

    // 
    //  9  KEY FILTER  (handles Up/Down/Tab/Escape in input line)
    // 

    // console_key_filter_
    //   class: event filter handling Up/Down/Tab/Escape in the input line.
    class console_key_filter_ : public QObject
    {
    public:
        console_key_filter_(
                qt_dev_console* _console,
                QObject*        _parent
            )
                : QObject(_parent),
                  m_console(_console)
            {}

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override
        {
            if (event->type() != QEvent::KeyPress)
            {
                return QObject::eventFilter(obj, event);
            }

            auto* key = static_cast<QKeyEvent*>(event);

            switch (key->key())
            {
            case Qt::Key_Up:
            {
                if constexpr (feat_autosuggest)
                {
                    if (m_console->m_model->suggest_visible)
                    {
                        component::dc_suggest_prev(*m_console->m_model);
                        m_console->show_suggestions();

                        return true;
                    }
                }
                if constexpr (feat_history)
                {
                    if (component::dc_history_prev(*m_console->m_model))
                    {
                        m_console->m_input_line->setText(
                            QString::fromStdString(
                                m_console->m_model->input));

                        return true;
                    }
                }

                return true;
            }

            case Qt::Key_Down:
            {
                if constexpr (feat_autosuggest)
                {
                    if (m_console->m_model->suggest_visible)
                    {
                        component::dc_suggest_next(*m_console->m_model);
                        m_console->show_suggestions();

                        return true;
                    }
                }
                if constexpr (feat_history)
                {
                    if (component::dc_history_next(*m_console->m_model))
                    {
                        m_console->m_input_line->setText(
                            QString::fromStdString(
                                m_console->m_model->input));

                        return true;
                    }
                }

                return true;
            }

            case Qt::Key_Tab:
            {
                if constexpr (feat_autosuggest)
                {
                    if (m_console->m_model->suggest_visible)
                    {
                        component::dc_accept_suggestion(
                            *m_console->m_model);
                        m_console->m_input_line->setText(
                            QString::fromStdString(
                                m_console->m_model->input));
                        m_console->hide_suggestions();
                    }
                    else
                    {
                        component::dc_update_suggestions(
                            *m_console->m_model);
                        if (m_console->m_model->suggest_visible)
                        {
                            m_console->show_suggestions();
                        }
                    }

                    return true;
                }
                break;
            }

            case Qt::Key_Escape:
            {
                if constexpr (feat_autosuggest)
                {
                    if (m_console->m_model->suggest_visible)
                    {
                        component::dc_dismiss_suggestions(
                            *m_console->m_model);
                        m_console->hide_suggestions();

                        return true;
                    }
                }
                if (m_console->m_cfg.hide_on_escape)
                {
                    m_console->hide();

                    return true;
                }
                break;
            }

            case Qt::Key_PageUp:
            {
                component::dc_page_up(*m_console->m_model);
                m_console->sync();

                return true;
            }

            case Qt::Key_PageDown:
            {
                component::dc_page_down(*m_console->m_model);
                m_console->sync();

                return true;
            }
            }

            return QObject::eventFilter(obj, event);
        }

    private:
        qt_dev_console* m_console;
    };

    // 
    //  10  HELPERS
    // 

    static QColor to_qcolor_(const rgb& c)
    {
        return QColor(c.r, c.g, c.b);
    }

    void position_suggest_popup_()
    {
        if ( (!m_suggest_list) ||
             (!m_input_line) )
        {
            return;
        }

        auto pos = m_input_line->mapToGlobal(QPoint(0, 0));
        m_suggest_list->move(pos.x(), pos.y() - m_suggest_list->height());
        m_suggest_list->setFixedWidth(m_input_line->width());

        return;
    }

    void start_auto_hide_timer_()
    {
        if ( (!m_cfg.auto_hide) ||
             (m_cfg.auto_hide_ms <= 0) )
        {
            return;
        }

        if (!m_auto_hide_timer)
        {
            m_auto_hide_timer = new QTimer(m_container);
            m_auto_hide_timer->setSingleShot(true);
            QObject::connect(m_auto_hide_timer, &QTimer::timeout,
                [this]() { hide(); });
        }

        m_auto_hide_timer->start(m_cfg.auto_hide_ms);

        return;
    }

    //  members 

    model_type* m_model = nullptr;
    console_config    m_cfg;
    QWidget* m_parent = nullptr;

    // core widgets
    QWidget* m_container = nullptr;
    QPlainTextEdit* m_output = nullptr;
    QLineEdit* m_input_line = nullptr;
    QListWidget* m_suggest_list = nullptr;
    QToolButton* m_toggle_btn = nullptr;
    QToolButton* m_level_button = nullptr;
    QWidget* m_toolbar_area = nullptr;

    // dock mode
    QDockWidget* m_dock_widget = nullptr;

    // auto-hide
    QTimer* m_auto_hide_timer = nullptr;

    // callback
    handler_type      m_handler;

    // timestamp baseline
    std::chrono::steady_clock::time_point m_start_time =
        std::chrono::steady_clock::now();
};


/*****************************************************************************/

// 
//  11  CONVENIENCE FACTORY
// 

// make_qt_console
//   convenience factory for a general-purpose console.
template <unsigned _F>
qt_dev_console<_F> make_qt_console(
    component::dev_console<_F>& model,
    QWidget*                    parent,
    console_config              cfg = {})
{
    return qt_dev_console<_F>(model, parent, std::move(cfg));
}

// make_qt_game_console
//   convenience factory for a Quake-style overlay console.
template <unsigned _F>
qt_dev_console<_F> make_qt_game_console(
    component::dev_console<_F>& model,
    QWidget*                    parent,
    const std::string&          toggle_key = "`")
{
    console_config cfg;
    cfg.mode = console_mode::overlay;
    cfg.toggle_key = toggle_key;
    cfg.show_button = false;
    cfg.hide_on_escape = true;
    cfg.auto_hide = false;
    cfg.opacity = 0.92f;
    cfg.height_ratio = 0.4f;
    cfg.slide_from = overlay_edge::top;

    return qt_dev_console<_F>(model, parent, std::move(cfg));
}

// make_qt_toolbar_console
//   convenience factory for a compact toolbar-mode console.
template <unsigned _F>
qt_dev_console<_F> make_qt_toolbar_console(
    component::dev_console<_F>& model,
    QWidget*                    parent)
{
    console_config cfg;
    cfg.mode = console_mode::toolbar;
    cfg.show_button = false;
    cfg.toggle_key.clear();
    cfg.hide_on_escape = false;

    return qt_dev_console<_F>(model, parent, std::move(cfg));
}

// make_qt_dock_console
//   convenience factory for a dockable console panel.
template <unsigned _F>
qt_dev_console<_F> make_qt_dock_console(
    component::dev_console<_F>& model,
    QMainWindow*                parent,
    dock_area                   pos = dock_area::bottom)
{
    console_config cfg;
    cfg.mode = console_mode::dock;
    cfg.dock_pos = pos;
    cfg.show_button = false;

    return qt_dev_console<_F>(model, parent, std::move(cfg));
}


#endif  // D_ENV_QT_AVAILABLE && D_ENV_QT_HAS_WIDGETS


NS_END  // qt
NS_END  // ui
NS_END  // uxoxo


#endif  // UXOXO_UI_QT_DEV_CONSOLE_