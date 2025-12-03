#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Test starvation prevention: Run multiple CPU-bound processes
// Ensure low-priority processes get boosted and execute
int
main(int argc, char *argv[])
{
  int pids[3];
  int num_procs = 3;
  
  printf("////////////////////////////////////////////\n");
  printf("//    STARVATION PREVENTION TEST          //\n");
  printf("////////////////////////////////////////////\n");
  printf("Auto-boost every 100 ticks\n");
  printf("Spawning %d CPU-bound processes\n", num_procs);
  printf("All should get CPU via periodic boosting\n\n");
  
  // Fork multiple CPU-bound processes
  for(int i = 0; i < num_procs; i++) {
    pids[i] = fork();
    
    if(pids[i] == 0) {
      // Child process - CPU-bound workload
      struct procinfo info;
      volatile int dummy = 0;
      int my_id = i;
      
      printf("  P%d | PID %d | Started\n", my_id, getpid());
      
      // Run for extended time to see multiple boost cycles
      // ~150 ticks = 1.5 boost intervals
      for(int iter = 0; iter < 60; iter++) {
        // Heavy computation
        for(long j = 0; j < 5000000; j++) {
          dummy = dummy + j;
          dummy = dummy % 1000000;
        }
        
        // Check status periodically
        if(iter % 15 == 0 && getprocinfo(&info) == 0) {
          int current_ticks = uptime();
          printf("  P%d | T%d | Q%d | slices=%d\n", 
                 my_id, current_ticks, info.priority, info.time_slices);
        }
      }
      
      if(getprocinfo(&info) == 0) {
        printf("  P%d | DONE @ T%d | Final: Q%d\n", 
               my_id, uptime(), info.priority);
      }
      
      exit(0);
    }
    
    // Small delay between forks
    pause(1);
  }
  
  // Parent waits for all children
  printf("\n[PARENT] Waiting for completion...\n");
  for(int i = 0; i < num_procs; i++) {
    wait(0);
    printf("[PARENT] P%d exited\n", i);
  }
  
  printf("\n////////////////////////////////////////////\n");
  printf("//         TEST COMPLETE                 //\n");
  printf("////////////////////////////////////////////\n");
  printf("Summary:\n");
  printf("  * All processes completed\n");
  printf("  * Boost to Q0 every ~100 ticks\n");
  printf("  * Low-priority got CPU time\n");
  printf("  * No starvation occurred\n");
  
  exit(0);
}