#include "fs.h"

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};

typedef struct proc_file {

  int (*fill_buffer)(char*, int, int, struct proc_file*, struct inode*);
  struct proc *curproc;

} proc_file;

// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];

};

// table mapping major device number to
// device functions
struct devsw {
  int (*isdir)(struct inode*);
  void (*iread)(struct inode*, struct inode*);
  int (*read)(struct inode*, char*, int, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

typedef struct {
  uint inum;          // Inode number
  proc_file pf;
} InumProcFile;

extern InumProcFile inum_proc_files[];

InumProcFile *add_inumproc_entry(uint inum, proc_file pf);
void remove_inumproc_entry(uint inum);
InumProcFile *get_inumproc_entry(uint inum);
InumProcFile *get_inumproc_entry_by_proc(struct proc *p);
InumProcFile *add_if_abscent_inumproc_entry(uint inum, proc_file pf);
void remove_if_exists_inumproc_entry(uint inum);


#define CONSOLE 1
#define PROCFS  2
#define IS_DEV_DIR(ip) (ip->type == T_DEV && devsw[ip->major].isdir && devsw[ip->major].isdir(ip))
