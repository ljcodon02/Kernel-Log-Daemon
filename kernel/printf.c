//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "klog.h" 

volatile int panicking = 0; // printing a panic message
volatile int panicked = 0; // spinning forever at end of a panic

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
} pr;

// Cờ kiểm soát chế độ in:
// 0: In bình thường (Ra màn hình + Log)
// 1: Chỉ ghi Log (Không ra màn hình)
static int logging_only = 0;

static char digits[] = "0123456789abcdef";

// Hàm trung gian để điều hướng ký tự
static void
putc_dispatch(int c)
{
  if(logging_only) {
    // Chế độ im lặng: Chỉ ghi vào bộ đệm log
    klog_putc(c);
  } else {
    // Chế độ bình thường: Gọi consputc 
    // (consputc trong console.c của bạn đã lo việc in ra UART + ghi klog)
    consputc(c);
  }
}

static void
printint(long long xx, int base, int sign)
{
  char buf[20];
  int i;
  unsigned long long x;

  if(sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    putc_dispatch(buf[i]); // Sửa thành putc_dispatch
}

static void
printptr(uint64 x)
{
  int i;
  putc_dispatch('0'); // Sửa thành putc_dispatch
  putc_dispatch('x'); // Sửa thành putc_dispatch
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    putc_dispatch(digits[x >> (sizeof(uint64) * 8 - 4)]); // Sửa thành putc_dispatch
}

// Print to the console (Standard printf)
int
printf(char *fmt, ...)
{
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;

  if(panicking == 0)
    acquire(&pr.lock);

  // Đảm bảo cờ tắt để in ra màn hình
  logging_only = 0;

  va_start(ap, fmt);
  for(i = 0; (cx = fmt[i] & 0xff) != 0; i++){
    if(cx != '%'){
      putc_dispatch(cx);
      continue;
    }
    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    if(c0) c1 = fmt[i+1] & 0xff;
    if(c1) c2 = fmt[i+2] & 0xff;
    if(c0 == 'd'){
      printint(va_arg(ap, int), 10, 1);
    } else if(c0 == 'l' && c1 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if(c0 == 'u'){
      printint(va_arg(ap, uint32), 10, 0);
    } else if(c0 == 'l' && c1 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if(c0 == 'x'){
      printint(va_arg(ap, uint32), 16, 0);
    } else if(c0 == 'l' && c1 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if(c0 == 'p'){
      printptr(va_arg(ap, uint64));
    } else if(c0 == 'c'){
      putc_dispatch(va_arg(ap, uint));
    } else if(c0 == 's'){
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        putc_dispatch(*s);
    } else if(c0 == '%'){
      putc_dispatch('%');
    } else if(c0 == 0){
      break;
    } else {
      // Print unknown % sequence to draw attention.
      putc_dispatch('%');
      putc_dispatch(c0);
    }
  }
  va_end(ap);

  if(panicking == 0)
    release(&pr.lock);

  return 0;
}

// Print ONLY to Kernel Log Buffer (Silent printf)
int
klog_printf(char *fmt, ...)
{
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;

  if(panicking == 0)
    acquire(&pr.lock);

  // BẬT chế độ chỉ ghi log
  logging_only = 1;

  va_start(ap, fmt);
  // (Copy nguyên logic vòng lặp của printf)
  for(i = 0; (cx = fmt[i] & 0xff) != 0; i++){
    if(cx != '%'){
      putc_dispatch(cx);
      continue;
    }
    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    if(c0) c1 = fmt[i+1] & 0xff;
    if(c1) c2 = fmt[i+2] & 0xff;
    if(c0 == 'd'){
      printint(va_arg(ap, int), 10, 1);
    } else if(c0 == 'l' && c1 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if(c0 == 'u'){
      printint(va_arg(ap, uint32), 10, 0);
    } else if(c0 == 'l' && c1 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if(c0 == 'x'){
      printint(va_arg(ap, uint32), 16, 0);
    } else if(c0 == 'l' && c1 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if(c0 == 'p'){
      printptr(va_arg(ap, uint64));
    } else if(c0 == 'c'){
      putc_dispatch(va_arg(ap, uint));
    } else if(c0 == 's'){
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        putc_dispatch(*s);
    } else if(c0 == '%'){
      putc_dispatch('%');
    } else if(c0 == 0){
      break;
    } else {
      putc_dispatch('%');
      putc_dispatch(c0);
    }
  }
  va_end(ap);

  // TẮT chế độ chỉ ghi log (trả lại mặc định)
  logging_only = 0;

  if(panicking == 0)
    release(&pr.lock);

  return 0;
}

void
panic(char *s)
{
  panicking = 1;
  printf("panic: ");
  printf("%s\n", s);
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
    ;
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
  kloginit();
}