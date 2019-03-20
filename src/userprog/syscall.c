#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/init.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "threads/malloc.h"
#include "pagedir.h"
#include "threads/vaddr.h"
#include <string.h>
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{	
	uint32_t n = *((uint32_t *)(f->esp));
	switch(n) {
		case SYS_HALT:
		{
			power_off ();
			break;
		}
		case SYS_EXIT:
		{
			//printf("");
			check_address(f->esp+4);
			uint32_t status = *((uint32_t *)(f->esp+4));
			//printf("status : %d\n", status);
			//printf("exit id : %d\n", status);
			sys_exit(status);

			break;
		}
		case SYS_EXEC:
		{
			check_address(f->esp+4);
			char *cmd_line = (char *) *((uint32_t *)(f->esp+4));
			pid_t exec_pid = process_execute(cmd_line);
			if (exec_pid < 0) f->eax = -1;
			else (f->eax) = exec_pid;
			//printf("exec pid : %d\n", exec_pid);
			break;
		}
		case SYS_WAIT:
		{
			check_address(f->esp+4);
			f->eax = sys_wait((int) *((uint32_t *)(f->esp+4)));
			break;
		}
		case SYS_CREATE:
		{
			check_address(f->esp+4);
			check_address(f->esp+8);
			const char *file = (char *) *((uint32_t *)(f->esp+4));
			unsigned initial_size = (unsigned) *((uint32_t *)(f->esp+8));
			f->eax = (bool) sys_create(file, initial_size);
			break;
		}

		case SYS_REMOVE:
		{
			check_address(f->esp+4);
			const char *file = (char *) *((uint32_t *)(f->esp+4));
			f->eax = sys_remove(file);
			break;
		}

		case SYS_OPEN:
		{
			check_address(f->esp+4);
			const char *file = (char *) *((uint32_t *)(f->esp+4));
			f->eax = sys_open(file);
			break;
		}

		case SYS_FILESIZE:
		{
			check_address(f->esp+4);
			int fd = (int) *((uint32_t *)(f->esp+4));
			f->eax = sys_filesize(fd);
			break;
		}

		case SYS_READ:
		{
			check_address(f->esp+4);
			check_address(f->esp+8);
			check_address(f->esp+12);

			int fd = (int) *((uint32_t *)(f->esp+4));	
			void *buffer = (void *) *((uint32_t *)(f->esp+8));	
			unsigned size = (unsigned) *((uint32_t *)(f->esp+12));	

			sys_read(fd, buffer, size);
			break;
		}

		case SYS_WRITE:
		{	
			check_address(f->esp+4);
			check_address(f->esp+8);
			check_address(f->esp+12);

			//printf("")
			int fd;
			const void *buffer;
			unsigned size;
			fd = (int)*((uint32_t *)(f->esp+4));
			buffer = (void *) *((uint32_t *)(f->esp+8));
			size = (unsigned)*((uint32_t *)(f->esp+12));

			sys_write(fd, buffer, size);
			break;
		}
		case SYS_SEEK:
		{
			check_address(f->esp+4);
			check_address(f->esp+8);
			int fd = (int) *((uint32_t *)(f->esp+4));
			unsigned position = (unsigned) *((uint32_t *)(f->esp+8));
			sys_seek(fd, position);
			break;
		}

		case SYS_TELL:
		{
			check_address(f->esp+4);
			int fd = (int) *((uint32_t *)(f->esp+4));
			f->eax = sys_tell(fd);
			break;
		}

		case SYS_CLOSE:
		{
			check_address(f->esp+4);
			int fd = (int) *((uint32_t *)(f->esp+4));
			sys_close(fd);
			break;
		}


	}
	//printf("%d : system number, %p : esp pointer\n", n, (f->esp) );
  //printf ("system call!\n");
}



void sys_exit(int status){
	printf ("%s: exit(%d)\n", thread_current()->name, status);
	thread_current()->exit_status = status;
	thread_exit();
}


int sys_write (int fd, const void *buffer, unsigned size) {
	// check address

  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1; 
}

int sys_exec(char *cmd_line){
	
	return process_execute(cmd_line);
}



int sys_wait(pid_t pid){
	process_wait(pid);
}

int sys_create(const char *file, unsigned initial_size)
 {
 	if (file == NULL) {
 		sys_exit(-1);
 		return 0; 
 	}
 	return filesys_create(file, initial_size);
 }

int sys_remove (const char *file) {
	//should remove this node
 	return filesys_remove(file);
}

int sys_open (const char *file) {
	if (*file == NULL) return -1;
	struct file *open_file = filesys_open(file);
	if (open_file == NULL) return -1;

	//printf("Pass here ?\n");
	struct file_fd *node = (struct file_fd *) malloc (sizeof (struct file_fd));
	//printf("correctly defined?\n");
	//printf("%d : current thread's fd\n", thread_current()->fd);
	node->fd = thread_current()->fd;
	//printf("correctly defined? 2\n");
	//printf("At here, fd = %d\n", thread_current()->fd);
	node->open_file = open_file;
	thread_current()->fd++;
	//printf("At here, fd = %d\n", thread_current()->fd);
	push_file_fd(node);
	//printf("work!\n");
	return node->fd;
}

int sys_filesize(int fd){
	struct file *file_for_size = find_file_from_fd(fd);
	return file_length(file_for_size);
}


int sys_read(int fd, void *buffer, unsigned size) {
	if (fd == 0)
		input_getc();
	return 0;
}
void sys_seek(int fd, unsigned position) {
}

unsigned sys_tell (int fd) {
	struct file *file_for_tell = find_file_from_fd(fd);
	return file_tell(file_for_tell);
}

void sys_close(int fd ){

}

//	  SYS_HALT,                   /* Halt the operating system. */
//    SYS_EXIT,                   /* Terminate this process. */
//    SYS_EXEC,                   /* Start another process. */
//    SYS_WAIT,                   /* Wait for a child process to die. */
//    SYS_CREATE,                 /* Create a file. */
//    SYS_REMOVE,                 /* Delete a file. */
//    SYS_OPEN,                   /* Open a file. */
//    SYS_FILESIZE,               /* Obtain a file's size. */
//    SYS_READ,                   /* Read from a file. */
//    SYS_WRITE,                  /* Write to a file. */
//    SYS_SEEK,                   /* Change position in a file. */
//    SYS_TELL,                   /* Report current position in a file. */
//    SYS_CLOSE,                  /* Close a file. */ 
