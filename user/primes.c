//
// Created by bml on 25-3-4.
// 共创建了12个进程，12个管道，进程逐层嵌套。
// 每一个子进程从父进程的读管道中读取筛选过后的整数，并对读取到的整数继续筛选，写到自己的写管道中，以此类推
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NULL ((void *)0)

// sieve 函数：从 leftfd 管道读取整数，读取到的第一个整数即为 prime；
// 然后打印它，并创建一个新的管道与子进程，用于继续筛选后续的整数。
void sieve(int leftfd) {
    int prime;
    // 尝试读取第一个整数（这将是当前管道中的最小数）
    if (read(leftfd, &prime, sizeof(prime)) == 0) {
        // 如果没有数据可读，说明已经没有新的素数可处理，直接退出
        close(leftfd);
        exit(0);
    }

    // 打印当前的素数(即读取的第一个整数)
    printf("prime %d\n", prime);

    // 为下一个筛选阶段创建一个新的管道
    int p[2];
    pipe(p);

    // 创建子进程，让子进程去处理下一个筛选阶段
    int pid = fork();
    if (pid < 0) {
        // fork 失败
        fprintf(2, "fork error\n");
        exit(1);
    } else if (pid == 0) {
        // 子进程：关闭不需要的写端 p[1]，在新的 sieve 调用中继续筛选
        close(p[1]);
        sieve(p[0]);
        exit(0);
    } else {
        // 父进程：关闭不需要的读端 p[0]
        close(p[0]);

        // 从 leftfd 继续读取剩余的整数，将不是 prime 倍数的数写入 p[1]
        int num;
        while (read(leftfd, &num, sizeof(num))) {
            if (num % prime != 0) {
                write(p[1], &num, sizeof(num));
            }
        }

        // 所有数据处理完毕，关闭 leftfd 和 p[1]
        close(leftfd);
        close(p[1]);

        // 等待子进程结束，防止僵尸进程
        wait(NULL);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    // 创建最初的管道，用来放置 2~35 的数字
    int p[2];
    pipe(p);

    // 创建子进程，让子进程在管道中进行筛选
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork error\n");
        exit(1);
    } else if (pid == 0) {
        // 子进程：关闭写端 p[1]，从 p[0] 中读数据并开始 sieve
        close(p[1]);
        sieve(p[0]);
        exit(0);
    } else {
        // 父进程：关闭读端 p[0]，将 2~35 写入 p[1]
        close(p[0]);
        for (int i = 2; i <= 35; i++) {
            write(p[1], &i, sizeof(i));
        }
        // 写完后关闭 p[1]
        close(p[1]);

        // 等待子进程及其所有子孙进程结束
        wait(NULL);
        exit(0);
    }
}

