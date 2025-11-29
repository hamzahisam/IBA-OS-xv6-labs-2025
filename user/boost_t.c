#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Test manual boostproc() syscall
// Demonstrates explicit priority reset functionality
int
main(int argc, char *argv[])
{
  struct procinfo info;
  volatile int dummy = 0;
  
  printf("=== Manual Boost Test (Week 3) ===\n");
  printf("Testing boostproc() system call\n\n");
  
  if(getprocinfo(&info) == 0) {
    printf("Initial state: PID=%d, Q%d, slices=%d\n", 
           info.pid, info.priority, info.time_slices);
  }
  
  printf("\nPhase 1: Demote to lower priority through CPU work\n");
  // Do enough work to demote to Q2 or Q3
  for(int iter = 0; iter < 20; iter++) {
    for(long j = 0; j < 10000000; j++) {
      dummy = dummy + j;
      dummy = dummy % 1000000;
    }
    
    if(iter % 5 == 0 && getprocinfo(&info) == 0) {
      printf("  Iteration %d: Q%d (slices=%d)\n", 
             iter, info.priority, info.time_slices);
    }
  }
  
  if(getprocinfo(&info) == 0) {
    printf("\nBefore boost: Q%d, slices=%d\n", 
           info.priority, info.time_slices);
  }
  
  printf("\nPhase 2: Manually trigger priority boost\n");
  printf("Calling boostproc()...\n");
  boostproc();
  
  // Small delay to let boost take effect
  pause(2);
  
  if(getprocinfo(&info) == 0) {
    printf("After boost: Q%d, slices=%d\n", 
           info.priority, info.time_slices);
    
    if(info.priority == 0 && info.time_slices == 0) {
      printf("\n✓ SUCCESS: Process was boosted to Q0 with reset slices!\n");
    } else {
      printf("\n✗ FAILED: Boost didn't work correctly\n");
    }
  }
  
  printf("\nPhase 3: Verify normal demotion still works\n");
  for(int iter = 0; iter < 10; iter++) {
    for(long j = 0; j < 10000000; j++) {
      dummy = dummy + j;
      dummy = dummy % 1000000;
    }
  }
  
  if(getprocinfo(&info) == 0) {
    printf("After more work: Q%d (should have demoted again)\n", info.priority);
    
    if(info.priority > 0) {
      printf("✓ Demotion still working after manual boost\n");
    }
  }
  
  printf("\n=== Manual Boost Test Complete ===\n");
  
  exit(0);
}