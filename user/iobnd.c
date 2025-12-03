#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// I/O-bound test: Frequent yields (sleeps)
// Should stay in Q0 or Q1 (high priority) throughout
int
main(int argc, char *argv[])
{
  struct procinfo info;
  int iter;
  
  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  printf("        I/O-BOUND WORKLOAD TEST       \n");
  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  printf("Alternating between short work and sleep.\n");
  printf("Goal: Maintain Q0 priority (frequent yields).\n\n");
  
  if(getprocinfo(&info) == 0) {
    printf("[INIT] PID: %d | Queue: Q%d | Slices: %d\n\n", 
           info.pid, info.priority, info.time_slices);
  }
  
  printf("Pattern: brief_work -> sleep(1 tick) x 30 iterations\n");
  printf("Expect: Q0 maintained (slices reset on sleep)\n\n");
  
  // Simulate I/O-bound behavior: short bursts with frequent sleeps
  // 30 iterations × 1 second per iteration = ~30 seconds total
  for(iter = 0; iter < 30; iter++) {
    // Do minimal work (much less than 2 ticks - the Q0 quantum)
    volatile int dummy = 0;
    for(long j = 0; j < 2000000; j++) {  // Small computation
      dummy = dummy + j;
    }
    
    // Sleep (simulating I/O wait) - voluntarily yields
    // This is the KEY behavior: yields before exhausting quantum
    pause(1);  // Sleep for 1 tick (~100ms)
    
    // Check priority after wakeup
    if(getprocinfo(&info) == 0) {
      if(iter % 5 == 0) {  // Print every 5 iterations to reduce clutter
        printf("  [%02d] Queue=Q%d, Slices=%d\n", 
               iter, info.priority, info.time_slices);
      }
    }
    
    // Verify we stay in high priority
    if(iter == 10 || iter == 20 || iter == 29) {
      if(getprocinfo(&info) == 0) {
        printf("       >> checkpoint: Q%d <<\n", info.priority);
      }
    }
  }
  
  printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  if(getprocinfo(&info) == 0) {
    printf("RESULT: Queue=Q%d | Total Slices=%d\n", 
           info.priority, info.time_slices);
    
    if(info.priority <= 1) {
      printf("STATUS: [PASS] Maintained Q%d (high priority)\n", info.priority);
      printf("        I/O-bound behavior rewarded!\n");
    } else {
      printf("STATUS: [FAIL] Dropped to Q%d (expected Q0/Q1)\n", info.priority);
      printf("        Check sleep/pause slice reset logic.\n");
    }
  }
  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  
  exit(0);
}
