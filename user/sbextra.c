#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/param.h"
#include "user/user.h"

static void
check(int ok)
{
  if (!ok) {
    printf("sbextra: FAIL\n");
    exit(1);
  }
}

static void
scenario(int which)
{
  char path[MAXPATH];
  if (which == 0) {
    check(interpose(1 << SYS_open, "README") == 0);
    int fd = open("README", O_RDONLY);
    check(fd >= 0);
    close(fd);
    check(open("./README", O_RDONLY) == -1);
    check(open((char *)-1, O_RDONLY) == -1);
    memset(path, 'a', sizeof(path));
    check(open(path, O_RDONLY) == -1);
  } else if (which == 1) {
    check(interpose(1 << SYS_open, "-") == 0);
    check(interpose(0, (char *)-1) == -1);
    check(interpose(0, "README") == -1);
    check(open("README", O_RDONLY) == -1);
  } else if (which == 2) {
    check(interpose(1 << SYS_open, "README") == 0);
    check(interpose(1 << SYS_getpid, "-") == 0);
    check(getpid() == -1);
    check(open("README", O_RDONLY) == -1);
    check(interpose(0, "README") == -1);
  } else if (which == 3) {
    memset(path, 'x', sizeof(path));
    check(interpose(1 << SYS_getpid, path) == -1);
    check(getpid() > 0);
    check(interpose(1 << SYS_getpid, "-") == 0);
    check(getpid() == -1);
  } else if (which == 4) {
    // A forked child may tighten its policy without modifying its parent.
    check(interpose(1 << SYS_open, "README") == 0);
    int child = fork();
    check(child >= 0);
    if (child == 0) {
      check(interpose(1 << SYS_getpid, "-") == 0);
      check(getpid() == -1 && open("README", O_RDONLY) == -1);
      exit(0);
    }
    int status;
    check(wait(&status) == child && status == 0);
    int fd = open("README", O_RDONLY);
    check(fd >= 0 && getpid() > 0);
    close(fd);
  } else {
    // Sign bit in an unsigned mask is safe; no valid syscall has number 31.
    check(interpose((int)(1U << 31), "-") == 0);
    check(getpid() > 0);
  }
}

int
main(void)
{
  for (int i = 0; i < 96; i++) {
    int child = fork();
    check(child >= 0);
    if (child == 0) {
      // Reused slots must not inherit a previous occupant's restrictions.
      check(getpid() > 0);
      int fd = open("README", O_RDONLY);
      check(fd >= 0);
      close(fd);
      scenario(i % 6);
      exit(0);
    }
    int status;
    check(wait(&status) == child && status == 0);
    check(getpid() > 0);
  }
  printf("sbextra: PASS 96 isolated scenarios\n");
  exit(0);
}
