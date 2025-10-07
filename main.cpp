// src/main.cpp
#include "bounded_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>

int main() {
    BoundedQueue<int> q(2);

    std::thread prod([&]{
        for (int i = 1; i <= 5; ++i) q.push(i);
        q.close();
    });

    std::thread cons([&]{
        try {
            while (true) {
                int v = q.pop();
                std::cout << v << " ";
            }
        } catch (const queue_closed&) {
            std::cout << "\nclosed\n";
        }
    });

    prod.join();
    cons.join();
    return 0;
}
