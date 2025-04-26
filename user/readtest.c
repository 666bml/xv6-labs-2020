//编写测试程序
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[512];
  int fd;
  int i;

  fd = open("README", 0);  // 只打开一次
  if(fd < 0){
    printf("open failed\n");
    exit(1);
  }

  for(i = 0; i < 1000; i++){
    read(fd, buf, sizeof(buf));  // 只读，不关
  }

  close(fd); // 全部读完后再关闭

  uint64 avg_cycles = getreadstats();
  printf("Average cycles per read: %d\n", (uint)avg_cycles);

  exit(0);
}
