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

//static bool install_page (void *upage, void *kpage, bool writable);

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
	spte->file = NULL;
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
	//if (spte->location != ON_FRAME) find_and_free_frame(spte);
	//printf("free spte %p\n", spte);
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
		printf("load with swap index = %d\n", spte->swap_index);
		kpage = allocate_frame(PAL_USER, spte);
		
		if (kpage == NULL) return false;
		if (spte == NULL) return false;
		
		swap_in(kpage, spte->swap_index);
		if(pagedir_get_page(thread_current()->pagedir, spte->user_vaddr)!=NULL || !pagedir_set_page(thread_current()->pagedir, spte->user_vaddr, kpage, spte->writable))
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
		printf("file load start\n");
		//printf("spte address : %p\n", spte);
		//printf("file position before seek : %d\n", file_tell(spte->file));

		file_seek (spte->file, spte->ofs);
		//printf("file position after seek : %d\n", file_tell(spte->file));
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
 			memset (kpage + spte->read_bytes, 0, spte->zero_bytes);
		}

		else {
	      kpage = allocate_frame(PAL_USER, spte);
	      if (file_read(spte->file, kpage, spte->read_bytes) != PGSIZE)
	        {
	          free_frame (kpage);
	          return false;
	        }
		}

		if(pagedir_get_page(thread_current()->pagedir, spte->user_vaddr)!=NULL || !pagedir_set_page(thread_current()->pagedir, spte->user_vaddr, kpage, spte->writable))
		{          //spte->accessed = false;
		  //spte->location = ON_FRAME;
          free_frame (kpage);
          return false;
        }
		//printf("file load finished\n");
	}

	else if (spte->location == ON_MMAP) {
		file_seek (spte->file, spte->ofs);
		printf("file mmap\n");
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
 			memset (kpage + spte->read_bytes, 0, spte->zero_bytes);
		}

		else {
	      kpage = allocate_frame(PAL_USER, spte);
	      if (file_read(spte->file, kpage, spte->read_bytes) != PGSIZE)
	        {
	          free_frame (kpage);
	          return false;
	        }
		}

		if(pagedir_get_page(thread_current()->pagedir, spte->user_vaddr)!=NULL || !pagedir_set_page(thread_current()->pagedir, spte->user_vaddr, kpage, spte->writable))
		{          //spte->accessed = false;
		  //spte->location = ON_FRAME;
          free_frame (kpage);
          return false;
        }
	}

	else if (spte->location == IMSI_EXTENDED) {
		printf("here all zero\n");
		memset(kpage,0,PGSIZE);
	}

	//print_all_frame();
    //printf("totally load success!\n");
	spte->accessed = false;
	spte->location = ON_FRAME;
	return true;
}

unsigned page_hash_hash(const struct hash_elem *element, void *aux UNUSED) {
	struct sup_page_table_entry *spte = hash_entry(element, struct  sup_page_table_entry, hs_elem);
	//return hash_int((int) spte->user_vaddr);
	return hash_bytes(&spte->user_vaddr, sizeof(spte->user_vaddr)); 
}

bool page_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct sup_page_table_entry *x = hash_entry(a, struct sup_page_table_entry, hs_elem);
	struct sup_page_table_entry *y = hash_entry(b, struct sup_page_table_entry, hs_elem);
	return  x->user_vaddr <  y->user_vaddr;
}



bool stack_growth(struct hash *supt, void *addr){
	printf("stack growth\n");
	//return true; //for swap testing, can delete if we want to debug stack growth
	struct sup_page_table_entry *spte = allocate_page(supt, addr);
	//printf("spte address : %p\n", spte);
	if (spte == NULL)
		return false;

    spte->location = ON_FRAME;

	uint8_t *kpage = allocate_frame(PAL_USER | PAL_ZERO, spte);
	//printf("allocate succeed in stack growth\n");
	if (kpage == NULL){
  		struct sup_page_table_entry imsi;
	  	imsi.user_vaddr = pg_round_down(addr);
		struct hash_elem *e = hash_find(&thread_current()->supt, &(imsi.hs_elem));
		hash_delete(&thread_current()->supt, e);
		free(spte);
		return false;
	}

	if(pagedir_get_page(thread_current()->pagedir, spte->user_vaddr)!=NULL || !pagedir_set_page(thread_current()->pagedir, spte->user_vaddr, kpage, true))
	{
    	free_frame (kpage);
        return false;
    }

	return true;
}



