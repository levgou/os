#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

PgdirPhysPagesEntry pgdir_phys[NPROC];

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
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

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

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

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  // task 2
  reset_all_pages_meta(&p->pmeta);

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");


  add_entry(p->pgdir);
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);

  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();
  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
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

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }

  memmove(&np->pmeta, &curproc->pmeta, sizeof(PageMeta));

  if(curproc->pid > 2 && curproc->pmeta.file_exists) {
    copySwapFile(curproc, np);
  }

  np->pmeta.rt_meta.num_page_faults = 0;
  np->pmeta.rt_meta.num_paged_out_ever = 0;

  arrange_pva_linklist_of_newborn_proc(np->pgdir, find_entry(curproc->pgdir)->first);

  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

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

  // task 2
  if(curproc->pmeta.file_exists)
    removeSwapFile(curproc);

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

  if(VERBOSE)
    procdump();

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;

        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

        // task 2
        memset(&p->pmeta, 0, sizeof(PageMeta));
        p->swapFile = 0;

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
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
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
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
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
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
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

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
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
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
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

    print_mem_stats(p);

    if(p->state == SLEEPING) {
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }

  print_total_pages_info();
}


void print_total_pages_info() {
  cprintf("%d / %d free pages in the system \n",
      PAGES_AVAILABLE_CURRENTLY,
      PAGES_AVAILABLE_KERNEL_START);
}


/*
 * Print the following about proc:
 *
 * <allocated memory pages>
 * <paged out>
 * <protected pages>
 * <page faults>
 * <total number of paged out>
 */
void
print_mem_stats(struct proc *p) {

  // allocated memory pages
  cprintf(" %d", count_phys_pages(p->pgdir));

  // paged out
  cprintf(" %d", count_paged_out(&p->pmeta));

  // protected pages
  cprintf(" %d", p->pmeta.rt_meta.num_protected_pages);

  // page faults
  cprintf(" %d", p->pmeta.rt_meta.num_page_faults);

  // total number of paged out
  cprintf(" %d", p->pmeta.rt_meta.num_paged_out_ever);

}

void
k_set_pmalloced_page(const void *va) {
  set_page_flag(va, PTE_PMALLOCED);
}

int
k_check_page_pmalloced(const void *va) {
  return check_page_flag(va, PTE_PMALLOCED);
}

int
protect_pg(const void *va) {
  remove_page_flag(va, PTE_W);
  myproc()->pmeta.rt_meta.num_protected_pages++;
  return 1;
}

int
k_check_page_protected(const void *va) {
  return !check_page_flag(va, PTE_W);
}

int
unprotect_pg(const void *va) {
  set_page_flag(va, PTE_W);
  myproc()->pmeta.rt_meta.num_protected_pages--;
  return 1;
}

void register_page(pde_t *pgdir, uint va, uint pa) {
  PgdirPhysPagesEntry *entry = find_entry(pgdir);
  entry->num_pages ++;

  if(POLICY == NONE)
    return;

  for (int i = 0; i < MAX_PSYC_PAGES; ++i) {

    if (entry->phys_pages[i].va != 0 || entry->phys_pages[i].pa != 0) {
        continue;

    } else {
      entry->phys_pages[i].va = va;
      entry->phys_pages[i].pa = pa;
      addPageToEndOfQueue(&entry->phys_pages[i], pgdir);
      return;
    }
  }

  panic("register - NO EMPTY PLACE!");
}

void print_pages(PgdirPhysPagesEntry *entry ) {

  cprintf("# |   pa    |  va  |   pp  |  next  |  prev\n");
  cprintf("--------------------------------------------------\n");

  for (int i = 0 ; i < MAX_PSYC_PAGES; ++i) {

    cprintf("%d | %p | %p | %p | %p  | %p \n",
        i, entry->phys_pages[i].pa, entry->phys_pages[i].va, &entry->phys_pages[i],
            entry->phys_pages[i].next, entry->phys_pages[i].prev);

    cprintf("--------------------------------------------------\n");

  }
}

void unregister_page(pde_t *pgdir, uint va, uint pa) {
  PgdirPhysPagesEntry *entry = find_entry(pgdir);
  entry->num_pages --;

  if(POLICY == NONE)
    return;

  int i = 0;
  for (; i < MAX_PSYC_PAGES; ++i) {
   if (entry->phys_pages[i].pa == pa && entry->phys_pages[i].va == va)
      goto found;
  }

  cprintf("blagan at pgdir %p -  pa %x va %x \n", pgdir, pa, va);
  panic("unregister_page");

found:
  entry->phys_pages[i].va = 0;
  entry->phys_pages[i].pa = 0;
}

PVA* update_page_va(struct proc *cur_proc, uint va, uint pa) {
  PgdirPhysPagesEntry *entry = find_entry(cur_proc->pgdir);

  int i = 0;
  for (; i < MAX_PSYC_PAGES; ++i) {
    if (entry->phys_pages[i].pa == pa) {
        entry->phys_pages[i].va = va;
        return &entry->phys_pages[i];
    }
  }

  cprintf("didnt find page with pa %x\n", pa);
  panic("didnt find!");
}

PgdirPhysPagesEntry *find_entry(pde_t *pgdir) {
  for (int i = 0; i < NPROC; ++i) {
    if (pgdir_phys[i].pgdir == pgdir)
      return &pgdir_phys[i];
  }

  cprintf("didnt find entry for %p\n", pgdir);
  panic("find_entry");
}

PgdirPhysPagesEntry *add_entry(pde_t *pgdir) {
  for (int i = 0; i < NPROC; ++i) {
    if (pgdir_phys[i].pgdir == 0) {
      pgdir_phys[i].pgdir = pgdir;
      return &pgdir_phys[i];
    }
  }

  cprintf("didnt find empty space for pgdir %p\n", pgdir);
  panic("add_entry");
}

void remove_entry(pde_t *pgdir) {
  PgdirPhysPagesEntry *entry;

  if(!(entry = find_entry(pgdir)))
    panic("remove_entry");

  memset(entry, 0, sizeof(PgdirPhysPagesEntry));
}

void print_paged_out(PageMeta *page_meta, struct proc *p) {
  for (int i = 0; i < PAGE_INFO_NUM; ++i) {
    pte_t *pt = non_stat_walkpgdir(p->pgdir, (void*)page_meta->paged_out_pages[i].virt_address, 0);
    cprintf("va[%d] %x [%x] \n", i, page_meta->paged_out_pages[i].virt_address, *pt & PTE_P);
  }
}

void print_entry(pde_t *pgdir) {
  PgdirPhysPagesEntry *entry = find_entry(pgdir);

  for(int i = 0; i < MAX_PSYC_PAGES; i++) {
    cprintf("i[%d] pa[%x] va[%x] pp [%p]\n"
        ,i, entry->phys_pages[i].pa, entry->phys_pages[i].va, &entry->phys_pages[i]);
  }
}

int count_phys_pages(pde_t *pgdir) {
  PgdirPhysPagesEntry *entry = find_entry(pgdir);
  return entry->num_pages;
}