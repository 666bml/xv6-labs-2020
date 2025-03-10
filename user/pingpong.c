//
// Created by bml on 25-3-4.
//

#include "kernel/types.h"
#include "user/user.h"

#define NULL ((void *)0)

int main() {
    int pipe1[2]; // 父进程 -> 子进程
    int pipe2[2]; // 子进程 -> 父进程
    char buffer[1];
    int pid;

    // 创建两个管道
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        fprintf(2, "pipe() failed\n");
        exit(1);
    }

    // 创建子进程
    pid = fork();

    if (pid < 0) {
        fprintf(2, "fork() failed\n");
        exit(1);
    }
    else if (pid == 0) { // 子进程
        // 关闭不需要的管道端
        close(pipe1[1]); // 关闭父进程写入到子进程的写端
        close(pipe2[0]); // 关闭子进程写入到父进程的读端

        // 从父进程读取一个字节
        read(pipe1[0], buffer, 1);
        printf("%d: received ping\n", getpid());

      /*read() 只有在以下情况下会阻塞：
            管道为空：没有数据可读。
            没有关闭写端：且没有任何写入操作时，read() 会持续等待数据。
        在我们的程序中：
            父进程在子进程执行 read() 之前就写入了数据。
            write() 之后，子进程执行 read() 立刻拿到数据。
            由于 read() 只请求 1 个字节，而管道中正好有 1 个字节，所以 read() 完成，程序继续执行，不会等待 EOF。
        write() 只在缓冲区满或者所有读端关闭时才会阻塞，这两个情况都没有发生。
      */
        // 发送一个字节给父进程
        write(pipe2[1], "P", 1);

        // 关闭所有管道
        close(pipe1[0]);
        close(pipe2[1]);

        exit(0);
    }
    else { // 父进程
        // 关闭不需要的管道端
        close(pipe1[0]); // 关闭父进程写入到子进程的读端
        close(pipe2[1]); // 关闭子进程写入到父进程的写端

        // 发送一个字节给子进程
        write(pipe1[1], "P", 1);

        // 从子进程读取一个字节
        read(pipe2[0], buffer, 1);
        printf("%d: received pong\n", getpid());

        // 关闭所有管道
        close(pipe1[1]);
        close(pipe2[0]);

        // 等待子进程退出，防止僵尸进程
        wait(NULL);

        exit(0);
    }
}
