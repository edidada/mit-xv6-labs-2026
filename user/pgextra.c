#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/memlayout.h"
#include "user/user.h"

static void
check(int ok)
{
  if (!ok) {
    printf("pgextra: FAIL\n");
    exit(1);
  }
}

int
main(int argc, char **argv)
{
  check(ugetpid() == getpid());
  if (argc == 2) {
    printf("pgextra: exec PID OK\n");
    exit(0);
  }
  char *raw = sbrk(35 * PGSIZE);
  check(raw != (char *)-1);
  volatile char *base = (char *)PGROUNDUP((uint64)raw);
  uchar bits[8];
  memset(bits, 0xcc, sizeof(bits));
  check(pgaccess((void *)base, 33, bits) == 0);
  base[0] = 1;
  base[8 * PGSIZE] = 2;
  base[32 * PGSIZE] = 3;
  check(pgaccess((void *)base, 33, (void *)USYSCALL) == -1);
  check(pgaccess((void *)base, 33, (void *)(MAXVA - 1)) == -1);
  check(pgaccess((void *)base, 33, bits) == 0);
  check(bits[0] == 1 && bits[1] == 1 && bits[2] == 0 &&
        bits[3] == 0 && bits[4] == 1);
  check(bits[5] == 0xcc && bits[6] == 0xcc && bits[7] == 0xcc);
  check(pgaccess((void *)base, 33, bits) == 0);
  for (int i = 0; i < 5; i++) check(bits[i] == 0);
  check(pgaccess((void *)base, -1, bits) == -1);
  check(pgaccess((void *)base, 0, bits) == -1);
  check(pgaccess((void *)base, 4097, bits) == -1);
  check(pgaccess((void *)MAXVA, 1, bits) == -1);
  check(pgaccess((void *)(MAXVA - PGSIZE), 2, bits) == -1);
  check(pgaccess((void *)-1, 1, bits) == -1);
  check(pgaccess((void *)TRAPFRAME, 1, bits) == -1);
  check(pgaccess((void *)base, 1, (void *)0) == -1);
  uint64 pte = pgpte((void *)USYSCALL);
  check((pte & (PTE_V | PTE_R | PTE_U | PTE_W | PTE_X)) ==
        (PTE_V | PTE_R | PTE_U));
  for (int i = 0; i < 3; i++) {
    int pid = fork();
    check(pid >= 0);
    if (pid == 0) {
      if (i == 0) {
        *(volatile int *)USYSCALL = 99;
      } else if (i == 1) {
        ((void (*)(void))USYSCALL)();
      } else {
        char *args[] = {"pgextra", "exec", 0};
        exec("pgextra", args);
      }
      exit(7);
    }
    int status;
    check(wait(&status) == pid);
    check(status == (i == 2 ? 0 : -1));
    check(ugetpid() == getpid());
  }
  uint64 super, regular;
  check(ksupernpte(&super, &regular) == 0);
  printf("pgextra: kernel super=%ld regular=%ld saved_table_pages=%ld\n",
         super, regular, super);
  printf("pgextra: PASS boundaries, clear, output width, RO/NX and exec\n");
  exit(0);
}
