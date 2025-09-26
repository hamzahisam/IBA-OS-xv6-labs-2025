#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5
#define MAXARGS 10
#define HIST_MAX 32
#define LINE_MAX 100

static char history[HIST_MAX][LINE_MAX];
static int hist_head = 0;
static int hist_len = 0;

static void trim_nl(char *s){
  int n = strlen(s);
  if(n > 0 && (s[n-1] == '\n' || s[n-1] == '\r'))
    s[n-1] = 0;
}

static void add_history(const char *line){
  int i = 0;
  while(i < LINE_MAX-1 && line[i]){
    history[hist_head][i] = line[i];
    i++;
  }
  history[hist_head][i] = 0;
  hist_head = (hist_head + 1) % HIST_MAX;
  if(hist_len < HIST_MAX) hist_len++;
}

static void print_history(){
  int start = (hist_head - hist_len + HIST_MAX) % HIST_MAX;
  for(int i = 0; i < hist_len; i++){
    int idx = (start + i) % HIST_MAX;
    printf("%d %s\n", i+1, history[idx]);
  }
}

static int history_get(int n, char *out, int outsz){
  if(n < 1 || n > hist_len) return -1;
  int start = (hist_head - hist_len + HIST_MAX) % HIST_MAX;
  int idx = (start + (n-1)) % HIST_MAX;
  int i = 0;
  while(i < outsz-1 && history[idx][i]){
    out[i] = history[idx][i];
    i++;
  }
  out[i] = 0;
  return 0;
}

struct cmd{ int type; };

struct execcmd{
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd{
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd{
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd{
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd{
  int type;
  struct cmd *cmd;
};

int fork1(void);
void panic(char *s);
struct cmd *parsecmd(char *s);
void runcmd(struct cmd *cmd) __attribute__((noreturn));

static int readline(char *buf, int nbuf);
static int interactive = 0;

static void putprompt(void){
  if(interactive) write(2, "$ ", 2);
}

void runcmd(struct cmd *cmd){
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0) exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0) exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;
  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;
  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0) runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;
  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0) panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;
  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0) runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

static int is_sep(char c){
  return c==' ' || c=='\t' || c=='\r' || c=='\n';
}

static int read_directory_matches(const char *dir, const char *prefix,
                                  char *only, int onlysz, int *multi){
  int fd = open((char*)dir, O_RDONLY);
  if(fd < 0) return 0;
  struct dirent de;
  int found = 0;
  *multi = 0;
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0) continue;
    char name[DIRSIZ+1];
    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = 0;
    if(strncmp(name, prefix, strlen(prefix)) == 0){
      if(found == 0){
        strncpy(only, name, onlysz-1);
        only[onlysz-1] = 0;
      }else{
        *multi = 1;
        printf("\n%s", name);
      }
      found++;
    }
  }
  close(fd);
  return found;
}

static void redraw(const char *buf){
  write(1, "\n", 1);
  putprompt();
  write(1, buf, strlen(buf));
}

static int complete(char *buf, int len, int nbuf){
  int start = len - 1;
  while(start >= 0 && !is_sep(buf[start])) start--;
  start++;
  char word[128];
  int wlen = 0;
  for(int i = start; i < len && wlen < (int)sizeof(word)-1; i++){
    word[wlen++] = buf[i];
  }
  word[wlen] = 0;

  const char *dir = ".";
  const char *prefix = word;
  char only[DIRSIZ+1];
  int multi = 0;
  int matches = read_directory_matches(dir, prefix, only, sizeof(only), &multi);

  if(matches == 0) return len;

  if(matches == 1){
    int add = strlen(only) - wlen;
    if(add < 0) add = 0;
    if(len + add >= nbuf) add = nbuf - 1 - len;
    memmove(buf + len, only + wlen, add);
    len += add;
    buf[len] = 0;
    redraw(buf);
    return len;
  }

  redraw(buf);
  return len;
}

