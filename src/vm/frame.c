#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <list.h>
#include <stdio.h>


struct list frame_table;
struct lock frame_lock;
/*
 * Initialize frame table
 */
void 
frame_init (void)
{
	list_init(&frame_table);
	lock_init(&frame_lock);
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
		//must be evicted
		return NULL;
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

bool evict_frame(void *frame) {
	return true; 
}