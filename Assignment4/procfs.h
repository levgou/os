//
// Created by lev on 6/13/19.
//

#include "proc.h"
#include "fs.h"

#ifndef ASSIGNMENT4_PROCFS_H
#define ASSIGNMENT4_PROCFS_H

#define IDEINFO_INUM 999
#define FILESTAT_INUM 888
#define INODEINFO_INUM 777
#define EMPTY_DIR_SZ 32

typedef struct {
  char name[DIRSIZ];
  int inum;
} NAME_INUM;

void procfsiread(struct inode* dp, struct inode *ip);

void init_proc_dev_dir(struct inode *ip);
void init_proc_dev_subs();

void add_pid_entry(struct proc *p);
void del_pid_entry(struct proc *p);
void set_dev_type(struct inode *ip);
void set_dev_size(struct inode *ip);

#endif //ASSIGNMENT4_PROCFS_H
