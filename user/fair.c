#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Long-running fairness test: Multiple workloads competing
// Tests that boosting prevents starvation over extended period
int
main(int argc, char *argv[])
{
  int cpu_pid, io_pid, mixed_pid;
  
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  printf("@     LONG-RUNNING FAIRNESS TEST        @\n");
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  printf("3 workloads competing for ~200 ticks:\n");
  printf("  [1] CPU-bound (demotes, gets boosted)\n");
  printf("  [2] I/O-bound (stays high priority)\n");
  printf("  [3] Mixed (alternating pattern)\n\n");
  
  // Process 1: Pure CPU-bound
  cpu_pid = fork();
  if(cpu_pid == 0) {
    struct procinfo info;
    volatile int dummy = 0;
    int boost_count = 0;
    int prev_priority = 0;
    
    printf("<CPU> Launched\n");
    
    for(int iter = 0; iter < 80; iter++) {
      // Heavy computation
      for(long j = 0; j < 8000000; j++) {
        dummy = dummy + j;
        dummy = dummy % 1000000;
      }
      
      if(getprocinfo(&info) == 0) {
        // Detect boost (priority decreased from previous)
        if(info.priority < prev_priority) {
          boost_count++;
          printf("<CPU> ++ BOOST #%d @ T%d (Q%d-->Q%d)\n", 
                 boost_count, uptime(), prev_priority, info.priority);
        }
        prev_priority = info.priority;
        
        if(iter % 20 == 0) {
          printf("<CPU> T%d: Q%d slices=%d\n", 
                 uptime(), info.priority, info.time_slices);
        }
      }
    }
    
    if(getprocinfo(&info) == 0) {
      printf("<CPU> END: Q%d | boosts=%d\n", 
             info.priority, boost_count);
      if(boost_count >= 1) {
        printf("<CPU> [PASS] Got boosted\n");
      } else {
        printf("<CPU> [FAIL] No boosts seen\n");
      }
    }
    exit(0);
  }
  
  pause(2);
  
  // Process 2: I/O-bound
  io_pid = fork();
  if(io_pid == 0) {
    struct procinfo info;
    volatile int dummy = 0;
    
    printf("<I/O> Launched\n");
    
    for(int iter = 0; iter < 100; iter++) {
      // Brief work
      for(long j = 0; j < 1000000; j++) {
        dummy = dummy + j;
      }
      
      // Yield frequently
      pause(2);
      
      if(iter % 25 == 0 && getprocinfo(&info) == 0) {
        printf("<I/O> T%d: Q%d slices=%d\n", 
               uptime(), info.priority, info.time_slices);
      }
    }
    
    if(getprocinfo(&info) == 0) {
      printf("<I/O> END: Q%d\n", info.priority);
      if(info.priority <= 1) {
        printf("<I/O> [PASS] Stayed high priority\n");
      }
    }
    exit(0);
  }
  
  pause(2);
  
  // Process 3: Mixed workload
  mixed_pid = fork();
  if(mixed_pid == 0) {
    struct procinfo info;
    volatile int dummy = 0;
    
    printf("<MIX> Launched\n");
    
    for(int iter = 0; iter < 50; iter++) {
      // Alternate: CPU burst then sleep
      for(long j = 0; j < 15000000; j++) {
        dummy = dummy + j;
        dummy = dummy % 1000000;
      }
      
      pause(3);
      
      if(iter % 15 == 0 && getprocinfo(&info) == 0) {
        printf("<MIX> T%d: Q%d slices=%d\n", 
               uptime(), info.priority, info.time_slices);
      }
    }
    
    if(getprocinfo(&info) == 0) {
      printf("<MIX> END: Q%d\n", info.priority);
    }
    exit(0);
  }
  
  // Parent waits
  printf("\n[Parent] Processes running...\n");
  wait(0);
  wait(0);
  wait(0);
  
  printf("\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  printf("@         FAIRNESS TEST DONE            @\n");
  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  printf("All workloads finished.\n");
  printf("Expected:\n");
  printf("  * CPU-bound: Got boosts (no starvation)\n");
  printf("  * I/O-bound: Stayed Q0/Q1\n");
  printf("  * Mixed: Balanced Q1-Q2\n");
  
  exit(0);
}