#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <hash.h>
#include "threads/palloc.h"


struct frame_table_entry
{
	uint32_t* frame;
	struct thread* owner;
	struct sup_page_table_entry* spte;
	struct list_elem ft_elem;
	struct hash_elem hs_elem;
};

void frame_init (void);
void* allocate_frame (enum palloc_flags flag, uint8_t *addr);
bool free_frame (void *fr);
unsigned frame_hash_hash(const struct hash_elem *element, void *aux);

bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);


#endif /* vm/frame.h */
