// Kernel log ring buffer implementation

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"

#define KLOG_BUFSIZE 4096

static struct {
  struct spinlock lock;
  char buf[KLOG_BUFSIZE];
  uint r;  // read index
  uint w;  // write index
} klog;

extern volatile int panicking;

void
kloginit(void)
{
  initlock(&klog.lock, "klog");
  klog.r = 0;
  klog.w = 0;
}

// Put a character into the ring buffer
// Safe to call from interrupt context or while panicking
void
klog_putc(int c)
{
  // If panicking, write without lock to avoid deadlock
  if(panicking) {
    klog.buf[klog.w % KLOG_BUFSIZE] = c;
    klog.w++;
    if(klog.w - klog.r > KLOG_BUFSIZE)
      klog.r = klog.w - KLOG_BUFSIZE;
    return;
  }

  acquire(&klog.lock);
  klog.buf[klog.w % KLOG_BUFSIZE] = c;
  klog.w++;
  
  // If buffer full, advance read pointer (drop oldest)
  if(klog.w - klog.r > KLOG_BUFSIZE)
    klog.r = klog.w - KLOG_BUFSIZE;
  
  // Wake up any readers waiting for data
  wakeup(&klog.r);
  release(&klog.lock);
}

// Read up to n bytes from klog buffer into user space
// Blocks if no data available initially, but returns when some data is read
int
klog_read_to_user(int user_dst, uint64 dst, int n)
{
  int i = 0;
  
  acquire(&klog.lock);
  
  // Wait for at least some data if buffer is empty
  while(klog.r == klog.w) {
    if(killed(myproc())) {
      release(&klog.lock);
      return -1;
    }
    sleep(&klog.r, &klog.lock);
  }
  
  // Read available data up to n bytes
  while(i < n && klog.r < klog.w) {
    // Read one byte
    char c = klog.buf[klog.r % KLOG_BUFSIZE];
    klog.r++;
    
    // Copy to user space
    if(either_copyout(user_dst, dst + i, &c, 1) == -1) {
      release(&klog.lock);
      return -1;
    }
    i++;
  }
  
  release(&klog.lock);
  return i;
}

// Drop any pending log data.
void
klogclear(void)
{
  if(panicking)
    return;
  acquire(&klog.lock);
  klog.r = klog.w;
  wakeup(&klog.r);
  release(&klog.lock);
}
