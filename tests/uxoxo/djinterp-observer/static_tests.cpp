/*******************************************************************************
* djinterp [test]                                              static_tests.cpp
*
*   Compile-time and runtime verification of the observer pattern library.
*
*   §1: sig_decompose
*   §2: delegate traits + behavior
*   §3: event traits + behavior
*   §4: observer traits + behavior
*   §5: connection / scoped_connection lifetime
*   §6: cross-checks (negative trait assertions)
*   §7: _Callable override (std::function in delegate/event)
*   §8: sizeof verification
*
*   Build:  g++ -std=c++17 -I. -o test static_tests.cpp && ./test
*******************************************************************************/

#include <pattern/observer.hpp>

#include <cassert>
#include <cstdio>
#include <functional>
#include <string>

using namespace djinterp::pattern;
namespace ot = djinterp::pattern::observer_traits;


// ═══════════════════════════════════════════════════════════════════════════════
//  §1  SIGNATURE DECOMPOSITION
// ═══════════════════════════════════════════════════════════════════════════════

static_assert(std::is_same_v<sig_decompose<void(int)>::return_type, void>);
static_assert(std::is_same_v<sig_decompose<void(int)>::fn_ptr_type, void(*)(int)>);
static_assert(sig_decompose<void(int)>::arity == 1);

static_assert(std::is_same_v<sig_decompose<int(float, double)>::return_type, int>);
static_assert(sig_decompose<int(float, double)>::arity == 2);

static_assert(std::is_same_v<sig_decompose<void()>::fn_ptr_type, void(*)()>);
static_assert(sig_decompose<void()>::arity == 0);


// ═══════════════════════════════════════════════════════════════════════════════
//  §2  DELEGATE — TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

using d_t = delegate<void(int)>;

// identity
static_assert( ot::is_observable_v<d_t>,                "delegate IS observable");
static_assert( ot::is_delegate_v<d_t>,                  "delegate IS a delegate");
static_assert(!ot::is_event_v<d_t>,                     "delegate is NOT an event");
static_assert(!ot::is_observer_v<d_t>,                  "delegate is NOT an observer");

// capabilities
static_assert( ot::has_connect_v<d_t>,                  "delegate has connect");
static_assert( ot::has_notify_v<d_t>,                   "delegate has notify");
static_assert( ot::has_count_v<d_t>,                    "delegate has count");
static_assert( ot::has_disconnect_all_v<d_t>,           "delegate has disconnect_all");
static_assert( ot::has_disconnect_void_v<d_t>,          "delegate has disconnect()");
static_assert(!ot::has_compact_v<d_t>,                  "delegate does NOT have compact");

// connect returns void
static_assert( ot::connect_returns_void_v<d_t>,         "delegate connect returns void");
static_assert(!ot::connect_returns_slot_id_v<d_t>,      "delegate connect does NOT return slot_id");
static_assert(!ot::connect_returns_connection_v<d_t>,   "delegate connect does NOT return connection");

// bounded
static_assert( ot::is_bounded_v<d_t>,                   "delegate IS bounded");
static_assert(!ot::has_connection_tracking_v<d_t>,      "delegate does NOT have connection tracking");

// aliases
static_assert( ot::has_signature_type_v<d_t>,           "delegate has signature_type");
static_assert( ot::has_callable_type_v<d_t>,            "delegate has callable_type");
static_assert( ot::has_return_type_v<d_t>,              "delegate has return_type");

// capacity
static_assert( d_t::capacity == 1,                      "delegate capacity == 1");

// arity
static_assert( ot::observable_arity_v<d_t> == 1,        "delegate<void(int)> arity == 1");


// ═══════════════════════════════════════════════════════════════════════════════
//  §3  EVENT — TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

using e_t = event<void(int), 4>;

// identity
static_assert( ot::is_observable_v<e_t>,                "event IS observable");
static_assert( ot::is_event_v<e_t>,                     "event IS an event");
static_assert(!ot::is_delegate_v<e_t>,                  "event is NOT a delegate");
static_assert(!ot::is_observer_v<e_t>,                  "event is NOT an observer");

