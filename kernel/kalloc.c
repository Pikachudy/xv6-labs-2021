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
} kmem;

// COW 页面引用计数数组
int page_ref[(PHYSTOP-KERNBASE)/PGSIZE];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  // 初始化引用次数
  for(int i=0;i<(PHYSTOP-KERNBASE)/PGSIZE;++i){
    page_ref[i]=0;
  }
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 判断是否引用为0
  if(page_ref[((uint64)pa-KERNBASE)/PGSIZE]>1){
    //printf("kfree: page_ref[%d] -- %d to ",((uint64)pa-KERNBASE)/PGSIZE,page_ref[((uint64)pa-KERNBASE)/PGSIZE]);
    page_ref[((uint64)pa-KERNBASE)/PGSIZE]--;
    //printf("page_ref[%d] -- %d\n",((uint64)pa-KERNBASE)/PGSIZE,page_ref[((uint64)pa-KERNBASE)/PGSIZE]);
    return;
  }
  page_ref[((uint64)pa-KERNBASE)/PGSIZE] = 0;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    page_ref[((uint64)r-KERNBASE)/PGSIZE] = 1; // 引用初始化
  }
  return (void*)r;
}
