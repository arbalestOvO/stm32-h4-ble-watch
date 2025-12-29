import os
import sys

def generate_command_files(command_ids):
    # 创建文件夹
    include_dir = "include"
    src_dir = "src"

    for folder in [include_dir, src_dir]:
        if not os.path.exists(folder):
            os.makedirs(folder)
            print(f"Created directory: {folder}")

    for cmd_id in command_ids:
        # 定义文件名
        h_file_path = os.path.join(include_dir, f"command{cmd_id}.h")
        c_file_path = os.path.join(src_dir, f"command{cmd_id}.c")

        # 1. 生成 .h 文件内容
        h_template = f"""//
// Created by 19571 on 2025/12/26.
//

#ifndef ABOLUO_EXIT_COMMAND{cmd_id}_H
#define ABOLUO_EXIT_COMMAND{cmd_id}_H
#include <stdint.h>

#include "auth_client.h"

int Handle{cmd_id}(AuthContext_t* ctx, uint8_t* data, int len);

#endif //ABOLUO_EXIT_COMMAND{cmd_id}_H
"""

        # 2. 生成 .c 文件内容
        c_template = f"""#include "commands/inc/command{cmd_id}.h"

#include <stdio.h>
#include <string.h>

#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle{cmd_id}(AuthContext_t* ctx, uint8_t* data, int len) {{
    // Implementation for command {cmd_id}
    return 0;
}}
"""

        # 写入 .h 文件
        with open(h_file_path, 'w', encoding='utf-8') as f:
            f.write(h_template)

        # 写入 .c 文件
        with open(c_file_path, 'w', encoding='utf-8') as f:
            f.write(c_template)

        print(f"Generated: {h_file_path} and {c_file_path}")

if __name__ == "__main__":
    # 获取命令行参数（排除脚本名称本身）
    args = sys.argv[1:]

    if not args:
        print("Usage: python gen_command.py <cmd_id1> <cmd_id2> ...")
        print("Example: python gen_command.py 0101 0102 0133")
    else:
        generate_command_files(args)
        print("\nAll files generated successfully.")