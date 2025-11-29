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
  
  printf("=== Comprehensive I/O-Bound Test ===\n");
  printf("This process will alternate between brief work and sleep.\n");
  printf("It should STAY in Q0 (never demote) because it yields frequently.\n\n");
  
  if(getprocinfo(&info) == 0) {
    printf("Starting: PID=%d, Priority=Q%d, TimeSlices=%d\n\n", 
           info.pid, info.priority, info.time_slices);
  }
  
  printf("Running 30 iterations of: brief_work() -> sleep(1_tick)\n");
  printf("Expected: Priority stays Q0 throughout (slices reset after sleep)\n\n");
  
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
        printf("Iteration %d: Priority=Q%d, TimeSlices=%d\n", 
               iter, info.priority, info.time_slices);
      }
    }
    
    // Verify we stay in high priority
    if(iter == 10 || iter == 20 || iter == 29) {
      if(getprocinfo(&info) == 0) {
        printf("  [Checkpoint] Still at Q%d\n", info.priority);
      }
    }
  }
  
  printf("\n");
  if(getprocinfo(&info) == 0) {
    printf("Final Result: Priority=Q%d, TimeSlices=%d\n", 
           info.priority, info.time_slices);
    
    if(info.priority <= 1) {
      printf("✓ SUCCESS: Stayed in Q%d (high priority maintained!)\n", info.priority);
      printf("  Demonstrates I/O-bound processes get preferential treatment.\n");
    } else {
      printf("✗ FAILED: Demoted to Q%d (should have stayed Q0/Q1)\n", info.priority);
      printf("  Sleep/pause may not be properly resetting time slices!\n");
    }
  }
  
  exit(0);
}
