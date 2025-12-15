#include "kernel/types.h"
#include "user/user.h"

static char buf[512];

int
main(int argc, char *argv[])
{
  int n;

  (void)argc;
  (void)argv;

  

  for(;;){
    // Đọc log từ kernel buffer
    n = klogread(buf, sizeof(buf));
    
    if(n < 0){
      fprintf(2, "klogd: klogread failed\n");
      exit(1);
    }

    // Nếu không có log mới, quay lại đầu vòng lặp
    if(n == 0)
      continue;

    // In log ra màn hình console
    if(write(1, buf, n) != n){
      fprintf(2, "klogd: write failed\n");
      exit(1);
    }
    // Ngủ 5 ticks (khoảng 0.5 giây trong xv6) để nhường quyền in cho process khác
    // Điều này giúp tách biệt log syscall và output của lệnh echo.
    pause(5);
  }
}