//
// Created by omryma on 6/17/19.
//

#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1, "<#device> <#inode> <is valid> <type> <(major,minor)> <hard links> <blocks used> \n");
  inodes_info();
  exit();
}
