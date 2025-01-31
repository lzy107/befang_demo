#include <cstdio>
#include <cstdint>
#include <pthread.h>
#include <sys/time.h>
#include <thread>
#include <chrono>

// 获取当前时间戳（微秒）
__attribute__((no_instrument_function))
static uint64_t get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

// 全局计数器和互斥锁
static size_t g_enter_count = 0;
static size_t g_exit_count = 0;
static __thread size_t g_stack_depth = 0;  // 线程局部存储的调用栈深度
static __thread int g_is_active = 0;  // 线程局部存储的递归保护
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_print_mutex = PTHREAD_MUTEX_INITIALIZER;  // 用于同步输出

// 前向声明
void test_function2();

// 测试函数
__attribute__((no_instrument_function))
void test_function() {
    pthread_mutex_lock(&g_print_mutex);
    fprintf(stderr, "测试函数被调用\n");
    pthread_mutex_unlock(&g_print_mutex);
    test_function2();
}

// 测试函数2
__attribute__((no_instrument_function))
void test_function2() {
    pthread_mutex_lock(&g_print_mutex);
    fprintf(stderr, "测试函数2被调用\n");
    pthread_mutex_unlock(&g_print_mutex);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// 最简单的函数插桩实现
extern "C" {
    void __cyg_profile_func_enter(void *this_fn, void *call_site) __attribute__((no_instrument_function));
    void __cyg_profile_func_exit(void *this_fn, void *call_site) __attribute__((no_instrument_function));
}

__attribute__((no_instrument_function))
static void print_indent(size_t depth) {
    for (size_t i = 0; i < depth; ++i) {
        fprintf(stderr, "  ");
    }
}

void __cyg_profile_func_enter(void *this_fn, void *call_site) {
    if (g_is_active) return;  // 防止递归
    g_is_active = 1;
    
    uint64_t timestamp = get_timestamp();
    size_t depth = g_stack_depth++;
    pthread_t tid = pthread_self();
    
    pthread_mutex_lock(&g_mutex);
    size_t count = g_enter_count++;
    pthread_mutex_unlock(&g_mutex);
    
    pthread_mutex_lock(&g_print_mutex);
    print_indent(depth);
    fprintf(stderr, "函数进入[%zu] 线程=%p 时间=%llu 深度=%zu: 函数地址=%p, 调用点=%p\n", 
            count, (void*)tid, timestamp, depth, this_fn, call_site);
    pthread_mutex_unlock(&g_print_mutex);
    
    g_is_active = 0;
}

void __cyg_profile_func_exit(void *this_fn, void *call_site) {
    if (g_is_active) return;  // 防止递归
    g_is_active = 1;
    
    uint64_t timestamp = get_timestamp();
    size_t depth = --g_stack_depth;
    pthread_t tid = pthread_self();
    
    pthread_mutex_lock(&g_mutex);
    size_t count = g_exit_count++;
    pthread_mutex_unlock(&g_mutex);
    
    pthread_mutex_lock(&g_print_mutex);
    print_indent(depth);
    fprintf(stderr, "函数退出[%zu] 线程=%p 时间=%llu 深度=%zu: 函数地址=%p, 调用点=%p\n", 
            count, (void*)tid, timestamp, depth, this_fn, call_site);
    pthread_mutex_unlock(&g_print_mutex);
    
    g_is_active = 0;
}

// 线程函数
__attribute__((no_instrument_function))
void* thread_func(void*) {
    test_function();
    return nullptr;
}

__attribute__((no_instrument_function))
int main() {
    pthread_mutex_lock(&g_print_mutex);
    fprintf(stderr, "开始测试...\n");
    void* main_addr = (void*)&main;
    void* test_addr = (void*)&test_function;
    void* test2_addr = (void*)&test_function2;
    
    fprintf(stderr, "main函数地址: %p\n", main_addr);
    fprintf(stderr, "test_function地址: %p\n", test_addr);
    fprintf(stderr, "test_function2地址: %p\n", test2_addr);
    pthread_mutex_unlock(&g_print_mutex);
    
    uint64_t start_time = get_timestamp();
    
    // 创建两个线程来测试
    pthread_t thread1, thread2;
    pthread_create(&thread1, nullptr, thread_func, nullptr);
    pthread_create(&thread2, nullptr, thread_func, nullptr);
    
    // 等待线程结束
    pthread_join(thread1, nullptr);
    pthread_join(thread2, nullptr);
    
    uint64_t end_time = get_timestamp();
    
    pthread_mutex_lock(&g_mutex);
    size_t total_enter = g_enter_count;
    size_t total_exit = g_exit_count;
    pthread_mutex_unlock(&g_mutex);
    
    pthread_mutex_lock(&g_print_mutex);
    fprintf(stderr, "总共记录了 %zu 次函数进入和 %zu 次函数退出\n", 
            total_enter, total_exit);
    fprintf(stderr, "总耗时: %llu 微秒\n", end_time - start_time);
    fprintf(stderr, "测试完成\n");
    pthread_mutex_unlock(&g_print_mutex);
    
    return 0;
}
