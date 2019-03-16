#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

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
			uint32_t status = *((uint32_t *)(f->esp+4));
			sys_exit(status);

			break;
		}
		case SYS_EXEC:
		{
			break;
		}
		case SYS_WAIT:
		{
			//printf("")
			break;
		}
		case SYS_CREATE:
		{
			//printf("")

			break;
		}

		case SYS_REMOVE:
		{
			//printf("")
			break;
		}

		case SYS_OPEN:
		{
			//printf("")
			break;
		}

		case SYS_FILESIZE:
		{
			//printf("")
			break;
		}

		case SYS_READ:
		{			
			break;
		}

		case SYS_WRITE:
		{

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
			//printf("")
			break;
		}

		case SYS_TELL:
		{
			//printf("")
			break;
		}

		case SYS_CLOSE:
		{
			//printf("")
			break;
		}


	}
	//printf("%d : system number, %p : esp pointer\n", n, (f->esp) );
  //printf ("system call!\n");
}



void sys_exit(int status){
	printf ("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}


int sys_write (int fd, const void *buffer, unsigned size) {


  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1; 
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
