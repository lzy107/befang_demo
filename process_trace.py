#!/usr/bin/env python3
import json
import subprocess
import os
from typing import Dict, List, Set
from dataclasses import dataclass
from collections import defaultdict

# 全局变量
runtime_base = None
symbol_table = {}

@dataclass
class Record:
    type: str
    func_addr: int
    caller_addr: int
    timestamp: float
    thread_id: str = ""
    cpu_usage: float = 0.0
    depth: int = 0  # 添加调用深度属性

def get_symbol_table(binary_path: str) -> Dict[int, str]:
    try:
        # 首先尝试使用 nm -n 获取按地址排序的符号表
        result = subprocess.run(['nm', '-n', binary_path], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Warning: Failed to get symbol table: {result.stderr}")
            return {}
        
        symbols = {}
        # 批量收集所有需要解析的符号
        mangled_symbols = []
        symbol_entries = []
        
        for line in result.stdout.splitlines():
            parts = line.strip().split(' ')
            if len(parts) >= 3:  # 确保行有足够的部分
                try:
                    addr = int(parts[0], 16)
                    symbol_type = parts[1]
                    name = parts[2]
                    
                    # 只处理代码段的符号
                    if symbol_type.lower() in ('t', 'w'):
                        symbol_entries.append((addr, name))
                        if name.startswith('__Z'):  # 需要解析的 C++ 符号
                            mangled_symbols.append(name)
                except ValueError:
                    continue
        
        # 批量解析 C++ 符号
        if mangled_symbols:
            try:
                demangler = subprocess.Popen(['c++filt'], 
                                          stdin=subprocess.PIPE,
                                          stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE,
                                          text=True)
                
                # 一次性发送所有符号进行解析
                mangled_input = '\n'.join(mangled_symbols)
                stdout, stderr = demangler.communicate(mangled_input)
                
                if demangler.returncode == 0:
                    demangled_names = stdout.strip().split('\n')
                    name_map = dict(zip(mangled_symbols, demangled_names))
                else:
                    print(f"Warning: c++filt failed: {stderr}")
                    name_map = {}
            except Exception as e:
                print(f"Warning: Failed to demangle symbols: {e}")
                name_map = {}
        else:
            name_map = {}
        
        # 构建最终的符号表
        for addr, name in symbol_entries:
            if name in name_map:
                symbols[addr] = name_map[name]
            else:
                symbols[addr] = name
        
        print(f"Loaded {len(symbols)} symbols")
        return symbols
        
    except Exception as e:
        print(f"Warning: Failed to get symbol table: {e}")
        return {}

def calculate_base_offset(binary_path: str) -> int:
    print(f"Calculating base offset for {binary_path}...")
    
    # 方法1: 使用 otool -l
    try:
        result = subprocess.run(['otool', '-l', binary_path], capture_output=True, text=True)
        if result.returncode == 0:
            lines = result.stdout.split('\n')
            for i, line in enumerate(lines):
                if 'segname __TEXT' in line:
                    # 在 __TEXT 段中寻找 vmaddr
                    for j in range(i, min(i + 10, len(lines))):
                        if 'vmaddr' in lines[j]:
                            vmaddr = int(lines[j].split()[1], 16)
                            print(f"Found base offset 0x{vmaddr:x} from __TEXT segment vmaddr")
                            return vmaddr
        else:
            print(f"Warning: otool command failed with error: {result.stderr}")
    except Exception as e:
        print(f"Warning: Failed to use otool: {e}")
    
    # 方法2: 使用 nm 查找 __mh_execute_header
    try:
        result = subprocess.run(['nm', binary_path], capture_output=True, text=True)
        if result.returncode == 0:
            for line in result.stdout.split('\n'):
                if '__mh_execute_header' in line:
                    try:
                        addr = int(line.split()[0], 16)
                        print(f"Found base offset 0x{addr:x} from __mh_execute_header symbol")
                        return addr
                    except (IndexError, ValueError) as e:
                        print(f"Warning: Failed to parse __mh_execute_header address: {e}")
        else:
            print(f"Warning: nm command failed with error: {result.stderr}")
    except Exception as e:
        print(f"Warning: Failed to use nm: {e}")
    
    # 方法3: 使用 vmmap (仅适用于正在运行的进程)
    try:
        result = subprocess.run(['vmmap', binary_path], capture_output=True, text=True)
        if result.returncode == 0:
            lines = result.stdout.split('\n')
            for line in lines:
                if '__TEXT' in line and 'exec' in line:
                    try:
                        addr = int(line.split()[0], 16)
                        print(f"Found base offset 0x{addr:x} from vmmap")
                        return addr
                    except (IndexError, ValueError) as e:
                        print(f"Warning: Failed to parse vmmap address: {e}")
        else:
            print(f"Warning: vmmap command failed with error: {result.stderr}")
    except Exception as e:
        print(f"Warning: Failed to use vmmap: {e}")
    
    # 如果所有方法都失败，使用默认值
    default_base = 0x100000000  # 典型的 macOS 64位程序基址
    print(f"Warning: Using default base offset 0x{default_base:x}")
    return default_base

def get_symbol_name(address, binary_path):
    """
    获取给定地址对应的符号名称
    :param address: 运行时地址
    :param binary_path: 二进制文件路径
    :return: 符号名称
    """
    try:
        # 将运行时地址转换为文件地址
        global runtime_base
        if runtime_base is None:
            return f"0x{address:x}"
            
        file_address = address - runtime_base
        
        # 使用atos命令获取符号名称
        cmd = ['atos', '-o', binary_path, '-l', hex(runtime_base), hex(address)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0 and result.stdout.strip() and 'not in' not in result.stdout:
            return result.stdout.strip()
            
        # 如果atos失败，尝试在符号表中查找最近的符号
        closest_symbol = None
        closest_distance = float('inf')
        
        for symbol_addr, symbol_name in symbol_table.items():
            if symbol_addr <= file_address:
                distance = file_address - symbol_addr
                if distance < closest_distance:
                    closest_distance = distance
                    closest_symbol = symbol_name
        
        if closest_symbol:
            # 对于C++符号，尝试使用c++filt进行解码
            if closest_symbol.startswith('__Z'):
                cmd = ['c++filt', closest_symbol]
                result = subprocess.run(cmd, capture_output=True, text=True)
                if result.returncode == 0:
                    return result.stdout.strip()
            return closest_symbol
            
        # 如果都失败了，返回十六进制地址
        return f"0x{address:x}"
    except Exception as e:
        print(f"警告: 获取地址 0x{address:x} 的符号名称时出错: {str(e)}")
        return f"0x{address:x}"

def print_call_stack(records, binary_path):
    """
    打印函数调用栈
    :param records: 记录列表
    :param binary_path: 二进制文件路径
    """
    # 按线程ID分组
    threads = defaultdict(list)
    for record in records:
        threads[record.thread_id].append(record)
    
    # 为每个线程打印调用栈
    for thread_id, thread_records in threads.items():
        print(f"\n{'='*80}")
        print(f"线程 {thread_id}:")
        print(f"{'='*80}")
        
        # 计算这个线程的总CPU时间
        total_cpu_time = sum(record.cpu_usage for record in thread_records)
        thread_duration = thread_records[-1].timestamp - thread_records[0].timestamp
        print(f"总执行时间: {thread_duration:.2f}ms")
        print(f"CPU使用率: {total_cpu_time/10:.1f}%")
        print(f"{'='*80}\n")
        
        # 用于跟踪每个调用层级的时间
        call_times = {}
        
        for record in thread_records:
            func_name = get_symbol_name(record.func_addr, binary_path)
            caller_name = get_symbol_name(record.caller_addr, binary_path)
            
            # 计算缩进
            indent = "  " * record.type.count('exit')
            
            # 为entry记录保存开始时间
            if record.type == 'entry':
                call_times[(record.func_addr, record.depth)] = record.timestamp
            
            # 为exit记录计算函数执行时间
            duration = ""
            if record.type == 'exit':
                start_time = call_times.get((record.func_addr, record.depth-1), record.timestamp)
                duration = f" [执行时间: {record.timestamp - start_time:.3f}ms]"
            
            # 构建调用箭头
            arrow = "└─>" if record.type == 'entry' else "└──"
            
            # 添加CPU使用信息
            cpu_info = f"(CPU: {record.cpu_usage/10:.1f}%)" if record.cpu_usage > 0 else ""
            
            # 打印完整的调用信息
            print(f"{indent}{arrow} {func_name}")
            if record.type == 'entry':
                print(f"{indent}    被调用者: {caller_name}")
            print(f"{indent}    时间戳: {record.timestamp:.3f}ms {cpu_info}{duration}")
            
            if record.type == 'exit':
                print(f"{indent}    {'─'*40}")

def process_trace_file(trace_file: str, binary_path: str):
    try:
        # 读取运行时基地址
        global runtime_base, symbol_table
        try:
            with open('runtime_base.json', 'r') as f:
                base_data = json.load(f)
                runtime_base = int(base_data['runtime_base'], 16)
                print(f"读取到运行时基地址: 0x{runtime_base:x}")
        except Exception as e:
            print(f"警告: 无法读取运行时基地址文件: {e}")
            runtime_base = None

        with open(trace_file, 'r') as f:
            data = json.load(f)
        
        total_time = data.get('total_time', 0) / 1000000  # 转换为毫秒
        json_records = data.get('records', [])
        
        records = []
        for item in json_records:
            record = Record(
                type=item['type'],
                func_addr=int(item['func'], 16),
                caller_addr=int(item['caller'], 16),
                timestamp=float(item['timestamp']) / 1000000,  # 转换为毫秒
                thread_id=item['thread_id'],
                cpu_usage=float(item.get('cpu_usage', 0)),
                depth=int(item.get('depth', 0))
            )
            records.append(record)
        
        print(f"Total records: {len(records)}")
        print(f"Total time: {total_time:.2f}ms")
        
        if records:
            # 如果有运行时基地址，使用它，否则计算基地址
            if runtime_base is not None:
                base_offset = runtime_base
                print(f"使用运行时基地址: 0x{base_offset:x}")
            else:
                base_offset = calculate_base_offset(binary_path)
                runtime_base = base_offset
            
            # Load symbol table
            symbol_table = get_symbol_table(binary_path)
            print(f"Loaded {len(symbol_table)} symbols")
            
            # Print call stack
            print("\nProcessing {} records...".format(len(records)))
            print_call_stack(records, binary_path)
            
    except Exception as e:
        print(f"Error processing trace file: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    import sys
    if len(sys.argv) != 3:
        print("Usage: python3 process_trace.py <trace_file> <binary_path>")
        sys.exit(1)
    
    trace_file = sys.argv[1]
    binary_path = sys.argv[2]
    process_trace_file(trace_file, binary_path)