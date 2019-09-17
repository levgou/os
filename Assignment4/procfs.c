#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "procfs.h"


uint set_bytes_to_read(char *dst, char *src, int off, int n, int buf_size);

void init_default_proc_subinodes();

struct dirent *find_relevant_dirent_proc_entries(int off);

struct dirent *find_empty_dir_entry(struct dirent direnet_buf[], int size);

int fill_buffer_pid_status(char *dst, int off, int n, proc_file *pf, struct inode *ip);

int fill_buffer_pid_name(char *dst, int off, int n, proc_file *pf, struct inode *ip);


#define dirent_s (sizeof(struct dirent))

#define STATUS_LEN 8
#define UINT_LEN 10
char *P_STATES[] = {
        [UNUSED]    "UNUSED",
        [EMBRYO]    "EMBRYO",
        [SLEEPING]  "SLEEPING",
        [RUNNABLE]  "RUNNABLE",
        [RUNNING]   "RUNNING",
        [ZOMBIE]    "ZOMBIE"
};

// array holding all dirents under /proc
#define NUM_DEFAULT_SUBPROCS 5
struct dirent PROC_ENTRIES[NUM_DEFAULT_SUBPROCS + NPROC];
struct dirent *PID_PROC_ENTRIES = PROC_ENTRIES + NUM_DEFAULT_SUBPROCS;
uint num_procs = 0;

// INUM from which we start mapping /proc/<PID> inodes
static uint PID_BASE_INUM = 210;

#define PID_INUM_JUMP 3
#define IS_PID(inum) (inum >= 200 && inum <= 500)
#define IS_PID_DIR(inum) (inum % 3 == 0)
#define IS_PID_NAME(inum) (inum % 3 == 1)
#define IS_PID_STATUS(inum) (inum % 3 == 2)


/*
 * Debug function
 * */
void print_PROC_ENTRIES() {
  cprintf("-------------------------------\n");
  for (int i = 0; i < 10; ++i) {
    cprintf("%d) %d  %s\n", i, PROC_ENTRIES[i].inum, PROC_ENTRIES[i].name);
  }
  cprintf("-------------------------------\n");
}


/*
 * returns true iff the supplied inode represents dev directory
 * */
int procfsisdir(struct inode *ip) {
  int isdir = ip->minor == DEV_DIR;
  return isdir;
}


void
procfsiread(struct inode *dp, struct inode *ip) {
  ip->major = PROCFS;
  ip->type = T_DEV;
  ip->valid = 1;

  set_dev_type(ip);
  set_dev_size(ip);
}

void
set_dev_type(struct inode *ip) {
  uint inum = ip->inum;
  if (inum == PROC_INUM ||
      (IS_PID(inum) && IS_PID_DIR(inum)) ||
      inum == INODEINFO_INUM) {
    ip->minor = DEV_DIR;
  } else ip->minor = DEV_FILE;
}

void
set_dev_size(struct inode *ip) {
  uint inum = ip->inum;

  if (IS_PID(inum)) {
    if (IS_PID_DIR(inum))
      ip->size = 2 * dirent_s;

    else if (IS_PID_NAME(inum))
      ip->size = DIRSIZ;

    else if (IS_PID_STATUS(inum))
      ip->size = STATUS_LEN + UINT_LEN + 1;
  } else if (inum == IDEINFO_INUM)
    ip->size = 1000; //TODO

  else if (inum == FILESTAT_INUM)
    ip->size = 100; //TODO

  else if (inum == INODEINFO_INUM)
    ip->size = 200; //TODO

  else if (inum == PROC_INUM)
    ip->size = EMPTY_DIR_SZ + (NUM_DEFAULT_SUBPROCS + num_procs) * dirent_s;
}

/*
 * Handles read for our dev dirs/files (called from iread)
 * */
int procfsread(struct inode *ip, char *dst, int off, int n) {
  InumProcFile *ipf = get_inumproc_entry(ip->inum);
  return ipf->pf.fill_buffer(dst, off, n, &ipf->pf, ip);
}


/*
 * Handles write for our dev dirs/files - our files are read only!
 * */
int procfswrite(struct inode *ip, char *buf, int n) {
  // todo
  cprintf("BBBBBBBBBBBBBBBBBBBBBBBB\n");
  return 0;
}


void
procfsinit(void) {
  devsw[PROCFS].isdir = procfsisdir;
  devsw[PROCFS].iread = procfsiread;
  devsw[PROCFS].write = procfswrite;
  devsw[PROCFS].read = procfsread;
}


/*
 * Update the given dirent with the relevant parameters
 * */
void update_dirent_entry(int inum, struct dirent *dst, char *name, char do_print) {
  struct dirent tmp_dirent;

  tmp_dirent.inum = (ushort) inum;
  memmove(tmp_dirent.name, name, DIRSIZ);
  memmove(dst, &tmp_dirent, dirent_s);

  if (do_print && DEBUG)
    cprintf("+++ added dirent with inum [%d] at [%p] name [%s] +++\n", inum, dst, tmp_dirent.name);
}


