#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include <list.h>
#include <stdio.h>


struct list frame_table;
struct lock frame_lock;
struct list_elem *clock_elem; 
/*
 * Initialize frame table
 */
void 
frame_init (void)
{
	list_init(&frame_table);
	lock_init(&frame_lock);
	clock_elem = NULL;
}


/* 
 * Make a new frame table entry for addr.
 */
void *
allocate_frame (enum palloc_flags flag, struct sup_page_table_entry *spte)
{
	lock_acquire(&frame_lock);
	//printf("frame allocation started\n");
	void *frame = palloc_get_page(flag);
	if (frame == NULL) {
		if(!evict_frame(thread_current()->pagedir)) {
			lock_release(&frame_lock);
			return NULL;
		}
		frame = palloc_get_page(flag | PAL_USER);
	}

	// make a fte corresponding to palloced frame
	struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
	if (fte == NULL) return NULL;

	//put info into fte
	fte->frame = frame;
	fte->owner = thread_current();
	fte->spte = spte;
	list_push_back(&frame_table, &fte->ft_elem);

	//printf("frame allocation finished\n");
	lock_release(&frame_lock);
	//printf("kpage : %p\n", frame);
	return frame;
}

void free_frame(uint8_t *kpage) {
	lock_acquire(&frame_lock);
	//printf("frame free started\n");

	struct list_elem *e;
	struct frame_table_entry *fte = NULL;
	for (e=list_begin(&frame_table); e!=list_end(&frame_table); e=list_next(e)) {
		if (list_entry(e, struct frame_table_entry, ft_elem)->frame == kpage) {
			fte = list_entry(e, struct frame_table_entry, ft_elem);
			break;
		}
	}

	if (fte == NULL) {
		return;
	}

	palloc_free_page(fte->frame);
	list_remove(&fte->ft_elem);
	free(fte);
	//printf("frame free finished\n");

	lock_release(&frame_lock);
}

struct list_elem* second_clock_elem (void) {
	if (clock_elem == NULL && !list_empty(&frame_table)) {
		clock_elem = list_begin(&frame_table);
	}

	else if (clock_elem == list_end(&frame_table)) {
		clock_elem = list_begin(&frame_table);
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
	printf("%d : list size\n", list_size(&frame_table));
	for (i = 0; i < 2*list_size(&frame_table); i++) {
		e = second_clock_elem();
		evict_frame_entry = list_entry(e, struct frame_table_entry, ft_elem);
		if (evict_frame_entry->spte->accessed) {
			continue;
		}

		
		else if (pagedir_is_accessed(pagedir, evict_frame_entry->spte->user_vaddr)) {
				pagedir_set_accessed(pagedir, evict_frame_entry->spte->user_vaddr, false);
				continue; 
		}

		break;
	}
	if (evict_frame_entry == NULL) return false;

	pagedir_clear_page(evict_frame_entry->owner->pagedir, evict_frame_entry->spte->user_vaddr);

	evict_frame_entry->spte->swap_index = swap_out(evict_frame_entry->frame);

	printf("swap index:%d\n", evict_frame_entry->spte->swap_index);
  	evict_frame_entry->spte->location = ON_SWAP;
	printf("%d : cur status\n", evict_frame_entry->spte->location);

  	list_remove(&evict_frame_entry->ft_elem);
	palloc_free_page(evict_frame_entry->frame);
  	
  	free(evict_frame_entry);
	printf("Evict finished\n");  	
  	return true;
}