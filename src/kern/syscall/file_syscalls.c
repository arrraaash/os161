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


int sys_open(const char *filename, int flags, int *retval) {
	
	char cin_filename[PATH_MAX]; 
	int copyinside = copyinstr((const_userptr_t)filename, cin_filename, len, &got);
	if (copyinside) {
		return copyinside;
	}
	while(curproc->file_table[i] != NULL){
		if(i == OPEN_MAX-1){
			return EMFILE;	
		}
		i++;
	}
	curproc->file_table[i] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
	KASSERT(curproc->file_table[i] != NULL);
	err = vfs_open(cin_filename, flags, 0, &curproc->file_table[i]->vnode);
	if (err) {
		kfree(curproc->file_table[i]);
		curproc->file_table[i] = NULL;
		return err;
	}
	if(flags & O_APPEND){
		struct stat statbuf;
		err = VOP_STAT(curproc->file_table[i]->vnode, &statbuf);
		if (err){
			kfree(curproc->file_table[i]);
			curproc->file_table[i] = NULL;
			return err;
		}
		curproc->file_table[i]->offset = statbuf.st_size;
	} else {
		curproc->file_table[i]->offset = 0;
	}
	curproc->file_table[i]->destroy_count = 1;
	curproc->file_table[i]->con_file = false;
	mode = flags & O_ACCMODE;
	switch(mode){
		case O_RDONLY:
			curproc->file_table[i]->mode_open = O_RDONLY;
			break;
		case O_WRONLY:
			curproc->file_table[i]->mode_open = O_WRONLY;
			break;
		case O_RDWR:
			curproc->file_table[i]->mode_open = O_RDWR;
			break;
		default:
			vfs_close(curproc->file_table[i]->vnode);
			kfree(curproc->file_table[i]);
			curproc->file_table[i] = NULL;
			return EINVAL;
	}
	curproc->file_table[i]->lock = lock_create("filehandle_lock");
	if(curproc->file_table[i]->lock == NULL) {	
		vfs_close(curproc->file_table[i]->vnode);
		kfree(curproc->file_table[i]);
		curproc->file_table[i] = NULL;
	}
	*retval = i;
	return 0;
}
