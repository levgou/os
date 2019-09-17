//
// Created by lev on 5/23/19.
//

#include "types.h"
#include "mmu.h"

#ifndef OS192ASSIGNMENT3_PAGGING_H
#define OS192ASSIGNMENT3_PAGGING_H


#define LIFO 1
#define SCFIFO 2
#define NONE 3

#define VERBOSE_FALSE 0
#define VERBOSE_TRUE 1

#if SELECTION == LIFO
  #define POLICY LIFO
#elif SELECTION == SCFIFO
  #define POLICY SCFIFO
#else
  #define POLICY NONE
#endif


#if VERBOSE_PRINT == VERBOSE_TRUE
  #define VERBOSE VERBOSE_TRUE
#else
  #define VERBOSE VERBOSE_FALSE
#endif



#define MAX_PSYC_PAGES 16
#define MAX_TOTAL_PAGES 32
#define PAGE_INFO_NUM (MAX_TOTAL_PAGES - MAX_PSYC_PAGES)

int PAGES_AVAILABLE_KERNEL_START;
int PAGES_AVAILABLE_CURRENTLY;

typedef struct {
  int num_protected_pages;
  int num_page_faults;
  int num_paged_out_ever;
} RuntimeMeta;

typedef struct {
  int is_valid;
  uint offset;
  uint virt_address;
} PageInfo;

typedef struct {
  PageInfo paged_out_pages[PAGE_INFO_NUM];
  int file_exists;
  uint cur_swap_offset;
  RuntimeMeta rt_meta;
} PageMeta;

int does_page_exist(PageMeta *pm, uint virt_address);
int offset_of_page(PageMeta *pm, uint virt_address);
int clean_page_info(PageMeta *pm, uint virt_address);
int save_page_info(PageMeta *pm, uint offset, uint virt_address);
int reset_all_pages_meta(PageMeta *pm);

int count_paged_out(PageMeta *pm);
int is_something_paged_out(PageMeta *pm);


#endif //OS192ASSIGNMENT3_PAGGING_H
