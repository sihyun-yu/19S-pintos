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
		if(!evict_frame()) {
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

void find_and_free_frame(struct sup_page_table_entry *spte) {
	struct frame_table_entry *fte = NULL;
	struct list_elem *e;
	for (e=list_begin(&frame_table); e!=list_end(&frame_table); e=list_next(e)) {
		if (list_entry(e, struct frame_table_entry, ft_elem)->spte == spte) {
			fte = list_entry(e, struct frame_table_entry, ft_elem);
			break;
		}
	}

	if (fte == NULL) {
		return; 
	}

	free_frame(fte->frame);
}

struct list_elem* find_clock_elem (void) {
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

bool evict_frame(void) {
	/*Determine the algoritm to be evicted*/
	/*For simplicity, we used FIFO*/
	//printf("Evict started\n");
	struct list_elem *e;
	struct frame_table_entry *evict_frame_entry = NULL;

	int i;
	int n;
	n = list_size(&frame_table);
	//printf("length : %d\n", list_size(&frame_table));
	for (i=0; i<n; i++) {
		e = find_clock_elem();
		evict_frame_entry = list_entry(e, struct frame_table_entry, ft_elem);	    

		if(evict_frame_entry->spte->accessed == false ){


			if (evict_frame_entry->owner->pagedir != NULL &&  evict_frame_entry->spte->user_vaddr != NULL)
				pagedir_clear_page(evict_frame_entry->owner->pagedir, evict_frame_entry->spte->user_vaddr);
			//printf("default passing\n");
			//printf("default passed\n");
			if(evict_frame_entry->spte->file == NULL)
			{
				//printf("1 passing\n");
				evict_frame_entry->spte->location = ON_SWAP;
				evict_frame_entry->spte->swap_index = swap_out(evict_frame_entry->frame);	        
				//printf("1 passed\n");
			}
			else if (evict_frame_entry->spte->writable) 
			{
				//printf("2 passing\n");
				evict_frame_entry->spte->location = ON_SWAP;
				evict_frame_entry->spte->swap_index = swap_out(evict_frame_entry->frame);	        				
			//	printf("2 passed\n");
			}
			else {
				//printf("3 passing\n");
				evict_frame_entry->spte->location = ON_FILESYS;
				//printf("3 passed\n");
			}


		clock_elem = e; 
		palloc_free_page(evict_frame_entry->frame);
		list_remove(&evict_frame_entry->ft_elem);
		free(evict_frame_entry);
		//printf("evict finished\n");
		return true;
		}
		e = find_clock_elem();

	}
	//printf("evict failed\n");
	return false; 	    
}