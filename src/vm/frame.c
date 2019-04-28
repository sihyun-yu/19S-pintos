#include "vm/frame.h"
#include "vm/page.h"
#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include <hash.h>
#include <list.h>
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
	hash_init(&f_h, frame_hash_hash, frame_hash_less, NULL);
	lock_init(&f_l);
}


/* 
 * Make a new frame table entry for addr.
 */
void *
allocate_frame (enum palloc_flags flag, uint8_t *addr)
{
	lock_acquire(&f_l);

	void *fr = palloc_get_page(flag | PAL_USER);
	struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
	if (fte == NULL) {
		lock_release(&f_l);
		return NULL;
	}

	fte->owner = thread_current();
	fte->u_page = addr;
	fte->k_page = fr; // check !
	
	hash_insert(&f_h, &fte->hs_elem);
	list_push_back(&f_t, &fte->ft_elem);
	lock_release(&f_l);

	return fr;
}

bool free_frame (void *fr) {

	lock_acquire(&f_l);
	struct frame_table_entry imsi;
	imsi.k_page = fr;
	
	struct hash_elem *hs_elem = hash_find(&f_h, &(imsi.hs_elem));
	struct frame_table_entry *fte = hash_entry(hs_elem, struct frame_table_entry, hs_elem);
	//struct list_elem *e;


	if (fte == NULL) return 0; 

	list_remove(&fte->ft_elem);
	hash_delete(&f_h, &fte->hs_elem);
	palloc_free_page(fte->k_page);
	free(fte);
	lock_release(&f_l);
}

unsigned frame_hash_hash(const struct hash_elem *element, void *aux UNUSED) {
	struct frame_table_entry *fte = hash_entry(element, struct frame_table_entry, hs_elem);
	return hash_bytes(&fte->k_page, sizeof(fte->k_page)); 
}

bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct frame_table_entry *x = hash_entry(a, struct frame_table_entry, hs_elem);
	struct frame_table_entry *y = hash_entry(b, struct frame_table_entry, hs_elem);
	return  x->k_page <  y->k_page;
}
