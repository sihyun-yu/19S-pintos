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
#include "vm/frame.h"
#include "vm/page.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"
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
	check_address(f->esp);
	uint32_t n = *((uint32_t *)(f->esp));
	thread_current()->esp = f->esp;
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
			//printf("Reached here 1");
			if (exec_pid < 0 ) f->eax = -1;
			else (f->eax) = exec_pid;
			//printf("Reached here 2");
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
		case SYS_MMAP:
		{
			check_address(f->esp+4);
			check_address(f->esp+8);
			int fd = (int) *((uint32_t *)(f->esp+4));
			void *addr = (void *) *((uint32_t *)(f->esp+8));
			f->eax = sys_mmap(fd, addr);
			break;
		}

		case SYS_MUNMAP:
		{
			check_address(f->esp+4);
			mapid_t mapping = (mapid_t) *((uint32_t *)(f->esp+4));
			sys_munmap(mapping);
			break;
		}

		case SYS_CHDIR:
		{
			check_address(f->esp+4);
			const char *dir = (const char*) *((uint32_t *)(f->esp+4));
			f->eax = (bool) sys_chdir(dir);
			break;
		}

		case SYS_MKDIR:
		{
			check_address(f->esp+4);
			const char *dir = (const char*) *((uint32_t *)(f->esp+4));
			f->eax = (bool) sys_mkdir(dir);
			break;
		}

		case SYS_READDIR:
		{
			check_address(f->esp+4);
			check_address(f->esp+8);
			int fd = (int) *((uint32_t *)(f->esp+4));
			char *name = (char *) *((uint32_t *)(f->esp+4));
			f->eax = (bool) sys_readdir(fd, name);
		}

		case SYS_ISDIR:
		{
			check_address(f->esp+4);
			int fd = (int) *((uint32_t *)(f->esp+4));
			f->eax = (bool) sys_isdir(fd);
			break;
		}

		case SYS_INUMBER:
		{
			check_address(f->esp+4);
			int fd = (int) *((uint32_t *)(f->esp+4));
			f->eax = sys_inumber(fd);
		}

	}
	//printf("%d : system number, %p : esp pointer\n", n, (f->esp) );
  //printf ("system call!\n");
}



void sys_exit(int status){
	printf ("%s: exit(%d)\n", thread_current()->name, status);
	thread_current()->exit_status = status;
	//int i;
	thread_exit();
}


int sys_write (int fd, const void *buffer, unsigned size) {
	//printf("write\n");
	int val;
	// check address
	if (fd == 1) {

		putbuf(buffer, size);
		val = size;
	}

	else {
		struct file *file_for_write = thread_current()->fds[fd];

		if (inode_is_dir(file_get_inode(file_for_write)) == true)
			return -1;

		if (file_for_write == NULL) {
			val = -1;
		}

		else {
			lock_acquire(&sys_lock);
			val = file_write(file_for_write, (char *)buffer, size);
			lock_release(&sys_lock);

		}
	}

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

	//struct file *open_file = filesys_open(file);
	/*remove_file_from_list (open_file);
	if (find_filefd_from_file(open_file) != NULL)
		palloc_free_page(find_filefd_from_file(open_file));*/
	return filesys_remove(file);
}

