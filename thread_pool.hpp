#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// 固定线程数的线程池：
//   - 构造时起 N 个工作线程，空转时阻塞在条件变量上，不占 CPU
//   - submit() 提交任意可调用对象（std::function<void()>），入队后唤醒一个线程
//   - stop() 停止接收新任务，把队列里剩余任务执行完，再回收所有线程
class ThreadPool {
public:
    explicit ThreadPool(unsigned int thread_count)
        : thread_count_(thread_count) {
        for (unsigned int i = 0; i < thread_count_; ++i) {
            workers_.emplace_back([this] { WorkerLoop(); });
        }
    }

    ThreadPool() : ThreadPool(std::thread::hardware_concurrency()) {}

    ~ThreadPool() { Stop(); }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 提交任务：加锁入队，解锁后通知一个空闲线程
    void Submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            tasks_.push(std::move(task));
        }
        cond_.notify_one();
    }

    // 停止：置标志 -> 唤醒所有线程 -> 等它们干完手里的活退出
    void Stop() {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (stop_) {
                return;
            }
            stop_ = true;
        }
        cond_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    void WorkerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);

                // 醒来的条件：收到停止信号，或者队列里有任务
                cond_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

                // 已停止且队列为空，才真正退出；停止后剩余任务仍要执行
                if (stop_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }  // 解锁后再执行任务，避免持锁执行阻塞其它线程

            task();
        }
    }

    unsigned int thread_count_{std::thread::hardware_concurrency()};
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    bool stop_{false};
    std::mutex mutex_;
    std::condition_variable cond_;
};
