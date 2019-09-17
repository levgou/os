#include "param.h"
#include "paging.h"


#ifndef OS192ASSIGNMENT3_PROC_H
#define OS192ASSIGNMENT3_PROC_H


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

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

typedef struct PVA{
  uint pa;
  uint va;
  struct PVA *prev;
  struct PVA *next;
} PVA;

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  //Swap file. must initiate with create swap file
  struct file *swapFile;      //page file
  PageMeta pmeta;
};

typedef struct {
  pde_t *pgdir;
  PVA phys_pages[MAX_PSYC_PAGES];
  PVA *first;
  PVA *last;
  int num_pages;
} PgdirPhysPagesEntry;


void register_page(pde_t *pgdir, uint va, uint pa);
void unregister_page(pde_t *pgdir, uint va, uint pa);
PVA* update_page_va(struct proc *cur_proc, uint va, uint pa);

void addPageToEndOfQueue(PVA *pva, pde_t * pgdir);
PVA* removePageFromQueue(PVA *pva, pde_t * pgdir);
PVA* select_loser_page_by_lifo();
PVA* select_loser_page_by_scfifo();

void arrange_pva_linklist_of_newborn_proc(pde_t *pgdir, PVA *fathers_first);

extern PgdirPhysPagesEntry pgdir_phys[];

PgdirPhysPagesEntry *find_entry(pde_t *pgdir);
PgdirPhysPagesEntry *add_entry(pde_t *pgdir);
void remove_entry(pde_t *pgdir);

void print_pages(PgdirPhysPagesEntry *entry );
void print_paged_out(PageMeta *page_meta, struct proc *p);
void print_entry(pde_t *pgdir);

int count_phys_pages(pde_t *pgdir);

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap



#endif