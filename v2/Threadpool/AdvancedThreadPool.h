#ifndef ADVANCED_THREAD_POOL_H
#define ADVANCED_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <type_traits>

class AdvancedThreadPool {
public:
    explicit AdvancedThreadPool(size_t threads = std::thread::hardware_concurrency());
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // 获取当前任务队列大小
    size_t taskCount() const;
    
    // 获取活跃线程数量
    size_t activeThreads() const;
    
    // 获取总线程数量
    size_t totalThreads() const;
    
    // 暂停线程池
    void pause();
    
    // 恢复线程池
    void resume();
    
    // 等待所有任务完成
    void waitAll();
    
    ~AdvancedThreadPool();
    
private:
    // 工作线程
    std::vector<std::thread> workers;
    
    // 任务队列
    std::queue<std::function<void()>> tasks;
    
    // 同步原语
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable completion_condition;
    
    // 状态控制
    std::atomic<bool> stop;
    std::atomic<bool> paused;
    std::atomic<size_t> active_count;
    std::atomic<size_t> total_tasks;
    std::atomic<size_t> completed_tasks;
};

inline AdvancedThreadPool::AdvancedThreadPool(size_t threads) 
    : stop(false), paused(false), active_count(0), total_tasks(0), completed_tasks(0) {
    
    if (threads == 0) {
        threads = std::thread::hardware_concurrency();
        if (threads == 0) threads = 1;
    }
    
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    
                    // 等待条件：有任务且未停止，或者需要停止且队列为空
                    this->condition.wait(lock, [this] {
                        return this->stop || (!this->tasks.empty() && !this->paused);
                    });
                    
                    // 如果停止且队列为空，退出
                    if (this->stop && this->tasks.empty()) {
                        return;
                    }
                    
                    // 如果暂停且队列不为空，继续等待
                    if (this->paused && !this->tasks.empty()) {
                        continue;
                    }
                    
                    // 获取任务
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                    
                    ++this->active_count;
                }
                
                // 执行任务
                task();
                
                {
                    std::lock_guard<std::mutex> lock(this->queue_mutex);
                    --this->active_count;
                    ++this->completed_tasks;
                    
                    // 如果所有任务都完成了，通知等待的线程
                    if (this->completed_tasks == this->total_tasks) {
                        this->completion_condition.notify_all();
                    }
                }
            }
        });
    }
}

template<class F, class... Args>
auto AdvancedThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        
        tasks.emplace([task]() { (*task)(); });
        ++total_tasks;
    }
    
    condition.notify_one();
    return res;
}

inline size_t AdvancedThreadPool::taskCount() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return tasks.size();
}

inline size_t AdvancedThreadPool::activeThreads() const {
    return active_count;
}

inline size_t AdvancedThreadPool::totalThreads() const {
    return workers.size();
}

inline void AdvancedThreadPool::pause() {
    paused = true;
}

inline void AdvancedThreadPool::resume() {
    paused = false;
    condition.notify_all();
}

inline void AdvancedThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    completion_condition.wait(lock, [this] {
        return completed_tasks == total_tasks;
    });
}

inline AdvancedThreadPool::~AdvancedThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    
    condition.notify_all();
    
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

#endif // ADVANCED_THREAD_POOL_H