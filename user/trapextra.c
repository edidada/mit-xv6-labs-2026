#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"

static volatile int count;
static void
check(int ok)
{
  if (!ok) {
    printf("trapextra: FAIL\n");
    exit(1);
  }
}
static void
handler(void)
{
  count++;
  sigalarm(0, 0);
  sigreturn();
}
static void
spin(int ticks)
{
  uint start = uptime();
  while (uptime() - start < ticks) {
    for (int i = 0; i < 100000; i++) asm volatile("nop");
  }
}
int
main(int argc, char **argv)
{
  check(sigreturn() == -1);
  if (argc == 2) {
    spin(4);
    check(count == 0);
    printf("trapextra: exec reset OK\n");
    exit(0);
  }
  unsigned int value = 0x00646c72;
  printf("H%x Wo%s\n", 57616, (char *)&value);
  check(sigalarm(1, handler) == 0);
  check(sigalarm(-1, handler) == -1);
  check(sigalarm(1, (void (*)())MAXVA) == -1);
  spin(10);
  check(count == 1);
  check(sigreturn() == -1);
  spin(4);
  check(count == 1);
  count = 0;
  check(sigalarm(1, handler) == 0);
  int pid = fork();
  check(pid >= 0);
  if (pid == 0) {
    // Newly allocated child has no alarm configuration.
    spin(4);
    check(count == 0);
    exit(0);
  }
  int status;
  check(wait(&status) == pid && status == 0);
  check(sigalarm(0, 0) == 0);
  count = 0;
  check(sigalarm(1, handler) == 0);
  char *args[] = {"trapextra", "exec", 0};
  exec("trapextra", args);
  exit(1);
}
