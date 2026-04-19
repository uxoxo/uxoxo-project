/*******************************************************************************
* uxoxo [component]                                    sqlite_login_form.hpp
*
* SQLite database login form:
*   Vendor-specific login form for SQLite connections.  Derives from
* database_login<database_type::sqlite, ...> with no network fields
* and no credentials by default.  The "database name" field serves as
* the file path (or ":memory:" / "" for in-memory / temporary).
*
*   SQLite-specific fields (gated by sqlite_login_feat):
*     slf_open_flags      open flag selector (readonly/readwrite/create/…)
*     slf_journal_mode    journal mode selector (delete/WAL/memory/…)
*     slf_transaction_mode default transaction mode (deferred/immediate/…)
*     slf_busy_timeout    busy timeout in milliseconds
*     slf_page_size       page size in bytes
*     slf_cache_size      cache size (negative = KiB, positive = pages)
*     slf_foreign_keys    enable foreign key enforcement
*     slf_wal             enable WAL mode shortcut
*     slf_shared_cache    shared cache toggle
*     slf_vfs             custom VFS name
*     slf_in_memory       convenience flag for :memory: databases
*
*   The free function sdbl_to_config() extracts a sqlite_connect_config.
*
* Contents:
*   §1  Feature flags (sqlite_login_feat)
*   §2  EBO mixins
*   §3  sqlite_login_form struct
*   §4  Free functions
*   §5  Traits (SFINAE detection)
*
*
* path:      /inc/uxoxo/component/sqlite_login_form.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                      date: 2026.04.10
*******************************************************************************/

#ifndef  UXOXO_COMPONENT_SQLITE_LOGIN_FORM_
#define  UXOXO_COMPONENT_SQLITE_LOGIN_FORM_ 1

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include <uxoxo>
#include <component/view_common.hpp>
#include <component/text_input.hpp>
#include <component/login_form.hpp>
#include <component/database_login.hpp>
#include <database/sqlite/sqlite.hpp>


NS_UXOXO
NS_COMPONENT


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  SQLITE LOGIN FEATURE FLAGS
// ═══════════════════════════════════════════════════════════════════════════════

enum sqlite_login_feat : unsigned
{
    slf_none             = 0,
    slf_open_flags       = 1u << 0,
    slf_journal_mode     = 1u << 1,
    slf_transaction_mode = 1u << 2,
    slf_busy_timeout     = 1u << 3,
    slf_page_size        = 1u << 4,
    slf_cache_size       = 1u << 5,
    slf_foreign_keys     = 1u << 6,
    slf_wal              = 1u << 7,
    slf_shared_cache     = 1u << 8,
    slf_vfs              = 1u << 9,
    slf_in_memory        = 1u << 10,

    // ── presets ──────────────────────────────────────────────────────
    //   minimal: just the file path (from dlf_database_name)
    slf_basic            = slf_journal_mode | slf_foreign_keys,
    //   full tuning
    slf_all              = slf_open_flags    | slf_journal_mode
                         | slf_transaction_mode | slf_busy_timeout
                         | slf_page_size    | slf_cache_size
                         | slf_foreign_keys | slf_wal
                         | slf_shared_cache | slf_vfs
                         | slf_in_memory
};

constexpr unsigned operator|(sqlite_login_feat a,
                             sqlite_login_feat b) noexcept
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

