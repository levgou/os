//
// Created by lev on 5/6/19.
//
#include "user.h"
#include "tournament_tree.h"
#include "kthread.h"

int is_acquire_request_valid(trnmnt_tree *tree, int ID);
int clean_and_leave_this_forsaken_land(int index_mutex_failed_alloc, trnmnt_tree *tree, int mid);
int get_daddy(int mutex_id, trnmnt_tree *tree);
int ID_to_mutex(int ID);
int is_release_request_valid(trnmnt_tree *tree, int ID);
void release_path_mutexes(int mutex_index, trnmnt_tree *tree);

int num_nodes_for_depth(int depth) { return 1 << depth; } //how many nodes in this level

trnmnt_tree* trnmnt_tree_alloc(int depth)
{
  int i;
  int num_mtx = num_nodes_for_depth(depth+1) - 1;

  trnmnt_tree *new_tree = (trnmnt_tree*)malloc(sizeof(trnmnt_tree));
  memset(new_tree, 0, sizeof(trnmnt_tree));

  int tree_mid = kthread_mutex_alloc();
  if(new_tree == 0 || tree_mid == -1){
    clean_and_leave_this_forsaken_land(0, new_tree, tree_mid);
    return 0;
  }
  new_tree->tree_mutex_id = tree_mid;
  new_tree->depth = depth;
  new_tree->size = num_mtx;
  new_tree->tid_in_root = ROOT_IS_EMPTY;

  int *new_mutexes =  (int*)malloc(num_mtx * sizeof(int));
  int *new_thread_ids =  (int*)malloc((num_mtx + 1) * sizeof(int));

  memset(new_mutexes, 0, num_mtx * sizeof(int));
  memset(new_thread_ids, 0, (num_mtx + 1) * sizeof(int));

  if(new_mutexes == 0 || new_thread_ids ==0) {
    clean_and_leave_this_forsaken_land(0, new_tree, new_tree->tree_mutex_id);
    return 0;
  }


  for(i = 0; i < num_mtx - 1; i++)
  {
    new_mutexes[i] = kthread_mutex_alloc();

    if (new_mutexes[i] == -1){
      clean_and_leave_this_forsaken_land(i, new_tree, new_tree->tree_mutex_id);
      return 0;
    }
  }

  new_tree->tree_mutexes = new_mutexes;
  new_tree->thread_ids = new_thread_ids;

  return new_tree;
}

int clean_and_leave_this_forsaken_land(int index_mutex_failed_alloc, trnmnt_tree *tree, int mid) {
  int j;
  if (!mid && !tree)
    return 0;
  if (!tree && mid > 0)
    return kthread_mutex_dealloc(mid);

  kthread_mutex_lock(tree->tree_mutex_id);
  for(j = index_mutex_failed_alloc; j >= 0; j--){
    if((kthread_mutex_dealloc(tree->tree_mutexes[j]) == -1)) {
      kthread_mutex_unlock(tree->tree_mutex_id);
      return -1;
    }
  }
  kthread_mutex_unlock(tree->tree_mutex_id);
  kthread_mutex_dealloc(tree->tree_mutex_id);
  free(tree->thread_ids);
  free(tree->tree_mutexes);
  free(tree);
  return 0;
}

int trnmnt_tree_dealloc(trnmnt_tree *tree){
  int i;
  kthread_mutex_lock(tree->tree_mutex_id);
  for(i = 0; i <= tree->size; i++){
    int cur_tid = tree->thread_ids[i];
    if (cur_tid) {
      kthread_mutex_unlock(tree->tree_mutex_id);
      return -1;
    }
  }
  kthread_mutex_unlock(tree->tree_mutex_id);
  return clean_and_leave_this_forsaken_land(tree->size-1, tree, tree->tree_mutex_id);

}

int get_daddy(int mutex_id, trnmnt_tree *tree){
  return ((mutex_id/2) + (tree->size/2 + 1));
}

int ID_to_mutex(int ID){
  return ID/2;
}

int trnmnt_tree_acquire(trnmnt_tree* tree,int ID){
  //printf(1, "IR R ID [%d]\n", ID);

  kthread_mutex_lock(tree->tree_mutex_id);
  if(!is_acquire_request_valid(tree, ID)){
    kthread_mutex_unlock(tree->tree_mutex_id);
    return -1;
  }
  int cur_tid = kthread_id();
  tree->thread_ids[ID] = cur_tid;
  kthread_mutex_unlock(tree->tree_mutex_id);

  int curr_node = ID_to_mutex(ID);
  while (curr_node >= 0 && curr_node < tree->size){
    kthread_mutex_lock(tree->tree_mutexes[curr_node]);
    curr_node = get_daddy(curr_node, tree);
  }
  tree->tid_in_root = kthread_id();
  return 0;
}

int is_acquire_request_valid(trnmnt_tree *tree, int ID) {
  int valid_range = (ID >= 0 && ID <= tree->size);
  int thread_ID_free = (!tree->thread_ids[ID]);
  int thread_not_allocated_already = 1;

  int i;
  for (i = 0; i < tree->size; ++i) {
    if(tree->thread_ids[i] == kthread_id()){
      thread_not_allocated_already = 0;
      break;
    }
  }

  return valid_range && thread_ID_free && thread_not_allocated_already;
}

int trnmnt_tree_release(trnmnt_tree* tree, int ID){
  int check = tree->thread_ids[ID];
  if(!is_release_request_valid(tree, ID))
    return -1;
  tree->tid_in_root = ROOT_IS_EMPTY;
  int first_mutex = ID_to_mutex(ID);

  //kthread_mutex_lock(tree->tree_mutex_id);
  release_path_mutexes(first_mutex, tree);
  //kthread_mutex_unlock(tree->tree_mutex_id);

  tree->thread_ids[ID] = 0;
  check+=2;
  //printf(1, "released with ID [%d]\n", ID);
  return 0;
}

void release_path_mutexes(int mutex_index, trnmnt_tree *tree){
  if (mutex_index >= tree->size)
    return;

  int my_daddy = get_daddy(mutex_index, tree);
  release_path_mutexes(my_daddy, tree);
  kthread_mutex_unlock(tree->tree_mutexes[mutex_index]);
  //printf(1, "released with mid [%d]\n", mutex_index);

}

int is_release_request_valid(trnmnt_tree *tree, int ID){
  int valid_range = (ID >= 0 && ID <= tree->size);
  int is_it_me = (kthread_id() == tree->thread_ids[ID]);

  int my_tid = kthread_id();
  int is_it_my_mutex = (my_tid == tree->tid_in_root);

  return valid_range && is_it_me && is_it_my_mutex;
}