/*
 * Read handler for inodeinfo dev dir
 * */
int fill_buffer_inode_info(char *dst, int off, int n, proc_file *pf, struct inode *ip) {
  struct dirent d;

  if (off == 0) {
    update_dirent_entry(ip->inum, &d, ".", 0);

  } else if (off == dirent_s) {
    update_dirent_entry(PROC_INUM, &d, "..", 0);

  } else {

    update_dirent_entry(ip->inum + 1, &d, "name", 0);
    add_if_abscent_inumproc_entry(ip->inum + 1, (proc_file) {fill_buffer_pid_name, pf->curproc});


  }

  memmove(dst, &d, dirent_s);
  return dirent_s;
}


/*
 * Read handler for ideinfo dev file
 * */
int fill_buffer_ide_info(char *dst, int off, int n, proc_file *pf, struct inode *ip) {

  char ide_info[1000];
  memset(ide_info, 0, 1000);
  get_ide_info(ide_info);

  return set_bytes_to_read(dst, ide_info, off, n, sizeof(ide_info));
}


/*
 * Read handler for filestat dev file
 * */
int fill_buffer_file_stat(char *dst, int off, int n, proc_file *pf, struct inode *ip) {
  char file_stat[1000];
  memset(file_stat, 0, 1000);
  get_file_stat(file_stat);

  return set_bytes_to_read(dst, file_stat, off, n, sizeof(file_stat));
}


/*
 * Read handler for /proc/<PID>/name dev file
 * */
int fill_buffer_pid_name(char *dst, int off, int n, proc_file *pf, struct inode *ip) {
  struct proc *curproc = pf->curproc;
  char proc_name[17];
  memset(proc_name, 0, 17);

  memmove(proc_name, curproc->name, (uint) strlen(curproc->name));
  memmove(proc_name + strlen(curproc->name), "\n", 1);

  return set_bytes_to_read(dst, proc_name, off, n, sizeof(proc_name));

}


/*
 * Read handler for /proc/<PID>/status dev file
 * */
int fill_buffer_pid_status(char *dst, int off, int n, proc_file *pf, struct inode *ip) {
  char status[STATUS_LEN + UINT_LEN + 2];
  char size[UINT_LEN];
  memset(size, 0, UINT_LEN);

  uint num_len = uint_to_str(pf->curproc->sz, size);

  memmove(status, P_STATES[pf->curproc->state], STATUS_LEN);
  memmove(status + STATUS_LEN, " ", 1);
  memmove(status + STATUS_LEN + 1, size, num_len);
  memmove(status + STATUS_LEN + 1 + num_len, "\n", 1);
  memset(status + STATUS_LEN + 1 + num_len + 1, 0, 1);    //  for strlen to finish after \n

  return set_bytes_to_read(dst, status, off, n, sizeof(status));
}


/*
 * checks if offset is valid according to the sizes,
 * and read n chars as possible
 */
uint set_bytes_to_read(char *dst, char *src, int off, int n, int buf_size) {

  uint bytes_to_move = (off >= buf_size) ? 0 :
                       (off + n <= buf_size) ? (uint) n :
                       (uint) (buf_size - off);

  if (!bytes_to_move)
    return 0;

  memmove(dst, src + off, bytes_to_move);
//  return bytes_to_move;
  return (uint) strlen(dst);     // fixes all the trailing \0
}


/*
 * Read handler for /proc/<PID> dev dir
 * */
int fill_buffer_pid(char *dst, int off, int n, proc_file *pf, struct inode *ip) {
  if (off > 3 * dirent_s)
    return 0;

  struct dirent d;

  if (off == 0) {
    update_dirent_entry(ip->inum, &d, ".", 0);

  } else if (off == dirent_s) {
    update_dirent_entry(PROC_INUM, &d, "..", 0);

  } else if (off == dirent_s * 2) {
    update_dirent_entry(ip->inum + 1, &d, "name", 0);
    add_if_abscent_inumproc_entry(ip->inum + 1, (proc_file) {fill_buffer_pid_name, pf->curproc});

  } else {
    update_dirent_entry(ip->inum + 2, &d, "status", 0);
    add_if_abscent_inumproc_entry(ip->inum + 2, (proc_file) {fill_buffer_pid_status, pf->curproc});
  }

  memmove(dst, &d, dirent_s);
  return dirent_s;
}


/*
 * This handler addresses reading of /proc
 * fill the buffer with dirent's for:
 * 1. ideinfo - dev file
 * 2. filestat - dev file
 * 3. inodeinfo - dev dir
 * 4-... <PID> - dev dir for each pid of proc in ptable
 * */