constexpr bool has_slf(unsigned           f,
                       sqlite_login_feat  bit) noexcept
{
    return (f & static_cast<unsigned>(bit)) != 0;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §2  EBO MIXINS
// ═══════════════════════════════════════════════════════════════════════════════

namespace sqlite_login_mixin {

    // ── open flags ───────────────────────────────────────────────────
    template <bool _Enable>
    struct open_flags_data
    {};

    template <>
    struct open_flags_data<true>
    {
        djinterp::db::sqlite::sqlite_open_flag open_flags =
            djinterp::db::sqlite::sqlite_open_flag::read_write;
    };

    // ── journal mode ─────────────────────────────────────────────────
    template <bool _Enable>
    struct journal_mode_data
    {};

    template <>
    struct journal_mode_data<true>
    {
        djinterp::db::sqlite::sqlite_journal_mode journal_mode =
            djinterp::db::sqlite::sqlite_journal_mode::mode_delete;
    };

    // ── transaction mode ─────────────────────────────────────────────
    template <bool _Enable>
    struct transaction_mode_data
    {};

    template <>
    struct transaction_mode_data<true>
    {
        djinterp::db::sqlite::sqlite_transaction_mode
            default_transaction_mode =
                djinterp::db::sqlite::sqlite_transaction_mode::deferred;
    };

    // ── busy timeout ─────────────────────────────────────────────────
    template <bool _Enable>
    struct busy_timeout_data
    {};

    template <>
    struct busy_timeout_data<true>
    {
        int busy_timeout_ms = 5000;
    };

    // ── page size ────────────────────────────────────────────────────
    template <bool _Enable>
    struct page_size_data
    {};

    template <>
    struct page_size_data<true>
    {
        int page_size = 4096;
    };

    // ── cache size ───────────────────────────────────────────────────
    template <bool _Enable>
    struct cache_size_data
    {};

    template <>
    struct cache_size_data<true>
    {
        int cache_size = -2000;     // negative = KiB
    };

    // ── foreign keys ─────────────────────────────────────────────────
    template <bool _Enable>
    struct foreign_keys_data
    {};

    template <>
    struct foreign_keys_data<true>
    {
        bool enable_foreign_keys = true;
    };

    // ── WAL shortcut ─────────────────────────────────────────────────
    template <bool _Enable>
    struct wal_data
    {};

    template <>
    struct wal_data<true>
    {
        bool enable_wal = false;
    };

    // ── shared cache ─────────────────────────────────────────────────
    template <bool _Enable>
    struct shared_cache_data
    {};

    template <>
    struct shared_cache_data<true>
    {
        bool enable_shared_cache = false;
    };

    // ── VFS name ─────────────────────────────────────────────────────
    template <bool _Enable>
    struct vfs_data
    {};

    template <>
    struct vfs_data<true>
    {
        std::string vfs_name;
    };

    // ── in-memory toggle ─────────────────────────────────────────────
    template <bool _Enable>
    struct in_memory_data
    {};

    template <>
    struct in_memory_data<true>
    {
        bool in_memory = false;
    };

}   // namespace sqlite_login_mixin


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §3  SQLITE LOGIN FORM
// ═══════════════════════════════════════════════════════════════════════════════
//   _LoginFeat   login_form_feat — defaults to lf_none (no auth)
//   _DbFeat      database_login_feat — defaults to dlf_embedded (db name only)
//   _SlFeat      sqlite_login_feat — defaults to slf_basic

template <unsigned _LoginFeat = lf_none,
          unsigned _DbFeat    = dlf_embedded,
          unsigned _SlFeat    = slf_basic>
struct sqlite_login_form
    : database_login<djinterp::db::database_type::sqlite,
                     _LoginFeat,
                     _DbFeat>
    , sqlite_login_mixin::open_flags_data       <has_slf(_SlFeat, slf_open_flags)>
    , sqlite_login_mixin::journal_mode_data     <has_slf(_SlFeat, slf_journal_mode)>
    , sqlite_login_mixin::transaction_mode_data <has_slf(_SlFeat, slf_transaction_mode)>
    , sqlite_login_mixin::busy_timeout_data     <has_slf(_SlFeat, slf_busy_timeout)>
    , sqlite_login_mixin::page_size_data        <has_slf(_SlFeat, slf_page_size)>
    , sqlite_login_mixin::cache_size_data       <has_slf(_SlFeat, slf_cache_size)>
    , sqlite_login_mixin::foreign_keys_data     <has_slf(_SlFeat, slf_foreign_keys)>
    , sqlite_login_mixin::wal_data              <has_slf(_SlFeat, slf_wal)>
    , sqlite_login_mixin::shared_cache_data     <has_slf(_SlFeat, slf_shared_cache)>
    , sqlite_login_mixin::vfs_data              <has_slf(_SlFeat, slf_vfs)>
    , sqlite_login_mixin::in_memory_data        <has_slf(_SlFeat, slf_in_memory)>
{
    using base_db_login = database_login<
        djinterp::db::database_type::sqlite,
        _LoginFeat,
        _DbFeat>;

    static constexpr unsigned sl_features          = _SlFeat;

    static constexpr bool has_open_flags           = has_slf(_SlFeat, slf_open_flags);
    static constexpr bool has_journal_mode         = has_slf(_SlFeat, slf_journal_mode);
    static constexpr bool has_transaction_mode     = has_slf(_SlFeat, slf_transaction_mode);
    static constexpr bool has_busy_timeout         = has_slf(_SlFeat, slf_busy_timeout);
    static constexpr bool has_page_size            = has_slf(_SlFeat, slf_page_size);
    static constexpr bool has_cache_size           = has_slf(_SlFeat, slf_cache_size);
    static constexpr bool has_foreign_keys_toggle  = has_slf(_SlFeat, slf_foreign_keys);
    static constexpr bool has_wal_toggle           = has_slf(_SlFeat, slf_wal);
    static constexpr bool has_shared_cache_toggle  = has_slf(_SlFeat, slf_shared_cache);
    static constexpr bool has_vfs                  = has_slf(_SlFeat, slf_vfs);
    static constexpr bool has_in_memory_toggle     = has_slf(_SlFeat, slf_in_memory);

    // ── construction ─────────────────────────────────────────────────
    sqlite_login_form() = default;
};


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §4  FREE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// sdbl_set_open_flags
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_set_open_flags(
    sqlite_login_form<_LF, _DF, _SF>&              sl,
    djinterp::db::sqlite::sqlite_open_flag          flags)
{
    static_assert(has_slf(_SF, slf_open_flags),
                  "requires slf_open_flags");

    sl.open_flags = flags;

    return;
}

// sdbl_set_journal_mode
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_set_journal_mode(
    sqlite_login_form<_LF, _DF, _SF>&              sl,
    djinterp::db::sqlite::sqlite_journal_mode       mode)
{
    static_assert(has_slf(_SF, slf_journal_mode),
                  "requires slf_journal_mode");

    sl.journal_mode = mode;

    return;
}

// sdbl_set_transaction_mode
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_set_transaction_mode(
    sqlite_login_form<_LF, _DF, _SF>&              sl,
    djinterp::db::sqlite::sqlite_transaction_mode   mode)
{
    static_assert(has_slf(_SF, slf_transaction_mode),
                  "requires slf_transaction_mode");

    sl.default_transaction_mode = mode;

    return;
}

// sdbl_set_busy_timeout
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_set_busy_timeout(
    sqlite_login_form<_LF, _DF, _SF>& sl,
    int                                ms)
{
    static_assert(has_slf(_SF, slf_busy_timeout),
                  "requires slf_busy_timeout");

    sl.busy_timeout_ms = ms;

    return;
}

// sdbl_set_page_size
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_set_page_size(
    sqlite_login_form<_LF, _DF, _SF>& sl,
    int                                size)
{
    static_assert(has_slf(_SF, slf_page_size),
                  "requires slf_page_size");

    sl.page_size = size;

    return;
}

// sdbl_set_cache_size
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_set_cache_size(
    sqlite_login_form<_LF, _DF, _SF>& sl,
    int                                size)
{
    static_assert(has_slf(_SF, slf_cache_size),
                  "requires slf_cache_size");

    sl.cache_size = size;

    return;
}

// sdbl_toggle_foreign_keys
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_toggle_foreign_keys(
    sqlite_login_form<_LF, _DF, _SF>& sl)
{
    static_assert(has_slf(_SF, slf_foreign_keys),
                  "requires slf_foreign_keys");

    sl.enable_foreign_keys = !sl.enable_foreign_keys;

    return;
}

// sdbl_toggle_wal
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_toggle_wal(
    sqlite_login_form<_LF, _DF, _SF>& sl)
{
    static_assert(has_slf(_SF, slf_wal), "requires slf_wal");

    sl.enable_wal = !sl.enable_wal;

    return;
}

// sdbl_toggle_shared_cache
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_toggle_shared_cache(
    sqlite_login_form<_LF, _DF, _SF>& sl)
{
    static_assert(has_slf(_SF, slf_shared_cache),
                  "requires slf_shared_cache");

    sl.enable_shared_cache = !sl.enable_shared_cache;

    return;
}

// sdbl_set_in_memory
//   sets the form to in-memory mode, replacing the file path with
// ":memory:" and adjusting open flags if present.
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_set_in_memory(
    sqlite_login_form<_LF, _DF, _SF>& sl,
    bool                               on)
{
    static_assert(has_slf(_SF, slf_in_memory),
                  "requires slf_in_memory");

    sl.in_memory = on;

    if constexpr (has_dlf(_DF, dlf_database_name))
    {
        if (on)
        {
            ti_set_value(sl.database_name, std::string(":memory:"));
        }
    }

    if constexpr (has_slf(_SF, slf_open_flags))
    {
        if (on)
        {
            using F = djinterp::db::sqlite::sqlite_open_flag;

            sl.open_flags = F::readwrite | F::create | F::memory;
        }
    }

    return;
}

// sdbl_set_vfs
template <unsigned _LF, unsigned _DF, unsigned _SF>
void sdbl_set_vfs(
    sqlite_login_form<_LF, _DF, _SF>& sl,
    std::string                        name)
{
    static_assert(has_slf(_SF, slf_vfs), "requires slf_vfs");

    sl.vfs_name = std::move(name);

    return;
}

// sdbl_to_config
//   extracts a sqlite_connect_config from the form.
template <unsigned _LF, unsigned _DF, unsigned _SF>
djinterp::db::sqlite::sqlite_connect_config
sdbl_to_config(const sqlite_login_form<_LF, _DF, _SF>& sl)
{
    djinterp::db::sqlite::sqlite_connect_config cfg;

    // file path from database_name
    if constexpr (has_dlf(_DF, dlf_database_name))
    {
        cfg.file_path = sl.database_name.value;
    }

    if constexpr (has_slf(_SF, slf_open_flags))
    {
        cfg.open_flags = sl.open_flags;
    }

    if constexpr (has_slf(_SF, slf_journal_mode))
    {
        cfg.journal_mode = sl.journal_mode;
    }

    if constexpr (has_slf(_SF, slf_transaction_mode))
    {
        cfg.default_transaction_mode = sl.default_transaction_mode;
    }

    if constexpr (has_slf(_SF, slf_busy_timeout))
    {
        cfg.busy_timeout_ms = sl.busy_timeout_ms;
    }

    if constexpr (has_slf(_SF, slf_page_size))
    {
        cfg.page_size = sl.page_size;
    }

    if constexpr (has_slf(_SF, slf_cache_size))
    {
        cfg.cache_size = sl.cache_size;
    }

    if constexpr (has_slf(_SF, slf_foreign_keys))
    {
        cfg.enable_foreign_keys = sl.enable_foreign_keys;
    }

    if constexpr (has_slf(_SF, slf_wal))
    {
        cfg.enable_wal = sl.enable_wal;
    }

    if constexpr (has_slf(_SF, slf_shared_cache))
    {
        cfg.enable_shared_cache = sl.enable_shared_cache;
    }

    if constexpr (has_slf(_SF, slf_vfs))
    {
        cfg.vfs_name = sl.vfs_name;
    }

    return cfg;
}


/*****************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════════
//  §5  TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

namespace sqlite_login_traits {
namespace detail {

    template <typename, typename = void>
    struct has_open_flags_member : std::false_type {};
    template <typename _T>
    struct has_open_flags_member<_T, std::void_t<
        decltype(std::declval<_T>().open_flags)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_journal_mode_member : std::false_type {};
    template <typename _T>
    struct has_journal_mode_member<_T, std::void_t<
        decltype(std::declval<_T>().journal_mode)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_busy_timeout_member : std::false_type {};
    template <typename _T>
    struct has_busy_timeout_member<_T, std::void_t<
        decltype(std::declval<_T>().busy_timeout_ms)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_foreign_keys_member : std::false_type {};
    template <typename _T>
    struct has_foreign_keys_member<_T, std::void_t<
        decltype(std::declval<_T>().enable_foreign_keys)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_enable_wal_member : std::false_type {};
    template <typename _T>
    struct has_enable_wal_member<_T, std::void_t<
        decltype(std::declval<_T>().enable_wal)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_in_memory_member : std::false_type {};
    template <typename _T>
    struct has_in_memory_member<_T, std::void_t<
        decltype(std::declval<_T>().in_memory)
    >> : std::true_type {};

    template <typename, typename = void>
    struct has_vfs_name_member : std::false_type {};
    template <typename _T>
    struct has_vfs_name_member<_T, std::void_t<
        decltype(std::declval<_T>().vfs_name)
    >> : std::true_type {};

}   // namespace detail

template <typename _T>
inline constexpr bool has_open_flags_v =
    detail::has_open_flags_member<_T>::value;
template <typename _T>
inline constexpr bool has_journal_mode_v =
    detail::has_journal_mode_member<_T>::value;
template <typename _T>
inline constexpr bool has_busy_timeout_v =
    detail::has_busy_timeout_member<_T>::value;
template <typename _T>
inline constexpr bool has_foreign_keys_v =
    detail::has_foreign_keys_member<_T>::value;
template <typename _T>
inline constexpr bool has_enable_wal_v =
    detail::has_enable_wal_member<_T>::value;
template <typename _T>
inline constexpr bool has_in_memory_v =
    detail::has_in_memory_member<_T>::value;
template <typename _T>
inline constexpr bool has_vfs_name_v =
    detail::has_vfs_name_member<_T>::value;

// is_sqlite_login_form
template <typename _Type>
struct is_sqlite_login_form : std::conjunction<
    database_login_traits::is_database_login<_Type>,
    std::bool_constant<_Type::db_type ==
        djinterp::db::database_type::sqlite>
>
{};

template <typename _T>
inline constexpr bool is_sqlite_login_form_v =
    is_sqlite_login_form<_T>::value;

// is_wal_sqlite_login
template <typename _Type>
struct is_wal_sqlite_login : std::conjunction<
    is_sqlite_login_form<_Type>,
    detail::has_enable_wal_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_wal_sqlite_login_v =
    is_wal_sqlite_login<_T>::value;

// is_in_memory_sqlite_login
template <typename _Type>
struct is_in_memory_sqlite_login : std::conjunction<
    is_sqlite_login_form<_Type>,
    detail::has_in_memory_member<_Type>
>
{};

template <typename _T>
inline constexpr bool is_in_memory_sqlite_login_v =
    is_in_memory_sqlite_login<_T>::value;

}   // namespace sqlite_login_traits


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_COMPONENT_SQLITE_LOGIN_FORM_
