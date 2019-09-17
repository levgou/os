//
// Created by lev on 5/23/19.
//

#include "paging.h"
#include "defs.h"
#include "mmu.h"
#include "proc.h"


PageInfo *find_page(PageMeta *pm, uint virt_address) {
  uint page_va = va_to_pg_va_uint(virt_address);

  for (int i = 0; i < PAGE_INFO_NUM; ++i) {
    if(!pm->paged_out_pages[i].is_valid)
      continue;

    if(pm->paged_out_pages[i].virt_address == page_va)
      return &pm->paged_out_pages[i];
  }

  return 0;
}

PageInfo *find_empty_page(PageMeta *pm) {
  for (int i = 0; i < PAGE_INFO_NUM; ++i) {
    if(!pm->paged_out_pages[i].is_valid)
      return &pm->paged_out_pages[i];
  }

  return 0;
}

int does_page_exist(PageMeta *pm, uint virt_address) {
  if(find_page(pm, virt_address))
    return 1;
  return 0;
}

int offset_of_page(PageMeta *pm, uint virt_address) {
  PageInfo *pinfo;

  if(!(pinfo = find_page(pm, virt_address)))
    return -1;

  return pinfo->offset;
}

int clean_page_info(PageMeta *pm, uint virt_address) {
  PageInfo *pinfo;
  uint offset;

  if(!(pinfo = find_page(pm, virt_address)))
    return 0;

  offset = pinfo->offset;
  memset(pinfo, 0, sizeof(PageInfo));
  pinfo->offset = offset;

  return 1;
}

int save_page_info(PageMeta *pm, uint offset, uint virt_address) {
  uint page_va = va_to_pg_va_uint(virt_address);

  PageInfo *pinfo;
  if (!(pinfo = find_empty_page(pm))) {
    return 0;
  }

  pinfo->is_valid = 1;
  pinfo->offset = offset;
  pinfo->virt_address = page_va;

  return 1;
}

int reset_all_pages_meta(PageMeta *pm) {
  memset(pm, 0, sizeof(PageMeta));

  int index = 0;
  uint i = 0;
  for (; i < PAGE_INFO_NUM * PGSIZE; i += PGSIZE) {
    pm->paged_out_pages[index].offset = i;
    index ++;
  }

  return 1;
}

int count_paged_out(PageMeta *pm) {
  int n = 0;
  for (int i = 0; i < PAGE_INFO_NUM; ++i) {
    n += pm->paged_out_pages[i].is_valid;
  }
  return n;
}

int is_something_paged_out(PageMeta *pm) {
  return count_paged_out(pm) ? 1 : 0;
}

//--------------- TASK 3 -------------------------

void addPageToEndOfQueue(PVA *pva, pde_t * pgdir){
  PgdirPhysPagesEntry *entry = find_entry(pgdir);

  if (!entry->first && !entry->last) {
    entry->first = pva;
    entry->last = pva;

    entry->first->next = 0;
    entry->first->prev = 0;
    return;
  }

  entry->last->next = pva;
  pva->prev = entry->last;
  entry->last = pva;

}

PVA* removePageFromQueue(PVA *pva, pde_t * pgdir){
  PgdirPhysPagesEntry *entry = find_entry(pgdir);


  if (!entry->first && !entry->last) {
    panic("NO WAY");
  }

  if (entry->first->next == 0 && entry->first == pva) {
    entry->first = entry->last = 0;
  } else if (pva->prev == 0) {
    pva->next->prev = 0;
    entry->first = pva->next;
  } else if (pva->next == 0) {
    pva->prev->next = 0;
    entry->last = pva->prev;
  } else {
    pva->prev->next = pva->next;
    pva->next->prev = pva->prev;
  }
  pva->next = 0;
  pva->prev = 0;

  return pva;
}

PVA* select_loser_page_by_lifo(){
  pde_t *pgdir = myproc()->pgdir;
  PVA *last = find_entry(pgdir)->last;
  PVA *loser_address = removePageFromQueue(last, pgdir);
  return loser_address;
}

PVA* select_loser_page_by_scfifo(){
  pde_t *pgdir = myproc()->pgdir;
  PgdirPhysPagesEntry *entry = find_entry(pgdir);
  PVA *cur_node = entry->first;

  while(cur_node != 0) {
    if (check_page_flag(&cur_node->va, PTE_A)) {
      remove_page_flag(&cur_node->va, PTE_A);
      cur_node = cur_node->next;
    }
    else {
      return removePageFromQueue(cur_node, pgdir);
    }
  }

  return removePageFromQueue(entry->first, pgdir);
}

PVA *find_equal_node_in_array(pde_t *pgdir, PVA *the_who) {
  int i = 0;
  PVA *cur;
  PgdirPhysPagesEntry *entry = find_entry(pgdir);

  for(; i < MAX_PSYC_PAGES; i++) {
    cur = &entry->phys_pages[i];
    if (cur != 0 && cur->va == the_who->va){
      return cur;
    }
  }
  return 0;
}

void arrange_pva_linklist_of_newborn_proc(pde_t *pgdir, PVA *fathers_first) {
  if(POLICY == NONE)
    return;

  PVA *fathers_node, *cur_new_node, *prev_new_node;;
  PgdirPhysPagesEntry *entry = find_entry(pgdir);

  if (!(entry->first = find_equal_node_in_array(pgdir, fathers_first)))
    panic("where first?");

  // after we find the first node - it still points to father's nodes
  fathers_node = entry->first->next;
  prev_new_node = entry->first;

  while (fathers_node) {
    if(!(cur_new_node = find_equal_node_in_array(pgdir, fathers_node) ))
      panic("Maybe find?");

    prev_new_node->next = cur_new_node;
    cur_new_node->prev = prev_new_node;
    cur_new_node->next = 0; // just in case

    entry->last = cur_new_node;
    prev_new_node = cur_new_node;
    fathers_node = fathers_node->next;
  }
}

