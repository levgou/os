#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

//#define MAX_PG 16
//#define PG_SIZE 4096

void paging_with_no_swap() {
  int pid;
  void *arr[16];
  printf(1, "\n***Paging with no swap test***\n\n");
  int i = 0;
  if ((pid = fork()) == 0) {
    printf(1, "mallocing %d*%d bytes", 4096, 1);
    for (; i < 16; i++) {
      if ((arr[i] = malloc(4096)) != 0)
        printf(1, ".");
      else break;
    }
    printf(1, "\nPress ^p now to see that there are some pages in physical memory and 0 in swap file\n\n");
    sleep(200);
    printf(1, "freeing %d*%d bytes", 4096, 1);
    i = 0;
    for (; i < 16; i++) {
      if (arr[i] != 0) {
        free(arr[i]);
        printf(1, ".");
      } else break;
    }
    exit();
  } else {
    printf(1, "wait -> [%d]\n", wait());
    printf(1, "\n***Paging with no swap ended***\n");
  }
}

void maximum_paging() {
  int pid;
  void *arr[32];
  printf(1, "\n***Maximum paging test***\n\n");
  int i = 0;
  if ((pid = fork()) == 0) {
    printf(1, "trying to malloc %d*%d bytes", 4096, 32);
    for (; i < 32; i++) {
      if ((arr[i] = malloc(4096)) != 0) printf(1, ".", i);
      else break;
    }
    printf(1, "\nPress ^p now to see that there are 16 pages in physical memory and 16 in swap file\n\n");
    sleep(200);
    printf(1, "freeing %d*%d bytes", 4096, i);
    i = 0;
    for (; i < 32; i++) {
      if (arr[i] != 0) {
        free(arr[i]);
        printf(1, ".");
      } else break;
    }
    exit();
  } else {
    printf(1, "wait -> [%d]\n", wait());
    printf(1, "\n***Maximum paging ended***\n");
  }
}

void paging_with_swap_access() {
  int pid;
  void *arr1[17];
  int arr2[17];

  int i = 0;
  printf(1, "\n***Paging with swap access test***\n\n");
  if ((pid = fork()) == 0) {
    printf(1, "mallocing %d*%d bytes", 4096, 17);
    for (; i < 17; i++) {
      arr1[i] = malloc(4096);
      printf(1, ".");
    }
    i = 0;
    for (; i < 17; i++) {
      *((int*)arr1[i]) = 42;
    }
    printf(1, "\nPress ^p now to see that there are 16 pages in physical memory and some in swap file\n\n");
    sleep(200);

    for (i=0; i < 17; i++) {
      arr2[i] = *((int*)arr1[i]);
    }

    for (; i < 17; i++) {
      if (arr2[i] != 42) {
        printf(1, "\n***Paging with swap access FAILED i[%d]-[%d]***\n", i, arr2[i]);
      }
    }

    printf(1, "freeing %d*%d bytes", 4096, 17);
    for (i =0 ; i < 17; i++)
      free(arr1[i]);
    exit();
  } else {
    wait();
    printf(1, "\n***Paging with swap access ended***\n");
  }
}

void swapping_speed() {
  int pid;
  void *arr1[17];
  char *dummy;
  int i = 0;
  printf(1, "\n***Paging with a lot of swappings in the selected policy, Hold your timers!***\n\n");
  if ((pid = fork()) == 0) {
    printf(1, "mallocing %d*%d bytes", 4096, 17);
    for (; i < 17; i++) {
      arr1[i] = malloc(4096);
      printf(1, ".");
    }
    i = 0;
    for (; i < 17; i++) {
      int num = i * 42;
      dummy = arr1[12 + num % 10];
      *dummy = 42;
    }
    printf(1, "\nPress ^p now to see that there are 16 pages in physical memory and some in swap file\n\n");
    sleep(200);
    printf(1, "freeing %d*%d bytes", 4096, 17);
    i = 0;
    for (; i < 17; i++)
      free(arr1[i]);
    exit();
  } else {
    wait();
    printf(1, "\n***Paging with a lot of swappings ended***\n");
    printf(1, "\n***performance results will be sent to your Email***\n");
  }
}

void fork_with_swap_file() {
  printf(1, "\n***Fork with swap test***\n\n");

  void *some_mem = malloc(4096 * 17);    // to ensure swap file
  int *swapped_int = (int*)(some_mem + 4096 * 17 - 4);
  *swapped_int = 42;

  if (fork() == 0) {
    if(*swapped_int != 42)
      printf(1, "\n***Fork with swap test FAIL ***\n\n");

    exit();
  } else {
    wait();
  }

  printf(1, "\n***Fork with swap test end ***\n\n");
}


void protect_page_test() {
  printf(1, "\n*** Protect page test ***\n\n");

  void *mk = malloc(42);
  void *pm = pmalloc();
  void *pm_not_protected = pmalloc();

  if(protect_page(pm) != 1)
    printf(1, "NOT protected !\n");

  if (fork() == 0) {
    *((int*)pm) = 42;
    printf(1, "\n***I should NOT reach here***\n\n");

    exit();
  } else {
    wait();
  }

  if(pfree(mk) == 1)
    printf(1, "Shouldn't work for malloced mem! \n");

  if(pfree(pm_not_protected) == 1)
    printf(1, "Shouldn't work for not protected mem \n");

  if(pfree(pm) != 1)
    printf(1, "DIDNT pfree \n");

  printf(1, "\n*** Protect page test end (you should see a trap above) ***\n\n");
}

int main(int argc, char *argv[]) {
  printf(1, "\n***MEMORY TESTS - R U READY ?!***\n");

  protect_page_test();
  paging_with_no_swap();
  maximum_paging();
  paging_with_swap_access();
  swapping_speed();
  fork_with_swap_file();

  printf(1, "\n***ALL TESTS ENDED***\n");
  exit();
}