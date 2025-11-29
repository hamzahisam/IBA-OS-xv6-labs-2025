#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Diagnostic test to check if time slices are being tracked
int
main(int argc, char *argv[])
{
  struct procinfo info;
  
  printf("=== MLFQ Diagnostics ===\n\n");
  
  // Check initial state
  if(getprocinfo(&info) == 0) {
    printf("Initial state:\n");
    printf("  PID: %d\n", info.pid);
    printf("  State: %d\n", info.state);
    printf("  Priority: Q%d\n", info.priority);
    printf("  Time Slices: %d\n\n", info.time_slices);
  }
  
  // Do a small amount of work and check repeatedly
  printf("Running work bursts and checking time_slices:\n");
  for(int i = 0; i < 20; i++) {
    // Work burst - enough to trigger timer interrupts
    volatile int dummy = 0;
    for(int j = 0; j < 5000000; j++) {
      dummy++;
    }
    
    if(getprocinfo(&info) == 0) {
      printf("  Iteration %2d: slices=%d priority=Q%d\n", 
             i, info.time_slices, info.priority);
      
      // If time_slices is still 0 after 10 iterations, something is wrong
      if(i == 10 && info.time_slices == 0) {
        printf("\n*** ERROR: time_slices not incrementing! ***\n");
        printf("*** Timer interrupts may not be working ***\n\n");
        break;
      }
      
      // If we see demotion, report it
      if(info.priority > 0) {
        printf("\n*** GOOD: Process demoted to Q%d after %d slices! ***\n\n", 
               info.priority, info.time_slices);
        break;
      }
    }
  }
  
  // Final state
  if(getprocinfo(&info) == 0) {
    printf("\nFinal state:\n");
    printf("  Priority: Q%d\n", info.priority);
    printf("  Time Slices: %d\n", info.time_slices);
    
    if(info.time_slices == 0) {
      printf("\n=== DIAGNOSIS ===\n");
      printf("Problem: time_slices never incremented\n");
      printf("Possible causes:\n");
      printf("  1. Timer interrupts not firing (check clockintr)\n");
      printf("  2. which_dev != 2 in usertrap()\n");
      printf("  3. p->time_slices++ not executing\n");
      printf("\nAdd debug prints in kernel/trap.c:\n");
      printf("  printf(\"Timer: which_dev=%%d pid=%%d\\n\", which_dev, p->pid);\n");
    } else {
      printf("\n=== SUCCESS ===\n");
      printf("Time slice tracking is working!\n");
    }
  }
  
  exit(0);
}
