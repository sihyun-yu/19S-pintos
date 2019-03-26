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


struct lock sys_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
	lock_init(&sys_lock);
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
			pid_t exec_pid = sys_exec(cmd_line);
			if (exec_pid < 0 ) f->eax = -1;
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

			f->eax = sys_read(fd, buffer, size);
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

			f->eax = sys_write(fd, buffer, size);
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
	int val;
	// check address
	lock_acquire(&sys_lock);
  if (fd == 1) {
    putbuf(buffer, size);
    val = size;
  }

  else {
  	struct file *file_for_write = thread_current()->fds[fd];
	if (file_for_write == NULL) {
		val = -1;
	}

	else {
		val = file_write(file_for_write, (char *)buffer, size);
	}
  }

  lock_release(&sys_lock);
  return val; 
}

int sys_exec(char *cmd_line){
	
	return process_execute(cmd_line);
}



int sys_wait(pid_t pid){
	return process_wait(pid);
}

int sys_create(const char *file, unsigned initial_size)
 {
 	if (file == NULL) {
 		//printf("error from here\n");
 		sys_exit(-1);
 		return 0; 
 	}
 	return filesys_create(file, initial_size);
 }

int sys_remove (const char *file) {

	struct file *open_file = filesys_open(file);
	/*remove_file_from_list (open_file);
	if (find_filefd_from_file(open_file) != NULL)
		palloc_free_page(find_filefd_from_file(open_file));*/
 	return filesys_remove(file);
}

int sys_open (const char *file) {
	if (file == NULL) return -1;
	lock_acquire(&sys_lock);

	struct file *open_file = filesys_open(file);

	lock_release(&sys_lock);
	
	if (open_file == NULL) {
		return -1;
	}


	//printf("Pass here ?\n");
	//printf("palloc fd : %d\n", thread_current()->fd);
	//struct file_fd *node = (struct file_fd *) malloc (sizeof (struct file_fd));
	//printf("correctly defined?\n");
	//printf("%d : current thread's fd\n", thread_current()->fd);
	//printf("At here, fd = %d\n", thread_current()->fd);
	int i;
	int fd = -1;
	for (i=3; i<100; i++) {
		if (thread_current()->fds[i] == NULL) {
			thread_current()->fds[i] = open_file;
			fd = i;
			break;
		}
	}

	if (!strcmp (file, thread_current()->name))
		file_deny_write(open_file);
	return fd;
}

int sys_filesize(int fd){
	struct file *file_for_size = thread_current()->fds[fd];
	return file_length(file_for_size);
}


int sys_read(int fd, void *buffer, unsigned size) {
	lock_acquire(&sys_lock);
	int val;
	if (fd == 0){
		input_getc();
		val = size;
	}

	else{
		struct file *file_for_read = thread_current()->fds[fd];
		if (file_for_read == NULL) 
		{
			val = -1;
		}
		else {
			val = file_read(file_for_read, buffer, size);
		}
	}
	lock_release(&sys_lock);
	return val;
}
void sys_seek(int fd, unsigned position) {
	struct file *file_for_seek = thread_current()->fds[fd];
	return file_seek(file_for_seek, position);

}

unsigned sys_tell (int fd) {
	struct file *file_for_tell = thread_current()->fds[fd];
	return file_tell(file_for_tell);
}

void sys_close(int fd){
	struct file *file_for_close = thread_current()->fds[fd];
	thread_current()->fds[fd] = NULL;
	return file_close(file_for_close);
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
