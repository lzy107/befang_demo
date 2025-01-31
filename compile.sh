#!/bin/bash
# 编译脚本，支持多种编译器插桩选项


# 编译器插桩编译函数
instrument_compile() {
    echo "使用 -finstrument-functions 进行编译..."

    # macOS ARM64编译
    clang++ -g -finstrument-functions \
        -fno-omit-frame-pointer \
        instrument_demo.cpp \
        -o instrument_demo

    if [ $? -eq 0 ]; then
        echo "编译成功，生成 instrument_demo 可执行文件"
        
        # 运行可执行文件
        ./instrument_demo
    else
        echo "编译失败"
        return 1
    fi

}


# 打印调试信息
echo "Compilation completed. Running program..."