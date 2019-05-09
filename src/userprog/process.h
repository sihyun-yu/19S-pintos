#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void push_stack_cmdline(char **cmdline_tokens, int cnt, void **esp);
struct sup_page_table_entry* check_address(void *address);

#ifdef VM
struct mmap_entry {
	int mm_id;
	struct file *file;
	void *mm_addr;
	int size; 
	struct list_elem mm_elem;
};
#endif


#endif /* userprog/process.h */
