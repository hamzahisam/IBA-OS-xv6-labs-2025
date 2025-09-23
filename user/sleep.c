#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[]){
    if (argc < 2) {
      fprintf(2, "Ticks not entered\n");
      exit(1);
    }
    
    int ticks = atoi(argv[1]);
    if (ticks < 0) {
      fprintf(2, "Ticks must not be negative\n");
      exit(1);
    }
    
    sleep(ticks);
    exit(0);
}
