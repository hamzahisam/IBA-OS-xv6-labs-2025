#include "kernel/types.h"
#include "user/user.h"

static inline uint8  rd8 (char *p) { return (uint8)p[0]; }
static inline uint16 rd16(char *p) { return (uint16)rd8(p) | ((uint16)rd8(p+1) << 8); }
static inline uint32 rd32(char *p) { return (uint32)rd16(p) | ((uint32)rd16(p+2) << 16); }
static inline uint64 rd64(char *p) { return (uint64)rd32(p) | ((uint64)rd32(p+4) << 32); }

void memdump(char *fmt, char *data)
{
  for (char *f = fmt; *f; f++) {
    switch (*f) {

    case 'i': {
      int x = (int)rd32(data);
      printf("%d\n", x);
      data += 4;
      break;
    }

    case 'h': {
      short x = (short)rd16(data);
      printf("%d\n", (int)x);
      data += 2;
      break;
    }

    case 'c': {
      char c = (char)rd8(data);
      printf("%c\n", c);
      data += 1;
      break;
    }

    case 'p': {
      uint64 x = rd64(data);
      printf("%p\n", (void*)x);
      data += 8;
      break;
    }

    case 's': {
      uint64 addr = rd64(data);
      printf("%s\n", (char*)addr);
      data += 8;
      break;
    }

    case 'S': {
      printf("%s\n", (char*)data);
      return;
    }

    default:
      break;
    }
  }
}

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*)a);

    printf("Example 2:\n");
    memdump("S", "a string");

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char*)&s);

    struct sss {
      char *ptr;
      int   num1;
      short num2;
      char  byte;
      char  bytes[8];
    } example;

    example.ptr  = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char*)&example);

    printf("Example 5:\n");
    memdump("sccccc", (char*)&example);

  } else if (argc == 2) {
    char data[512];
    int n = 0;
    memset(data, 0, sizeof(data));
    while (n < (int)sizeof(data)) {
      int m = read(0, data + n, sizeof(data) - n);
      if (m <= 0) break;
      n += m;
    }
    memdump(argv[1], data);

  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }

  exit(0);
}
