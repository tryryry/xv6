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

uint bcache_ticks;

#define NBUCKETS 17
struct bucket {
  struct spinlock lock;
  struct buf head;
};

int hash_func(int n){
  return n % NBUCKETS;
}

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  struct bucket bucket[NBUCKETS];
} bcache;

void binit(void) {
  struct buf *b;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = b;
    b->prev = b;
  }
  bcache_ticks = 1;
  initlock(&bcache.lock, "bcache");
  for(int i = 0; i < NBUCKETS; i++){
    initlock(&bcache.bucket[i].lock, "bcache");
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;

  int index = hash_func(blockno);

  // Is the block already cached?
  acquire(&bcache.bucket[index].lock);
  for(b = bcache.bucket[index].head.next; b != &bcache.bucket[index].head; b = b->next){
     if (b->dev == dev && b->blockno == blockno) {
        b->refcnt++;
        b->ticks = bcache_ticks;
        bcache_ticks++;
        release(&bcache.bucket[index].lock);
        //release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
     }
  }
  release(&bcache.bucket[index].lock);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  acquire(&bcache.lock);
  acquire(&bcache.bucket[index].lock);
  int least_count = -1;
  int least_i = -1;
  for(int i = 0; i < NBUF; i++){
    if(bcache.buf[i].refcnt == 0 && (least_count == -1 || bcache.buf[i].ticks < least_count)){
      least_i = i;
      least_count = bcache.buf[i].ticks;
    }
  }
  if(least_i == -1)   panic("bget: no buffers");
  
  b = &bcache.buf[least_i];
  int prev_block_num = b->blockno;
  int prev_index = hash_func(prev_block_num);
  int pre_ticks = b->ticks;
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  
  b->valid = 0;
  //acquire(&bcache_ticks_lock);
  b->ticks = bcache_ticks;
  bcache_ticks++;
  //release(&bcache_ticks_lock);
  release(&bcache.lock);
  release(&bcache.bucket[index].lock);

  if(prev_index != index && pre_ticks != 0){
    if(prev_index < index){
      acquire(&bcache.bucket[prev_index].lock);
      acquire(&bcache.bucket[index].lock);
    }else{
      acquire(&bcache.bucket[index].lock);
      acquire(&bcache.bucket[prev_index].lock);
    }
  }else{
    acquire(&bcache.bucket[index].lock);
  }
    
  b->next->prev = b->prev;
  b->prev->next = b->next;
 
  b->next = bcache.bucket[index].head.next;
  b->prev = &bcache.bucket[index].head;
  bcache.bucket[index].head.next->prev = b;
  bcache.bucket[index].head.next = b;

  if(prev_index != index && pre_ticks != 0)
    release(&bcache.bucket[prev_index].lock);
  release(&bcache.bucket[index].lock);
  //release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
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
 
  int index = hash_func(b->blockno);
  acquire(&bcache.bucket[index].lock);
  //acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    //acquire(&bcache_ticks_lock);
    b->ticks = bcache_ticks;
    bcache_ticks++;
    //release(&bcache_ticks_lock);
  }
  //release(&bcache.lock);
  release(&bcache.bucket[index].lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


