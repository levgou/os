//
// Created by lev on 4/22/19.
//

#include <stdbool.h>
#include "proc.h"
#include "kthread.h"
#include "x86.h"
#include "defs.h"
#include "types.h"

int find_empty_index_in_mutex_arr();
void clean_mutex(kthread_mutex_t* mutex);


int next_mid = 1;


int kthread_id() {
  return my_thread()->tid;
}

void kthread_exit() {
  exit1(1);
}

int kthread_create(void (*start_func)(), void *stack) {

  kthread *t, *cur_thread;
  acquire(&ptable.lock);

  if ((t = alloc_thread(myproc())) == 0){
    release(&ptable.lock);
    return -1;
  }

  cur_thread = my_thread();
  *t->tf = *cur_thread->tf;
  t->tf->eip = (uint)start_func;
  t->tf->esp = (uint)stack;
  t->state = T_RUNNABLE;
  release(&ptable.lock);

  return t->tid;
}

int kthread_join(int thread_id) {
  struct proc *p = myproc();
  kthread *t;

  acquire(&ptable.lock);
  t = find_thread_by_tid(p, thread_id);

  if (t == 0 || t->state == T_UNUSED) {
    release(&ptable.lock);
    return -1;
  }

  while(t->state != T_ZOMBIE){
    sleep(t, &ptable.lock);
  }

  clean_thread(t);
  release(&ptable.lock);
  return 0;
}

int kthread_mutex_alloc(){
  char mutex_name[] = "mutex lock  ";


  kthread_mutex_t new_mutex;
  new_mutex.locked = 0;
  new_mutex.mid = next_mid++;
  new_mutex.tid = -1;
  new_mutex.pid = myproc()->pid;

  mutex_name[11] = (char)(new_mutex.mid + '0');
  struct spinlock myspinlock;
  initlock(&myspinlock, mutex_name);
  new_mutex.lk = myspinlock;

  acquire(&mutexes_arr.mutex_arr_lock);

  if (mutexes_arr.mutex_arr_counter == MAX_MUTEXES) {
      release(&mutexes_arr.mutex_arr_lock);
      return -1;
  }

  int i = find_empty_index_in_mutex_arr();
  mutexes_arr.mutexes_holder[i] = new_mutex;
  mutexes_arr.mutex_arr_counter += 1;

  release(&mutexes_arr.mutex_arr_lock);

  return new_mutex.mid;
}

int find_empty_index_in_mutex_arr(){
  int i;
  for(i = 0; i < MAX_MUTEXES; i++){
    if (mutexes_arr.mutexes_holder[i].mid <= 0)
      return i;
  }
  return -1;
}

kthread_mutex_t* find_mutex_by_mid(int mid){
  int i;
  kthread_mutex_t *curr_mutex;
  for(i = 0; i < MAX_MUTEXES; i++){
    curr_mutex = &mutexes_arr.mutexes_holder[i];
    if (curr_mutex->mid == mid){
      return curr_mutex;
    }
  }
  return 0;
}

int kthread_mutex_dealloc(int mutex_id){

  acquire(&mutexes_arr.mutex_arr_lock);
  kthread_mutex_t *tbr_mutex = find_mutex_by_mid(mutex_id);
  if (tbr_mutex == 0 || tbr_mutex->lk.locked == 1){
    release(&mutexes_arr.mutex_arr_lock);
    return -1;
  }

  clean_mutex(tbr_mutex);
  mutexes_arr.mutex_arr_counter -= 1;
  release(&mutexes_arr.mutex_arr_lock);
  return 0;
}

void clean_mutex(kthread_mutex_t* mutex){
    dealloc_lock(&mutex->lk);
    mutex->mid = 0;
    mutex->tid = 0;
    mutex->pid = 0;
    mutex->locked = 0;
}

kthread_mutex_t *
get_my_mutex_safely(int mutex_id, int is_unlock)
{
  acquire(&mutexes_arr.mutex_arr_lock);

  kthread_mutex_t *wanted_mutex = find_mutex_by_mid(mutex_id);
  if (wanted_mutex == 0)
  {
    release(&mutexes_arr.mutex_arr_lock);
    return 0;
  }

  int is_thread_correct = 1;
  int tid = my_thread()->tid;

  if (tid != wanted_mutex->tid && is_unlock)
    is_thread_correct = 0;

  int pid = myproc()->pid;
  if(!is_thread_correct || pid != wanted_mutex->pid)
  {
    release(&mutexes_arr.mutex_arr_lock);
    return 0;
  }

  release(&mutexes_arr.mutex_arr_lock);
  return wanted_mutex;
}

int kthread_mutex_lock(int mutex_id){

  kthread_mutex_t *wanted_mutex;
  if(!(wanted_mutex = get_my_mutex_safely(mutex_id, 0)))
    return -1;

  acquire(&wanted_mutex->lk);
  while (wanted_mutex->locked)
    sleep(wanted_mutex, &wanted_mutex->lk);

  wanted_mutex->locked = 1;
//  wanted_mutex->pid = myproc()->pid;
  wanted_mutex->tid = my_thread()->tid;

  release(&wanted_mutex->lk);
  return 0;
}

int kthread_mutex_unlock(int mutex_id)
{
  kthread_mutex_t *wanted_mutex;
  if(!(wanted_mutex = get_my_mutex_safely(mutex_id, 1)))
    return -1;

  acquire(&wanted_mutex->lk);
  wanted_mutex->locked = 0;
  wanted_mutex->tid = 0;
  wakeup(wanted_mutex);
  release(&wanted_mutex->lk);
  return 0;
}

