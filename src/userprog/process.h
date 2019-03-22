#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void push_stack_cmdline(const char **cmdline_tokens, int cnt, void **esp);
void check_address(void *address);


#endif /* userprog/process.h */
