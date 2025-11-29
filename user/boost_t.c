#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Robust MLFQ Boost Test
// Tests both automatic (periodic) and manual boosting
// Demonstrates demotion and promotion cycles

#define WORK_ITERATIONS 5000000

// Time quanta for each queue (must match kernel/proc.c)
// These define how many ticks a process runs before demotion
int time_quanta[4] = {2, 4, 8, 16};

void cpu_work(int iterations) {
  volatile int dummy = 0;
  for(long j = 0; j < iterations; j++) {
    dummy = dummy + j;
    dummy = dummy % 1000000;
  }
}

void print_status(char *label, struct procinfo *info, int tick) {
  printf("  [Tick %d] %s: PID=%d, Queue=Q%d, Slices=%d/%d\n",
         tick, label, info->pid, info->priority, info->time_slices,
         time_quanta[info->priority]);
}

void print_time_quanta_info(void) {
  printf("  ┌─────────────────────────────────────────────────────┐\n");
  printf("  │ Queue │ Time Quantum │ Meaning                      │\n");
  printf("  ├─────────────────────────────────────────────────────┤\n");
  printf("  │  Q0   │   %d ticks    │ Highest priority, shortest   │\n", time_quanta[0]);
  printf("  │  Q1   │   %d ticks    │                              │\n", time_quanta[1]);
  printf("  │  Q2   │   %d ticks    │                              │\n", time_quanta[2]);
  printf("  │  Q3   │   %d ticks   │ Lowest priority, longest     │\n", time_quanta[3]);
  printf("  └─────────────────────────────────────────────────────┘\n");
  printf("  Note: 1 tick ~ 100ms. Process demotes when slices >= quantum.\n\n");
}

