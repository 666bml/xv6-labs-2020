#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"
#include "proc.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
//  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0){
//        printf("walk: kalloc failed for va=%p\n", va);
        return 0;
      }
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;

  if(myproc()->kernelpt == 0)
    panic("kvmpa: kernelpt is NULL");
  pte = walk(myproc()->kernelpt, va, 0);
//  pte = walk(kernel_pagetable, va, 0);

  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
    {
//      printf("mappages: walk failed for va=%p in pagetable=%p\n", a, pagetable);
      return -1;
    }
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
//  uint64 n, va0, pa0;
//
//  while(len > 0){
//    va0 = PGROUNDDOWN(srcva);
//    pa0 = walkaddr(pagetable, va0);
//    if(pa0 == 0)
//      return -1;
//    n = PGSIZE - (srcva - va0);
//    if(n > len)
//      n = len;
//    memmove(dst, (void *)(pa0 + (srcva - va0)), n);//(void *)(pa0 + (srcva - va0)，内核虚拟地址
//
//    len -= n;
//    dst += n;
//    srcva = va0 + PGSIZE;
//  }
//  return 0;
  return copyin_new(pagetable, dst, srcva, len);
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
//  uint64 n, va0, pa0;
//  int got_null = 0;
//
//  while(got_null == 0 && max > 0){
//    va0 = PGROUNDDOWN(srcva);
//    pa0 = walkaddr(pagetable, va0);
//    if(pa0 == 0)
//      return -1;
//    n = PGSIZE - (srcva - va0);
//    if(n > max)
//      n = max;
//
//    char *p = (char *) (pa0 + (srcva - va0));
//    while(n > 0){
//      if(*p == '\0'){
//        *dst = '\0';
//        got_null = 1;
//        break;
//      } else {
//        *dst = *p;
//      }
//      --n;
//      --max;
//      p++;
//      dst++;
//    }
//
//    srcva = va0 + PGSIZE;
//  }
//  if(got_null){
//    return 0;
//  } else {
//    return -1;
//  }
  return copyinstr_new(pagetable, dst, srcva, max);
}

// 递归打印页表
void vmprint_recursive(pagetable_t pagetable, int level) {
  if (pagetable == 0)
    return;

  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];

    if (pte & PTE_V) {  // 只打印有效的 PTE
      uint64 pa = PTE2PA(pte);  // 获取物理地址
      for (int j = 0; j < level; j++)
        printf(".. ");  // 根据层级打印缩进
      printf("%d: pte %p pa %p\n", i, pte, pa);

      if ((pte & PTE_R) == 0 && (pte & PTE_W) == 0 && (pte & PTE_X) == 0) {
        // 该 PTE 不是一个映射到物理页的叶子节点，而是一个页表指针
        vmprint_recursive((pagetable_t) PTE2PA(pte), level + 1);
      }
    }
  }
}

// 入口函数
void vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  vmprint_recursive(pagetable, 1);
}

// Just follow the kvmmap on vm.c
void
uvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(pagetable == 0)
    panic("uvmmap: pagetable is NULL");

  if(mappages(pagetable, va, sz, pa, perm) != 0)
    panic("uvmmap");
}

// Store kernel page table to SATP register
void
proc_inithart(pagetable_t kpt){
  w_satp(MAKE_SATP(kpt));
  sfence_vma();
}