int fill_buffer_proc(char *dst, int off, int n, proc_file *pf, struct inode *ip) {
  init_proc_dev_subs();
  struct dirent *cur_dirent = find_relevant_dirent_proc_entries(off);

  if (cur_dirent == 0) {
    return 0;
  }

  memmove(dst, cur_dirent, dirent_s);
  return dirent_s;
}


/*
 * converts off to index (off / dirent), and returns
 * dirent with inum != 0, @index if available
 * */
struct dirent* find_relevant_dirent(int off, struct dirent *buf, int buf_size) {
  int index = off / dirent_s;

  for (int i = 0; i < buf_size; ++i) {

    if (buf[i].inum == 0)
      continue;

    if (index == 0)
      return &buf[i];
    else
      index--;
  }

  return 0;
}


struct dirent *find_relevant_dirent_proc_entries(int off) {
  return find_relevant_dirent(off, PROC_ENTRIES, NUM_DEFAULT_SUBPROCS + NPROC);
}


/*
 * find dirent in supplied array with inum == supplied inum
 * */
struct dirent *find_dir_entry(struct dirent direnet_buf[], int size, uint inum) {
  for (int i = 0; i < size; ++i) {
    if (direnet_buf[i].inum == inum)
      return &direnet_buf[i];
  }

  panic("NO DIRENT WITH INUM");
}


/*
 * find dirent in supplied array with inum == 0
 * */
struct dirent *find_empty_dir_entry(struct dirent direnet_buf[], int size) {
  return find_dir_entry(direnet_buf, size, 0);
}


/*
 * add dirent to PROC_ENTRIES that represents p->pid
 * */
void add_pid_entry(struct proc *p) {
  if (DEBUG) cprintf("Adding entry for pid %d \n", p->pid);

  char cur_nam[DIRSIZ];
  memset(cur_nam, 0, DIRSIZ);
  uint_to_str((uint) p->pid, cur_nam);

  uint cur_inum = PID_BASE_INUM;
  PID_BASE_INUM += PID_INUM_JUMP;  // leave place for name and status

  struct dirent *cur_dir_entry = find_empty_dir_entry(PID_PROC_ENTRIES, NPROC);

  update_dirent_entry(cur_inum, cur_dir_entry, cur_nam, 1);
  add_inumproc_entry(cur_inum, (proc_file){fill_buffer_pid, p});

  num_procs++;
}


/*
 * remove dirent from PROC_ENTRIES that represents p->pid
 * */
void del_pid_entry(struct proc *p) {
  uint proc_inum = get_inumproc_entry_by_proc(p)->inum;
  struct dirent *proc_dirent = find_dir_entry(PID_PROC_ENTRIES, NPROC, proc_inum);

  if (DEBUG)
    cprintf("--- removing dirent with inum [%d] at [%p] name [%s] ---\n", proc_inum, proc_dirent, proc_dirent->name);
  memset(proc_dirent, 0, dirent_s);

  remove_inumproc_entry(proc_inum);
  remove_if_exists_inumproc_entry(proc_inum + 1);  // name
  remove_if_exists_inumproc_entry(proc_inum + 2);  // status

  num_procs--;
}


/*
 * initialize PROC_ENTRIES with relevant dirent's
 * */
void init_default_proc_subinodes() {
  update_dirent_entry(PROC_INUM, &PROC_ENTRIES[0], ".", 1);
  update_dirent_entry(1, &PROC_ENTRIES[1], "..", 1);

  update_dirent_entry(999, &PROC_ENTRIES[2], "ideinfo", 1);
  add_inumproc_entry(999, (proc_file) {fill_buffer_ide_info, 0});

  update_dirent_entry(888, &PROC_ENTRIES[3], "filestat", 1);
  add_inumproc_entry(888, (proc_file) {fill_buffer_file_stat, 0});

  update_dirent_entry(777, &PROC_ENTRIES[4], "inodeinfo", 1);
  add_inumproc_entry(777, (proc_file) {fill_buffer_inode_info, 0});
}


/*
 * register read handler for /proc, and save /proc's inode's inum
 * */
void init_proc_dev_dir(struct inode *ip) {
  PROC_INUM = ip->inum;
  add_inumproc_entry(ip->inum, (proc_file) {fill_buffer_proc, 0});
}


/*
 * first time we read from /proc
 * initialize all default dirs/files needed
 * */
void init_proc_dev_subs() {
  if (PROC_ENTRIES[0].inum == 0) {
    init_default_proc_subinodes();
  }
}


uint uint_to_str(uint num, char *dst) {
  char digits[] = "0123456789";

  uint i;
  uint x = num;

  i = 0;
  do {
    if (dst[0]) {
      int j;
      for (j = strlen(dst); j >= 1; j--)
        dst[j] = dst[j - 1];
    }
    dst[0] = digits[x % 10];
    i++;
  } while ((x /= 10) != 0);

  return i;
}