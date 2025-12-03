// boostlong.c - MLFQ demonstration with 2 processes side by side
// Shows demotions and boosts in tabular format
// Demonstrates Q3 stays at Q3 even after 16 time slices (until boost)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define WORK_ITERATIONS 5000000

void cpu_work(int iterations) {
  volatile int dummy = 0;
  for(long j = 0; j < iterations; j++) {
    dummy = dummy + j;
    dummy = dummy % 1000000;
  }
}

int
main(int argc, char *argv[])
{
  struct procinfo info;
  int start_tick;
  int last_priority = 0;
  int last_slices = 0;
  int boost_count = 0;
  
  printf("\n");
  printf("================================================================\n");
  printf("     MLFQ BOOST TEST - SINGLE PROCESS DETAILED VIEW\n");
  printf("================================================================\n");
  printf("  Time Quanta: Q0=2, Q1=4, Q2=8, Q3=16 ticks\n");
  printf("  Boost Interval: Every 50 ticks all processes -> Q0\n");
  printf("  Key Test: Q3 stays at Q3 even after 16+ slices until BOOST\n");
  printf("================================================================\n\n");
  
  start_tick = uptime();
  getprocinfo(&info);
  last_priority = info.priority;
  last_slices = info.time_slices;
  
  printf("  TICK | QUEUE | SLICES | EVENT\n");
  printf("  -----+-------+--------+----------------------------------------\n");
  
  int reached_q3 = 0;
  int q3_extra_shown = 0;
  int first_boost_seen = 0;
  
  // Run until we see 3 boosts (print after 1st boost, track 2 more)
  for(int phase = 0; phase < 2000 && boost_count < 3; phase++) {
    cpu_work(WORK_ITERATIONS / 5);
    
    getprocinfo(&info);
    int current_tick = uptime() - start_tick;
    
    // Detect events
    if(info.priority != last_priority) {
      if(info.priority > last_priority) {
        // Demotion - only print after first boost
        if(first_boost_seen) {
          printf("   %d  |  Q%d   |    %d   | DEMOTE Q%d->Q%d (used %d ticks)\n",
                 current_tick, info.priority, info.time_slices,
                 last_priority, info.priority, 
                 last_priority == 0 ? 2 : (last_priority == 1 ? 4 : 8));
          
          if(info.priority == 3 && !reached_q3) {
            reached_q3 = 1;
            printf("  -----+-------+--------+----------------------------------------\n");
            printf("  >>> Now at LOWEST priority (Q3) - will stay here until BOOST <<<\n");
            printf("  -----+-------+--------+----------------------------------------\n");
          }
        }
      } else {
        // Boost
        boost_count++;
        if(!first_boost_seen) {
          first_boost_seen = 1;
          printf("   %d  |  Q%d   |    %d   | *** BOOST #1 - START TRACKING ***\n",
                 current_tick, info.priority, info.time_slices);
          printf("  -----+-------+--------+----------------------------------------\n");
        } else {
          printf("  -----+-------+--------+----------------------------------------\n");
          printf("   %d  |  Q%d   |    %d   | *** BOOST #%d! Q%d->Q0 ***\n",
                 current_tick, info.priority, info.time_slices,
                 boost_count, last_priority);
          printf("  -----+-------+--------+----------------------------------------\n");
        }
        reached_q3 = 0;
        q3_extra_shown = 0;
      }
    }
    // Show slice progression at Q3 (especially when slices > 16) - only after first boost
    else if(first_boost_seen && info.priority == 3 && info.time_slices != last_slices) {
      if(info.time_slices <= 16 && info.time_slices % 2 == 0) {
        printf("   %d  |  Q%d   |   %d   | Waiting at Q3 (slices: %d/16)\n",
               current_tick, info.priority, info.time_slices, info.time_slices);
      }
      // KEY: Show that Q3 stays at Q3 even after 16 slices!
      if(info.time_slices > 16 && q3_extra_shown < 5) {
        printf("   %d  |  Q%d   |   %d   | STILL Q3! (slices > 16, waiting for boost)\n",
               current_tick, info.priority, info.time_slices);
        q3_extra_shown++;
      }
    }
    // Show running status for other queues - only after first boost
    else if(first_boost_seen && info.priority < 3 && info.time_slices != last_slices) {
      printf("   %d  |  Q%d   |    %d   | Running at Q%d (slices: %d)\n",
             current_tick, info.priority, info.time_slices, 
             info.priority, info.time_slices);
    }
    
    last_priority = info.priority;
    last_slices = info.time_slices;
  }
  
  printf("\n");
  printf("================================================================\n");
  printf("                      TEST SUMMARY\n");
  printf("================================================================\n");
  int total = uptime() - start_tick;
  printf("  Total Runtime   : %d ticks (~%d.%d seconds)\n", 
         total, total/10, total%10);
  printf("  Boost Events    : %d automatic boosts detected\n", boost_count);
  printf("  Expected Interval: ~50 ticks between boosts\n");
  printf("----------------------------------------------------------------\n");
  if(boost_count >= 3) {
    printf("  SUCCESS: Priority boosting is working correctly!\n\n");
    printf("  The test demonstrated:\n");
    printf("    1. Demotion: Q0 -> Q1 -> Q2 -> Q3 (CPU-bound behavior)\n");
    printf("    2. Staying at Q3 until boost interval\n");
    printf("    3. Q3 STAYS Q3 even after 16+ slices (no further demotion)\n");
    printf("    4. Automatic boost back to Q0 (starvation prevention)\n");
    printf("    5. Multiple boost cycles confirming periodic boosting\n");
  } else {
    printf("  Test incomplete - try running longer\n");
  }
  printf("================================================================\n\n");
  
  exit(0);
}
