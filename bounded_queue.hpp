#include <mutex>
#include <stdexcept>

template <typename T> //T acts as a placeholder and allows you to put in any variable type, and cpp will work with whatever type you input.
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity)
        : capacity_(capacity) {
        if (capacity_ == 0) throw std::invalid_argument("capacity must be > 0");
    }

    // Blocking push (lvalue)
    void push(const T& value) {
        std::unique_lock<std::mutex> lk(m_);
        // wait until there is space OR the queue is closed
        cv_not_full_.wait(lk, [&]{ return q_.size() < capacity_ || closed_; });
        if (closed_) throw queue_closed("push() on closed queue");
        q_.push_back(value);
        lk.unlock();
        cv_not_empty_.notify_one(); // someone waiting to pop
    }
};


