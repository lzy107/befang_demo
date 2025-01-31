# Function Instrumentation Demo

这是一个函数插桩的演示项目，用于记录和分析程序中的函数调用。

## 功能特点

- 使用编译器插桩技术记录函数调用
- 记录函数的入口地址和调用点
- 使用互斥锁保护的计数器统计调用次数
- 支持防止递归调用导致的死循环

## 编译和运行

```bash
# 编译（禁用优化以确保所有函数调用都被记录）
clang++ -g -O0 -finstrument-functions instrument_demo.cpp -o instrument_demo

# 运行
./instrument_demo
```

## 输出示例

程序会输出每个函数调用的以下信息：
- 调用序号
- 被调用函数的地址
- 调用点的地址

## 注意事项

- 使用 fprintf 而不是 std::cout 避免递归调用
- 需要禁用编译器优化(-O0)以确保所有函数调用都被记录
- 使用 __attribute__((no_instrument_function)) 防止特定函数被插桩