// capabilities
static_assert( ot::has_connect_v<e_t>,                  "event has connect");
static_assert( ot::has_notify_v<e_t>,                   "event has notify");
static_assert( ot::has_count_v<e_t>,                    "event has count");
static_assert( ot::has_disconnect_all_v<e_t>,           "event has disconnect_all");
static_assert( ot::has_disconnect_by_id_v<e_t>,         "event has disconnect(slot_id)");
static_assert( ot::has_compact_v<e_t>,                  "event has compact");

// connect returns slot_id
static_assert( ot::connect_returns_slot_id_v<e_t>,      "event connect returns slot_id");
static_assert(!ot::connect_returns_void_v<e_t>,         "event connect does NOT return void");
static_assert(!ot::connect_returns_connection_v<e_t>,   "event connect does NOT return connection");

// bounded
static_assert( ot::is_bounded_v<e_t>,                   "event IS bounded");
static_assert(!ot::has_connection_tracking_v<e_t>,      "event does NOT have connection tracking");

// capacity
static_assert( e_t::capacity == 4,                      "event<_,4> capacity == 4");

// arity
static_assert( ot::observable_arity_v<e_t> == 1,        "event<void(int),4> arity == 1");


// ═══════════════════════════════════════════════════════════════════════════════
//  §4  OBSERVER — TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

using o_t = observer<void(int)>;

// identity
static_assert( ot::is_observable_v<o_t>,                "observer IS observable");
static_assert( ot::is_observer_v<o_t>,                  "observer IS an observer");
static_assert(!ot::is_delegate_v<o_t>,                  "observer is NOT a delegate");
static_assert(!ot::is_event_v<o_t>,                     "observer is NOT an event");

// capabilities
static_assert( ot::has_connect_v<o_t>,                  "observer has connect");
static_assert( ot::has_notify_v<o_t>,                   "observer has notify");
static_assert( ot::has_count_v<o_t>,                    "observer has count");
static_assert( ot::has_disconnect_all_v<o_t>,           "observer has disconnect_all");
static_assert( ot::has_compact_v<o_t>,                  "observer has compact");

// connect returns connection
static_assert( ot::connect_returns_connection_v<o_t>,   "observer connect returns connection");
static_assert(!ot::connect_returns_void_v<o_t>,         "observer connect does NOT return void");
static_assert(!ot::connect_returns_slot_id_v<o_t>,      "observer connect does NOT return slot_id");

// unbounded
static_assert(!ot::is_bounded_v<o_t>,                   "observer is NOT bounded");
static_assert( ot::has_connection_tracking_v<o_t>,      "observer HAS connection tracking");

// capacity
static_assert( o_t::capacity == 0,                      "observer capacity == 0 (unbounded)");

// arity
static_assert( ot::observable_arity_v<o_t> == 1,        "observer<void(int)> arity == 1");


// ═══════════════════════════════════════════════════════════════════════════════
//  §5  CROSS-CHECKS
// ═══════════════════════════════════════════════════════════════════════════════

// std::string is not observable
static_assert(!ot::is_observable_v<std::string>,   "string is NOT observable");
static_assert(!ot::is_delegate_v<int>,             "int is NOT a delegate");
static_assert(!ot::is_event_v<float>,              "float is NOT an event");
static_assert(!ot::is_observer_v<std::vector<int>>,"vector is NOT an observer");

// multi-arg signatures
using d2 = delegate<int(float, double)>;
static_assert(d2::capacity == 1);
static_assert(ot::observable_arity_v<d2> == 2);
static_assert(std::is_same_v<d2::return_type, int>);

// void() signature
using d0 = delegate<void()>;
static_assert(ot::observable_arity_v<d0> == 0);
static_assert(ot::is_delegate_v<d0>);

// observer_ptr is a delegate
using op = observer_ptr<void(int)>;
static_assert(ot::is_delegate_v<op>,    "observer_ptr IS a delegate");
static_assert(!ot::is_observer_v<op>,   "observer_ptr is NOT an observer");


// ═══════════════════════════════════════════════════════════════════════════════
//  §6  CALLABLE OVERRIDE TRAITS
// ═══════════════════════════════════════════════════════════════════════════════

// delegate with std::function
using d_fn = delegate<void(int), std::function<void(int)>>;
static_assert(ot::is_delegate_v<d_fn>,  "delegate<_,std::function> IS a delegate");
static_assert(ot::is_observable_v<d_fn>);

