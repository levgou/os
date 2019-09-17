
#ifndef kthread_h
#define kthread_h

#include "spinlock.h"
#include "mutex.h"
#include "tournament_tree.h"


#define MAX_STACK_SIZE 4000
#define MAX_MUTEXES 640


/********************************
        The API of the KLT package
 ********************************/

int kthread_create(void (*start_func)(), void* stack);
int kthread_id();
void kthread_exit();
int kthread_join(int thread_id);

int kthread_mutex_alloc();
int kthread_mutex_dealloc(int mutex_id);
int kthread_mutex_lock(int mutex_id);
int kthread_mutex_unlock(int mutex_id);

trnmnt_tree* trnmnt_tree_alloc(int depth);
int trnmnt_tree_dealloc(trnmnt_tree* tree);
int trnmnt_tree_acquire(trnmnt_tree* tree,int ID);
int trnmnt_tree_release(trnmnt_tree* tree,int ID);

typedef struct{

    kthread_mutex_t mutexes_holder[MAX_MUTEXES];
    struct spinlock mutex_arr_lock;
    int mutex_arr_counter;


} mutexes_array;

mutexes_array mutexes_arr;

#endif