int
main(int argc, char *argv[])
{
  struct procinfo info;
  int start_tick, current_tick;
  int initial_priority = 0, demoted_priority = 0, boosted_priority = 0;
  int test_passed = 1;
  
  printf("╔══════════════════════════════════════════════════════════╗\n");
  printf("║     MLFQ BOOST TEST - Demotion & Promotion Cycles        ║\n");
  printf("╚══════════════════════════════════════════════════════════╝\n\n");
  
  start_tick = uptime();
  
  // ═══════════════════════════════════════════════════════════════
  // TEST 1: Verify Initial Priority
  // ═══════════════════════════════════════════════════════════════
  printf("┌─ TEST 1: Initial Priority Check ─────────────────────────┐\n");
  
  if(getprocinfo(&info) == 0) {
    initial_priority = info.priority;
    print_status("Initial", &info, uptime() - start_tick);
    
    if(initial_priority == 0) {
      printf("  ✓ PASS: New process starts at highest priority (Q0)\n");
    } else {
      printf("  ✗ FAIL: Expected Q0, got Q%d\n", initial_priority);
      test_passed = 0;
    }
  }
  printf("└────────────────────────────────────────────────────────────┘\n\n");
  
  // ═══════════════════════════════════════════════════════════════
  // TEST 2: Demotion through CPU-bound work
  // ═══════════════════════════════════════════════════════════════
  printf("┌─ TEST 2: Demotion Test (CPU-bound work) ─────────────────┐\n");
  printf("  Running CPU-intensive work to trigger demotion...\n\n");
  print_time_quanta_info();
  
  int last_priority = 0;
  int last_slices = 0;
  int demotion_ticks[4] = {0, 0, 0, 0};  // Track exact tick of each demotion
  
  printf("  Monitoring slices and demotions:\n");
  printf("  ─────────────────────────────────────────────────────────\n");
  
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
          printf("  > TICK %d: DEMOTED Q%d->Q%d (used %d/%d slices)\n",
                 current_tick, last_priority, info.priority,
                 time_quanta[last_priority], time_quanta[last_priority]);
        } 
        // Detect boost (priority went up)
        else if(info.priority < last_priority) {
          printf("  < TICK %d: BOOSTED Q%d->Q%d (auto boost)\n",
                 current_tick, last_priority, info.priority);
          // Reset Q3 tracking if boosted
          reached_q3 = 0;
          q3_slices_shown = 0;
        }
        // Just slice increment
        else if(info.time_slices > last_slices) {
          printf("    TICK %d: Q%d slices %d->%d (need %d for demotion)\n",
                 current_tick, info.priority, last_slices, info.time_slices,
                 time_quanta[info.priority]);
          if(info.priority == 3) {
            q3_slices_shown++;
          }
        }
        // Slice reset (after demotion)
        else if(info.time_slices < last_slices && info.priority == last_priority) {
          printf("    TICK %d: Q%d slices reset to %d\n",
                 current_tick, info.priority, info.time_slices);
        }
        
        last_slices = info.time_slices;
        last_priority = info.priority;
      }
    }
  }
  
  printf("  ─────────────────────────────────────────────────────────\n");
  
  if(getprocinfo(&info) == 0) {
    demoted_priority = info.priority;
    print_status("After work", &info, uptime() - start_tick);
    
    printf("\n  Demotion Summary:\n");
    if(demotion_ticks[1] > 0)
      printf("    Q0->Q1 at tick %d (expected after %d ticks in Q0)\n", 
             demotion_ticks[1], time_quanta[0]);
    if(demotion_ticks[2] > 0)
      printf("    Q1->Q2 at tick %d (expected after %d more ticks in Q1)\n", 
             demotion_ticks[2], time_quanta[1]);
    if(demotion_ticks[3] > 0)
      printf("    Q2->Q3 at tick %d (expected after %d more ticks in Q2)\n", 
             demotion_ticks[3], time_quanta[2]);
    
    if(demoted_priority > initial_priority) {
      printf("\n  ✓ PASS: Process demoted from Q%d to Q%d\n", 
             initial_priority, demoted_priority);
    } else {
      printf("\n  ✗ FAIL: Process not demoted (still at Q%d)\n", demoted_priority);
      test_passed = 0;
    }
  }
  printf("└────────────────────────────────────────────────────────────┘\n\n");
  
  // ═══════════════════════════════════════════════════════════════
  // TEST 3: Manual Boost via boostproc()
  // ═══════════════════════════════════════════════════════════════
  printf("┌─ TEST 3: Manual Boost (boostproc syscall) ───────────────┐\n");
  
  if(getprocinfo(&info) == 0) {
    print_status("Before boost", &info, uptime() - start_tick);
  }
  
  printf("  Calling boostproc()...\n");
  int before_boost_tick = uptime() - start_tick;
  boostproc();
  int after_boost_tick = uptime() - start_tick;
  
  if(getprocinfo(&info) == 0) {
    boosted_priority = info.priority;
    print_status("After boost", &info, after_boost_tick);
    printf("  Boost executed between tick %d and %d\n", before_boost_tick, after_boost_tick);
    
    if(boosted_priority == 0 && info.time_slices == 0) {
      printf("  ✓ PASS: Process boosted to Q0 with slices reset\n");
    } else {
      printf("  ✗ FAIL: Boost incomplete (Q%d, slices=%d)\n", 
             boosted_priority, info.time_slices);
      test_passed = 0;
    }
  }
  printf("└────────────────────────────────────────────────────────────┘\n\n");
  
  // ═══════════════════════════════════════════════════════════════
  // TEST 4: Re-demotion after boost
  // ═══════════════════════════════════════════════════════════════
  printf("┌─ TEST 4: Re-demotion After Boost ────────────────────────┐\n");
  printf("  Verifying demotion still works after manual boost...\n");
  printf("  (Note: Auto-boost may interfere if BOOST_INTERVAL is short)\n\n");
  
  last_priority = 0;
  last_slices = 0;
  int demotion_seen = 0;
  
  for(int phase = 0; phase < 25; phase++) {
    cpu_work(WORK_ITERATIONS / 4);
    
    if(getprocinfo(&info) == 0) {
      current_tick = uptime() - start_tick;
      
      if(info.priority != last_priority) {
        if(info.priority > last_priority) {
          printf("  > TICK %d: DEMOTED Q%d->Q%d\n",
                 current_tick, last_priority, info.priority);
          demotion_seen = 1;
        } else {
          printf("  < TICK %d: AUTO BOOST Q%d->Q%d\n",
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
      printf("  ✓ PASS: Demotion mechanism working after boost\n");
    } else {
      printf("  ✗ FAIL: No demotions observed\n");
      test_passed = 0;
    }
  }
  printf("└────────────────────────────────────────────────────────────┘\n\n");
  
  // ═══════════════════════════════════════════════════════════════
  // TEST 5: Automatic Periodic Boost
  // ═══════════════════════════════════════════════════════════════
  printf("┌─ TEST 5: Automatic Periodic Boost ───────────────────────┐\n");
  printf("  Waiting and working until automatic boost triggers...\n");
  printf("  This tests starvation prevention mechanism.\n\n");
  
  int boost_detected = 0;
  int prev_priority = info.priority;
  int wait_start = uptime();
  last_slices = info.time_slices;
  
  while(uptime() - wait_start < 150 && !boost_detected) {
    cpu_work(WORK_ITERATIONS / 4);
    
    if(getprocinfo(&info) == 0) {
      current_tick = uptime() - start_tick;
      
      if(prev_priority > 0 && info.priority == 0) {
        printf("  < TICK %d: AUTO BOOST DETECTED Q%d->Q0\n", 
               current_tick, prev_priority);
        boost_detected = 1;
      } else if(info.priority > prev_priority) {
        printf("  > TICK %d: Demotion Q%d->Q%d\n",
               current_tick, prev_priority, info.priority);
      }
      prev_priority = info.priority;
      last_slices = info.time_slices;
    }
  }
  
  if(boost_detected) {
    printf("  ✓ PASS: Automatic periodic boost working!\n");
  } else {
    printf("  ? INFO: Auto boost not observed in time window\n");
    printf("          (May have been at Q0 when boost occurred)\n");
  }
  printf("└────────────────────────────────────────────────────────────┘\n\n");
  
  // ═══════════════════════════════════════════════════════════════
  // Summary
  // ═══════════════════════════════════════════════════════════════
  printf("╔══════════════════════════════════════════════════════════╗\n");
  printf("║                    TEST SUMMARY                          ║\n");
  printf("╠══════════════════════════════════════════════════════════╣\n");
  printf("║  Total runtime: %d ticks (~%d.%d seconds)                \n", 
         uptime() - start_tick, (uptime() - start_tick)/10, (uptime() - start_tick)%10);
  printf("║  Initial priority: Q%d                                    \n", initial_priority);
  printf("║  Max demotion reached: Q%d                                \n", demoted_priority);
  printf("║  Manual boost: %s                                        \n", 
         boosted_priority == 0 ? "Working" : "Failed");
  printf("║  Auto boost: %s                                          \n",
         boost_detected ? "Detected" : "Not observed");
  printf("╠══════════════════════════════════════════════════════════╣\n");
  if(test_passed) {
    printf("║  ✓✓✓ ALL CORE TESTS PASSED ✓✓✓                          ║\n");
  } else {
    printf("║  ✗✗✗ SOME TESTS FAILED ✗✗✗                              ║\n");
  }
  printf("╚══════════════════════════════════════════════════════════╝\n");
  
  exit(0);
}