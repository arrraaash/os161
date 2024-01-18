#ifndef _FILE_SYSCALLS_H_
#define _FILE_SYSCALLS_H_

int sys_write(int fd, const void *buf, size_t nbytes, *retval);
int sys_close(int fd);
int sys_open(const char *filename, int flags, int *retval);