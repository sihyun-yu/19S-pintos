#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>
#include "vm/frame.h"

struct sup_page_table {
	struct hash pm;
};

struct sup_page_table_entry 
{
	void *k_page;
	void *u_page;
	uint32_t* user_vaddr;
	uint64_t access_time;

	struct hash_elem hs_elem;
	bool dirty;
	bool accessed;
};

struct sup_page_table * page_init (void);
struct sup_page_table_entry *allocate_page (struct sup_page_table *supt, void *u_page, void *k_page);
unsigned page_hash_hash(const struct hash_elem *element, void *aux);
bool page_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
void free_sup_page_table (struct sup_page_table *supt);
void free_all_pages (struct hash_elem *hs_elem, void *aux);
#endif /* vm/page.h */
