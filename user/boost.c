#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Robust MLFQ Boost Test
// Tests both automatic (periodic) and manual boosting
// Demonstrates demotion and promotion cycles

#define WORK_ITERATIONS 5000000

// Time quanta for each queue (must match kernel/proc.c)
// These define how many ticks a process runs before demotion
int time_quanta[4] = {3, 6, 12, 24};

void cpu_work(int iterations) {
  volatile int dummy = 0;
  for(long j = 0; j < iterations; j++) {
    dummy = dummy + j;
    dummy = dummy % 1000000;
  }
}

void print_status(char *label, struct procinfo *info, int tick) {
  printf("  @ T%d | %s | PID:%d Q%d slices:%d/%d\n",
         tick, label, info->pid, info->priority, info->time_slices,
         time_quanta[info->priority]);
}

void print_time_quanta_info(void) {
  printf("  | Queue | Quantum | Description          |\n");
  printf("  |  Q0   | %d ticks | Highest (interactive)|\n", time_quanta[0]);
  printf("  |  Q1   | %d ticks | High                 |\n", time_quanta[1]);
  printf("  |  Q2   | %d ticks | Medium               |\n", time_quanta[2]);
  printf("  |  Q3   | %d ticks| Low (CPU-bound)      |\n", time_quanta[3]);
  printf("  (1 tick ~ 100ms; demotion at slices >= quantum)\n\n");
}

