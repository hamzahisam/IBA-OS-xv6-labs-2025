#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// CPU-bound test: Long-running intensive computation
// Should demote from Q0 -> Q1 -> Q2 -> Q3 over time
int
main(int argc, char *argv[])
{
  struct procinfo info;
  volatile int dummy = 0;
  
  printf("=== Comprehensive CPU-Bound Test ===\n");
  printf("This process will run CPU-intensive work for ~30 seconds.\n");
  printf("Watch priority change from Q0 -> Q1 -> Q2 -> Q3\n\n");
  
  if(getprocinfo(&info) == 0) {
    printf("Starting: PID=%d, Priority=Q%d, TimeSlices=%d\n\n", 
           info.pid, info.priority, info.time_slices);
  }
  
  // Massive continuous CPU work without any yields
  // Each iteration takes significant time (multiple timer ticks)
  printf("Running continuous computation (no sleeps, no syscalls)...\n");
  printf("Expected demotions:\n");
  printf("  0-2 ticks   : Q0 (initial)\n");
  printf("  2-6 ticks   : Q1 (after first demotion)\n");
  printf("  6-14 ticks  : Q2 (after second demotion)\n");
  printf("  14+ ticks   : Q3 (after third demotion)\n\n");
  
  // Do MASSIVE amount of work - much more than purecpu
  // 500M iterations = ~5 seconds of continuous work
  // Should easily hit all 4 queues multiple times
  for(int iter = 0; iter < 50; iter++) {
    // Each inner loop: 10M iterations
    for(long j = 0; j < 10000000; j++) {
      dummy = dummy + j;
      dummy = dummy % 1000000;
    }
    
    // Every iteration, check status
    if(getprocinfo(&info) == 0) {
      printf("Checkpoint %d: Priority=Q%d, TimeSlices=%d\n", 
             iter, info.priority, info.time_slices);
    }
    
    // Stop if we've seen enough data
    if(iter == 30) {
      printf("\n(Continuing in background for full test...)\n");
    }
  }
  
  // Final check
  printf("\n");
  if(getprocinfo(&info) == 0) {
    printf("Final Result: Priority=Q%d, TimeSlices=%d\n", 
           info.priority, info.time_slices);
    
    if(info.priority == 3) {
      printf("✓ EXCELLENT: Reached Q3 (CPU-bound category)\n");
    } else if(info.priority == 2) {
      printf("✓ GOOD: Reached Q2 (mid-range demotion)\n");
    } else if(info.priority == 1) {
      printf("~ PARTIAL: Only reached Q1\n");
    } else {
      printf("✗ FAILED: Stayed at Q0\n");
    }
  }
  
  exit(0);
}
