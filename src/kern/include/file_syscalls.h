#ifndef FILE_SYSCALLS_H
#define FILE_SYSCALLS_H

#include <types.h>

// OPEN A FILE
int sys_open(const char *filename, int flags, int *retfd);
// CLOSE A FILE
int sys_close(int fd);
// READ FROM A FILE
int sys_read(int fd, void *buf, size_t buflen, int *retval);
// WRITE TO A FILE
int sys_write(int fd, const void *buf, size_t nbytes, int *retval);
// SET THE OFFSET OF A FILE
int sys_lseek(int fd, off_t pos, int whence, off_t *retval);
// DUPLICATE A FILE DESCRIPTOR
int sys_dup2(int oldfd, int newfd, int *retval);
// CHANGE THE CURRENT WORKING DIRECTORY
int sys_chdir(const char *pathname);
// GET THE CURRENT WORKING DIRECTORY
int sys___getcwd(char *buf, size_t buflen, int *retval);
// REMOVE A FILE
int sys_remove(const char *pathname);

#endif /* FILE_SYSCALLS_H */