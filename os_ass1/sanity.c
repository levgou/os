#include "types.h"
#include "stat.h"
#include "perf.h"
#include "user.h"

// --------------------------------------- Defs ---------------------------------------


#define UNUSED(x) (void)(x)  // use this on variables u dont want to use but want to keep
#define NOT_MANY_TIMES 100
#define MANY_TIMES 200000000
#define ALLITLE_BIT_MANY_TIMES 10000000
#define NUM_OF_CHILDREN 5
#define NUM_PROCS_FOR_STATS 20

typedef int (*TestFunc)(void);

typedef void (*PerfsHandler)(struct perf[], int);

enum TestStatus {
  PASS = 1, FAIL = 0
};

typedef struct {
  TestFunc testFunc;
  char *testName;
} Test;

// --------------------------------------- Helpers ---------------------------------------


void
short_task_to_do() {
  for (int j = 0; j < NOT_MANY_TIMES; ++j) {
    printf(0, "");
  }
}

void
medium_task_to_do() {
  for (int j = 0; j < ALLITLE_BIT_MANY_TIMES; ++j) {
    printf(0, "");
  }
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

}

void
infinite_task_to_work_on() {
  while (1) {
    printf(0, "");
  }
}

void
run_many_procs_with_long_tasks(int priority_zero_allowed, PerfsHandler perfsHandler) {

  int i, curPriority;
  struct perf perfs[NUM_PROCS_FOR_STATS];
  struct perf *curPerf = perfs;

  for (i = 0; i < NUM_PROCS_FOR_STATS; ++i) {
    curPriority = i % 10;
    curPriority = (!curPriority && !priority_zero_allowed) ? 1 : curPriority;

    if (!fork()) {
      priority(curPriority);
      medium_task_to_do();
      exit(0);
    }
  }

  for (i = 0; i < NUM_PROCS_FOR_STATS; ++i) {
    wait_stat(null, curPerf);
    curPerf++;
  }

  if (perfsHandler) {
    perfsHandler(perfs, NUM_PROCS_FOR_STATS);
  }
}

void
print_stats(struct perf perfs[], int numOfperfs) {
  int i;
  volatile int numOfTimeAllWaited = 0, allTurnAroundTime = 0;
  for (i = 0; i < numOfperfs; ++i) {
    numOfTimeAllWaited += perfs[i].retime;
    allTurnAroundTime +=  perfs[i].ttime- perfs[i].ctime ;
  }

  printf(2, "Total wait time: [%d], Avg wait time: [%d] \n",
          numOfTimeAllWaited, numOfTimeAllWaited / numOfperfs);

  int avgTurnAround = allTurnAroundTime / numOfperfs;
  printf(2, "Total wait time: [%d], Avg TurnAround time: [%d] \n", allTurnAroundTime, avgTurnAround);
}
// --------------------------------------- TESTS ---------------------------------------

int
test_exit_code() {
  int child_status;

  if (fork()) {

    wait(&child_status);
    if (child_status == 7) {
      return PASS;
    } else {
      return FAIL;
    }

    // child:
  } else {
    exit(7);
  }

}


int
test_detach() {
  int pid, child_status, first_detach_status, second_detach_status;

  if ((pid = fork())) {
    first_detach_status = detach(pid);
    second_detach_status = detach(777);
    wait(&child_status);

    if (first_detach_status) {
      printf(2, "Failed to detach child with pid %d \n", pid);
      return FAIL;
    }

    if (second_detach_status > 0) {
      printf(2, "Succeeded to detach unexciting child  with pid %d \n", 777);
      return FAIL;
    }
    return PASS;

  } else {
    sleep(1000);
    exit(0);
  }
}

/*
 * Test that the processes finish in oder of execution
 */