int
main(int argc, char *argv[])
{
  struct procinfo info;
  int start_tick, current_tick;
  int initial_priority = 0, demoted_priority = 0, boosted_priority = 0;
  int test_passed = 1;
  
  printf("        MLFQ BOOST TEST: Demotion & Promotion Cycles        \n");
  printf("**************************************************************\n\n");
  
  start_tick = uptime();

  printf("[TEST 1] Initial Priority Check\n");
  
  if(getprocinfo(&info) == 0) {
    initial_priority = info.priority;
    print_status("Initial", &info, uptime() - start_tick);
    
    if(initial_priority == 0) {
      printf("  >> PASS: New process starts at Q0 (highest)\n");
    } else {
      printf("  >> FAIL: Expected Q0, got Q%d\n", initial_priority);
      test_passed = 0;
    }
  }
  
  printf("[TEST 2] Demotion Test (CPU-bound work)\n");
  printf("**************************************************************\n");
  printf("  Running CPU-intensive work to trigger demotion...\n\n");
  print_time_quanta_info();
  
  int last_priority = 0;
  int last_slices = 0;
  int demotion_ticks[4] = {0, 0, 0, 0};  // Track exact tick of each demotion
  
  printf("  Monitoring slices and demotions:\n");
  
  // Run until we reach Q3 or max 80 phases
  // Q0(2) + Q1(4) + Q2(8) = 14 ticks to reach Q3
  // Continue a bit in Q3 to show it's working
  int reached_q3 = 0;
  int q3_slices_shown = 0;
  
  for(int phase = 0; phase < 100; phase++) {
    // Small work unit to get finer granularity
    cpu_work(WORK_ITERATIONS / 4);
    
    if(getprocinfo(&info) == 0) {
      current_tick = uptime() - start_tick;
      
      // Track when we reach Q3
      if(info.priority == 3 && !reached_q3) {
        reached_q3 = 1;
      }
      
      // Stop after showing a few Q3 slices
      if(reached_q3 && info.priority == 3 && q3_slices_shown >= 4) {
        break;
      }
      
      // Print every tick change with slice info
      if(info.time_slices != last_slices || info.priority != last_priority) {
        
        // Detect demotion
        if(info.priority > last_priority) {
          demotion_ticks[info.priority] = current_tick;
          printf("  [T%d] DEMOTE: Q%d --> Q%d (used %d/%d)\n",
                 current_tick, last_priority, info.priority,
                 time_quanta[last_priority], time_quanta[last_priority]);
        } 
        // Detect boost (priority went up)
        else if(info.priority < last_priority) {
          printf("  [T%d] BOOST:  Q%d --> Q%d (auto)\n",
                 current_tick, last_priority, info.priority);
          // Reset Q3 tracking if boosted
          reached_q3 = 0;
          q3_slices_shown = 0;
        }
        // Just slice increment
        else if(info.time_slices > last_slices) {
          printf("  [T%d] Q%d: slices %d->%d (need %d)\n",
                 current_tick, info.priority, last_slices, info.time_slices,
                 time_quanta[info.priority]);
          if(info.priority == 3) {
            q3_slices_shown++;
          }
        }
        // Slice reset (after demotion)
        else if(info.time_slices < last_slices && info.priority == last_priority) {
          printf("  [T%d] Q%d: slices reset -> %d\n",
                 current_tick, info.priority, info.time_slices);
        }
        
        last_slices = info.time_slices;
        last_priority = info.priority;
      }
    }
  }
  
  
  if(getprocinfo(&info) == 0) {
    demoted_priority = info.priority;
    print_status("After work", &info, uptime() - start_tick);
    
    printf("\n  Demotion Summary:\n");
    if(demotion_ticks[1] > 0)
      printf("    * Q0->Q1 @ tick %d (expected: %d ticks in Q0)\n", 
             demotion_ticks[1], time_quanta[0]);
    if(demotion_ticks[2] > 0)
      printf("    * Q1->Q2 @ tick %d (expected: %d more in Q1)\n", 
             demotion_ticks[2], time_quanta[1]);
    if(demotion_ticks[3] > 0)
      printf("    * Q2->Q3 @ tick %d (expected: %d more in Q2)\n", 
             demotion_ticks[3], time_quanta[2]);
    
    if(demoted_priority > initial_priority) {
      printf("\n  >> PASS: Demoted from Q%d to Q%d\n", 
             initial_priority, demoted_priority);
    } else {
      printf("\n  >> FAIL: Not demoted (still Q%d)\n", demoted_priority);
      test_passed = 0;
    }
  }
  
  printf("[TEST 3] Manual Boost (boostproc syscall)\n");
  
  if(getprocinfo(&info) == 0) {
    print_status("Before boost", &info, uptime() - start_tick);
  }
  
  printf("  Invoking boostproc()...\n");
  int before_boost_tick = uptime() - start_tick;
  boostproc();
  int after_boost_tick = uptime() - start_tick;
  
  if(getprocinfo(&info) == 0) {
    boosted_priority = info.priority;
    print_status("After boost", &info, after_boost_tick);
    printf("  Boost ran between T%d and T%d\n", before_boost_tick, after_boost_tick);
    
    if(boosted_priority == 0 && info.time_slices == 0) {
      printf("  >> PASS: Boosted to Q0, slices reset\n");
    } else {
      printf("  >> FAIL: Incomplete (Q%d, slices=%d)\n", 
             boosted_priority, info.time_slices);
      test_passed = 0;
    }
  }

  printf("[TEST 4] Re-demotion After Boost\n");
  printf("  Verifying demotion still works post-boost...\n");
  printf("  (Auto-boost may interfere if BOOST_INTERVAL is short)\n\n");
  
  last_priority = 0;
  last_slices = 0;
  int demotion_seen = 0;
  
  for(int phase = 0; phase < 25; phase++) {
    cpu_work(WORK_ITERATIONS / 4);
    
    if(getprocinfo(&info) == 0) {
      current_tick = uptime() - start_tick;
      
      if(info.priority != last_priority) {
        if(info.priority > last_priority) {
          printf("  [T%d] DEMOTE: Q%d --> Q%d\n",
                 current_tick, last_priority, info.priority);
          demotion_seen = 1;
        } else {
          printf("  [T%d] AUTO:   Q%d --> Q%d\n",
                 current_tick, last_priority, info.priority);
        }
        last_priority = info.priority;
      }
      last_slices = info.time_slices;
    }
  }
  
  if(getprocinfo(&info) == 0) {
    print_status("Final", &info, uptime() - start_tick);
    
    if(demotion_seen) {
      printf("  >> PASS: Demotion works after boost\n");
    } else {
      printf("  >> FAIL: No demotions observed\n");
      test_passed = 0;
    }
  }
  
  printf("[TEST 5] Automatic Periodic Boost\n");
  printf("  Waiting for automatic boost to trigger...\n");
  printf("  (Tests starvation prevention)\n\n");
  
  int boost_detected = 0;
  int prev_priority = info.priority;
  int wait_start = uptime();
  last_slices = info.time_slices;
  
  while(uptime() - wait_start < 150 && !boost_detected) {
    cpu_work(WORK_ITERATIONS / 4);
    
    if(getprocinfo(&info) == 0) {
      current_tick = uptime() - start_tick;
      
      if(prev_priority > 0 && info.priority == 0) {
        printf("  [T%d] AUTO BOOST: Q%d --> Q0\n", 
               current_tick, prev_priority);
        boost_detected = 1;
      } else if(info.priority > prev_priority) {
        printf("  [T%d] DEMOTE: Q%d --> Q%d\n",
               current_tick, prev_priority, info.priority);
      }
      prev_priority = info.priority;
      last_slices = info.time_slices;
    }
  }
  
  if(boost_detected) {
    printf("  >> PASS: Auto periodic boost working\n");
  } else {
    printf("  >> INFO: Auto boost not seen in window\n");
    printf("           (May have been at Q0 already)\n");
  }
  
  printf("TEST SUMMARY\n");
  printf("**************************************************************\n");
  printf("  Runtime: %d ticks (~%d.%d sec)\n", 
         uptime() - start_tick, (uptime() - start_tick)/10, (uptime() - start_tick)%10);
  printf("  Initial Queue: Q%d\n", initial_priority);
  printf("  Max Demotion:  Q%d\n", demoted_priority);
  printf("  Manual Boost:  %s\n", 
         boosted_priority == 0 ? "OK" : "FAILED");
  printf("  Auto Boost:    %s\n",
         boost_detected ? "Detected" : "Not seen");
  if(test_passed) {
    printf("  >>> ALL CORE TESTS PASSED <<<\n");
  } else {
    printf("  >>> SOME TESTS FAILED <<<\n");
  }  
  exit(0);
}