#include "vm/page.h"
#include "vm/frame.h"
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
/*
 * Initialize supplementary page table
 */

static bool install_page (void *upage, void *kpage, bool writable);

void 
page_init (struct hash *supt)
{
	int flag = hash_init(supt, page_hash_hash, page_hash_less, NULL);
	printf("hash init success? %d\n", flag);
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
	//printf("reached here \n");
	if(hash_insert(supt, &spte->hs_elem) != NULL) {
		//printf("hash insert failed \n");
		free(spte);
		return NULL;
	}
	printf("hash table size : %d\n", hash_size(supt));
	//printf("page allocation finished");
	return spte;
}

void free_page(struct hash_elem *hs_elem, void *aux UNUSED) {
	//printf("page free started\n");
	struct sup_page_table_entry *spte = hash_entry(hs_elem, struct sup_page_table_entry, hs_elem);
	free(spte);
	//printf("page free finished\n");

}

void destroy_supt(struct hash *supt, void *aux UNUSED) {
	hash_destroy(supt, free_page);
}

bool load_page(struct sup_page_table_entry *spte) {
	if (spte->location == ON_FRAME) return true;
	else if (spte->location == ON_SWAP) {
		return true;
	}

	/*do we need to seperate the case zerobytes=0?*/

	else if (spte->location == ON_FILESYS) {

		struct file *file = spte->file;
		void *kpage = allocate_frame(PAL_USER | PAL_ZERO, spte);
		size_t page_read_bytes = spte->read_bytes;
		size_t page_zero_bytes = spte->zero_bytes;
		uint8_t * upage = spte->user_vaddr;
		bool writable = spte->writable;

		if (kpage == NULL) return false; 

		if (page_zero_bytes == PGSIZE) {
			//printf("memset\n");
			memset(kpage,0,PGSIZE);
		}
		file_seek (spte->file, spte->ofs);
		
		if (page_zero_bytes == 0 ){
	      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
	        {
	        	printf("error !\n");
	          free_frame (kpage);
	          return false;
	        }
		}

		else {
	      if (file_read (file, kpage, page_read_bytes) != PGSIZE)
	        {
	          free_frame (kpage);
	          return false;
	        }

			memset (kpage + spte->read_bytes, 0, spte->zero_bytes);
		}
      	if (!install_page (upage, kpage, writable))
        {
          free_frame (kpage);
          return false;
        }

		spte->location = ON_FRAME;
	}
	return true;
}

unsigned page_hash_hash(const struct hash_elem *element, void *aux UNUSED) {
	struct sup_page_table_entry *spte = hash_entry(element, struct  sup_page_table_entry, hs_elem);
	return hash_bytes(&spte->user_vaddr, sizeof(spte->user_vaddr)); 
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
