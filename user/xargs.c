//
// Created by bml on 25-3-5.
// 1 find . b | xargs grep hello
// 2 find 执行，输出 ./a/b ./c/b ./b，但这些输出被管道 (|) 捕获，不会直接显示到终端。
// 3 xargs 通过 read(0, ...) 从标准输入读取 ./a/b ./c/b ./b。
// 4 xargs 运行 grep hello ./a/b , grep hello ./c/b ，grep hello ./b。
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_LINE 512  // 每行最大长度

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs command [args ...]\n");
        exit(1);
    }

    char buf[MAX_LINE];  // 存储输入行
    int index = 0;

    while (read(0, &buf[index], 1) == 1) {  // 逐个字符读取
        if (buf[index] == '\n') {  // 读取到换行符，执行命令
            buf[index] = '\0';  // 替换 '\n' 为 '\0' 结束字符串
            index = 0;

            char *xargs_argv[MAXARG];
            int i;
            for (i = 0; i < argc - 1; i++) {
                xargs_argv[i] = argv[i + 1];  // 复制用户传入的命令及参数
            }
            xargs_argv[i++] = buf;  // 添加从标准输入读取的参数
            xargs_argv[i] = 0;  // 以 NULL 结尾，符合 exec() 的要求

            if (fork() == 0) {
                exec(xargs_argv[0], xargs_argv);
                fprintf(2, "xargs: exec %s failed\n", xargs_argv[0]);
                exit(1);
            } else {
                wait(0);  // 等待子进程完成
            }
        } else {
            index++;
            if (index >= MAX_LINE - 1) {  // 防止缓冲区溢出
                fprintf(2, "xargs: input line too long\n");
                exit(1);
            }
        }
    }
    exit(0);
}
