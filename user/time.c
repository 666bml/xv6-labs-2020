//
// Created by bml on 25-3-10.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// 将整数转换为字符串，写入 buf 中，buf_size 为 buf 大小（包括结尾的 '\0'）。
void int_to_str(int n, char *buf, int buf_size) {
    char temp[buf_size];  // 临时数组保存反转后的字符
    int i = 0;
    int negative = 0;

    if (buf_size <= 0)
        return;  // 如果 buffer 大小无效直接返回

    // 处理负数情况
    if (n < 0) {
        negative = 1;
        n = -n;
    }
    // 特殊情况：n 为 0
    if (n == 0) {
        if (buf_size > 1) {
            buf[0] = '0';
            buf[1] = '\0';
        } else {
            buf[0] = '\0';
        }
        return;
    }
    // 将数字各位存入 temp（反序）
    while (n != 0 && i < buf_size - 1) {  // 留一个位置用于 '\0'
        temp[i++] = '0' + (n % 10);
        n /= 10;
    }
    // 如果数字为负，加入负号
    if (negative && i < buf_size - 1) {
        temp[i++] = '-';
    }
    // 反转 temp 的内容存入 buf
    int j, len = i;
    if (len >= buf_size)
        len = buf_size - 1;  // 防止越界
    for (j = 0; j < len; j++) {
        buf[j] = temp[len - j - 1];
    }
    buf[len] = '\0';
}

int main() {
    int ticks = uptime();
    char buffer[100];

    int_to_str(ticks, buffer, sizeof(buffer));
    printf("Ticks: %s\n", buffer);

    int fd = open("time.txt", O_WRONLY | O_CREATE);
    if (fd < 0) {
        printf("Failed to create or open time.txt\n");
        exit(1);
    }

    int bytes_written = write(fd, buffer, strlen(buffer));
    if (bytes_written < 0) {
        printf("Failed to write to time.txt\n");
        exit(1);
    }

    close(fd);
    exit(0);
}