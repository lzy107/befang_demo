#!/bin/bash
# 简化编译选项
g++ -std=c++11 -g -Wall -Wextra -Wpedantic \
    probe.cpp tetris.cpp -o tetris -pthread

# 打印调试信息
echo "Compilation completed. Running program..."