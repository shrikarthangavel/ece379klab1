#include "bounded_queue.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

int main(int argc, char** argv) {
    size_t K = 1000000; // default items
    size_t capacity = 1024; // default queue capacity

    if (argc >= 2) {
        std::istringstream iss(argv[1]); size_t v; if (iss >> v) K = v;
    }
    if (argc >= 3) {
        std::istringstream iss(argv[2]); size_t v; if (iss >> v) capacity = v;
    }

    std::cout << "Driver: streaming K=" << K << " items with capacity=" << capacity << "\n";

    BoundedQueue<size_t> q(capacity);

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&]{
        for (size_t i = 0; i < K; ++i) q.push(i);
        q.close();
    });

    bool ok = true;
    size_t seen = 0;
    try {
        while (true) {
            size_t v = q.pop();
            if (v != seen) {
                std::cerr << "Ordering mismatch: expected " << seen << " got " << v << "\n";
                ok = false;
                break;
            }
            ++seen;
        }
    } catch (const queue_closed&) {
        // queue closed; expected when producer finished
    }

    producer.join();

    auto end = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();

    if (ok && seen != K) {
        std::cerr << "Mismatch: seen " << seen << " but expected " << K << "\n";
        ok = false;
    }

    std::cout << (ok?"PASS":"FAIL") << ": seen=" << seen << ", time=" << secs << "s, ops/s=" << (seen / secs) << "\n";

    return ok ? 0 : 2;
}
