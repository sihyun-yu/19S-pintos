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
	struct hash_elem hs_elem;
	bool accessed; 
};

void frame_init (void);
void* allocate_frame (enum palloc_flags flag, uint8_t *addr);
bool free_frame (void *fr);
bool evict_frame(uint32_t *pagedir);
unsigned frame_hash_hash(const struct hash_elem *element, void *aux);
bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
struct list_elem* second_clock_elem (void);


#endif /* vm/frame.h */
