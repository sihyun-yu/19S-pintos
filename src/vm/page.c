#include "vm/page.h"
#include "threads/malloc.h"

/*
 * Initialize supplementary page table
 */

//struct list sup_page_table;

struct sup_page_table *
page_init (void)
{
	struct sup_page_table *supt = (struct sup_page_table*) malloc(sizeof(struct sup_page_table));
   hash_init (&supt->pm, page_hash_hash, page_hash_less, NULL);
  return supt;
}

/*
 * Make new supplementary page table entry for addr 
 */
struct sup_page_table_entry *
allocate_page (struct sup_page_table *supt, void *u_page, void *k_page)
{
	struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
	spte->k_page = k_page;
	spte->u_page = u_page;
	spte->dirty = false;

	if (hash_insert(supt, &spte->hs_elem) != NULL) {
		free(spte);
		return NULL;
	}

	return spte;
}

void free_sup_page_table (struct sup_page_table *supt) {
	hash_destroy(supt, free_all_pages);
	free(supt);
}

void free_all_pages (struct hash_elem *hs_elem, void *aux UNUSED) {
	free(hash_entry(hs_elem, struct sup_page_table_entry, hs_elem));
}

unsigned page_hash_hash(const struct hash_elem *element, void *aux UNUSED) {
	struct frame_table_entry *fte = hash_entry(element, struct  sup_page_table_entry, hs_elem);
	return hash_bytes(&fte->k_page, sizeof(fte->k_page)); 
}

bool page_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct frame_table_entry *x = hash_entry(a, struct sup_page_table_entry, hs_elem);
	struct frame_table_entry *y = hash_entry(b, struct sup_page_table_entry, hs_elem);
	return  x->k_page <  y->k_page;
}
