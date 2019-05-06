#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include <stdio.h>
#include "userprog/pagedir.h"
#include <hash.h>
#include "lib/kernel/hash.h"
#include "devices/disk.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <bitmap.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "threads/thread.h"
#include <stdlib.h>
#include <string.h>

/*
 * Initialize supplementary page table
 */

static bool install_page (void *upage, void *kpage, bool writable);

void 
page_init (struct hash *supt)
{
	hash_init(supt, page_hash_hash, page_hash_less, NULL);
	//printf("hash init success? %d\n", flag);
}

/*
 * Make new supplementary page table entry for addr 
 */
struct sup_page_table_entry *
allocate_page (struct hash *supt, void *addr)
{
	//printf("page allocation started\n");
	struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
	//printf("spte %p\n", spte);
	if (spte == NULL) return NULL;

	spte->user_vaddr = pg_round_down(addr);
	spte->accessed = true;
	spte->swap_index = -1;
	spte->writable = true;
	//printf("reached here \n");
	if(hash_insert(supt, &spte->hs_elem) != NULL) {
		//printf("hash insert failed \n");
		free(spte);
		return NULL;
	}
	//printf("hash table size : %d\n", hash_size(supt));
	//printf("page allocation finished");
	return spte;
}

void free_page(struct hash_elem *hs_elem, void *aux UNUSED) {
	//printf("page free started\n");
	struct sup_page_table_entry *spte = hash_entry(hs_elem, struct sup_page_table_entry, hs_elem);
	//if (spte->file != NULL) file_close(spte->file);
	if(spte->location == ON_SWAP) swap_free(spte->swap_index);
	//find_and_free_frame(spte);
	free(spte);
	//need to free the frame 
	//printf("page free finished\n");

}

void destroy_supt(struct hash *supt, void *aux UNUSED) {
	hash_destroy(supt, free_page);
}

bool load_page(struct sup_page_table_entry *spte) {
	
	//printf("page load start\n");
	void *kpage = NULL;
	if (spte->location == ON_FRAME){
		//spte->accessed = false;
		spte->location = ON_FRAME;
		return true;
		//printf("page load start\n");
	} 

	else if (spte->location == ON_SWAP) {
		//printf("load with swap index = %d\n", spte->swap_index);
		kpage = allocate_frame(PAL_USER, spte);
		
		if (kpage == NULL) return false;
		if (spte == NULL) return false;
		
		swap_in(kpage, spte->swap_index);
		if (!install_page (spte->user_vaddr, kpage, spte->writable))
        {
          //spte->accessed = false;
		  //spte->location = ON_FRAME;
          free_frame (kpage);
          return false;
        }
		spte->accessed = false;
		spte->location = ON_FRAME;
		//printf("swap load success!\n");

	}

	else if (spte->location == ON_FILESYS) {
		//printf("file load start\n");
		file_seek (spte->file, spte->ofs);


		if (spte->zero_bytes == PGSIZE) {
			//printf("memset\n");
			kpage = allocate_frame(PAL_USER | PAL_ZERO, spte);
			if (kpage == NULL) return false;
			memset(kpage,0,PGSIZE);
		}
		
		else if (spte->zero_bytes != 0 ){
	      kpage = allocate_frame(PAL_USER, spte);
	      if (file_read(spte->file, kpage, spte->read_bytes) != (int) spte->read_bytes)
	        {
	          //printf("error 2!\n");
	          free_frame (kpage);
	          return false;
	        }	        
		}

		else {
	      kpage = allocate_frame(PAL_USER, spte);
	      if (file_read(spte->file, kpage, spte->read_bytes) != PGSIZE)
	        {
	          free_frame (kpage);
	          return false;
	        }
			memset (kpage + spte->read_bytes, 0, spte->zero_bytes);
		}


		if (!install_page (spte->user_vaddr, kpage, spte->writable))
        {
          //spte->accessed = false;
		  //spte->location = ON_FRAME;
          free_frame (kpage);
          return false;
        }
		//printf("file load finished\n");
	}

    //printf("totally load success!\n");
	spte->accessed = false;
	spte->location = ON_FRAME;
	return true;
}

unsigned page_hash_hash(const struct hash_elem *element, void *aux UNUSED) {
	struct sup_page_table_entry *spte = hash_entry(element, struct  sup_page_table_entry, hs_elem);
	return hash_int((int) spte->user_vaddr);
	//return hash_bytes(&spte->user_vaddr, sizeof(spte->user_vaddr)); 
}

bool page_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct sup_page_table_entry *x = hash_entry(a, struct sup_page_table_entry, hs_elem);
	struct sup_page_table_entry *y = hash_entry(b, struct sup_page_table_entry, hs_elem);
	return  x->user_vaddr <  y->user_vaddr;
}

static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));

}


bool stack_growth(struct hash *supt, void *addr){
	return true; //for swap testing, can delete if we want to debug stack growth
	struct sup_page_table_entry *spte = allocate_page(supt, addr);
	
	if (spte == NULL)
		return false;

	uint8_t *kpage = allocate_frame(PAL_USER | PAL_ZERO, spte);
	/*if (kpage == NULL){
		free_page(spte, 0);
		return false;
	} 지호야 이 부분 argument가 이상하게 가지는 거 같은데 */
	//if (/*hash_find(supt, spte->hs_elem) != NULL || */!hash_insert(supt, spte->hs_elem)){
	//if supt has the spte, return NULL. if supt doesn't have the spte, returns modified supt.
	//	free_page(spte);
	//	free_frame(kpage);
	//	return false;
	//}
	return true;
}