// event with std::function
using e_fn = event<void(int), 4, std::function<void(int)>>;
static_assert(ot::is_event_v<e_fn>,     "event<_,4,std::function> IS an event");
static_assert(ot::is_observable_v<e_fn>);

// observer with raw fn_ptr
using o_fp = observer<void(int), void(*)(int)>;
static_assert(ot::is_observer_v<o_fp>,  "observer<_,fn_ptr> IS an observer");


/*****************************************************************************/
//  RUNTIME TESTS
/*****************************************************************************/

static int g_acc;

void add_to_acc(int x) { g_acc += x; }
void mul_acc(int x)    { g_acc *= x; }

int main()
{
    std::printf("── delegate ─────────────────────────────────\n");
    {
        delegate<void(int)> d;
        assert(d.count() == 0);
        assert(!d.connected());

        d.connect(add_to_acc);
        assert(d.count() == 1);
        assert(d.connected());

        g_acc = 0;
        d.notify(10);
        assert(g_acc == 10);

        // operator()
        d(5);
        assert(g_acc == 15);

        // connect replaces
        d.connect(mul_acc);
        d.notify(3);
        assert(g_acc == 45);

        // disconnect
        d.disconnect();
        assert(!d.connected());
        d.notify(999);      // no-op
        assert(g_acc == 45);

        std::printf("  ✓ connect, replace, disconnect, notify\n");
    }

    // delegate with return value
    {
        delegate<int(int, int)> d;
        d.connect([](int a, int b) { return a + b; });
        assert(d.notify(3, 4) == 7);
        d.disconnect();
        assert(d.notify(3, 4) == 0);  // returns int{} == 0

        std::printf("  ✓ non-void return type\n");
    }

    std::printf("── event<4> ─────────────────────────────────\n");
    {
        event<void(int), 4> e;
        assert(e.count() == 0);
        assert(e.empty());

        slot_id s0 = e.connect(add_to_acc);
        slot_id s1 = e.connect(mul_acc);
        assert(s0 == 0);
        assert(s1 == 1);
        assert(e.count() == 2);
        assert(!e.full());
        assert(e.slot_connected(s0));

        g_acc = 1;
        e.notify(5);            // add_to_acc(5) → 6, mul_acc(5) → 30
        assert(g_acc == 30);

        // disconnect slot 0
        e.disconnect(s0);
        assert(e.count() == 1);
        assert(!e.slot_connected(s0));

        g_acc = 2;
        e.notify(3);            // mul_acc(3) only → 6
        assert(g_acc == 6);

        // fill remaining
        e.connect(add_to_acc);  // slot 0 (reused)
        e.connect(add_to_acc);  // slot 2
        e.connect(add_to_acc);  // slot 3
        assert(e.full());

        // overflow
        slot_id bad = e.connect(add_to_acc);
        assert(bad == no_slot);

        // disconnect_all
        e.disconnect_all();
        assert(e.empty());

        std::printf("  ✓ connect, disconnect, full, overflow, reuse\n");
    }

    // event compact
    {
        event<void(int), 4> e;
        e.connect(add_to_acc);  // slot 0
        e.connect(mul_acc);     // slot 1
        e.connect(add_to_acc);  // slot 2
        e.disconnect(0);
        e.disconnect(2);        // holes at 0 and 2
        assert(e.count() == 1);

        e.compact();            // mul_acc moves to slot 0
        assert(e.count() == 1);
        assert(e.slot_connected(0));
        assert(!e.slot_connected(1));

        g_acc = 4;
        e.notify(3);
        assert(g_acc == 12);   // mul_acc(3) → 12

        std::printf("  ✓ compact defragments correctly\n");
    }

    std::printf("── observer ─────────────────────────────────\n");
    {
        observer<void(int)> o;
        assert(o.count() == 0);
        assert(o.empty());

        int local = 0;
        auto c1 = o.connect([&](int x) { local += x; });
        auto c2 = o.connect([&](int x) { local *= x; });
        assert(o.count() == 2);
        assert(c1.connected());
        assert(c2.connected());

        o.notify(5);            // local: 0+5=5, 5*5=25
        assert(local == 25);

        // disconnect via handle
        c1.disconnect();
        assert(!c1.connected());
        assert(o.count() == 1);

        local = 3;
        o.notify(4);            // 3*4 = 12
        assert(local == 12);

        // compact
        assert(o.size_including_dead() == 2);
        o.compact();
        assert(o.size_including_dead() == 1);
        assert(o.count() == 1);

        // disconnect_all
        o.disconnect_all();
        assert(o.empty());
        assert(!c2.connected());

        std::printf("  ✓ connect, disconnect, compact, disconnect_all\n");
    }

    // operator+=
    {
        observer<void()> o;
        int x = 0;
        auto c = (o += [&]() { x = 42; });
        o();
        assert(x == 42);
        c.disconnect();

        std::printf("  ✓ operator+= shorthand\n");
    }

    std::printf("── connection lifetime ──────────────────────\n");
    {
        observer<void()> o;
        int x = 0;

        // scoped_connection disconnects on destruction
        {
            scoped_connection sc = o.connect([&]() { x++; });
            o.notify();
            assert(x == 1);
        }
        // sc destroyed → disconnected
        o.notify();
        assert(x == 1);   // not incremented

        std::printf("  ✓ scoped_connection RAII disconnect\n");
    }
    {
        observer<void()> o;
        int x = 0;

        // scoped_connection::release()
        connection c;
        {
            scoped_connection sc = o.connect([&]() { x++; });
            c = sc.release();    // guard surrenders ownership
        }
        // sc destroyed, but slot still live because we released
        o.notify();
        assert(x == 1);
        c.disconnect();
        o.notify();
        assert(x == 1);    // now dead

        std::printf("  ✓ scoped_connection release()\n");
    }
    {
        // scoped_connections (plural)
        observer<void()> o;
        int x = 0;
        {
            scoped_connections sc;
            sc += o.connect([&]() { x += 1; });
            sc += o.connect([&]() { x += 10; });
            sc += o.connect([&]() { x += 100; });
            o();
            assert(x == 111);
        }
        o();
        assert(x == 111);  // all disconnected

        std::printf("  ✓ scoped_connections batch RAII\n");
    }

    std::printf("── callable override ────────────────────────\n");
    {
        // delegate with std::function — supports captures
        int capture = 7;
        delegate<void(int), std::function<void(int)>> d;
        d.connect([&capture](int x) { capture += x; });
        d.notify(3);
        assert(capture == 10);

        std::printf("  ✓ delegate<_, std::function> supports captures\n");
    }
    {
        // event with std::function
        event<void(int), 2, std::function<void(int)>> e;
        int a = 0, b = 0;
        e.connect([&a](int x) { a += x; });
        e.connect([&b](int x) { b += x; });
        e.notify(5);
        assert(a == 5 && b == 5);

        std::printf("  ✓ event<_, 2, std::function> supports captures\n");
    }
    {
        // observer with raw fn_ptr only (no captures allowed)
        observer<void(int), void(*)(int)> o;
        g_acc = 0;
        auto c = o.connect(add_to_acc);
        o.notify(7);
        assert(g_acc == 7);
        c.disconnect();

        std::printf("  ✓ observer<_, fn_ptr> restricts to fn_ptrs\n");
    }

    std::printf("── sizeof verification ──────────────────────\n");
    {
        std::printf("  delegate<void(int)>         : %zu bytes\n", 
            sizeof(delegate<void(int)>));
        std::printf("  event<void(int), 4>         : %zu bytes\n", 
            sizeof(event<void(int), 4>));
        std::printf("  event<void(int), 8>         : %zu bytes\n", 
            sizeof(event<void(int), 8>));
        std::printf("  observer<void(int)>         : %zu bytes\n", 
            sizeof(observer<void(int)>));

        // delegate with fn_ptr should be exactly pointer-sized
        static_assert(sizeof(delegate<void(int)>) == sizeof(void(*)(int)),
            "delegate with fn_ptr should be exactly one pointer");

        std::printf("  ✓ delegate == sizeof(fn_ptr) == %zu bytes\n", 
            sizeof(void(*)(int)));
    }

    std::printf("\n── ALL TESTS PASSED ─────────────────────────\n");
    return 0;
}
