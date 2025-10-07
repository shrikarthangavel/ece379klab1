#include <cassert>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include "../bounded_queue.hpp"

using namespace std::chrono_literals;

// Test 1: check if basic FIFO works
static void test_single_thread_fifo() {
    BoundedQueue<int> q(3);
    assert(q.capacity() == 3);
    assert(q.empty());

    q.push(10); q.push(20); q.push(30);
    assert(q.size() == 3);

    int a = q.pop(); int b = q.pop(); int c = q.pop();
    assert(a == 10 && b == 20 && c == 30);
    assert(q.empty());

    q.close();
    try { q.pop(); assert(false && "pop after close+empty must throw"); }
    catch (const queue_closed&) {}
    try { q.push(42); assert(false && "push after close must throw"); }
    catch (const queue_closed&) {}
}

// Test 2: Validates graceful shutdown: after close()
static void test_spsc_drain_then_close() {
    BoundedQueue<int> q(2);

    std::thread producer([&]{ q.push(1); q.push(2); q.push(3); q.close(); });

    std::vector<int> got;
    std::thread consumer([&]{
        try { while (true) got.push_back(q.pop()); }
        catch (const queue_closed&) {}
    });

    producer.join(); consumer.join();
    assert((got.size() == 3) && got[0] == 1 && got[1] == 2 && got[2] == 3);

    try { (void)q.pop(); assert(false); } catch (const queue_closed&) {}
    try { q.push(7);     assert(false); } catch (const queue_closed&) {}
}

// Test 3: Ensures the queue provides real backpressure
static void test_backpressure_blocks() {
    BoundedQueue<int> q(1);
    q.push(111); // full

    std::atomic<bool> entered{false};
    std::atomic<bool> finished{false};

    std::thread producer([&]{
        entered.store(true, std::memory_order_release);
        q.push(222); // should block until a pop happens
        finished.store(true, std::memory_order_release);
    });

    while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(50ms);
    assert(!finished.load(std::memory_order_acquire)); // still blocked

    assert(q.pop() == 111); // makes space, unblocks producer
    producer.join();
    assert(finished.load(std::memory_order_acquire));
    assert(q.pop() == 222);

    q.close();
    try { (void)q.pop(); assert(false); } catch (const queue_closed&) {}
}

// Test 4: Consumer blocks until an item arrives
static void test_pop_waits_for_item() {
    BoundedQueue<int> q(2);

    std::atomic<bool> popped{false};
    std::thread consumer([&]{
        int v = q.pop(); // block until producer pushes
        assert(v == 7);
        popped.store(true);
    });

    std::this_thread::sleep_for(30ms);
    assert(!popped.load());
    q.push(7);
    consumer.join();
    assert(popped.load());

    q.close();
}

// Test 5: close() wakes blocked waiters (both sides) ---
static void test_close_wakes_waiters() {
    BoundedQueue<int> q(1);

    // 1) Start a consumer that will block on empty pop()
    std::atomic<bool> consumer_started{false};
    std::atomic<bool> consumer_threw{false};
    std::thread cons([&]{
        consumer_started.store(true, std::memory_order_release);
        try {
            (void)q.pop();                  // blocks (queue empty)
            assert(false && "pop should not return normally after close");
        } catch (const queue_closed&) {
            consumer_threw.store(true, std::memory_order_release);
        }
    });

    // 2) Make the queue full, then start a producer that will block on push()
    q.push(1); // queue now full
    std::atomic<bool> producer_started{false};
    std::atomic<bool> producer_threw{false};
    std::thread prod([&]{
        producer_started.store(true, std::memory_order_release);
        try {
            q.push(2);                      // blocks (queue full)
            assert(false && "push should not return normally after close");
        } catch (const queue_closed&) {
            producer_threw.store(true, std::memory_order_release);
        }
    });

    // 3) Ensure both are waiting, then close to wake them
    while (!consumer_started.load(std::memory_order_acquire) ||
           !producer_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(30ms); // give them time to block
    q.close();

    prod.join();
    cons.join();

    // 4) Both should have been woken and thrown
    assert(consumer_threw.load(std::memory_order_acquire));
    assert(producer_threw.load(std::memory_order_acquire));
}

int main() {
    test_single_thread_fifo();
    test_spsc_drain_then_close();
    test_backpressure_blocks();
    test_pop_waits_for_item();
    test_close_wakes_waiters();
    return 0; // any failed assert aborts with file:line
}
