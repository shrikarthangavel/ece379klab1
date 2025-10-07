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
    //input is any unchang values
    //output: none
    void push(const T& value) {
        std::unique_lock<std::mutex> lk(m_); //lock mutex
        // wait until there is space OR the queue is closed
        cv_not_full_.wait(lk, [&]{ return q_.size() < capacity_ || closed_; });
        if (closed_) throw queue_closed("push() on closed queue"); //error if queue is closed
        q_.push_back(value); //push value
        lk.unlock(); //unlock the mutex
        cv_not_empty_.notify_one(); // notifies someone waiting to pop
    }


     // similarly, lock the mutex, wait for space or closed (throw error if closed), else push and unlock 
    // input: temporary values
     //output: none
    void push(T&& value) {
        std::unique_lock<std::mutex> lk(m_);
        cv_not_full_.wait(lk, [&]{ return q_.size() < capacity_ || closed_; });
        if (closed_) throw queue_closed("push() on closed queue");
        q_.push_back(std::move(value));
        lk.unlock();
        cv_not_empty_.notify_one();
    }

    // take out of queue, unless queue is empty
    // inputs: none
    //output: popped element
    T pop() {
        std::unique_lock<std::mutex> lk(m_); //must lock to avoid undefined behavior
        cv_not_empty_.wait(lk, [&]{ return !q_.empty() || closed_; });
        if (q_.empty()) {
            // empty or closed error
            throw queue_closed("pop() on closed and empty queue");
        }
        T v = std::move(q_.front()); //not copying, but v will hold what is to be popped
        q_.pop_front();
        lk.unlock();
        cv_not_full_.notify_one(); //lets push know space is available
        return v;
    }

    //safely marks the queue as closed
    //no inputs or outputs
    void close() {
        std::lock_guard<std::mutex> lk(m_);
        if (!closed_) {
            closed_ = true; //allows push and pop to throw exceptions
            cv_not_full_.notify_all(); //wakes all to close all
            cv_not_empty_.notify_all();
        }
    }

    private:
        const size_t capacity_;
        mutable std::mutex m_;
        std::condition_variable cv_not_full_;
        std::condition_variable cv_not_empty_;
        std::deque<T> q_;
        bool closed_ = false;
    
};


