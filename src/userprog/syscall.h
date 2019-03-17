#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int pid_t;
void syscall_init (void);
void sys_exit(int status);
int sys_write (int fd, const void *buffer, unsigned size);
pid_t sys_exec(char *cmd_line);	
int sys_filesize(int fd);
int sys_wait(pid_t pid);

#endif /* userprog/syscall.h */
