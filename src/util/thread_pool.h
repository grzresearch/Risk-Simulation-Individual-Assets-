#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace macro::util {

// Fixed-size thread pool. Submitted tasks return std::future and run on a
// dedicated worker. Destruction joins all threads.
class ThreadPool {
public:
    explicit ThreadPool(unsigned threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
        using R = typename std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto fut = task->get_future();
        {
            std::lock_guard<std::mutex> lk(mu_);
            queue_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    unsigned size() const { return static_cast<unsigned>(workers_.size()); }

private:
    void worker_loop();

    std::vector<std::thread>           workers_;
    std::queue<std::function<void()>>  queue_;
    std::mutex                         mu_;
    std::condition_variable            cv_;
    bool                               stop_ = false;
};

} // namespace macro::util
