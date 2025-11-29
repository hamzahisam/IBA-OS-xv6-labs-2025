#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Mixed workload test: CPU-bound vs I/O-bound side by side
// Demonstrates scheduler prioritizes I/O-bound over CPU-bound.
int
main(int argc, char *argv[])
{
  int cpid1, cpid2;
  
  printf("=== Comprehensive Mixed Workload Test ===\n");
  printf("Running CPU-bound and I/O-bound processes concurrently.\n");
  printf("This shows the scheduler's fairness: CPU-bound demotes, I/O-bound stays high.\n\n");
  
  // Fork CPU-bound process
  cpid1 = fork();
  if(cpid1 == 0) {
    printf("[CPU-BOUND] Starting long computation...\n");
    volatile int dummy = 0;
    
    // Do lots of CPU work continuously - should demote significantly
    for(int iter = 0; iter < 40; iter++) {
      for(long j = 0; j < 10000000; j++) {
        dummy = dummy + j;
        dummy = dummy % 1000000;
      }
      
      // Checkpoint every 10 iterations
      struct procinfo info;
      if(iter % 10 == 0 && getprocinfo(&info) == 0) {
        printf("[CPU-BOUND] Iteration %d: Q%d (slices=%d)\n", 
               iter, info.priority, info.time_slices);
      }
    }
    
    struct procinfo final;
    if(getprocinfo(&final) == 0) {
      printf("[CPU-BOUND] Final: Q%d (TimeSlices=%d)\n", 
             final.priority, final.time_slices);
    }
    exit(0);
  }
  
  // Small delay to let CPU process start first
  pause(1);
  
  // Fork I/O-bound process
  cpid2 = fork();
  if(cpid2 == 0) {
    printf("[I/O-BOUND] Starting brief work + frequent sleeps...\n");
    
    // Do brief work then sleep - should stay high priority
    for(int iter = 0; iter < 25; iter++) {
      volatile int dummy = 0;
      for(long j = 0; j < 2000000; j++) {
        dummy = dummy + j;
      }
      
      pause(1);  // Sleep - yields before quantum exhausted
      
      // Checkpoint every 5 iterations
      struct procinfo info;
      if(iter % 5 == 0 && getprocinfo(&info) == 0) {
        printf("[I/O-BOUND] Iteration %d: Q%d (slices=%d)\n", 
               iter, info.priority, info.time_slices);
      }
    }
    
    struct procinfo final;
    if(getprocinfo(&final) == 0) {
      printf("[I/O-BOUND] Final: Q%d (TimeSlices=%d)\n", 
             final.priority, final.time_slices);
    }
    exit(0);
  }
  
  // Parent waits for both children
  wait(0);
  wait(0);
  
  printf("\n=== Test Complete ===\n");
  printf("Expected Behavior:\n");
  printf("  CPU-bound:  Demoted to Q2 or Q3 (lower priority)\n");
  printf("  I/O-bound:  Stayed in Q0 or Q1 (higher priority)\n");
  printf("  Result:     I/O-bound process got preference despite CPU competition\n");
  
  exit(0);
}

