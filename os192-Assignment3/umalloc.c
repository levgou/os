#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"
#include "mmu.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{
  // ap point to sbrk start + 1

  Header *bp, *p;

  bp = (Header*)ap - 1;   // header at start of sbrk

  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;

  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;  // first time memory over we get here?
  freep = p;        // p will point to sbrk memory?
}

static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  return freep;
}

void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }

  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void*)(p + 1);
    }
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}

#define PAGE_IN_UNITS             (4096 / 8)
#define MIN_PLACES_IN_BLOCK    PAGE_IN_UNITS * 2


uint
index_in_block_for_pmalloc(uint size) {
  uint indx =  (size / PAGE_IN_UNITS - 1) * PAGE_IN_UNITS - 1;
  return indx;
}


void*
pmalloc()
{
  Header *p, *prevp;
  uint nunits;

  nunits = (PGSIZE + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }

  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= MIN_PLACES_IN_BLOCK){

      p->s.size = index_in_block_for_pmalloc(p->s.size);
      p += p->s.size;
      p->s.size = nunits;

      freep = prevp;
      set_pmalloced_page(p + 1);
      return (void*)(p + 1);
    }
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}

int page_size_is_valid(void *ap){
  Header *h_ap = ((Header*)ap) - 1;
  if (h_ap->s.size != PAGE_IN_UNITS + 1) {
    return 0;
  }
  return 1;
}

int protect_page(void *ap) {

  if (!page_size_is_valid(ap)) {
    return -1;
  }

  if (!check_page_pmalloced(ap)) {
    //printf(1, "didnt pmalloc man! \n");
    return -1;
  }

  return protect_pg(ap);
}

int pfree(void *ap){
  if (!page_size_is_valid(ap)){
    return -1;
  }

  if(!check_page_pmalloced(ap)){
    //printf(1, "didnt pmalloc man! \n");
    return -1;
  }

  if(!check_page_protected(ap)){
    return -1;
  }

  unprotect_pg(ap);
  free(ap);
  return 1;
}

