
#ifndef tournament_tree_h
#define tournament_tree_h
#define ROOT_IS_EMPTY -999
#include "mutex.h"

typedef struct
{
  int size;     //num of entire nodes
  int depth;
  int *tree_mutexes; //in the i'th cell -> mutex id
  int *thread_ids;   //in the ID'th cell -> thread id
  int tree_mutex_id;
  int tid_in_root;
} trnmnt_tree;


#endif