int
test_round_robbing() {

  int child_exit_codes[NUM_OF_CHILDREN] = {-1};
  int *cur_child = child_exit_codes;
  int i;

  policy(1);

  for (i = 0; i < NUM_OF_CHILDREN; ++i) {
    sleep(50); // to ensure that the processes are added to ptable in the right order
    if (!fork()) {
      printf(2, "START %d \n", i);
      long_task_to_perform();
      exit(i + 1);
    }
  }

  // wait for children
  while (wait(cur_child) != -1) {
    cur_child++;
  }

  // verify that the exit codes are in order of exec (because same priority)
  for (i = 0; i < NUM_OF_CHILDREN; ++i) {
    if (child_exit_codes[i] != i + 1) {
      printf(2, "Child number %d should have exited with status %d but got %d \n", i, i + 1, child_exit_codes[i]);

      printf(2, "All statuses: ");
      for (i = 0; i < NUM_OF_CHILDREN; ++i) {
        printf(2, "%d ", child_exit_codes[i]);
      }
      printf(2, "\n");

      return FAIL;
    }
  }

  return PASS;
}

/*
 * Run princesses with priorities 10, 5 , 1 -> verify they finish in order 1, 5, 10
 */
int
test_priority_policy() {

  int child_exit_codes[3] = {-1};
  int *cur_child = child_exit_codes;

  policy(2);

  if (!fork()) {
    priority(10);
    long_task_to_perform();
    exit(10);
  }

  if (!fork()) {
    priority(5);
    long_task_to_perform();
    exit(5);
  }

  if (!fork()) {
    priority(1);
    long_task_to_perform();
    exit(1);
  }

  // wait for children
  while (wait(cur_child) != -1) {
    cur_child++;
  }

  if (child_exit_codes[0] != 1 || child_exit_codes[1] != 5 || child_exit_codes[2] != 10) {
    printf(2, "Child exit code should be in order 1, 5, 10  but are %d, %d, %d \n",
           child_exit_codes[0], child_exit_codes[1], child_exit_codes[2]);
    return FAIL;
  }
  return PASS;

}

/*
 * Run princesses with priorities 10, 0 - 0 with infinite task - test that 10 finishes first -> then kill 0
 */
int
test_priority_policy_with_zero() {

  int first_child_exit_code, child_to_kill_pid;


  policy(3);

  if (!(child_to_kill_pid = fork())) {
    priority(0);
    infinite_task_to_work_on();
    exit(0);
  }

  if (!fork()) {
    priority(10);
    short_task_to_do();
    exit(10);
  }

  wait(&first_child_exit_code);
  kill(child_to_kill_pid);
  wait(0);  // to prevent zombies from eating my brain

  if (first_child_exit_code != 10) {
    printf(2, "For some not apparent reason 10 didnt finish, but %d did! \n", first_child_exit_code);
    return FAIL;
  }

  return PASS;

}

int
test_policies_stats() {
  int i;

  for (i = 1; i <= 3; ++i) {
    priority(i);
    printf(2, "--- PRIORITY %d --- \n", i);
    run_many_procs_with_long_tasks(0, print_stats);
  }

  return PASS;
}


int
main(int argc, char *argv[]) {

  Test tests[] = {
          {test_exit_code,                 "test_exit_code"},
          {test_detach,                    "test_detach"},
          {test_round_robbing,             "test_round_robbing"},
          {test_priority_policy,           "test_priority_policy"},
          {test_priority_policy_with_zero, "test_priority_policy_with_zero"},
          {test_policies_stats,            "test_policies_stats"},
  };

  int numOfTests = sizeof(tests) / sizeof(Test);

  for (int i = 0; i < numOfTests; ++i) {
    printf(2, "\n---- Test %d [%s] STARTED! ----\n", i, tests[i].testName);

    if (tests[i].testFunc() != PASS)
      printf(2, "---- Test %d [%s] FAILED! ---- \n", i, tests[i].testName);
    else
      printf(2, "---- Test %d [%s] PASSED ---- \n", i, tests[i].testName);
  }

  exit(0);

}
