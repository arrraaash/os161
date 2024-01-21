#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <synch.h>
#include <vfs.h>
#include <fs.h>
#include <vnode.h>
#include <test.h>
#include <fh.h>
#include <file_syscalls.h>

int 
test_syscalls(int nargs, char **args)
{
    int fd;
    char filename[20] = "testfile_arash";
    char data[20] = "Hello, OS161!";
    size_t len = strlen(data);

    /* Test sys_open */
    fd = open(filename, O_WRONLY | O_CREAT);
    if (fd < 0) {
        kprintf("sys_open failed\n");
        return 1;
    }

    /* Test sys_write */
    if (write(fd, data, len) != len) {
        kprintf("sys_write failed\n");
        return 1;
    }

    /* Test sys_close */
    if (close(fd) != 0) {
        kprintf("sys_close failed\n");
        return 1;
    }

    kprintf("All syscalls passed\n");
    return 0;
}
