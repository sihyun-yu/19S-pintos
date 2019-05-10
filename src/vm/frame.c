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
	while (frame == NULL) {
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
	spte->pin = true;
	fte->frame = frame;
	fte->owner = thread_current();
	fte->spte = spte;
	list_push_back(&frame_table, &fte->ft_elem);

	//printf("frame allocation finished, length : %d\n", list_size(&frame_table));
	//if (fte->owner == NULL) printf("??????\n");
	lock_release(&frame_lock);
	//printf("kpage : %p\n", frame);
	return frame;
}

void free_frame(uint8_t *kpage) {
	lock_acquire(&frame_lock);
	//printf("frame free started");
	struct list_elem *e;
	struct frame_table_entry *fte = NULL;
	for (e=list_begin(&frame_table); e!=list_end(&frame_table); e=list_next(e)) {
		if (list_entry(e, struct frame_table_entry, ft_elem)->frame == kpage) {
			fte = list_entry(e, struct frame_table_entry, ft_elem);
			break;
		}

		if (list_entry(e, struct frame_table_entry, ft_elem)->owner == NULL) {
			//printf("@@@@@@ while free frame\n");
		}

	}

	if (fte == NULL) {
		return;
	}
	//printf("frame free started with %p\n", fte->spte);
	palloc_free_page(fte->frame);
	list_remove(&fte->ft_elem);
	free(fte);
	//printf("frame free finished, length : %d\n", list_size(&frame_table));

	lock_release(&frame_lock);
}

void print_all_frame() {
	struct list_elem *e;
	for (e=list_begin(&frame_table); e!=list_end(&frame_table); e=list_next(e)) {
		//printf("%p : current list entry\n", list_entry(e, struct frame_table_entry, ft_elem)->spte->user_vaddr);
	}
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

	//printf("frame %p corresponded to %p\n", fte, spte);
	palloc_free_page(fte->frame);
	list_remove(&fte->ft_elem);
	free(fte);
	//printf("deleted\n");
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

	int cnt = 0;
	int flag = 0;
	int n, i;
	n = list_size(&frame_table);
	//printf("length : %d\n", list_size(&frame_table));
	for (i=0; i<2 * n + 1; i++) {
		e=find_clock_elem();
		cnt++;
		//e = find_clock_elem();
		evict_frame_entry = list_entry(e, struct frame_table_entry, ft_elem);	  
		//if(evict_frame_entry->owner == NULL) printf("!!!!!!!!\n");  
		if (evict_frame_entry->spte->pin == false && pagedir_is_accessed(thread_current()->pagedir, evict_frame_entry->spte->user_vaddr)) {
			pagedir_set_accessed(thread_current()->pagedir, evict_frame_entry->spte->user_vaddr, false);
			continue;
		}

		else if(evict_frame_entry->spte->pin == false ){

			if(evict_frame_entry->spte->file == NULL)
			{
				evict_frame_entry->spte->location = ON_SWAP;
				evict_frame_entry->spte->swap_index = swap_out(evict_frame_entry->frame);	        
			}
			else if (evict_frame_entry->spte->writable) 
			{
				evict_frame_entry->spte->location = ON_SWAP;
				evict_frame_entry->spte->swap_index = swap_out(evict_frame_entry->frame);	        				
			}
			else {
				evict_frame_entry->spte->location = ON_FILESYS;
			}
		flag = 1;
		break;
		}
	}

	if (flag) {
		if (evict_frame_entry->owner->pagedir != NULL) 
	 	{
	 		pagedir_clear_page(evict_frame_entry->owner->pagedir, evict_frame_entry->spte->user_vaddr);
		 	//printf("clear page clear\n");
	 	}
	 	//printf("clear page access\n");
		palloc_free_page(evict_frame_entry->frame);
		//printf("Palloc success\n");
		list_remove(&evict_frame_entry->ft_elem);
		//printf("List remove\n");
		free(evict_frame_entry);
		//printf("Evict finished\n");
		return true;
	}

	return false;
}



void free_frame_nolock (uint8_t *kpage) {
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
	//printf("frame free started with %p\n", fte->spte);

	palloc_free_page(fte->frame);
	list_remove(&fte->ft_elem);
	free(fte);
	//printf("frame free finished\n");

}


