#include <cstdio>
#include <pthread.h>

// 全局计数器和互斥锁
static size_t g_call_count = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

// 测试函数
void test_function() {
    fprintf(stderr, "测试函数被调用\n");
}

// 测试函数2
void test_function2() {
    fprintf(stderr, "测试函数2被调用\n");
}

// 最简单的函数插桩实现
extern "C" {
    void __cyg_profile_func_enter(void *this_fn, void *call_site) __attribute__((no_instrument_function));
    void __cyg_profile_func_exit(void *this_fn, void *call_site) __attribute__((no_instrument_function));
}

void __cyg_profile_func_enter(void *this_fn, void *call_site) {
    static int is_active = 0;
    if (is_active) return;  // 防止递归
    
    is_active = 1;
    pthread_mutex_lock(&g_mutex);
    size_t count = g_call_count++;
    pthread_mutex_unlock(&g_mutex);
    fprintf(stderr, "函数进入[%zu]: 函数地址=%p, 调用点=%p\n", count, this_fn, call_site);
    is_active = 0;
}

void __cyg_profile_func_exit(void *this_fn, void *call_site) {
    // 暂时为空
}

__attribute__((no_instrument_function))
int main() {
    fprintf(stderr, "开始测试...\n");
    void* main_addr = (void*)&main;
    void* test_addr = (void*)&test_function;
    void* test2_addr = (void*)&test_function2;
    
    fprintf(stderr, "main函数地址: %p\n", main_addr);
    fprintf(stderr, "test_function地址: %p\n", test_addr);
    fprintf(stderr, "test_function2地址: %p\n", test2_addr);
    
    // 测试函数调用
    test_function();
    test_function2();
    
    pthread_mutex_lock(&g_mutex);
    size_t total = g_call_count;
    pthread_mutex_unlock(&g_mutex);
    
    fprintf(stderr, "总共记录了 %zu 次函数调用\n", total);
    fprintf(stderr, "测试完成\n");
    return 0;
}
