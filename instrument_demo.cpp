#include <stdio.h>
#include <pthread.h>
#include <mutex>
#include <iostream>
#include <sys/time.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>

// 前向声明
__attribute__((no_instrument_function)) void test_function();
__attribute__((no_instrument_function)) void test_function2();
__attribute__((no_instrument_function)) void* thread_func(void*);

// 函数调用记录结构
struct FunctionRecord {
    bool is_entry;              // true表示函数进入，false表示函数退出
    void* func;                 // 函数地址
    void* caller;               // 调用者地址
    uint64_t timestamp;         // 时间戳（微秒）
    uint64_t thread_id;         // 线程ID
    int depth;                  // 调用栈深度
    int record_id;              // 记录ID
};

// 全局变量
static __thread bool is_active = false;
static __thread int call_depth = 0;
static std::mutex g_print_mutex;
static const int MAX_RECORDS = 10000;
static FunctionRecord g_records[MAX_RECORDS];
static int g_record_count = 0;
static uint64_t g_start_time = 0;

// 获取当前时间戳（微秒）
__attribute__((no_instrument_function))
static uint64_t get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// 获取线程ID
__attribute__((no_instrument_function))
static uint64_t get_thread_id() {
    pthread_t tid = pthread_self();
    return (uint64_t)tid;
}

// 保存函数调用记录
__attribute__((no_instrument_function))
static void save_record(bool is_entry, void* func, void* caller) {
    int idx = __sync_fetch_and_add(&g_record_count, 1);
    if (idx < MAX_RECORDS) {
        g_records[idx].is_entry = is_entry;
        g_records[idx].func = func;
        g_records[idx].caller = caller;
        g_records[idx].timestamp = get_timestamp();
        g_records[idx].thread_id = get_thread_id();
        g_records[idx].depth = call_depth;
        g_records[idx].record_id = idx;
    }
}

// 将记录保存到文件
__attribute__((no_instrument_function))
static void save_records_to_file(const char* filename) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return;
    }

    // 写入JSON格式的数据
    file << "{\n";
    file << "  \"total_time\": " << (get_timestamp() - g_start_time) << ",\n";
    file << "  \"records\": [\n";
    
    int count = g_record_count;
    if (count > MAX_RECORDS) count = MAX_RECORDS;
    
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            file << ",\n";
        }
        
        const auto& record = g_records[i];
        file << "    {\n";
        file << "      \"type\": \"" << (record.is_entry ? "entry" : "exit") << "\",\n";
        file << "      \"func\": \"0x" << std::hex << (uint64_t)record.func << "\",\n";
        file << "      \"caller\": \"0x" << std::hex << (uint64_t)record.caller << "\",\n";
        file << "      \"timestamp\": " << std::dec << record.timestamp << ",\n";
        file << "      \"thread_id\": \"0x" << std::hex << record.thread_id << "\",\n";
        file << "      \"depth\": " << std::dec << record.depth << ",\n";
        file << "      \"record_id\": " << std::dec << record.record_id;
        file << "\n    }";
    }
    
    file << "\n  ]\n}\n";
    file.close();
    
    std::cout << "记录已保存到文件: " << filename << std::endl;
    std::cout << "总共记录了 " << count << " 条记录" << std::endl;
    std::cout << "总耗时: " << (get_timestamp() - g_start_time) << " 微秒" << std::endl;
}

extern "C" {
void __cyg_profile_func_enter(void* this_fn, void* call_site) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void* this_fn, void* call_site) __attribute__((no_instrument_function));
}

void __cyg_profile_func_enter(void* this_fn, void* call_site) {
    if (is_active) return;
    is_active = true;
    
    if (g_start_time == 0) {
        g_start_time = get_timestamp();
    }
    
    save_record(true, this_fn, call_site);
    call_depth++;
    
    is_active = false;
}

void __cyg_profile_func_exit(void* this_fn, void* call_site) {
    if (is_active) return;
    is_active = true;
    
    call_depth--;
    save_record(false, this_fn, call_site);
    
    is_active = false;
}

// 测试函数
__attribute__((no_instrument_function))
void test_function() {
    std::cout << "测试函数被调用" << std::endl;
    test_function2();
}

__attribute__((no_instrument_function))
void test_function2() {
    std::cout << "测试函数2被调用" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// 线程函数
__attribute__((no_instrument_function))
void* thread_func(void*) {
    test_function();
    return nullptr;
}

__attribute__((no_instrument_function))
int main() {
    std::cout << "开始测试..." << std::endl;
    
    // 打印函数地址
    printf("main函数地址: %p\n", (void*)main);
    printf("test_function地址: %p\n", (void*)test_function);
    printf("test_function2地址: %p\n", (void*)test_function2);
    
    // 创建两个线程执行测试
    pthread_t thread1, thread2;
    pthread_create(&thread1, NULL, thread_func, NULL);
    pthread_create(&thread2, NULL, thread_func, NULL);
    
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    // 保存记录到文件
    save_records_to_file("function_trace.json");
    
    std::cout << "测试完成" << std::endl;
    return 0;
}
