#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include <stdio.h>
#include "userprog/pagedir.h"

/*
 * Initialize supplementary page table
 */

//struct list sup_page_table;

bool page_init (struct hash *supt)
{
   //printf("Page init started\n");
   hash_init (supt, page_hash_hash, page_hash_less, NULL);
   //printf("Page init finished\n");
  return true;
}

/*
 * Make new supplementary page table entry for addr 
 */
struct sup_page_table_entry *
allocate_page (struct hash *supt, void *u_page)
{
	//printf("Page allocation started\n");
	struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
	//printf("spte : %p\n", spte);
	if (spte == NULL) return NULL;
	//spte->k_page = k_page;
	spte->u_page = u_page;
	spte->dirty = false;
	spte->swap_index = -1;
	spte->cur_status = ON_FRAME;

	//printf("reached here\n");

	if (hash_insert(supt, &spte->hs_elem) != NULL) {
		//printf("Not null\n");
		free(spte);
		return NULL;
	}

	//printf("Page allocation finished\n");
	return spte;
}

bool load_page(struct hash *supt, uint32_t *pagedir, void *addr) {

	//printf("Page load started\n");
	struct sup_page_table_entry imsi;
	imsi.u_page = addr;
	
 	struct hash_elem *e = hash_find(supt, &(imsi.hs_elem)); 
	struct sup_page_table_entry *spte = hash_entry(e, struct sup_page_table_entry, hs_elem);

	if (spte == NULL) return false;
	if (spte->cur_status == ON_FRAME) return true;

	else if (spte->cur_status == ON_SWAP) {
		struct frame_table_entry* k_page = allocate_frame(PAL_USER, spte);
		if (k_page == NULL) return false;
		
		if (spte == NULL) return false;
		
		swap_in(k_page, spte->swap_index);

		if(pagedir_get_page(pagedir, spte->u_page)!=NULL || !pagedir_set_page(pagedir, spte->u_page, k_page, true)){
	        free_frame(k_page);
		    //printf("Page load finished with free page\n");
	        return false;
	    }

	    spte->k_page = k_page;
	    spte->cur_status = ON_FRAME;
	    k_page->accessed = false; 

	    //printf("Page load finished\n");
	}
	return true;
}

void free_sup_page_table (struct hash *supt) {
	if (supt == NULL) return;
	printf("sup page table free started : supt = %p\n", supt);
	hash_destroy(supt, free_all_pages);
	printf("destory done\n");
	printf("sup page table free finished\n");
}

void free_all_pages (struct hash_elem *hs_elem, void *aux UNUSED) {
	struct sup_page_table_entry *spte = hash_entry(hs_elem, struct sup_page_table_entry, hs_elem);
	if (spte->cur_status == ON_SWAP) swap_free(spte->swap_index);
	free(hash_entry(hs_elem, struct sup_page_table_entry, hs_elem));
}


/*For hash table management*/
unsigned page_hash_hash(const struct hash_elem *element, void *aux UNUSED) {
	struct sup_page_table_entry *spte = hash_entry(element, struct  sup_page_table_entry, hs_elem);
	return hash_int((int) spte->u_page); 
}

bool page_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct sup_page_table_entry *x = hash_entry(a, struct sup_page_table_entry, hs_elem);
	struct sup_page_table_entry *y = hash_entry(b, struct sup_page_table_entry, hs_elem);
	return  x->u_page <  y->u_page;
}
