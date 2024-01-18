#include <stdlib.h>
#include <types.h>
#include <proc.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <fh.h>
#include <uio.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <limits.h>
#include <kern/unistd.h>
#include <endian.h>
#include <stat.h>
#include <lib.h>

//ARASH
int sys_open(const char *filename, int flags, int *retval) {
	
	char checked_fn[PATH_MAX];
	size_t len = PATH_MAX;
	size_t got;
	int fd=3; // first 3 is already used by STDIN,STDOUT,STDERR
	int err=0;
	int copyinside = copyinstr(filename, checked_fn , len, &got);  //chacking the validity of the filename
	if (copyinside) {    //copyinstr returns null-terminated on success
		return EFAULT;
	}
	// curproc is always the current thread's process definged in current.h
	while(curproc->p_ft[fd] != NULL){
		if(fd == OPEN_MAX-1){
			return EMFILE;	
		}
		fd++;
	}
	// checking the flags
	switch (flags){
		case O_RDONLY: break;
		case O_WRONLY: break;
		case O_RDWR: break;

		case O_RDONLY|O_CREAT: break;
		case O_WRONLY|O_CREAT: break;
		case O_RDWR|O_CREAT: break;

		case O_RDWR|O_CREAT|O_TRUNC: break;

		case O_WRONLY|O_APPEND: break;
		case O_RDWR|O_APPEND: break;

		default: return EINVAL;
	}
	// allocating space and definging the file table
	curproc->p_ft[fd] = (struct fh *)kmalloc(sizeof(struct fh));
	KASSERT(curproc->p_ft[fd] != NULL);
	//complete explanatin is in /vfs/vfspath.c
	err = vfs_open(checked_fn, flags, &curproc->p_tf[fd]->vnode);
	// destroy the created space and file table in case of error
	if (err) {
		kfree(curproc->p_ft[fd]);
		curproc->p_ft[fd] = NULL;
		return err;
	}
	curproc->p_ft[fd]->flag = flags;
	/* how = flags & O_ACCMODE;  //take from implementation of vfs_open() --->changed it to the way is done upper
	switch(how){
		case O_RDONLY:
			curproc->p_ft[fd]->flag = O_RDONLY;
			break;
		case O_WRONLY:
			curproc->p_ft[fd]->flag = O_WRONLY;
			break;
		case O_RDWR:
			curproc->p_ft[fd]->flag = O_RDWR;
			break;
		default:   // one of above mode should exsit otherwise we have an error
			vfs_close(curproc->p_ft[fd]->vnode);
			kfree(curproc->p_ft[fd]);
			curproc->p_ft[fd] = NULL;
			return EINVAL;
	} */
	switch (flags){  // if there is append the difference is we have to continue writing/reading from a last index
		case O_WRONLY|O_APPEND : curproc->p_ft[fd]->offset = offset + 1; //not sure just have to try ????
		case O_RDWR|O_APPEND :  curproc->p_ft[fd]->offset = offset + 1; //not sure just have to try ????
		default: curproc->p_ft[fd]->offset = 0;
	}
	// let it to have read or write after open
	curproc->p_ft[fd]->lock = TRUE;
	/* 
	checking the name of file --> done
	checking the mode --> done
	considering append --> done
	considering creat --> implemented in vfs_open() but i also check at first
	considering excl --> implemented in vfs_open() but i also check at first
	considering trunc --> implemented in vfs_open() but i also check at first
	*/
	// i just configur vars and flages which are allocated in mem -> things we have to know about our file !!!!!!!
	//  but actuall work is done in vfs_open()
	
	// maybe its not necessary but I is added because the lock is important to get TRUE after open and we have to see the error if its not
	if(curproc->p_ft[fd]->lock == NULL) {	
		vfs_close(curproc->p_ft[fd]->vnode);
		kfree(curproc->p_ft[fd]);
		curproc->p_ft[fd] = NULL;
	}
	// it returns the pointer to the created file table
	*retval = fd;
	return 0;
}

int SYS_close(int fd){
	if (curproc->p_ft[fd] != NULL || curproc->p_ft[fd]->lock == TRUE) {
		kfree(curproc->p_ft[fd]);
		curproc->p_ft[fd] = NULL;
	}
	return 0;
}
// we have to check if the buffer is valid, we need to initialize struct uio{}  before useing vop_write because we need
// to pass vnode pointers and uio into the MACRO
ssize_t write(int fd, const void *buf, size_t nbytes, *retval){

	// checking if fd is valid -- the write flage -- 
	if()
	if(fd>OPEN_MAX || fd<0 || curproc->p_ft[fd]==NULL || (curproc->p_ft[fd]->flags & O_WRONLY ==0)){
		return EBADF;
	}
	curproc->p_ft[fd]->lock = FALSE;
	char *buffer = (char *)kmalloc(sizeof(*buf)*nbytes);

	// completly explained in uio.h line 127
	//it is used to specify the buffer and length of the data to be read or written.

	struct uio uio_buff;
	// complete explanation is in iovec.h (it is used for distinguish user pointer from kernel pointer) 
	struct iovec iov;
	uio_kinit(&iov, &uio_buff, buffer, nbytes, curproc->p_ft[fd]->offset, UIO_WRITE); //UIO_WRITE -> uio -> enum uio_rw

	int vop_write_res = vop_write(curproc->p_ft[fd]->vnode, &uio);
	if (vop_write_res){
		kfree(buffer);
		return vop_write_res;
	}
	*retval = uio_buff->uio_offset - curproc->p_ft[fd]->offset;
	curproc->p_ft[fd]->offset = uio_buff->uio_offset;
	kfree(buffer);
	curproc->p_ft[fd]->lock = TRUE;
	return 0;
}
