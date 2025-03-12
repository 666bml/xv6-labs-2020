#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_trace(void) {
  int mask;
  if (argint(0, &mask) < 0) {   // 从用户空间获取参数
    return -1;
  }
  struct proc *p = myproc();
  p->trace_mask = mask;         // 设置当前进程的跟踪掩码
  return 0;
}

uint64
sys_sysinfo(void)
{
  // 第一步：从用户空间获取参数，参数是指向struct sysinfo的指针
  uint64 usr_addr;
  if (argaddr(0, &usr_addr) < 0) {
    return -1;
  }

  // 第二步：构造一个内核态的临时 sysinfo 结构
  struct sysinfo info;
  info.freemem = kfreemem();  // 返回空闲内存字节数
  info.nproc   = proc_count(); // 返回非UNUSED进程数

  // 第三步：把内核态的 info 拷贝到用户态指针 usr_addr 指向的地址
  //         copyout() 的用法可参考 sys_fstat() / filestat() 等
  if (copyout(myproc()->pagetable, usr_addr, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}