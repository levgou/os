#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "kthread.h"



static struct proc *initproc;
int nextpid = 1;
int next_tid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void clean_thread(kthread *t);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

kthread*
my_thread(void) {
  struct cpu *c;
  kthread* t;
  pushcli();
  c = mycpu();
  t = c->thread;
  popcli();
  return t;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.

kthread *
find_thread_by_state(struct proc *p, enum kthread_state state)
{
  kthread *t;
  for(t = p->threads; t < &p->threads[NTHREAD]; t++)
    if (t->state == state)
      return t;
  return 0;
}

kthread *
find_other_active_thread(struct proc *p, kthread *my_thread)
{
  kthread *t;
  for(t = p->threads; t < &p->threads[NTHREAD]; t++) {
    if(t->tid == my_thread->tid)
      continue;
    if (t->state != T_ZOMBIE && t->state != T_UNUSED)
      return t;
  }
  return 0;
}

kthread *
find_thread_by_tid(struct proc *p, int tid)
{
  kthread *t;
  for(t = p->threads; t < &p->threads[NTHREAD]; t++)
    if (t->tid == tid)
      return t;
  return 0;
}


kthread*
alloc_thread(struct proc *p)
{
  kthread *t;
  char *sp;

  if((t = find_thread_by_state(p, T_UNUSED)))
    goto found;

  return 0;

found:
  t->state = T_EMBRYO;
  t->tid = next_tid++;

  if((t->kstack = kalloc()) == 0){
    t->state = T_UNUSED;
    return 0;
  }
  sp = t->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *t->tf;
  t->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *t->context;
  t->context = (struct context*)sp;
  memset(t->context, 0, sizeof *t->context);
  t->context->eip = (uint)forkret;

  return t;
}


static struct proc*
allocproc(kthread** new_thread)
{
  struct proc *p;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  if(!(*new_thread = alloc_thread(p)))
    return 0;

  return p;
}


//PAGEBREAK: 32
// Set up first user process.
void
userinit_thread(struct proc *p, kthread* t)
{
  memset(t->tf, 0, sizeof(*t->tf));
  t->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  t->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  t->tf->es = t->tf->ds;
  t->tf->ss = t->tf->ds;
  t->tf->eflags = FL_IF;
  t->tf->esp = PGSIZE;
  t->tf->eip = 0;  // beginning of initcode.S
}

void
userinit_mutexes(){
//  mutexes_arr.locked = 0;
  mutexes_arr.mutex_arr_counter = 0;
  initlock(&mutexes_arr.mutex_arr_lock, "mutexes table locker");
}

void
userinit(void)
{
  struct proc *p;
  kthread *t;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc(&t);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;

  safestrcpy(p->name, "initcode", sizeof(p->name));

  p->cwd = namei("/");

  userinit_thread(p, t);
  userinit_mutexes();

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = ACTIVE;
  t->state = T_RUNNABLE;

  release(&ptable.lock);
}


// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();
  kthread *t = my_thread();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc, t);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();
  kthread *cur_thread = my_thread();
  kthread *new_thread;

  // Allocate process and init his new thread.
  if((np = allocproc(&new_thread)) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(new_thread->kstack);
    new_thread->kstack = 0;
    np->state = UNUSED;
    new_thread->state = T_UNUSED;
    return -1;
  }

  np->sz = curproc->sz;
  np->parent = curproc;
  *new_thread->tf = *cur_thread->tf;

  // Clear %eax so that fork returns 0 in the child.
  new_thread->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);

  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = ACTIVE;
  new_thread->state = T_RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.

int
zombify_and_check_if_last(struct proc *p, kthread *t) {

  acquire(&ptable.lock);

  if (t != p->the_one_who_lived) {
    t->state = T_ZOMBIE;
  }

  int ret = last_thread_alive(p, t) ? 1 : 0;
  release(&ptable.lock);

  return ret;
}

void
verify_proc_killed(struct proc *p, kthread *t, int is_thread_exit) {
  if (p->killed == 0) {
    acquire(&ptable.lock);

    if (!is_thread_exit || last_thread_alive(p, t)) {
      p->killed = 1;
    }
    release(&ptable.lock);
  }
}

void clean_thread(kthread *t) {
  kfree(t->kstack);
  t->tid = 0;
  t->chan = 0;
  t->kstack = 0;
  t->state = T_UNUSED;
}

void
exit1(int is_thread_exit)
{
  /*/
   * 1. If from kthread exit:
   *    if im the last thread, kill the proc. else, zombify myself.
   * 2. If from original exit:
   *    a. kill the proc.
   *    b. each thread zombify himself in some time.
   *    c. If I from exec:
   *       a. wait to be lucky and be last one. if I do, revive the proc with me as only thread.
   */

  struct proc *curproc = myproc();
  kthread *cur_thread = my_thread();
  struct proc *p;
  int fd;

  verify_proc_killed(curproc, cur_thread, is_thread_exit);

  if(!zombify_and_check_if_last(curproc, cur_thread)) {
    acquire(&ptable.lock);
    if (is_thread_exit) {
      wakeup1(cur_thread);
    }
    sched();
  }

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }


  begin_op();

  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  cur_thread->state = T_ZOMBIE;  // if execution of exit was interrupted
  sched();
  panic("zombie exit");

}

void
exit(void){
  exit1(0);
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  kthread *t;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        while((t = find_thread_by_state(p, T_ZOMBIE))){
          clean_thread(t);
        }
        pid = p->pid;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}



//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  kthread *t;
  struct cpu *c = mycpu();
  c->proc = 0;
  c->thread = 0;

  for(;;){

    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);


    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != ACTIVE)
        continue;
      if(!(t = find_thread_by_state(p, T_RUNNABLE)))
        continue;
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.

      c->proc = p;
      c->thread = t;

      switchuvm(p,t);
      t->state = T_RUNNING;
      swtch(&(c->scheduler), t->context);

      if(t->killed) {
        release(&ptable.lock);
        exit1(1);
      }

      switchkvm();

      // Thread is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      c->thread = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  kthread *t = my_thread();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(t->state == T_RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&t->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  my_thread()->state = T_RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  kthread *t = my_thread();

  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  t->chan = chan;
  t->state = T_SLEEPING;

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  kthread *t;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == ACTIVE) {
      for (t = p->threads; t < &p->threads[NTHREAD]; t++) {
        if (t->state == T_SLEEPING && t->chan == chan) {
          t->state = T_RUNNABLE;
        }
      }
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;
  kthread *t;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      while((t = find_thread_by_state(p, T_SLEEPING))){
        t->state = T_RUNNABLE;
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [ACTIVE]   "active",
  [ZOMBIE]    "zombie"
  };

//  static char *t_states[] = {
//  [T_UNUSED]    "t_unused",
//  [T_EMBRYO]    "t_embryo",
//  [T_SLEEPING]   "t_sleeping",
//  [T_RUNNABLE]    "t_runnable",
//  [T_RUNNING]    "t_running",
//  [T_ZOMBIE]    "t_zombie",
//  };

  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);

    if(my_thread()->state == T_SLEEPING){
      getcallerpcs((uint*)my_thread()->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int
last_thread_alive(struct proc *p, kthread *cur_thread)
{
  kthread *t;
  for(t = p->threads; t < &p->threads[NTHREAD]; t++) {
    if (t == cur_thread)
      continue;

    if (t->state != T_ZOMBIE && t->state != T_UNUSED) {
      return 0;
    }
  }
  return 1;
}

