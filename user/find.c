#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

static int matchhere(char *re, char *text);
static int matchstar(int c, char *re, char *text);
static int isesc(char c) { return c == '\\'; }

static int match(char *re, char *text) {
  if (re[0] == '^')
    return matchhere(re + 1, text);
  do {
    if (matchhere(re, text))
      return 1;
  } while (*text++ != '\0');
  return 0;
}

static int matchhere(char *re, char *text) {
  if (re[0] == '\0')
    return 1;

  if (isesc(re[0]) && re[1] && re[2] == '*')
    return matchstar(re[1], re + 3, text);

  if (re[1] == '*')
    return matchstar(re[0], re + 2, text);

  if (re[0] == '$' && re[1] == '\0')
    return *text == '\0';

  if (isesc(re[0]) && re[1] && *text != '\0' && re[1] == *text)
    return matchhere(re + 2, text + 1);

  if (*text != '\0' && (re[0] == '.' || re[0] == *text))
    return matchhere(re + 1, text + 1);

  return 0;
}

static int matchstar(int c, char *re, char *text) {
  do {
    if (matchhere(re, text))
      return 1;
  } while (*text != '\0' && (*text++ == c || c == '.'));
  return 0;
}

static int do_exec;
static char *basev[MAXARG];
static int basec;

static void run_exec(const char *path) {
  char *argv[MAXARG];
  int ac = 0;

  for (int i = 0; i < basec && ac < MAXARG - 1; i++)
    argv[ac++] = basev[i];

  if (ac < MAXARG - 1)
    argv[ac++] = (char *)path;

  argv[ac] = 0;

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "find: fork failed\n");
    return;
  }
  if (pid == 0) {
    exec(argv[0], argv);
    fprintf(2, "find: exec %s failed\n", argv[0]);
    exit(1);
  }
  wait(0);
}

static void findrec(const char *path, const char *pattern) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_FILE) {
    const char *base = path;
    for (const char *p = path; *p; p++)
      if (*p == '/')
        base = p + 1;

    if (match((char *)pattern, (char *)base)) {
      if (do_exec)
        run_exec(path);
      else
        printf("%s\n", path);
    }
  } else if (st.type == T_DIR) {
    char buf[512];
    int n = strlen((char *)path);
    if (n + 1 + DIRSIZ + 1 > sizeof(buf)) {
      fprintf(2, "find: path too long: %s\n", path);
      close(fd);
      return;
    }

    struct dirent de;
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;

      char name[DIRSIZ + 1];
      memmove(name, de.name, DIRSIZ);
      name[DIRSIZ] = 0;

      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        continue;

      strcpy(buf, (char *)path);
      buf[n] = '/';
      buf[n + 1] = 0;
      strcpy(buf + n + 1, name);

      findrec(buf, pattern);
    }
  }

  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(2, "usage: find <start-path> <regex> [-exec <cmd> [args...]]\n");
    exit(1);
  }

  do_exec = 0;
  basec = 0;

  if (argc >= 4 && strcmp(argv[3], "-exec") == 0) {
    if (argc < 5) {
      fprintf(2, "find: -exec requires a command\n");
      exit(1);
    }
    do_exec = 1;
    for (int i = 4; i < argc && basec < MAXARG - 1; i++)
      basev[basec++] = argv[i];
    basev[basec] = 0;
  }

  findrec(argv[1], argv[2]);
  exit(0);
}