static int readline(char *buf, int nbuf){
  int i = 0;
  int hcur = hist_len;
  char c;
  for(;;){
    int n = read(0, &c, 1);
    if(n != 1) break;

    if(c == '\r' || c == '\n'){
      buf[i] = 0;
      write(1, "\n", 1);
      return i;
    }

    if(c == '\t'){
      buf[i] = 0;
      i = complete(buf, i, nbuf);
      continue;
    }
    
    if(c == '<'){
      char t[4];
      int ok = (read(0,&t[0],1)==1 && read(0,&t[1],1)==1 &&
                read(0,&t[2],1)==1 && read(0,&t[3],1)==1);
      if(ok && t[0]=='T' && t[1]=='a' && t[2]=='b' && t[3]=='>'){
        buf[i] = 0;
        i = complete(buf, i, nbuf);
        continue;
      }else{
        if(i < nbuf-1) buf[i++] = '<';
        for(int k=0;k<4 && ok && i<nbuf-1;k++) buf[i++] = t[k];
        continue;
      }
    }

    if(c == 0x7f || c == '\b'){
      if(i > 0){
        i--;
        if(interactive) write(1, "\b \b", 3);
      }
      continue;
    }
    
    if((uchar)c == 0x1b){
      char s1, s2;
      if(read(0, &s1, 1) != 1) continue;

      if(s1 == '[' || s1 == 'O'){
        if(read(0, &s2, 1) != 1) continue;

        if(s2 == 'A'){
          if(hist_len > 0){
            if(hcur > 0) hcur--;
            char line[LINE_MAX];
            if(history_get(hcur+1, line, sizeof(line)) == 0){
              int k = 0;
              while(k < nbuf-1 && line[k]){ buf[k] = line[k]; k++; }
              buf[k] = 0;
              i = k;
              redraw(buf);
            }
          }
        } else if(s2 == 'B'){
          if(hcur < hist_len) hcur++;
          if(hcur == hist_len){
            i = 0;
            buf[0] = 0;
            redraw(buf);
          } else {
            char line[LINE_MAX];
            if(history_get(hcur+1, line, sizeof(line)) == 0){
              int k = 0;
              while(k < nbuf-1 && line[k]){ buf[k] = line[k]; k++; }
              buf[k] = 0;
              i = k;
              redraw(buf);
            }
          }
        }
      }
      continue;
    }
    
    // Ctrl-B (previous history)
    if(c == 0x02){
      if(hist_len > 0){
        if(hcur > 0) hcur--;
        char line[LINE_MAX];
        if(history_get(hcur+1, line, sizeof(line)) == 0){
          int k = 0;
          while(k < nbuf-1 && line[k]){ buf[k] = line[k]; k++; }
          buf[k] = 0;
          i = k;
          redraw(buf);
        }
      }
      continue;
    }

    // Ctrl-N (next history)
    if(c == 0x0e){
      if(hcur < hist_len) hcur++;
      if(hcur == hist_len){
        i = 0; buf[0] = 0;
        redraw(buf);
      }else{
        char line[LINE_MAX];
        if(history_get(hcur+1, line, sizeof(line)) == 0){
          int k = 0;
          while(k < nbuf-1 && line[k]){ buf[k] = line[k]; k++; }
          buf[k] = 0;
          i = k;
          redraw(buf);
        }
      }
      continue;
    }

    if(i + 1 < nbuf){
      buf[i++] = c;
      hcur = hist_len;
    }
  }
  buf[i] = 0;
  return i ? i : -1;
}

int getcmd(char *buf, int nbuf){
  if(interactive) write(2, "$ ", 2);
  memset(buf, 0, nbuf);
  int n = readline(buf, nbuf);
  if(n < 0) return -1;
  if(buf[0] == 0) return -1;

  int i, empty = 1;
  for(i = 0; buf[i]; i++){
    char c = buf[i];
    if(c != ' ' && c != '\t' && c != '\n' && c != '\r'){
      empty = 0; break;
    }
  }
  if(!empty){
    char tmp[LINE_MAX];
    int j = 0;
    while(j < LINE_MAX-1 && buf[j]){
      tmp[j] = buf[j];
      j++;
    }
    tmp[j] = 0;
    trim_nl(tmp);
    add_history(tmp);
  }
  return 0;
}

