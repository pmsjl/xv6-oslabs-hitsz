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

#define NBUCKET 13

struct {
  struct spinlock eviction;
  struct buf buf[NBUF];
  struct {
    struct spinlock lock;
    struct buf *head;
  } bucket[NBUCKET];
} bcache;

static uint
bucket_index(uint dev, uint blockno)
{
  return (dev * 31U + blockno) % NBUCKET;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.eviction, "bcache.eviction");
  for(int index = 0; index < NBUCKET; index++)
    initlock(&bcache.bucket[index].lock, "bcache.bucket");
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->dev = ~0U;
    b->blockno = b - bcache.buf;
    uint index = bucket_index(b->dev, b->blockno);
    b->next = bcache.bucket[index].head;
    bcache.bucket[index].head = b;
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint target = bucket_index(dev, blockno);
  acquire(&bcache.bucket[target].lock);

  // Is the block already cached?
  for(b = bcache.bucket[target].head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket[target].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket[target].lock);

  acquire(&bcache.eviction);
  acquire(&bcache.bucket[target].lock);
  for(b = bcache.bucket[target].head; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bucket[target].lock);
      release(&bcache.eviction);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket[target].lock);

  for(uint donor = 0; donor < NBUCKET; donor++) {
    uint first = donor < target ? donor : target;
    uint second = donor < target ? target : donor;
    acquire(&bcache.bucket[first].lock);
    if(first != second) acquire(&bcache.bucket[second].lock);
    struct buf **link = &bcache.bucket[donor].head;
    while(*link && (*link)->refcnt != 0) link = &(*link)->next;
    b = *link;
    if(b) {
      *link = b->next;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->next = bcache.bucket[target].head;
      bcache.bucket[target].head = b;
      if(first != second) release(&bcache.bucket[second].lock);
      release(&bcache.bucket[first].lock);
      release(&bcache.eviction);
      acquiresleep(&b->lock);
      return b;
    }
    if(first != second) release(&bcache.bucket[second].lock);
    release(&bcache.bucket[first].lock);
  }
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
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint index = bucket_index(b->dev, b->blockno);
  acquire(&bcache.bucket[index].lock);
  b->refcnt--;
  release(&bcache.bucket[index].lock);
}

void
bpin(struct buf *b) {
  uint index = bucket_index(b->dev, b->blockno);
  acquire(&bcache.bucket[index].lock);
  b->refcnt++;
  release(&bcache.bucket[index].lock);
}

void
bunpin(struct buf *b) {
  uint index = bucket_index(b->dev, b->blockno);
  acquire(&bcache.bucket[index].lock);
  b->refcnt--;
  release(&bcache.bucket[index].lock);
}

