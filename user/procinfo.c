#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct procinfo info;
  
  // Get information about current process
  if(getprocinfo(&info) < 0) {
    printf("getprocinfo failed\n");
    exit(1);
  }
  
  printf("Process Information:\n");
  printf("  PID: %d\n", info.pid);
  printf("  State: %d\n", info.state);
  printf("  Priority Queue: %d (0=highest, 3=lowest)\n", info.priority);
  printf("  Time Slices Used: %d\n", info.time_slices);
  printf("\nMLFQ Scheduler Test - Week 1\n");
  printf("Successfully retrieved process info via getprocinfo syscall!\n");
  
  exit(0);
}
