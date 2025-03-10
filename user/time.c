//
// Created by bml on 25-3-10.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    int ticks = uptime();  // 调用 uptime() 系统调用
    printf("Uptime: %d ticks\n", ticks);
    exit(0);
}
