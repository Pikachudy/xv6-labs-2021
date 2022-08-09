// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


struct {
  struct spinlock lock[NBUCKET];//一个桶一个锁
  struct buf buf[NBUF]; 
  struct spinlock bcache_lock; // bcache锁

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.

  struct buf head[NBUCKET];
} bcache;

extern uint ticks;
void
binit(void)
{
  struct buf *b;
  // 初始化锁
  initlock(&bcache.bcache_lock, "bcache_lock");
  for(int i=0;i<NBUCKET;++i){
    initlock(&bcache.lock[i],"bcache_bucket");
  }

  // 为每一个桶构建buffer双向链表
  for(int i=0;i<NBUCKET;++i){
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // 加至链表头
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int index = blockno % NBUCKET; // 块号-桶 相应索引

  acquire(&bcache.lock[index]);
  // 遍历该桶
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      // 若已缓存好
      b->refcnt++;
      release(&bcache.lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[index]);


  // 若没有缓存好
  acquire(&bcache.bcache_lock);
  acquire(&bcache.lock[index]);
  // 在执行89-90行过程中，已经释放了该桶的锁，此时有可能目标块被其他进程缓存到了该桶中，于是需要再遍历一遍该桶
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      // 若已缓存好
      b->refcnt++;
      release(&bcache.lock[index]);
      release(&bcache.bcache_lock); 
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  struct buf *lru_block = 0;
  int min_tick=0;
  for (b = bcache.head[index].next; b != &bcache.head[index]; b = b->next) {
    if (b->refcnt == 0 && (lru_block==0||b->lastuse_tick < min_tick)) {
      // 目前而言最近未使用
      min_tick = b->lastuse_tick;
      lru_block = b;
    }
  }
  // 若该桶未满，则lru_block为最近最久未使用，返回
  if(lru_block!=0){
    lru_block->dev = dev;
    lru_block->blockno = blockno;
    lru_block->refcnt++;
    lru_block->valid = 0;

    release(&bcache.lock[index]);
    release(&bcache.bcache_lock);
    // 需要返回加锁的buffer
    acquiresleep(&lru_block->lock);
    return lru_block;
  }

  //该桶没找到，则尝试窃取别的桶的块
  for (int other_index = (index + 1) % NBUCKET; other_index != index; other_index = (other_index + 1) % NBUCKET) {
    acquire(&bcache.lock[other_index]);
    // 遍历其他桶的块
    for (b = bcache.head[other_index].next; b != &bcache.head[other_index]; b = b->next) {
      if (b->refcnt == 0 && (lru_block==0||b->lastuse_tick < min_tick)) {
      // 目前而言最近未使用
      min_tick = b->lastuse_tick;
      lru_block = b;
    }
    }
    // 若其他桶有,则lru_block为最近最久未使用。
    // 将块迁移至本桶中，返回
    if(lru_block) {
      lru_block->dev = dev;
      lru_block->refcnt++;
      lru_block->valid = 0;
      lru_block->blockno = blockno;

      // 在原先桶中删除块
      lru_block->next->prev = lru_block->prev;
      lru_block->prev->next = lru_block->next;
      release(&bcache.lock[other_index]);

      // 加至当前桶中
      lru_block->next = bcache.head[index].next;
      lru_block->prev = &bcache.head[index];
      bcache.head[index].next->prev = lru_block;
      bcache.head[index].next = lru_block;
      release(&bcache.lock[index]);
      release(&bcache.bcache_lock);
      
      // 需要返回加锁的buffer
      acquiresleep(&lru_block->lock);
      return lru_block;
    }
    // 未找到则换下一个桶遍历，不要忘记将此桶解锁！
    release(&bcache.lock[other_index]); 
  }

  // 均无剩余
  release(&bcache.lock[index]);
  release(&bcache.bcache_lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock[b->blockno % NBUCKET]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    // 改用 ticks
    b->lastuse_tick=ticks;
  }
  
  release(&bcache.lock[b->blockno % NBUCKET]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno%NBUCKET]);
  b->refcnt++;
  release(&bcache.lock[b->blockno%NBUCKET]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno%NBUCKET]);
  b->refcnt--;
  release(&bcache.lock[b->blockno%NBUCKET]);
}


