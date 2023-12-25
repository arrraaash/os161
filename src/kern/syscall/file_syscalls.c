#include <stdlib.h>
#include <types.h>
#include <proc.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <file_syscall.h>
#include <limits.h>
#include <kern/unistd.h>
#include <endian.h>
#include <stat.h>
#include <lib.h>


int sys_open(const char *filename, int flags, mode_t mode, int *retval) {
	
	char dest[PATH_MAX];
	size_t len = PATH_MAX;
	size_t got;
	int i=3; // first 3 is already used by STDIN,STDOUT,STDERR
	int copyinside = copyinstr(filename, dest , len, &got);  //chacking the validity of the filename
	if (copyinside) {    //copyinstr returns null-terminated on success
		return EFAULT;
	}

	// curproc is always the current thread's process definged in current.h
	while(curproc->p_ft[i] != NULL){
		if(i == OPEN_MAX-1){
			return EMFILE;	
		}
		i++;
	}
	// allocating space and definging the file table
	curproc->p_ft[i] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
	KASSERT(curproc->p_ft[i] != NULL);
	err = vfs_open(dest, flags, 0, &curproc->p_tf[i]->vnode);
	// destroy the created space and file table in case of error
	if (err) {
		kfree(curproc->p_ft[i]);
		curproc->p_ft[i] = NULL;
		return err;
	}
	switch(mode){
		case O_RDONLY:
			curproc->p_ft[i]->flag = O_RDONLY;
			break;
		case O_WRONLY:
			curproc->p_ft[i]->flag = O_WRONLY;
			break;
		case O_RDWR:
			curproc->p_ft[i]->flag = O_RDWR;
			break;
		default:   // one of above mode should exsit otherwise we have an error
			vfs_close(curproc->p_ft[i]->vnode);
			kfree(curproc->p_ft[i]);
			curproc->p_ft[i] = NULL;
			return EINVAL;
	}
	if(flags & O_APPEND){  // if there is append the difference is we have to continue writing/reading from a last index
		curproc->p_ft[i]->offset = ;//???????????
	} else {
		curproc->p_ft[i]->offset = 0;
	}
	curproc->p_ft[i]->destroy_count = 1;
	curproc->p_ft[i]->con_file = false;
	
	curproc->file_table[i]->lock = TRUE;
	if(curproc->file_table[i]->lock == NULL) {	
		vfs_close(curproc->file_table[i]->vnode);
		kfree(curproc->file_table[i]);
		curproc->file_table[i] = NULL;
	}
	retval = p_ft[i];
	return 0;
}
