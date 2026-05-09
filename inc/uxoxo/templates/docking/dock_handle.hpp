/*******************************************************************************
* uxoxo [component]                                              dock_handle.hpp
*
* Type-erased handle to any structurally-conforming dockable:
*   The dock host stores heterogeneous dockable components - a panel, a
* toolbar, a tab control, a tree view - in a single homogeneous
* container.  Because the framework's dockable contract is structural
* (any component carrying current_zone / target_zone / dock_requested
* satisfies is_dockable_v) rather than inheritance-based, the host
* cannot use a virtual base type for storage.  Instead it stores
* dock_handle objects: 16-byte non-owning views that pair an opaque
* pointer to the underlying component with a vtable of free-function
* pointers covering the small surface the host actually needs to read
* and mutate.
*
*   The vtable is statically constructed once per dockable type (on
* demand, deduplicated at link time) by `vtable_for<_T>`, with the
* function pointers built from C++20-conforming captureless lambdas
* that delegate back into the public verbs from dockable_common.hpp.
* This means the handle never bypasses the documented dockable surface
* and any future evolution of the contract that preserves the verbs
* leaves existing handles working.
*
*   Handles are NON-OWNING.  The dockable component must outlive every
* handle that references it.  Destroying a dockable without first
* unregistering its handles from the host is a use-after-free.  The
* host's unregister_dockable verb takes the underlying component
* address, making cleanup straightforward.
*
* Contents:
*   1.  vtable          - per-type function-pointer table
*   2.  vtable_for      - per-type singleton vtable instance
*   3.  dock_handle     - the opaque pointer + vtable handle
*
*
* path:      /inc/uxoxo/templates/component/dockable/dock_handle.hpp
* link(s):   TBA
* author(s): Samuel 'teer' Neal-Blim                        created: 2026.05.07
*******************************************************************************/

#ifndef  UXOXO_DOCK_HANDLE_
#define  UXOXO_DOCK_HANDLE_ 1

// std
#include <type_traits>
// djinterp
#include <djinterp/core/djinterp.hpp>
// uxoxo
#include "../../../uxoxo.hpp"
#include "./dockable_common.hpp"
#include "./dockable_traits.hpp"
#include "./dockable_types.hpp"


NS_UXOXO
NS_COMPONENT


// ===========================================================================
//  1.  VTABLE
// ===========================================================================

NS_INTERNAL

    // dock_handle_vtable
    //   struct: per-type function-pointer table read by every
    // dock_handle method.  Each entry takes an opaque pointer that
    // the trampoline casts back to the concrete dockable type before
    // delegating to the corresponding verb from dockable_common.hpp.
    struct dock_handle_vtable
    {
        DDockZone (*get_current_zone)(const void* _p);
        DDockZone (*get_target_zone) (const void* _p);
        bool      (*is_dock_pending) (const void* _p);
        bool      (*is_popup_open)   (const void* _p);
        void      (*confirm)         (void*       _p);
    };

    // make_dock_handle_vtable
    //   trait: builds the vtable instance for a given dockable type.
    // Each trampoline is a captureless lambda decayed to a function
    // pointer; the popup branch is `if constexpr`-gated so types
    // without popup_open compile clean and report `false`.
    template<typename _Type>
    struct make_dock_handle_vtable
    {
        static constexpr DDockZone get_current_zone(const void* _p)
        {
            return uxoxo::component::get_current_zone(
                       *static_cast<const _Type*>(_p));
        }

        static constexpr DDockZone get_target_zone(const void* _p)
        {
            return uxoxo::component::get_target_zone(
                       *static_cast<const _Type*>(_p));
        }

        static constexpr bool is_dock_pending(const void* _p)
        {
            return uxoxo::component::is_dock_pending(
                       *static_cast<const _Type*>(_p));
        }

        static constexpr bool is_popup_open(const void* _p)
        {
            if constexpr (has_dock_popup_v<_Type>)
            {
                return uxoxo::component::is_dock_popup_open(
                           *static_cast<const _Type*>(_p));
            }
            else
            {
                return false;
            }
        }

        static constexpr void confirm(void* _p)
        {
            uxoxo::component::confirm_dock(*static_cast<_Type*>(_p));

            return;
        }

        static inline const dock_handle_vtable value
        {
            &get_current_zone,
            &get_target_zone,
            &is_dock_pending,
            &is_popup_open,
            &confirm
        };
    };

NS_END  // internal




// ===========================================================================
//  2.  DOCK HANDLE
// ===========================================================================

// dock_handle
//   class: non-owning, type-erased reference to a dockable component.
// Constructed from any component satisfying is_dockable_v; carries a
// pointer to the component plus a pointer to a per-type vtable that
// dispatches read/write operations through the public verbs.
class dock_handle
{
public:
    dock_handle() = default;

    // dock_handle (constructor)
    //   function: builds a handle referencing the supplied dockable.
    // Constrained to types satisfying the dockable contract.  The
    // referenced component must outlive every copy of the handle.
    template<typename _Type,
             std::enable_if_t<
                 ( is_dockable_v<_Type> &&
                   !std::is_same_v<std::decay_t<_Type>, dock_handle> ),
                 int> = 0>
    explicit dock_handle(_Type& _dockable) noexcept
        : m_ptr(static_cast<void*>(&_dockable))
        , m_vt (&internal::make_dock_handle_vtable<_Type>::value)
    {}

    [[nodiscard]] DDockZone
    get_current_zone() const noexcept
    {
        return m_vt->get_current_zone(m_ptr);
    }

    [[nodiscard]] DDockZone
    get_target_zone() const noexcept
    {
        return m_vt->get_target_zone(m_ptr);
    }

    [[nodiscard]] bool
    is_dock_pending() const noexcept
    {
        return m_vt->is_dock_pending(m_ptr);
    }

    [[nodiscard]] bool
    is_popup_open() const noexcept
    {
        return m_vt->is_popup_open(m_ptr);
    }

    void
    confirm()
    {
        m_vt->confirm(m_ptr);

        return;
    }

    [[nodiscard]] void*
    raw() const noexcept
    {
        return m_ptr;
    }

    [[nodiscard]] bool
    valid() const noexcept
    {
        return ( (m_ptr != nullptr) &&
                 (m_vt  != nullptr) );
    }

    // operator==
    //   function: handles compare equal iff they reference the same
    // underlying component.  Vtables are not compared because two
    // handles to the same object always carry the same vtable pointer.
    [[nodiscard]] friend bool
    operator==(const dock_handle& _a,
               const dock_handle& _b) noexcept
    {
        return (_a.m_ptr == _b.m_ptr);
    }

    [[nodiscard]] friend bool
    operator!=(const dock_handle& _a,
               const dock_handle& _b) noexcept
    {
        return !(_a == _b);
    }

private:
    void*                              m_ptr = nullptr;
    const internal::dock_handle_vtable* m_vt = nullptr;
};


NS_END  // component
NS_END  // uxoxo


#endif  // UXOXO_DOCK_HANDLE_
