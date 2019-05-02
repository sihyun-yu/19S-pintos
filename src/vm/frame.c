#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
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
struct list_elem *clock_elem;
/*
 * Initialize frame table
 */

//hash_hash_func *hash, hash_less_func *less,
void 
frame_init (void)
{
	//printf("frame init started\n");
	clock_elem = NULL;
	list_init(&f_t);
	//hash_init(&f_h, frame_hash_hash, frame_hash_less, NULL);
	lock_init(&f_l);
	//printf("frame init finished\n");
}


/* 
 * Make a new frame table entry for addr.
 */
void *
allocate_frame (enum palloc_flags flag, struct sup_page_table_entry *addr)
{
	//우리의 목표 : process가 만들어지고, 걔가 (user)page table entry 생성, 
	// 그럼 거기에 대응되는 page(fr)과 frame_table_entry 만드는 것
	lock_acquire(&f_l);
	//printf("frame allocation started\n");
	/*user page load*/
	void *fr = palloc_get_page(flag | PAL_USER);

	struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
	if (fte == NULL) {
		lock_release(&f_l);
		return NULL;
	}

	if (fr == NULL) {
		if(!evict_frame(thread_current()->pagedir)) {
			lock_release(&f_l);
			return NULL;
		}
		fr = palloc_get_page(flag | PAL_USER);
	}

	list_push_back(&f_t, &fte->ft_elem);
	fte->spte = addr;
	fte->k_page = fr; // check !
	//printf("%d : swap_index\n", addr->swap_index);
	fte->owner = thread_current();
	fte->accessed = true; // for second clock algo. cannot be evicted
	//printf("frame allocation finished\n");
	lock_release(&f_l);

	return fr;
}

bool free_frame (void *fr) {

	lock_acquire(&f_l);
	//printf("frame free started\n");
	
	struct list_elem *e;
	struct frame_table_entry *imsi = NULL;
	for (e=list_begin(&f_t); e!=list_end(&f_t); e=list_next(e)) {
		imsi = list_entry(e, struct frame_table_entry, ft_elem);
		if (imsi->k_page == fr) break;
	}
	struct frame_table_entry *fte = imsi;


	if (fte == NULL) {
		lock_release(&f_l);
		return false; 
	}


	list_remove(&fte->ft_elem);

	palloc_free_page(fr);
	free(fte);
	//printf("frame free finished\n");
	lock_release(&f_l);

	return true;
}

struct list_elem* second_clock_elem (void) {
	if (clock_elem == NULL && !list_empty(&f_t)) {
		clock_elem = list_begin(&f_t);
	}

	else if (clock_elem == list_end(&f_t)) {
		clock_elem = list_begin(&f_t);
	}

	else {
		clock_elem = list_next(clock_elem);
	}

	return clock_elem;
}

bool evict_frame(uint32_t *pagedir) {


	/*Determine the algoritm to be evicted*/
	/*For simplicity, we used FIFO*/
	printf("Evict started\n");
	struct list_elem *e;
	struct frame_table_entry *evict_frame_entry = NULL;

	size_t i;
	printf("%d : list size\n", list_size(&f_t));
	for (i = 0; i < 2*list_size(&f_t); i++) {
		e = second_clock_elem();
		evict_frame_entry = list_entry(e, struct frame_table_entry, ft_elem);
		if (evict_frame_entry->accessed) {
			printf("at %d, accessed : %d\n", i, evict_frame_entry->accessed);
			continue;
		}

		
		else if (pagedir_is_accessed(pagedir, evict_frame_entry->spte->u_page)) {
				pagedir_set_accessed(pagedir, evict_frame_entry->spte->u_page, false);
				continue; 
		}

		break;
	}
	if (evict_frame_entry == NULL) return false;
	printf("Entered here\n");
	printf("%p : evict_frame_entry address\n", evict_frame_entry);
	printf("%p : spte address\n", evict_frame_entry->spte);
	printf("%p : swap index accessible?\n", &evict_frame_entry->spte->swap_index);

	pagedir_clear_page(evict_frame_entry->owner->pagedir, evict_frame_entry->spte->u_page);

	evict_frame_entry->spte->swap_index = swap_out(evict_frame_entry->k_page);

	printf("swap index:%d\n", evict_frame_entry->spte->swap_index);
  	evict_frame_entry->spte->cur_status = ON_SWAP;
	printf("%d : cur status\n", evict_frame_entry->spte->cur_status);

  	list_remove(&evict_frame_entry->ft_elem);
	palloc_free_page(evict_frame_entry->k_page);
  	
  	evict_frame_entry->spte->k_page = NULL;
	free(evict_frame_entry);
	printf("Evict finished\n");  	
  	return true;
}

