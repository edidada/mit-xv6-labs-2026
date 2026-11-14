#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // The lab deliberately omits page clearing. Search newly allocated pages
  // for secret.c's public marker rather than guessing the secret or page order.
  enum { BYTES = 128 * PGSIZE };
  char *data = sbrk(BYTES);
  const char marker[] = "Here it is: ";
  if (data == (char *)-1)
    exit(1);
  for (int i = 0; i + sizeof(marker) < BYTES; i++) {
    if (memcmp(data + i, marker, sizeof(marker) - 1) != 0)
      continue;
    char *s = data + i + sizeof(marker) - 1;
    int n = 0;
    while (s + n < data + BYTES && n < 128 &&
           ((s[n] >= '0' && s[n] <= '9') ||
            (s[n] >= 'A' && s[n] <= 'Z') ||
            (s[n] >= 'a' && s[n] <= 'z')))
      n++;
    if (n > 0 && s + n < data + BYTES && s[n] == 0) {
      write(1, s, n);
      write(1, "\n", 1);
      exit(0);
    }
  }

  exit(1);
}
