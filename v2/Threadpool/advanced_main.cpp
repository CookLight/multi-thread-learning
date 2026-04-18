#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

#include "AdvancedThreadPool.h"

// 模拟一些计算任务
int compute_task(int id, int duration_ms) {
    std::cout << "Task " << id << " started on thread: " 
              << std::this_thread::get_id() << std::endl;
    
    // 模拟工作负载
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    
    std::cout << "Task " << id << " completed after " 
              << duration_ms << "ms" << std::endl;
    
    return id * id;
}

void example_usage() {
    std::cout << "=== Advanced Thread Pool Example ===" << std::endl;
    
    // 创建线程池，使用默认线程数（CPU核心数）
    AdvancedThreadPool pool;
    
    std::cout << "Thread pool created with " << pool.totalThreads() 
              << " threads" << std::endl;
    
    std::vector<std::future<int>> results;
    const int num_tasks = 12;
    
    // 随机数生成器用于生成不同的任务持续时间
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 1000);
    
    // 提交任务
    for (int i = 0; i < num_tasks; ++i) {
        int duration = dis(gen);
        results.emplace_back(
            pool.enqueue(compute_task, i, duration)
        );
    }
    
    std::cout << "\nAll tasks submitted. Queue size: " 
              << pool.taskCount() << std::endl;
    
    // 等待一段时间后暂停线程池
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "\nPausing thread pool..." << std::endl;
    pool.pause();
    
    std::cout << "Active threads: " << pool.activeThreads() 
              << ", Queue size: " << pool.taskCount() << std::endl;
    
    // 等待2秒后恢复
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "\nResuming thread pool..." << std::endl;
    pool.resume();
    
    // 等待所有任务完成
    std::cout << "\nWaiting for all tasks to complete..." << std::endl;
    pool.waitAll();
    
    // 收集结果
    std::cout << "\nResults: ";
    for (auto& result : results) {
        std::cout << result.get() << " ";
    }
    std::cout << std::endl;
    
    std::cout << "\nAll tasks completed successfully!" << std::endl;
}

void stress_test() {
    std::cout << "\n=== Stress Test ===" << std::endl;
    
    AdvancedThreadPool pool(8);
    const int num_tasks = 100;
    
    std::vector<std::future<int>> results;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_tasks; ++i) {
        results.emplace_back(
            pool.enqueue([](int id) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return id;
            }, i)
        );
    }
    
    pool.waitAll();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    );
    
    std::cout << "Completed " << num_tasks << " tasks in " 
              << duration.count() << "ms" << std::endl;
    std::cout << "Average time per task: " 
              << std::fixed << std::setprecision(2)
              << static_cast<double>(duration.count()) / num_tasks 
              << "ms" << std::endl;
}

int main() {
    try {
        example_usage();
        stress_test();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}