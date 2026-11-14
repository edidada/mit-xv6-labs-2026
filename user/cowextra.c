#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"

static volatile char target[PGSIZE] __attribute__((aligned(PGSIZE)));
static void
check(int ok)
{
  if (!ok) {
    printf("cowextra: FAIL\n");
    exit(1);
  }
}
int
main(void)
{
  char *raw = sbrk(6 * PGSIZE);
  check(raw != (char *)-1);
  char *b = (char *)PGROUNDUP((uint64)raw);
  memset(b, 0x21, 4 * PGSIZE);
  for (int round = 0; round < 8; round++) {
    for (int k = 0; k < 4; k++) {
      int fds[2];
      check(pipe(fds) == 0);
      char payload[8] = {1,2,3,4,5,6,7,8};
      check(write(fds[1], payload, 8) == 8);
      close(fds[1]);
      int pid = fork();
      check(pid >= 0);
      if (pid == 0) {
        // Kernel copyout crosses two shared pages; direct writes split others.
        check(read(fds[0], b + PGSIZE - 3, 8) == 8);
        check(memcmp(b + PGSIZE - 3, payload, 8) == 0);
        b[2 * PGSIZE] = k;
        b[3 * PGSIZE] = round;
        check(b[2 * PGSIZE] == k && b[3 * PGSIZE] == round);
        close(fds[0]);
        exit(0);
      }
      close(fds[0]);
    }
    for (int k = 0; k < 4; k++) {
      int status;
      check(wait(&status) > 0 && status == 0);
    }
    for (int i = 0; i < 4 * PGSIZE; i++) check(b[i] == 0x21);
  }
  // Lazy holes must remain private after fork and be zero-filled on demand.
  char *lazy = sbrklazy(3 * PGSIZE);
  check(lazy != (char *)-1);
  int pid = fork();
  check(pid >= 0);
  if (pid == 0) {
    lazy[PGSIZE] = 99;
    check(lazy[PGSIZE] == 99);
    exit(0);
  }
  int status;
  check(wait(&status) == pid && status == 0);
  check(lazy[PGSIZE] == 0);
  // Real exhaustion: retain an unwritten shared page, use all free pages,
  // then a COW write must terminate this child rather than corrupt the parent.
  target[0] = 123;
  pid = fork();
  check(pid >= 0);
  if (pid == 0) {
    while (sbrk(PGSIZE) != (char *)-1) { }
    // A failed new page-table allocation can leave one free physical page.
    // Pipe buffers consume that tail without needing new user page tables.
    for (int i = 0; i < 4; i++) {
      int fds[2];
      pipe(fds);
    }
    target[0] = 42;
    printf("cowextra: OOM write unexpectedly returned\n");
    exit(1);
  }
  check(wait(&status) == pid && status == -1);
  check(target[0] == 123);
  printf("cowextra: PASS 32 fork-copyout cases, lazy isolation and OOM fault\n");
  exit(0);
}
