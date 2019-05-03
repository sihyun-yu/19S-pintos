#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"

struct frame_table_entry
{
	uint8_t* frame; /*for the palloced address*/
	struct thread* owner; /*current thread*/
	struct sup_page_table_entry* spte;
	struct list_elem ft_elem;
};

void frame_init (void);
void* allocate_frame (enum palloc_flags flag, struct sup_page_table_entry* spte);
void free_frame(uint8_t *kpage);
bool evict_frame(uint32_t *pagedir);
struct list_elem* second_clock_elem (void);
#endif /* vm/frame.h */
