//
// Created by lev on 4/27/19.
//


#include "types.h"
#include "stat.h"
#include "user.h"
#include "kthread.h"
#include "tournament_tree.h"

// --------------------------------------- Defs ---------------------------------------


#define UNUSED(x) (void)(x)  // use this on variables u dont want to use but want to keep
#define NOT_MANY_TIMES 1000
#define MANY_TIMES 200000000
#define ALLITLE_BIT_MANY_TIMES 10000000
#define A_FEW_ATTEMPTS 5
#define NUM_THREADS 8

int counters[6] = {0};
int counter = 0;

trnmnt_tree *tree;

void medium_task_to_do();

typedef int (*TestFunc)(void);

enum TestStatus {
  PASS = 1, FAIL = 0
};

typedef struct {
  TestFunc testFunc;
  char *testName;
} Test;

// --------------------------------------- Helpers ---------------------------------------


void
short_task_to_do_without_exiting() {
  for (int j = 0; j < NOT_MANY_TIMES; ++j) {
    printf(0, "");
  }
}

void
medium_task_to_do() {
  printf(1, "thread created successfully, his tid is %d\n", kthread_id());
  for (int j = 0; j < ALLITLE_BIT_MANY_TIMES; ++j) {
    printf(0, "");
  }
  kthread_exit();
}

void
long_task_to_perform() {
  char some_str[2] = {'!'};
  char zero = '0';
  char curChar;

  for (int i = 0; i < MANY_TIMES; ++i) {
    curChar = zero + 1 + i % 9; // some char between 1-9
    some_str[i % 2] = curChar;
    UNUSED(some_str);
    printf(0, "");
  }
  kthread_exit();

}

void
infinite_task_to_work_on() {
  while (1) {
    printf(0, "");
  }
}

void
run_many_threads_with_long_tasks() {


  int i;
  for (i = 0; i < NUM_THREADS; ++i) {

    if (!fork()) {
      medium_task_to_do();
      kthread_exit();
    }
  }

}

void mutex_fun() { //check it prints 1-10 consistently

  kthread_mutex_lock(0);
  if (counter < 6) {
    counters[counter] = counter;
    counter++;
  }
  kthread_mutex_unlock(0);
  kthread_exit();

}

void reset_array(int arr[], int size){
  int i;
  for(i = 0; i < size; i++){
    arr[i] = 0;
  }
}

int verify_array(int arr[], int size){
  int i;
  for(i = 0; i < size; i++){
    printf(1, "%d ", arr[i]);
    if(arr[i] != i)
      return FAIL;
  }
  return PASS;
}

int myID;
int myID_protector;
int was_in_root[NUM_THREADS];  //verifies that every thread visited root;

//
//int even_counter = 0;
//int odd_counter = 1;
//int even_mid;
//int odd_mid;
//int tree_competitors[NUM_THREADS] = {0};
//int tree_index = 0;
//
//
//void even_tree_func(){
//  short_task_to_do_without_exiting();
//
//  int my_id;
//  kthread_mutex_lock(even_mid);
//  my_id = even_counter;
//  even_counter += 2;
//  kthread_mutex_unlock(even_mid);
//
//  trnmnt_tree_acquire(tree, my_id);
//  printf(1, "EEEEE index is %d\n", tree_index);
//  tree_competitors[tree_index++] = 2;
//  trnmnt_tree_release(tree, my_id);
//
//  kthread_exit();
//}
//
//void odd_tree_func(){
//  int my_id;
//  kthread_mutex_lock(odd_mid);
//  my_id = odd_counter;
//  odd_counter += 2;
//  kthread_mutex_unlock(odd_mid);
//
//  trnmnt_tree_acquire(tree, my_id);
//  printf(1, "curr tree index is %d\n", tree_index);
//  tree_competitors[tree_index++] = 1;
//  trnmnt_tree_release(tree, my_id);
//
//  kthread_exit();
//}
//
//
//int tree_outcome_is_the_way_i_like_it_like_it() {
//  int i;
//
//  for (i = 0; i < NUM_THREADS ; ++i) {
//    printf(1, "%d ", tree_competitors[i]);
//  }
//  printf(1, "\n");
//
//  for (i = 0; i < NUM_THREADS/2 ; ++i) {
//    if (tree_competitors[i] != 1)
//      return FAIL;
//  }
//
//  for (; i < NUM_THREADS; ++i) {
//    if (tree_competitors[i] != 2)
//      return FAIL;
//  }
//
//  return PASS;
//}

