#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#define mapid_t int

struct lock sys_lock;

typedef int pid_t;
void syscall_init (void);
void sys_exit(int status);
int sys_write (int fd, const void *buffer, unsigned size);
int sys_wait(pid_t pid);
int sys_exec(char *cmd_line);
int sys_create(const char *file, unsigned initial_size);
int sys_remove (const char *file);
int sys_open (const char *file);
int sys_filesize (int fd);
int sys_read(int fd, void *buffer, unsigned size);
int sys_write(int fd, const void *buffer, unsigned size );
void sys_seek(int fd, unsigned position);
unsigned sys_tell (int fd);
void sys_close(int fd );
mapid_t sys_mmap(int fd, void *addr);
void sys_munmap(mapid_t mapping);
int sys_chdir (const char *dir);
int sys_mkdir (const char *dir);
int sys_readdir (int fd, char *name);
int sys_isdir (int fd);
int sys_inumber (int fd);


#endif /* userprog/syscall.h */