int main(void){
  static char buf[100];
  int fd;
  struct stat st;

  if(fstat(0, &st) >= 0 && st.type == T_DEVICE){
    interactive = 1;
  }else{
    interactive = 0;
  }

  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  while(getcmd(buf, sizeof(buf)) >= 0){
    char *cmd = buf;
    while(*cmd == ' ' || *cmd == '\t') cmd++;
    if(*cmd == '\n') continue;

    if(cmd[0] == 'h' && cmd[1] == 'i' && cmd[2] == 's' && cmd[3] == 't' &&
       cmd[4] == 'o' && cmd[5] == 'r' && cmd[6] == 'y' &&
       (cmd[7] == '\n' || cmd[7] == 0 || cmd[7] == ' ')){
      print_history();
      continue;
    }

    if(cmd[0] == '!' ){
      char recalled[LINE_MAX];
      if(cmd[1] == '!'){
        if(hist_len == 0){
          fprintf(2, "history: empty\n");
          continue;
        }
        if(history_get(hist_len, recalled, sizeof(recalled)) < 0){
          fprintf(2, "history: invalid\n");
          continue;
        }
      } else {
        int n = atoi(cmd+1);
        if(history_get(n, recalled, sizeof(recalled)) < 0){
          fprintf(2, "history: invalid index\n");
          continue;
        }
      }
      printf("%s\n", recalled);
      int k = 0;
      while(k < LINE_MAX-2 && recalled[k]){ buf[k] = recalled[k]; k++; }
      buf[k++] = '\n';
      buf[k] = 0;
      cmd = buf;
    }
    
    if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' '){
      cmd[strlen(cmd)-1] = 0;
      if(chdir(cmd+3) < 0)
        fprintf(2, "cannot cd %s\n", cmd+3);
    } else if(cmd[0]=='w' && cmd[1]=='a' && cmd[2]=='i' && cmd[3]=='t' &&
              (cmd[4]=='\n' || cmd[4]==0)){
      while(wait(0) > 0) ;
    } else {
      if(fork() == 0)
        runcmd(parsecmd(cmd));
      wait(0);
    }
  }
  exit(0);
}

void panic(char *s){
  fprintf(2, "%s\n", s);
  exit(1);
}

int fork1(void){
  int pid = fork();
  if(pid == -1) panic("fork");
  return pid;
}

struct cmd* execcmd(void){
  struct execcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd* redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd){
  struct redircmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd* pipecmd(struct cmd *left, struct cmd *right){
  struct pipecmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd* listcmd(struct cmd *left, struct cmd *right){
  struct listcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd* backcmd(struct cmd *subcmd){
  struct backcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq){
  char *s = *ps;
  int ret;
  while(s < es && strchr(whitespace, *s)) s++;
  if(q) *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)) s++;
    break;
  }
  if(eq) *eq = s;
  while(s < es && strchr(whitespace, *s)) s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks){
  char *s = *ps;
  while(s < es && strchr(whitespace, *s)) s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **ps, char *es);
struct cmd *parsepipe(char **ps, char *es);
struct cmd *parseexec(char **ps, char *es);
struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es);
struct cmd *parseblock(char **ps, char *es);
struct cmd *nulterminate(struct cmd *cmd);

struct cmd* parsecmd(char *s){
  char *es = s + strlen(s);
  struct cmd *cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd* parseline(char **ps, char *es){
  struct cmd *cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd* parsepipe(char **ps, char *es){
  struct cmd *cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd* parseredirs(struct cmd *cmd, char **ps, char *es){
  int tok;
  char *q, *eq;
  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') panic("missing file");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd* parseblock(char **ps, char *es){
  struct cmd *cmd;
  if(!peek(ps, es, "(")) panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")")) panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd* parseexec(char **ps, char *es){
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "(")) return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok = gettoken(ps, es, &q, &eq)) == 0) break;
    if(tok != 'a') panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS) panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

struct cmd* nulterminate(struct cmd *cmd){
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0) return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i = 0; ecmd->argv[i]; i++) *ecmd->eargv[i] = 0;
    break;
  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;
  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;
  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;
  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
