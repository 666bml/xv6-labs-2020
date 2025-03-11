//
// Created by bml on 25-3-10.
//

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// 简单的整数转字符串函数，写入 buf 中，buf_size 包括终止符 '\0'
void int_to_str(int n, char *buf, int buf_size) {
    char temp[buf_size];
    int i = 0;
    int negative = 0;

    if (buf_size <= 0)
        return;

    if (n < 0) {
        negative = 1;
        n = -n;
    }

    if (n == 0) {
        if (buf_size > 1) {
            buf[0] = '0';
            buf[1] = '\0';
        } else {
            buf[0] = '\0';
        }
        return;
    }

    while (n != 0 && i < buf_size - 1) {  // 留一个位置给 '\0'
        temp[i++] = '0' + (n % 10);
        n /= 10;
    }

    if (negative && i < buf_size - 1) {
        temp[i++] = '-';
    }

    int j, len = i;
    if (len >= buf_size)
        len = buf_size - 1;
    for (j = 0; j < len; j++) {
        buf[j] = temp[len - j - 1];
    }
    buf[len] = '\0';
}

int main(int argc, char *argv[]) {
    int ticks = uptime();  // 获取 ticks
    char buffer[100];
    int_to_str(ticks, buffer, sizeof(buffer));
    printf("Ticks: %s\n", buffer);
    // 使用 open 来创建 time.txt 文件
    int fd = open("time.txt", O_CREATE|O_RDWR);
    if (fd < 0) {
        printf("Cannot create time.txt\n");
        exit(1);
    }

    int len = strlen(buffer);
    if (write(fd, buffer, len) != len) {
        printf("Cannot write to time.txt\n");
        close(fd);
        exit(1);
    }

    close(fd);
    exit(0);
}
