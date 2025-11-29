#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Long-running fairness test: Multiple workloads competing
// Tests that boosting prevents starvation over extended period
int
main(int argc, char *argv[])
{
  int cpu_pid, io_pid, mixed_pid;
  
  printf("=== Long-Running Fairness Test (Week 3) ===\n");
  printf("Running 3 different workloads for ~200 ticks\n");
  printf("  1. Pure CPU-bound (should demote but get boosted)\n");
  printf("  2. Pure I/O-bound (should stay high priority)\n");
  printf("  3. Mixed workload\n\n");
  
  // Process 1: Pure CPU-bound
  cpu_pid = fork();
  if(cpu_pid == 0) {
    struct procinfo info;
    volatile int dummy = 0;
    int boost_count = 0;
    int prev_priority = 0;
    
    printf("[CPU-BOUND] Starting...\n");
    
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
          printf("[CPU-BOUND] BOOST #%d detected at tick %d (Q%d->Q%d)\n", 
                 boost_count, uptime(), prev_priority, info.priority);
        }
        prev_priority = info.priority;
        
        if(iter % 20 == 0) {
          printf("[CPU-BOUND] Tick %d: Q%d (slices=%d)\n", 
                 uptime(), info.priority, info.time_slices);
        }
      }
    }
    
    if(getprocinfo(&info) == 0) {
      printf("[CPU-BOUND] FINAL: Q%d, %d boosts detected\n", 
             info.priority, boost_count);
      if(boost_count >= 1) {
        printf("[CPU-BOUND] ✓ Received at least one boost!\n");
      } else {
        printf("[CPU-BOUND] ✗ No boosts detected (problem!)\n");
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
    
    printf("[I/O-BOUND] Starting...\n");
    
    for(int iter = 0; iter < 100; iter++) {
      // Brief work
      for(long j = 0; j < 1000000; j++) {
        dummy = dummy + j;
      }
      
      // Yield frequently
      pause(2);
      
      if(iter % 25 == 0 && getprocinfo(&info) == 0) {
        printf("[I/O-BOUND] Tick %d: Q%d (slices=%d)\n", 
               uptime(), info.priority, info.time_slices);
      }
    }
    
    if(getprocinfo(&info) == 0) {
      printf("[I/O-BOUND] FINAL: Q%d\n", info.priority);
      if(info.priority <= 1) {
        printf("[I/O-BOUND] ✓ Maintained high priority\n");
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
    
    printf("[MIXED] Starting...\n");
    
    for(int iter = 0; iter < 50; iter++) {
      // Alternate: CPU burst then sleep
      for(long j = 0; j < 15000000; j++) {
        dummy = dummy + j;
        dummy = dummy % 1000000;
      }
      
      pause(3);
      
      if(iter % 15 == 0 && getprocinfo(&info) == 0) {
        printf("[MIXED] Tick %d: Q%d (slices=%d)\n", 
               uptime(), info.priority, info.time_slices);
      }
    }
    
    if(getprocinfo(&info) == 0) {
      printf("[MIXED] FINAL: Q%d\n", info.priority);
    }
    exit(0);
  }
  
  // Parent waits
  printf("\n[Parent] All processes running...\n");
  wait(0);
  wait(0);
  wait(0);
  
  printf("\n=== Fairness Test Complete ===\n");
  printf("All three workloads completed successfully!\n");
  printf("Expected results:\n");
  printf("  - CPU-bound: Experienced boosts (no starvation)\n");
  printf("  - I/O-bound: Stayed in high priority queues\n");
  printf("  - Mixed: Balanced between Q1-Q2\n");
  
  exit(0);
}