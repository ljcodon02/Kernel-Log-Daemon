// Kernel log ring buffer
#ifndef KLOG_H
#define KLOG_H

#include "types.h"

#define KLOG_BUFSIZE 4096

// Initialize kernel log subsystem
void kloginit(void);

// Put a character into the log buffer (safe from any context)
void klog_putc(int c);

// Read up to n bytes into user space (blocking)
int klog_read_to_user(int user_dst, uint64 dst, int n);

// Drop any pending data (advance read pointer to write pointer).
void klogclear(void);

#endif
