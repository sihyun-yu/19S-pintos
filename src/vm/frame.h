#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <hash.h>
#include "threads/palloc.h"


struct frame_table_entry
{
	void *k_page;
	struct sup_page_table_entry* spte; /*User page*/
	struct thread* owner;
	struct list_elem ft_elem;
	bool accessed;
};

void frame_init (void);
void* allocate_frame (enum palloc_flags flag, struct sup_page_table_entry *addr);
bool free_frame (void *fr);
bool evict_frame(uint32_t *pagedir);
struct list_elem* second_clock_elem (void);


#endif /* vm/frame.h */
