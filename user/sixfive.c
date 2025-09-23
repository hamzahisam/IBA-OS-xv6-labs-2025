#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

static char *seps = " -\r\t\n./,";

static int is_sep(char c) {
  for (char *p = seps; *p; p++)
    if (*p == c) return 1;
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "Hello xv6\n");
    exit(1);
  }

  char buf[512], numbuf[32];
  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      fprintf(2, "sixfive: open fail %s\n", argv[i]);
      continue;
    }

    int n, pos = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
      for (int j = 0; j < n; j++) {
        char c = buf[j];
        if (c >= '0' && c <= '9') {
          if (pos < sizeof(numbuf) - 1)
            numbuf[pos++] = c;
        } else if (is_sep(c)) {
          if (pos > 0) {
            numbuf[pos] = '\0';
            int val = atoi(numbuf);
            if (val % 5 == 0 || val % 6 == 0)
              printf("%d\n", val);
            pos = 0;
          }
        } else {
          pos = 0;
        }
      }
    }

    if (pos > 0) {
      numbuf[pos] = '\0';
      int val = atoi(numbuf);
      if (val % 5 == 0 || val % 6 == 0)
        printf("%d\n", val);
    }

    close(fd);
  }
  exit(0);
}
