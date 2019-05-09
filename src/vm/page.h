#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>
#include <inttypes.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/off_t.h"

enum page_location {
	ON_FRAME,
	ON_SWAP,
	ON_FILESYS,
	ON_MMAP,
	IMSI_EXTENDED
};

struct sup_page_table_entry 
{	/*for lazy load*/
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
	void *inode; 
	uint32_t* user_vaddr; /*upage*/
	uint64_t access_time;

	struct hash_elem hs_elem;
	int swap_index;
	bool dirty;
	bool accessed; /*for swap eviction */
	enum page_location location;


};

void page_init (struct hash *supt);
struct sup_page_table_entry *allocate_page (struct hash *supt, void *addr);
void free_page(struct hash_elem *hs_elem, void *aux);
void destroy_supt(struct hash *supt, void *aux);
bool load_page(struct sup_page_table_entry *spte);
unsigned page_hash_hash(const struct hash_elem *element, void *aux );
bool page_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
bool stack_growth(struct hash *supt, void *addr);



#endif /* vm/page.h */
