#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Pure CPU-bound test: Does ONLY computation, checks priority at end
int
main(int argc, char *argv[])
{
  struct procinfo info;
  volatile int dummy = 0;
  
  printf("\n***********************************\n");
  printf("*     PURE CPU-BOUND TEST         *\n");
  printf("***********************************\n");
  
  if(getprocinfo(&info) == 0) {
    printf("[INIT] PID: %d | Queue: Q%d | Slices: %d\n\n", 
           info.pid, info.priority, info.time_slices);
  }
  
  printf("Executing continuous CPU work...\n");
  printf("(This takes several seconds)\n\n");
  
  // Do MASSIVE amount of work without any syscalls
  // Increased significantly to trigger multiple demotions
  for(int i = 0; i < 500000000; i++) {
    dummy = dummy + i;
    // Add more computation to slow it down
    if(i % 1000 == 0) {
      dummy = dummy * 2;
      dummy = dummy / 2;
    }
  }
  
  printf("...done! Checking results.\n\n");
  
  if(getprocinfo(&info) == 0) {
    printf("[FINAL] PID: %d | Queue: Q%d | Slices: %d\n", 
           info.pid, info.priority, info.time_slices);
    printf("\nExpected:\n");
    printf("  * Many timer ticks consumed\n");
    printf("  * Demoted through queues\n");
    printf("  * End at Q2 or Q3\n\n");
    
    if(info.priority >= 2) {
      printf("STATUS: [PASS] Demoted to Q%d\n", info.priority);
    } else if(info.priority == 1) {
      printf("STATUS: [WARN] Only reached Q1\n");
    } else {
      printf("STATUS: [FAIL] Stuck at Q0\n");
    }
  }
  printf("***********************************\n");
  
  exit(0);
}
