// 线程池用法示例：4 个工作线程，提交 10 个任务
// 编译：g++ -std=c++17 -O2 -Wall -Wextra main.cpp -o thread_pool_demo

#include <chrono>
#include <iostream>
#include <mutex>

#include "thread_pool.hpp"

int main() {
    std::mutex cout_mutex;   // 保护 std::cout，让多线程输出不交叉

    ThreadPool pool(4);

    for (int id = 0; id < 10; ++id) {
        pool.Submit([id, &cout_mutex]() {
            {
                std::lock_guard<std::mutex> guard(cout_mutex);
                std::cout << "任务 " << id << " 由线程 "
                          << std::this_thread::get_id() << " 执行\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }

    pool.Stop();
    std::cout << "全部任务执行完毕\n";
    return 0;
}
