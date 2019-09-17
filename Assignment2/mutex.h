//
// Created by fpd on 5/5/19.
//

#ifndef ASSIGNMENT2_MUTEX_H
#define ASSIGNMENT2_MUTEX_H

#include "spinlock.h"

typedef struct{

  uint locked;       // Is the lock held?
  int mid;           // unique id of mutex
  struct spinlock lk;
  int tid;                //the owner thread
  int pid;                //the owner process

} kthread_mutex_t;

#endif //ASSIGNMENT2_MUTEX_H
