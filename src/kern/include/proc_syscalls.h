#ifndef _PROC_SYSCALLS_H_
#define _PROC_SYSCALLS_H_

#include <types.h>

int sys_getpid(pid_t *retval);
int sys_fork(struct trapframe *tf, int32_t *retval);
void sys_exit (int status);
// int sys_waitpid(pid_t pid, int32_t *retval, int32_t options);
int sys_waitpid(pid_t pid,const struct __userptr * status,int32_t *retval, int32_t options);
int sys_execv(const char *program, char **args, int *retval);

#endif /* _PROC_SYSCALLS_H_ */