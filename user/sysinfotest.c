#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"


void
sinfo(struct sysinfo *info) {
  if (sysinfo(info) < 0) {
    printf("FAIL: sysinfo failed");
    exit(1);
  }
}

//
// use sbrk() to count how many free physical memory pages there are.
//
int
countfree()
{
  uint64 sz0 = (uint64)sbrk(0);
  struct sysinfo info;
  int n = 0;

  while(1){
    if((uint64)sbrk(PGSIZE) == 0xffffffffffffffff){
      break;
    }
    n += PGSIZE;
  }
  sinfo(&info);
  if (info.freemem != 0) {
    printf("FAIL: there is no free mem, but sysinfo.freemem=%d\n",
      info.freemem);
    exit(1);
  }
  sbrk(-((uint64)sbrk(0) - sz0));
  return n;
}

void
testmem() {
  struct sysinfo info;
  uint64 n = countfree();

  sinfo(&info);

  if (info.freemem!= n) {
    printf("FAIL: free mem %d (bytes) instead of %d\n", info.freemem, n);
    exit(1);
  }

  if((uint64)sbrk(PGSIZE) == 0xffffffffffffffff){
    printf("sbrk failed");
    exit(1);
  }

  sinfo(&info);

  if (info.freemem != n-PGSIZE) {
    printf("FAIL: free mem %d (bytes) instead of %d\n", n-PGSIZE, info.freemem);
    exit(1);
  }

  if((uint64)sbrk(-PGSIZE) == 0xffffffffffffffff){
    printf("sbrk failed");
    exit(1);
  }

  sinfo(&info);

  if (info.freemem != n) {
    printf("FAIL: free mem %d (bytes) instead of %d\n", n, info.freemem);
    exit(1);
  }
}

void
testcall() {
  struct sysinfo info;

  if (sysinfo(&info) < 0) {
    printf("FAIL: sysinfo failed\n");
    exit(1);
  }

  if (sysinfo((struct sysinfo *) 0xeaeb0b5b00002f5e) !=  0xffffffffffffffff) {
    printf("FAIL: sysinfo succeeded with bad argument\n");
    exit(1);
  }
}

void testproc() {
  struct sysinfo info;
  uint64 nproc;
  int status;
  int pid;

  sinfo(&info);
  nproc = info.nproc;

  pid = fork();
  if(pid < 0){
    printf("sysinfotest: fork failed\n");
    exit(1);
  }
  if(pid == 0){
    sinfo(&info);
    if(info.nproc != nproc+1) {
      printf("sysinfotest: FAIL nproc is %d instead of %d\n", info.nproc, nproc+1);
      exit(1);
    }
    exit(0);
  }
  wait(&status);
  sinfo(&info);
  if(info.nproc != nproc) {
      printf("sysinfotest: FAIL nproc is %d instead of %d\n", info.nproc, nproc);
      exit(1);
  }
}

int
main(int argc, char *argv[])
{
  printf("sysinfotest: start\n");
  testcall();
  testmem();
  testproc();
  printf("sysinfotest: OK\n");
  exit(0);
}

//#include "kernel/types.h"
//#include "kernel/sysinfo.h"
//#include "kernel/riscv.h"
//#include "user/user.h"
//// 获取系统信息并输出
//void print_sysinfo() {
//    struct sysinfo info;
//    if (sysinfo(&info) < 0) {
//        printf("sysinfo failed\n");
//        exit(1);
//    }
//    printf("Free memory: %d bytes, Running processes: %d\n", info.freemem, info.nproc);
//}
//
//int
//main(int argc, char *argv[])
//{
//    printf("==== Initial System State ====\n");
//    print_sysinfo(); // 打印初始系统状态
//
//    // 🌟 测试1: 创建新进程，观察进程数变化
//    printf("\n[TEST] Creating a new process using fork...\n");
//    int pid = fork();
//    if (pid < 0) {
//        printf("fork failed\n");
//        exit(1);
//    } else if (pid == 0) {
//        // 子进程
//        printf("[CHILD] Child process running...\n");
//        print_sysinfo(); // 打印子进程中的系统状态
//        exit(0);
//    } else {
//        // 父进程等待子进程退出
//        wait(0);
//        printf("[PARENT] After child exits...\n");
//        print_sysinfo(); // 子进程退出后，进程数应减少
//    }
//
//    // 🌟 测试2: 动态分配内存，观察内存减少
//    printf("\n[TEST] Allocating memory using malloc...\n");
//    char *buffer = malloc(4096 * 10); // 分配40KB内存
//    if (buffer == 0) {
//        printf("malloc failed\n");
//        exit(1);
//    }
//    print_sysinfo(); // 分配内存后打印
//
//    // 🌟 测试3: 释放内存，观察内存回收
//    printf("\n[TEST] Freeing allocated memory...\n");
//    free(buffer);
//    sbrk(-4096 * 10); // 使用sbrk收缩堆
//    print_sysinfo(); // 释放内存后，空闲内存应该增加
//
//    // 🌟 测试4: 多次fork，观察进程数量增加
//    printf("\n[TEST] Forking multiple processes...\n");
//    for (int i = 0; i < 3; i++) {
//        if (fork() == 0) {
//            printf("[CHILD %d] Running...\n", i + 1);
//            print_sysinfo(); // 子进程中的系统状态
//            exit(0);
//        }
//    }
//
//    for (int i = 0; i < 3; i++) {
//        wait(0); // 等待所有子进程退出
//    }
//
//    printf("\n[TEST] After all children exit...\n");
//    print_sysinfo(); // 最终的系统状态
//
//    printf("\n==== sysinfotest: COMPLETE ====\n");
//
//    exit(0);
//}