int sys_open (const char *file) {
	//printf("open\n");
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
	for (i=3; i<200; i++) {
		if (thread_current()->fds[i] == NULL) {
			thread_current()->fds[i] = open_file;
			if (inode_is_dir(file_get_inode(open_file))) {
				thread_current()->fds_dir[i] = dir_open(inode_reopen(file_get_inode(open_file)));
			}
			else {
				thread_current()->fds_dir[i] = NULL;
			}
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
	if (inode_is_dir(file_get_inode(file_for_size)) == true)
		return -1;

	return file_length(file_for_size);
}


int sys_read(int fd, void *buffer, unsigned size) {

	unsigned i;

	int val;
	if (fd == 0){
		input_getc();
		val = size;
	}

	else{
		struct file *file_for_read = thread_current()->fds[fd];

		if (inode_is_dir(file_get_inode(file_for_read)) == true)
			return -1;

		if (file_for_read == NULL) 
		{
			val = -1;
		}
		else {
			lock_acquire(&sys_lock);
			val = file_read(file_for_read, buffer, size);
			lock_release(&sys_lock);

		}
	}
	return val;
}
void sys_seek(int fd, unsigned position) {
	struct file *file_for_seek = thread_current()->fds[fd];
	if (inode_is_dir(file_get_inode(file_for_seek)) == true)
		return;

	file_seek(file_for_seek, position);
	return;
}

unsigned sys_tell (int fd) {
	struct file *file_for_tell = thread_current()->fds[fd];
	if (inode_is_dir(file_get_inode(file_for_tell)) == true)
		return -1;
	return file_tell(file_for_tell);
}

void sys_close(int fd){
	struct file *file_for_close = thread_current()->fds[fd];
	thread_current()->fds[fd] = NULL;
	dir_close(thread_current()->fds_dir[fd]);
	return file_close(file_for_close);
}

mapid_t sys_mmap(int fd, void *addr) {

	lock_acquire(&sys_lock);
	struct file *file = thread_current()->fds[fd];
	if (inode_is_dir(file_get_inode(file)) == true)
		return -1;
	file = file_reopen(file);
	uint32_t read_bytes = file_length(file);
	lock_release(&sys_lock);
	off_t ofs = 0;

	if(pg_ofs(addr) != 0) return -1;
	if(addr == 0) return -1;
	if (read_bytes == 0) {
		//lock_release(&sys_lock);
		return -1;
	}

	void *mm_addr = addr;
	int size = read_bytes;
	while (read_bytes > 0) 
	{
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		struct sup_page_table_entry *spte = allocate_page(&thread_current()->supt, addr);
      //printf("allocated with spte = %p, upage was %p\n", spte, pg_round_down(upage));
      //find_and_free_frame(spte);
		if (spte == NULL) {
			//lock_release(&sys_lock);
			return -1;
		}
#ifdef VM
		spte->location = ON_MMAP;
		spte->file = file;
		spte->ofs = ofs;
		spte->read_bytes =page_read_bytes;
		spte->zero_bytes = page_zero_bytes;
		spte->writable = true;
#endif
      /* Advance. */
		read_bytes -= page_read_bytes;
		addr += PGSIZE;
		ofs += page_read_bytes;
	}

	struct mmap_entry *mm = malloc(sizeof(struct mmap_entry));
	mm->mm_id = (thread_current()->mm_id)++;
	mm->file = file;
	mm->mm_addr = mm_addr;
	mm->size = size;
	list_push_back(&thread_current()->mm_list, &mm->mm_elem);
	return mm->mm_id;
}

void sys_munmap(mapid_t mapping) {
	struct list_elem *e;
	off_t ofs = 0;
	struct mmap_entry *mm = NULL;
	lock_acquire(&sys_lock);
	for(e = list_begin(&thread_current()->mm_list); e != list_end(&thread_current()->mm_list); e = list_next(e)){
		if(list_entry(e, struct mmap_entry, mm_elem)->mm_id == mapping) {
			mm = list_entry(e, struct mmap_entry, mm_elem);
			list_remove(e);
			break;
		}
	}
	if (mm == NULL) {
		lock_release(&sys_lock);
		return;
	}

	//mm is not null

	int i = mm->size;
	for (i=mm->size; i>0; i -= PGSIZE) {
		struct sup_page_table_entry imsi;
		imsi.user_vaddr = pg_round_down(mm->mm_addr);
		struct hash_elem *e = hash_find(&thread_current()->supt, &(imsi.hs_elem));
		struct sup_page_table_entry *spte = hash_entry(e, struct sup_page_table_entry, hs_elem);

		if(spte->location == ON_FRAME) {
			if(pagedir_is_dirty(thread_current()->pagedir, spte->user_vaddr)) {
				file_seek(mm->file, ofs);
				file_write(mm->file, spte->user_vaddr, spte->read_bytes);
			}
			pagedir_clear_page(thread_current()->pagedir, spte->user_vaddr);
			find_and_free_frame(spte);

		}

		hash_delete(&thread_current()->supt, &spte->hs_elem);
		free(spte);

		ofs+= PGSIZE;
		mm->mm_addr += PGSIZE;
	}

	file_close(mm->file);
	free(mm);
	lock_release(&sys_lock);
}

int sys_chdir (const char *dir) {
  lock_acquire(&sys_lock);

  char path[strlen(dir) + 1];
  memset(path,0, strlen(dir)+1);
  
  char* name = filename_from_path(dir, path);
  struct dir *next_dir = dir_from_path (path);

  if(next_dir == NULL)
  {
    lock_release(&sys_lock);
    return 0;
  }
  else {
  	struct inode *inode = NULL;
    dir_lookup (next_dir, name, &inode);
    if(inode == NULL) return 0;
  	if (thread_current()->dir != NULL) {
  		dir_close(thread_current()->dir);
  	}


  	thread_current()->dir = next_dir;
  	lock_release(&sys_lock);
  	return 1;
  }
}

int sys_mkdir (const char *dir) {
  
  lock_acquire (&sys_lock);
  bool ret = filesys_create(dir, 0);
  lock_release (&sys_lock);
  return (int) ret;
}

int sys_readdir (int fd, char *name) {
	lock_acquire(&sys_lock);
	struct file *file = thread_current()->fds[fd];
	if (file == NULL) {
		lock_release(&sys_lock);
		return 0;
	}
	struct inode *inode = file_get_inode(file);
	if (inode == NULL) {
		lock_release(&sys_lock);
		return 0;
	}

	if (inode_is_dir(inode) == false) {
		lock_release(&sys_lock);
		return 0;
	}

	if (thread_current()->fds_dir[fd] == NULL) {
		lock_release(&sys_lock);
		return 0;
	}

  	bool ret = dir_readdir(thread_current()->fds_dir[fd], name);

	lock_release(&sys_lock);
  	return (int) ret;
}

int sys_isdir (int fd) {

	struct file *file = thread_current()->fds[fd];
	if (inode_is_dir(file_get_inode(file)) == true) {
		return 1;
	}

	else {
		return 0;
	}
}

int sys_inumber (int fd) {
	struct file *file = thread_current()->fds[fd];
	return inode_number(file_get_inode(file));
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