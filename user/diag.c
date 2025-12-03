#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Diagnostic test to check if time slices are being tracked
int
main(int argc, char *argv[])
{
  struct procinfo info;
  
  printf("+------------------------------------+\n");
  printf("|       MLFQ DIAGNOSTIC TOOL         |\n");
  printf("+------------------------------------+\n\n");
  
  // Check initial state
  if(getprocinfo(&info) == 0) {
    printf("[INIT] Process Info:\n");
    printf("        PID: %d\n", info.pid);
    printf("        State: %d\n", info.state);
    printf("        Queue: Q%d\n", info.priority);
    printf("        Slices: %d\n\n", info.time_slices);
  }
  
  // Do a small amount of work and check repeatedly
  printf("[TEST] Running work bursts...\n");
  for(int i = 0; i < 20; i++) {
    // Work burst - enough to trigger timer interrupts
    volatile int dummy = 0;
    for(int j = 0; j < 5000000; j++) {
      dummy++;
    }
    
    if(getprocinfo(&info) == 0) {
      printf("        [%02d] slices=%d, queue=Q%d\n", 
             i, info.time_slices, info.priority);
      
      // If time_slices is still 0 after 10 iterations, something is wrong
      if(i == 10 && info.time_slices == 0) {
        printf("\n!! ERROR: time_slices stuck at 0 !!\n");
        printf("!! Timer interrupts may be broken !!\n\n");
        break;
      }
      
      // If we see demotion, report it
      if(info.priority > 0) {
        printf("\n>> DETECTED: Demoted to Q%d after %d slices <<\n\n", 
               info.priority, info.time_slices);
        break;
      }
    }
  }
  
  // Final state
  if(getprocinfo(&info) == 0) {
    printf("\n[FINAL] Process Info:\n");
    printf("        Queue: Q%d\n", info.priority);
    printf("        Slices: %d\n", info.time_slices);
    
    if(info.time_slices == 0) {
      printf("\n+------------------------------------+\n");
      printf("|         DIAGNOSIS: ISSUE           |\n");
      printf("+------------------------------------+\n");
      printf("Problem: time_slices never incremented\n");
      printf("Check these:\n");
      printf("  1. Timer interrupts (clockintr)\n");
      printf("  2. which_dev != 2 in usertrap()\n");
      printf("  3. p->time_slices++ not running\n");
      printf("\nDebug tip for kernel/trap.c:\n");
      printf("  printf(\"Timer: dev=%%d pid=%%d\\n\", which_dev, p->pid);\n");
    } else {
      printf("\n+------------------------------------+\n");
      printf("|         DIAGNOSIS: OK              |\n");
      printf("+------------------------------------+\n");
      printf("Time slice tracking operational.\n");
    }
  }
  
  exit(0);
}
