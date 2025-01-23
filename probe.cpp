#include <iostream>
#include <fstream>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>

// 全局原子计数器和文件流
std::atomic<int> function_entry_count(0);
std::atomic<int> function_exit_count(0);
std::mutex file_mutex;

// 安全的日志函数
void safe_log_to_file(const std::string& message) {
    try {
        std::lock_guard<std::mutex> lock(file_mutex);
        std::ofstream trace_file("a.txt", std::ios::app);
        if (trace_file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto now_c = std::chrono::system_clock::to_time_t(now);
            trace_file << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") 
                       << " | " << message << std::endl;
            trace_file.close();
        }
    } catch (const std::exception& e) {
        std::cerr << "日志记录错误: " << e.what() << std::endl;
    }
}

// 注释掉函数插桩
#if 0
extern "C" {
    void __cyg_profile_func_enter(void *func, void *caller) {}
    void __cyg_profile_func_exit(void *func, void *caller) {}
}
#endif