void tree_func(){
  int this_thread_id, this_index_ID;
  kthread_mutex_lock(myID_protector);
  this_index_ID = myID;
  trnmnt_tree_acquire(tree, myID++);
  kthread_mutex_unlock(myID_protector);
  while(1) {
    if(tree->tid_in_root != ROOT_IS_EMPTY){
      this_thread_id = kthread_id();
      printf(1, "thread with ID %d in root (tid [%d])\n", this_index_ID, this_thread_id);
      was_in_root[this_index_ID] = this_thread_id;
      break;
    }
  }
  trnmnt_tree_release(tree, this_index_ID);
  kthread_exit();
}

int verify_all_threads_visited_root(int size){
  for (int i = 0; i < size; ++i) {
    if (was_in_root[i] < 0)
      return FAIL;
  }
  return PASS;
}


// --------------------------------------- TESTS ---------------------------------------

int
test_kthread() {
  int threads_arr[NUM_THREADS];
  int i, tid;
  void *st;

  for(i = 0; i < NUM_THREADS; i++) {

    st = malloc(MAX_STACK_SIZE);
    tid = kthread_create(&medium_task_to_do, st);
    threads_arr[i] = tid;

    if (tid < 0) {
      printf(1, "thread %d created unsuccessfully\n", i);

      for(int j = 0; j < i; j++)
        kthread_join(threads_arr[j]);

      return FAIL;
    }
  }

  int join_suc;
  for(int i = 0; i < NUM_THREADS; i++){
    join_suc = kthread_join(threads_arr[i]);
    if (join_suc == 0)
      printf(1, "thread %d joined successfully\n", threads_arr[i]);
    else {
      printf(1, "thread %d joined terribley wrong\n", threads_arr[i]);
      return FAIL;
    }
  }
  return PASS;
}

int test_mutex() {
  int j;
  for(j = 0; j <= A_FEW_ATTEMPTS; j++) {
    printf(1, "attempt no. %d ::\n", j);
    counter = 0;
    int threads_arr[NUM_THREADS];
    int i;
    int mid = kthread_mutex_alloc();
    if (mid < 0) {
      printf(1, "mutex allocated terribley wrong\n");
      return FAIL;
    } //else printf(1, "mutex allocated successfully\n");

    for (i = 0; i < NUM_THREADS; i++) {
      void *st = malloc(MAX_STACK_SIZE);
      threads_arr[i] = kthread_create(&mutex_fun, st);
    }

    if ((kthread_mutex_dealloc(0)) == -1) {
      printf(1, "mutex de-allocated terribley wrong\n");
      return FAIL;
    }
    //else printf(1, "mutex de-allocated successfully\n");

    for (i = 0; i < NUM_THREADS; i++)
      kthread_join(threads_arr[i]);
    if (verify_array(counters, 6)){
      printf(1, "\nattempt %d succeeded\n", j);
    } else {
      printf(1, "\nattempt %d didnt succeed", j);
      return FAIL;
    }
    reset_array(counters, 6);
  }
  return PASS;

}

int
test_trnmnt_tree() {
  int i;
  int threads_arr[NUM_THREADS];
  tree = trnmnt_tree_alloc(2);
  myID = 0;
  myID_protector = kthread_mutex_alloc();
  reset_array(was_in_root, NUM_THREADS);

//  even_mid = kthread_mutex_alloc();
//  odd_mid = kthread_mutex_alloc();

  if (tree == 0){
    printf(1, "tree allocated unsuccessfully");
    return FAIL;
  }
  void *st;
  for (i = 0; i < NUM_THREADS; i++) {
    st = malloc(MAX_STACK_SIZE);
//    if (i%2 == 0){
//      threads_arr[i] = kthread_create(&even_tree_func, st);
//    }
//    else {
      threads_arr[i] = kthread_create(&tree_func, st);
//    }
  }

  for(i = 0; i < NUM_THREADS; i++) {
    kthread_join(threads_arr[i]);
  }

  return verify_all_threads_visited_root(NUM_THREADS);
}

int
main(int argc, char *argv[]) {

  Test tests[] = {
          {test_kthread,                 "test_kthread"},
          {test_mutex,                    "test_mutex"},
          {test_trnmnt_tree,             "test_trnmnt_tree"}
  };

  int numOfTests = sizeof(tests) / sizeof(Test);
  int t;
  for (t = 0; t < numOfTests; ++t) {
    printf(2, "\n---- Test %d [%s] STARTED! ----\n", t, tests[t].testName);

    if (tests[t].testFunc() != PASS)
      printf(2, "---- Test %d [%s] FAILED! ---- \n", t, tests[t].testName);
    else
      printf(2, "---- Test %d [%s] PASSED ---- \n", t, tests[t].testName);
  }

  exit();

}