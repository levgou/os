// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock;
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf*);

// Wait for IDE disk to become ready.
static int
idewait(int checkerr)
{
  int r;

  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

void
ideinit(void)
{
  int i;

  initlock(&idelock, "ide");
  ioapicenable(IRQ_IDE, ncpu - 1);
  idewait(0);

  // Check if disk 1 is present
  outb(0x1f6, 0xe0 | (1<<4));
  for(i=0; i<1000; i++){
    if(inb(0x1f7) != 0){
      havedisk1 = 1;
      break;
    }
  }

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));
}

// Start the request for b.  Caller must hold idelock.
static void
idestart(struct buf *b)
{
  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  if (sector_per_block > 7) panic("idestart");

  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, sector_per_block);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
  if(b->flags & B_DIRTY){
    outb(0x1f7, write_cmd);
    outsl(0x1f0, b->data, BSIZE/4);
  } else {
    outb(0x1f7, read_cmd);
  }
}

// Interrupt handler.
void
ideintr(void)
{
  struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock);

  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  // Read data if needed.
  if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  // Start disk on next buf in queue.
  if(idequeue != 0)
    idestart(idequeue);

  release(&idelock);
}

//PAGEBREAK!
// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
void
iderw(struct buf *b)
{
  struct buf **pp;

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");
  if(b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  acquire(&idelock);  //DOC:acquire-lock

  // Append b to idequeue.
  b->qnext = 0;
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue
    ;
  *pp = b;

  // Start disk if necessary.
  if(idequeue == b)
    idestart(b);

  // Wait for request to finish.
  while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
    sleep(b, &idelock);
  }


  release(&idelock);
}

void
get_ide_info(char buf[]) {
  uint blocks_buf[1000];
  int read_counter = 0;
  int write_counter = 0;
  acquire(&idelock);
  struct buf *cur_block = idequeue;

  memset(blocks_buf, 0, sizeof(blocks_buf));
  uint *p = blocks_buf;

  while (cur_block){
    if(!(cur_block->flags & B_VALID))
      read_counter++;
    if (cur_block->flags & B_DIRTY)
      write_counter++;

    *p++ = cur_block->dev;
    *p++ = cur_block->blockno;

    cur_block = cur_block->qnext;
  }
    release(&idelock);

  char *c = buf;
  char helper_buf[20] = {0};
  uint wait_ops = (uint)(read_counter + write_counter);
  uint_to_str(wait_ops, helper_buf);

  memmove(c, "Waiting operations: ", strlen("Waiting operations: "));
  c += strlen("Waiting operations: ");
  memmove(c, helper_buf, (uint)strlen(helper_buf));
  c += strlen(helper_buf);
  memmove(c++, "\n", 1);
  memset(helper_buf, 0, sizeof(helper_buf));

  memmove(c, "Read Waiting operations: ", strlen("Read Waiting operations: "));
  c += strlen("Read Waiting operations: ");
  uint_to_str((uint)read_counter, helper_buf);
  memmove(c, helper_buf, (uint)strlen(helper_buf));
  c += strlen(helper_buf);
  memmove(c++, "\n", 1);
  memset(helper_buf, 0, sizeof(helper_buf));

  memmove(c, "Write Waiting operations: ", strlen("Write Waiting operations: "));
  c += strlen("Write Waiting operations: ");
  uint_to_str((uint)write_counter, helper_buf);
  memmove(c, helper_buf, (uint)strlen(helper_buf));
  c += strlen(helper_buf);
  memmove(c++, "\n", 1);
  memset(helper_buf, 0, sizeof(helper_buf));

  memmove(c, "Working blocks: ", strlen("Working blocks: "));
  c += strlen("Working blocks: ");
  p = blocks_buf;
  while(*p) {
    memmove(c++, "(", 1);
    memmove(c++, p++, 1);
    memmove(c++, ",", 1);
    memmove(c++, p++, 1);
    memmove(c++, "(", 1);
  }
  memmove(c++, "\n", 1);

}
