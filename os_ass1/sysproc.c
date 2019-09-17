#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "perf.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit()
{
  int exit_status;

  if(argint(0, &exit_status) < 0)
    return -1;

  exit(exit_status);
  return 0;  // not reached
}

int
sys_wait(void)
{
  char *child_id;
  if(argptr(0, &child_id, 4) < 0)
    return -1;
  return wait((int*)child_id);
}

int
sys_wait_stat(void){
  char *child_id, *performance;

  if(argptr(0, &child_id, 4) < 0)
    return -1;

  if(argptr(1, &performance, sizeof(struct perf)) < 0)
    return -1;

  return wait_stat((int*)child_id, (struct perf*)performance);
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_detach(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return detach(pid);
}

void
sys_priority(void){
  int pr;
  argint(0, &pr);

  // sanity check
  if(pr < 0 || pr > 10)
    return;
  priority(pr);
}

void
sys_policy(void){
  int pol;
  argint(0, &pol);
  if(pol < 1 || pol > 3)
    return;
  policy(pol);
}
