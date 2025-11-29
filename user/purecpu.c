#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Pure CPU-bound test: Does ONLY computation, checks priority at end
int
main(int argc, char *argv[])
{
  struct procinfo info;
  volatile int dummy = 0;
  
  printf("\n=== Pure CPU-Bound Test ===\n");
  
  if(getprocinfo(&info) == 0) {
    printf("Starting: PID=%d, Q%d, slices=%d\n\n", 
           info.pid, info.priority, info.time_slices);
  }
  
  printf("Running continuous CPU work (no interruptions)...\n");
  printf("This will take several seconds...\n\n");
  
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
  
  printf("Work complete! Checking final state...\n\n");
  
  if(getprocinfo(&info) == 0) {
    printf("Final: PID=%d, Q%d, slices=%d\n", 
           info.pid, info.priority, info.time_slices);
    printf("\nExpected behavior:\n");
    printf("  - Should have used many timer ticks\n");
    printf("  - Should have demoted through queues\n");
    printf("  - Should be at Q3 (or Q2/Q3)\n\n");
    
    if(info.priority >= 2) {
      printf("✓ SUCCESS: Demoted to Q%d\n", info.priority);
    } else if(info.priority == 1) {
      printf("~ PARTIAL: Only reached Q1 (expected Q2 or Q3)\n");
    } else {
      printf("✗ FAILED: Still at Q0 (no demotion occurred)\n");
    }
  }
  
  exit(0);
}
