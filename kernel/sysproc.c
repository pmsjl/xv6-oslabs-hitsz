#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

extern struct proc proc[NPROC];

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  int flags;
  if (argaddr(0, &p) < 0 || argint(1, &flags) < 0) return -1;
  return wait(p, flags);
}

uint64 sys_yield(void) {
  struct proc *current = myproc();
  printf("Save the context of the process to the memory region from address %p to %p\n",
         &current->context, &current->context + 1);
  printf("Current running process pid is %d and user pc is %p\n",
         current->pid, current->trapframe->epc);
  int index = current - proc;
  for (int offset = 1; offset < NPROC; offset++) {
    struct proc *candidate = &proc[(index + offset) % NPROC];
    acquire(&candidate->lock);
    if (candidate->state == RUNNABLE) {
      printf("Next runnable process pid is %d and user pc is %p\n",
             candidate->pid, candidate->trapframe->epc);
      release(&candidate->lock);
      break;
    }
    release(&candidate->lock);
  }
  yield();
  return 0;
}

uint64 sys_seccomp_ctl(void) {
  int operation;
  uint64 argument;
  if (argint(0, &operation) < 0 || argaddr(1, &argument) < 0) return -1;
  struct proc *current = myproc();
  if (operation == 0) {
    current->seccomp_mask = argument;
    current->seccomp_enabled = 1;
    return 0;
  }
  if (operation == 1 && argument <= 0x7fffffffUL) {
    current->max_children = argument;
    return 0;
  }
  return -1;
}

uint64 sys_seccomp_getlog(void) {
  uint64 buffer, length_address;
  int capacity;
  struct proc *current = myproc();
  if (argaddr(0, &buffer) < 0 || argaddr(1, &length_address) < 0 ||
      copyin(current->pagetable, (char *)&capacity, length_address, sizeof(capacity)) < 0 ||
      capacity < 0) return -1;
  int count = capacity < current->audit_count ? capacity : current->audit_count;
  if ((count && copyout(current->pagetable, buffer, (char *)current->audit_log,
                       count * sizeof(uint64)) < 0) ||
      copyout(current->pagetable, length_address, (char *)&count, sizeof(count)) < 0)
    return -1;
  return 0;
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}
