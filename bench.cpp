#include "bounded_queue.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <sstream>

using namespace std::chrono;

struct Config {
    int producers;
    int consumers;
    size_t capacity;
    size_t items_per_producer;
};

void run_trial(const Config& c, std::ostream& out) {
    size_t total_items = (size_t)c.producers * c.items_per_producer;
    BoundedQueue<size_t> q(c.capacity);

    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};

    auto start = high_resolution_clock::now();

    // producers
    std::vector<std::thread> prods;
    for (int p = 0; p < c.producers; ++p) {
        prods.emplace_back([&, p]{
            for (size_t i = 0; i < c.items_per_producer; ++i) {
                q.push(p * c.items_per_producer + i);
                ++produced;
            }
        });
    }

    // consumers
    std::vector<std::thread> comps;
    for (int m = 0; m < c.consumers; ++m) {
        comps.emplace_back([&]{
            try {
                while (true) {
                    q.pop();
                    ++consumed;
                }
            } catch (const queue_closed&) {
                return;
            }
        });
    }

    for (auto &t : prods) t.join();

    // all produced, close the queue to let consumers finish
    q.close();

    for (auto &t : comps) t.join();

    auto end = high_resolution_clock::now();
    double seconds = duration_cast<duration<double>>(end - start).count();

    // sanity check
    bool ok = (produced.load() == total_items) && (consumed.load() == total_items);

    out << c.producers << ',' << c.consumers << ',' << c.capacity << ',' << c.items_per_producer
        << ',' << total_items << ',' << (ok?"ok":"mismatch") << ','
        << seconds << ',' << (total_items / seconds) << '\n';
}

int main(int argc, char** argv) {
    // default parameter grid
    std::vector<int> Ns = {1, 2, 4, 8};
    std::vector<int> Ms = {1, 2, 4, 8};
    std::vector<size_t> caps = {1, 2, 4, 16, 64};
    size_t items_per_producer = 100000; // default

    // allow overriding items per producer from argv[1]
    if (argc >= 2) {
        std::istringstream iss(argv[1]);
        size_t v; if (iss >> v) items_per_producer = v;
    }

    // header
    std::cout << "producers,consumers,capacity,items_per_producer,total_items,status,seconds,ops_per_sec\n";

    for (int n : Ns) for (int m : Ms) for (size_t c : caps) {
        Config cfg{n,m,c,items_per_producer};
        run_trial(cfg, std::cout);
    }

    return 0;
}
