// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU]; // N 个 CPU 各维护一段

void
kinit()
{
  // 为每一个 CPU kem段分配一把锁
  for(int i=0;i<NCPU;++i){
    initlock(&kmem[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int cpu_id; // cpu Id
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  // 获取 cpu Id 
  push_off();
  cpu_id=cpuid();
  pop_off();

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpu_id;
  // 获取 cpu Id 
  push_off();
  cpu_id=cpuid();
  pop_off();

  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;

  // 若当前cpu维护列表有空闲块，则ok；若无则尝试从其他 CPU “窃取”一段
  if(r){
    kmem[cpu_id].freelist = r->next;
  }
  else{
    for (int i = 0; i < NCPU; i++) {
      // 遍历其他 CPU 维护内存空间
      if (i == cpu_id) {
        continue;
      }
      acquire(&kmem[i].lock); // 不要忘记锁住
      r = kmem[i].freelist;
      if(r){
        kmem[i].freelist = r->next;
      }
      release(&kmem[i].lock); // 不要忘记释放锁
      if(r) {
        break;
      }
    }
  }
  release(&kmem[cpu_id].lock);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
