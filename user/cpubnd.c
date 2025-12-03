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
  
  printf("--------------------------------------\n");
  printf("       CPU-BOUND WORKLOAD TEST        \n");
  printf("--------------------------------------\n");
  printf("Running heavy CPU computation for ~30 sec\n");
  printf("Observing priority demotion: Q0 -> Q1 -> Q2 -> Q3\n\n");
  
  if(getprocinfo(&info) == 0) {
    printf("[INIT] PID: %d | Queue: Q%d | Slices: %d\n\n", 
           info.pid, info.priority, info.time_slices);
  }
  
  // Massive continuous CPU work without any yields
  // Each iteration takes significant time (multiple timer ticks)
  printf("Executing intensive loop (pure CPU, no I/O)...\n");
  printf("Demotion schedule:\n");
  printf("  * Q0: 0-2 ticks (start)\n");
  printf("  * Q1: 2-6 ticks (1st demotion)\n");
  printf("  * Q2: 6-14 ticks (2nd demotion)\n");
  printf("  * Q3: 14+ ticks (final level)\n\n");
  
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
      printf("  [%02d] Queue=Q%d, Slices=%d\n", 
             iter, info.priority, info.time_slices);
    }
    
    // Stop if we've seen enough data
    if(iter == 30) {
      printf("\n  ... continuing to completion ...\n");
    }
  }
  
  // Final check
  printf("\n--------------------------------------\n");
  if(getprocinfo(&info) == 0) {
    printf("RESULT: Queue=Q%d | Total Slices=%d\n", 
           info.priority, info.time_slices);
    
    if(info.priority == 3) {
      printf("STATUS: [PASS] Reached lowest queue Q3\n");
    } else if(info.priority == 2) {
      printf("STATUS: [OK] Demoted to Q2\n");
    } else if(info.priority == 1) {
      printf("STATUS: [WARN] Only reached Q1\n");
    } else {
      printf("STATUS: [FAIL] No demotion occurred\n");
    }
  }
  printf("--------------------------------------\n");
  
  exit(0);
}
