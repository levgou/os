#include "spinlock.h"
#include "param.h"
#include "mmu.h"
#include "types.h"
#include "kthread.h"


#ifndef PROC_H
#define PROC_H
# define NTHREAD 16

enum kthread_state { T_UNUSED, T_EMBRYO, T_SLEEPING, T_RUNNABLE, T_RUNNING, T_ZOMBIE };


typedef struct {
  char *kstack;                   // Bottom of kernel stack for this thread
  enum kthread_state state;       // Thread state
  int tid;                         // id per thread
  struct trapframe *tf;           // Trap frame for current syscall
  struct context *context;        // swtch() here to run process
  void *chan;                       // If non-zero, sleeping on chan

  int killed;                     // used for threads to kill each other
} kthread;

kthread *my_thread(void);

// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
  kthread *thread;             // The thread running on current proccess

};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.

struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, ZOMBIE, ACTIVE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)               // - shared between threads
  pde_t* pgdir;                // Page table                                   // - ???
//  char *kstack;                // Bottom of kernel stack for this process    // moved to thread
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
//  struct trapframe *tf;        // Trap frame for current syscall              // moved to thread
//  struct context *context;     // swtch() here to run process                 // per thread
//  void *chan;                  // If non-zero, sleeping on chan               // per thread
  int killed;                  // If non-zero, have been killed                 // - ??
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  kthread threads[NTHREAD];
  kthread *the_one_who_lived;    // when performing exec and want to kill all threads but current thread
};

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

int
last_thread_alive(struct proc *p, kthread *cur_thread);

kthread*
alloc_thread(struct proc *p);

kthread *
find_thread_by_state(struct proc *p, enum kthread_state state);

kthread *
find_other_active_thread(struct proc *p, kthread *my_thread);

kthread *
find_thread_by_tid(struct proc *p, int tid);

void clean_thread(kthread *t);

void
exit1(int);



#endif