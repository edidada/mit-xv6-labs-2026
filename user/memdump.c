#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data, int len);

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = {61810, 2026};
    memdump("ii", (char *)a, sizeof(a));

    printf("Example 2:\n");
    memdump("S", "a string", sizeof("a string"));

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *)&s, sizeof(s));

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;

    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char *)&example, sizeof(example));

    printf("Example 5:\n");
    memdump("sccccc", (char *)&example, sizeof(example));
  } else if (argc == 2) {
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while (n < sizeof(data)) {
      int nn = read(0, data + n, sizeof(data) - n);
      if (nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data, n);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data, int len)
{
  char *end = data + len;
  for (; *fmt; fmt++) {
    int width;
    int number;
    short half;
    uint64 pointer;
    char *terminator;
    switch (*fmt) {
    case 'i': width = sizeof(number); break;
    case 'h': width = sizeof(half); break;
    case 'p':
    case 's': width = sizeof(pointer); break;
    case 'c': width = 1; break;
    case 'S':
      terminator = data;
      while (terminator < end && *terminator) terminator++;
      if (terminator == end) {
        fprintf(2, "memdump: not enough data\n");
        return;
      }
      width = terminator - data + 1;
      break;
    default:
      fprintf(2, "memdump: unknown format %c\n", *fmt);
      return;
    }
    if (end - data < width) {
      fprintf(2, "memdump: not enough data\n");
      return;
    }
    switch (*fmt) {
    case 'i': memmove(&number, data, width); printf("%d\n", number); break;
    case 'h': memmove(&half, data, width); printf("%d\n", half); break;
    case 'p': memmove(&pointer, data, width); printf("%lx\n", pointer); break;
    case 's': memmove(&pointer, data, width); printf("%s\n", (char *)pointer); break;
    case 'c': printf("%c\n", *data); break;
    case 'S': printf("%s\n", data); break;
    }
    data += width;
  }

}
