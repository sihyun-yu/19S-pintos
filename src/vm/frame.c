#include "vm/frame.h"
#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

struct list f_t;
struct lock f_l;
struct hash f_h;

/*
 * Initialize frame table
 */

//hash_hash_func *hash, hash_less_func *less,
void 
frame_init (void)
{
	list_init(&f_t);
	hash_init(&f_h, frame_hash_hash, frame_hash_less);
	lock_init(&f_l);
}


/* 
 * Make a new frame table entry for addr.
 */
bool
allocate_frame (void *addr)
{
	void *fr = palloc_get_page(PAL_USER);
	struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
	
	fte->spte->u_page = addr;
	fte->spte->k_page = fr; // check !
	
	lock_acquire(&f_l);
	hash_insert(&f_h, &fte->hs_elem);
	list_push_back(&f_t, &fte->ft_elem);
	lock_release(&f_l);
}

bool free_frame (void *fr) {
	struct frame_table_entry *fte;
	struct frame_table_entry *imsi;
	struct list_elem *e;

	for (e=list_begin(&f_t); e!=list_end(&f_t); e=list_next(e)) {
		imsi = list_entry(e, struct frame_table_entry, ft_elem);
		if (imsi->spte->k_page == fr) fte = imsi;
	}
	if (fte == NULL return); 

	lock_acquire(&f_l);
	list_remove(&fte->ft_elem);
	hash_delete(&f_h, &fte->hs_elem);
	palloc_free_page(fte->spte->k_page);
	free(fte);
	lock_release(&f_l);
}

unsigned frame_hash_hash(const struct hash_elem *element) {
	struct frame_table_entry *fte = list_entry(element, struct frame_table_entry, hs_elem);
	return hash_bytes(&fte->k_page, sizeof(fte->k_page)); 
}

bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b) {
	struct frame_table_entry *x = list_entry(a, struct frame_table_entry, hs_elem);
	struct frame_table_entry *y = list_entry(b, struct frame_table_entry, hs_elem);
	return (unsigned int) x->k_page > (unsigned int) y->k_page;
}