// 供每个进程初始化内核页表
pagetable_t kvmmake() {
//  pagetable_t kpgtbl = (pagetable_t)kalloc();
  pagetable_t kernelpt = uvmcreate();
  if (kernelpt == 0) return 0;
  memset(kernelpt, 0, PGSIZE);

  // 添加标准内核映射
  uvmmap(kernelpt, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  uvmmap(kernelpt, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  uvmmap(kernelpt, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  uvmmap(kernelpt, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  uvmmap(kernelpt, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);
  uvmmap(kernelpt, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);
  uvmmap(kernelpt, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
  return kernelpt;
}

// 实现仅释放页表结构而不释放物理页的函数。此函数仅释放页表页本身（如页目录、页表页），不释放叶子页表项指向的物理页。
// 对共享的物理页（如设备内存）的映射会被清除，但物理页不会被释放。
void free_kernel_pgtable(pagetable_t pagetable) {
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V) {
      uint64 child = PTE2PA(pte);
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0) {  // 非叶子页表项？
        free_kernel_pgtable((pagetable_t)child);// 递归释放下级页表页
      }
      pagetable[i] = 0;  // 清除当前页表项
    }
  }
  kfree((void*)pagetable);  // 释放当前页表页
}

/*deepseek
// 将用户页表的映射复制到内核页表，去除PTE_U标志
int
copy_user_mappings(pagetable_t kpgtbl, pagetable_t upgtbl, uint64 start, uint64 end)
{
  pte_t *upte;
  uint64 pa;
  uint flags;

  for(uint64 va = start; va < end; va += PGSIZE){
    // 查找用户页表中的页表条目
    if((upte = walk(upgtbl, va, 0)) == 0)
      continue;
    // 检查页表条目是否有效
    if((*upte & PTE_V) == 0)
      continue;
    pa = PTE2PA(*upte);// 获取物理地址
    flags = (PTE_FLAGS(*upte) & ~PTE_U) | PTE_W; // 移除PTE_U，添加PTE_W（按需）
    if(mappages(kpgtbl, va, PGSIZE, pa, flags) != 0)// 将映射复制到内核页表
    {
      return -1;
    }
  }
  return 0;
}

// 取消内核页表中的用户映射（用于exec等场景）
void
uvmunmap_proc_kpgtbl(pagetable_t kpgtbl, uint64 va, uint64 npages, int do_free)
{
  for(uint64 a = va; a < va + npages*PGSIZE; a += PGSIZE){
    pte_t *pte = walk(kpgtbl, a, 0);
    if(pte == 0)
      continue;
    if(PTE_FLAGS(*pte) & PTE_V){
      uint64 pa = PTE2PA(*pte);
      *pte = 0;
      if(do_free)
        kfree((void*)pa);
    }
  }
}*/

/*//答案
void
u2kvmcopy(pagetable_t pagetable, pagetable_t kernelpt, uint64 oldsz, uint64 newsz){
  pte_t *pte_from, *pte_to;
  oldsz = PGROUNDUP(oldsz);
  for (uint64 i = oldsz; i < newsz; i += PGSIZE){
    if((pte_from = walk(pagetable, i, 0)) == 0)
      panic("u2kvmcopy: src pte does not exist");
    if((pte_to = walk(kernelpt, i, 1)) == 0)
      panic("u2kvmcopy: pte walk failed");
    uint64 pa = PTE2PA(*pte_from);
    uint flags = (PTE_FLAGS(*pte_from)) & (~PTE_U);
    *pte_to = PA2PTE(pa) | flags;
  }
}
*/


int
utok_mappages(pagetable_t kpagetable, uint64 va, uint64 size, uint64 pa, int perm)    //为新映射装载PTE
{          //在kpagetable中，flags标志位，大小为size，将pa指向的物理地址与va指向的虚拟地址做映射
  uint64 a, last;                                    //这里的va一定是在PLIC下面的
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(kpagetable, a, 1)) == 0)
      return -1;
    //唯一不同，将remap的panic去掉

    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}



//将pagetable从begin开始，到end，进程大小sz，复制到kpagetable中
int
utok_vmcopy(pagetable_t pagetable,pagetable_t kpagetable, uint64 begin, uint64 end)
{           //
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  begin = PGROUNDUP(begin);  //用来对齐4096字节

  for(i = begin; i < end; i += PGSIZE){
    if((pte = walk(pagetable, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte) & (~PTE_U);   //这里把PTE_U的标志位去掉，这样在内核中就可以访问这个PTE


    if(utok_mappages(kpagetable, i, PGSIZE, pa, flags) != 0){         //这里我们不开辟新的内存空间，只是把映射拷贝过去
      goto err;
    }
  }
  return 0;

  err:
   uvmunmap(kpagetable, 0, i / PGSIZE, 1);   //若失败了，就将刚刚循环中完成的映射都给释放掉
  return -1;
}