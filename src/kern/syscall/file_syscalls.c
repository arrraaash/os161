// FILE-RELATED SYSCALLS

#include <filetable.h>
#include <file_syscalls.h>
#include <vnode.h>
#include <kern/limits.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <types.h>
#include <kern/unistd.h>
#include <current.h>
#include <synch.h>
#include <uio.h>
#include <kern/iovec.h>
#include <kern/seek.h>
#include <stat.h>
#include <vfs.h>
#include <proc.h>

// OPEN A FILE.
// const char * LETS THE POINTER TO FILENAME TO BE MODIFIED, BUT NOT THE CONTENT OF THE STRING
int sys_open(const char *filename, int flags, int *retfd) {

    // DECLARE VARIABLES
    struct openfile *file;
    struct vnode *vn;
    int result;
    bool no_lock;

    // OPEN THE FILE
    // THE ERROR MESSAGES ARE MANAGED BY vfs_open()
    // THE PERMISSION MODE IS SET TO 0664, SO THE OWNER CAN READ AND WRITE THE FILE, WHILE
    // THE OTHER USERS CAN ONLY READ IT
    result = vfs_open((char *) filename, flags, 0664, &vn);
    if (result) {
        kprintf("sys_open: %s\n", strerror(result));
        return result;
    }
    
    // INITIALIZE THE OPENFILE STRUCT
    // THE EVENTUALLY RETURNED ERROR IS RELATED EITHER TO kmalloc() (ENOMEM) OR TO THE IMPOSSIBILITY 
    // TO CREATE A LOCK (NULL VALUE RETURNED). 
    // SINCE THESE TWO CASES DO NOT FALL WITHIN THE EXPECTED ERROR CODES FOR open(), RETURN THEM AS 
    // THEY ARE (SO ENOMEM AND -1)   
    result = openfile_init(vn, flags, &file);
    if (result) {
        kprintf("sys_open: %s\n", strerror(result));
        return result;
    }

    // CHECK IF THE FLAGS INCLUDE THE APPEND ONE, IN ORDER TO UPDATE THE OFFSET ACCORDINGLY
    // THIS HELPS THE OTHER FUNCTIONS TO START WRITING FROM A CORRECT POSITION
    // THE stat() FUNCTION CAN BE EXPLOITED TO KNOW THE LENGTH OF THE FILE AND, THUS
    // WHERE EOF IS LOCATED
    if (flags & O_APPEND) {
        
        // INITIALIZE THE STAT STRUCT AND THE OFFSET 
        off_t eof;
        struct stat *stat = kmalloc(sizeof(struct stat));
        stat = kmalloc(sizeof(struct stat));
        if (stat == NULL) {
            kprintf("sys_open: %s\n", strerror(ENOMEM));
            return ENOMEM;
        }
        
        // GET THE STAT OF THE FILE AND THE OFFSET
        VOP_STAT(file->vn, stat);
        eof = stat->st_size;
        file->offset = eof;
        kfree(stat);
        
    }

    // ADD THE FILE TO THE FILETABLE.
    // THE FILETABLE CAN BE LOCKED INTERNALLY TO THE FUNCTION, SO no_lock IS SET TO FALSE.
    // ON ERROR, filetable_add_generic() RETURNS THE EMFILE ERROR, WHICH MEANS THAT THE FILETABLE IS FULL
    no_lock = false;
    result = filetable_add_generic(curproc->p_filetable, file, retfd, no_lock);
    if (result) {
        kprintf("sys_open: %s\n", strerror(result));
        return result;
    }

    return 0;

}

// CLOSE A FILE
int sys_close(int fd) {

    // DECLARE VARIABLES
    struct openfile *file;
    int result;
    bool no_lock;

    // SINCE THE FILETABLE HAS TO BE LOCKED THROUGHOUT THE ENTIRE FUNCTION, THE LOCK IS
    // ACQUIRED HERE AND no_lock IS SET TO TRUE
    no_lock = true;
    lock_acquire(curproc->p_filetable->lock);

    // GET THE FILE FROM THE FILETABLE.
    // ON ERROR IT RETURNS EBADF, WHICH MEANS THAT THE FILE DESCRIPTOR fd IS NOT VALID (NULL FILETABLE ENTRY
    // OR fd OUTSIDE THE FILETABLE BOUNDARIES 0 AND OPEN_MAX)
    result = filetable_get(curproc->p_filetable, fd, no_lock, &file);
    if (result) {
        lock_release(curproc->p_filetable->lock);
        kprintf("sys_close: %s\n", strerror(result));
        return result;
    } 

    // CLOSE THE FILE
    vfs_close(file->vn);
    
    // REMOVE THE FILE FROM THE FILETABLE
    filetable_remove(curproc->p_filetable, fd, no_lock);

    // RELEASE THE LOCK ON THE FILETABLE
    lock_release(curproc->p_filetable->lock);

    return 0;

}

// READ FROM A FILE
// USING userptr_t FOR buf GENERATES ERROR, SO IT IS USED as void * AND THEN CASTED TO userptr_t IN ORDER ALSO
// TO ACCOMPLISH THE DEFINITION OF read() IN THE MAN PAGE
int sys_read(int fd, void *buf, size_t buflen, int *retval) {

    // DECLARE VARIABLES
    struct openfile *file;
    struct uio userio;
    struct iovec iov;
    int result;
    off_t offset;
    bool no_lock;

    // SINCE THE FILETABLE HAS TO BE LOCKED THROUGHOUT THE ENTIRE FUNCTION, THE LOCK IS
    // ACQUIRED HERE AND no_lock IS SET TO TRUE
    no_lock = true;
    lock_acquire(curproc->p_filetable->lock);

    // GET THE FILE FROM THE FILETABLE.
    // ON ERROR IT RETURNS EBADF, WHICH MEANS THAT THE FILE DESCRIPTOR fd IS NOT VALID (NULL FILETABLE ENTRY
    // OR fd OUTSIDE THE FILETABLE BOUNDARIES 0 AND OPEN_MAX)
    result = filetable_get(curproc->p_filetable, fd, no_lock, &file);
    if (result) {
        lock_release(curproc->p_filetable->lock);
        kprintf("sys_read: %s\n", strerror(result));
        return result;
    }

    // LOCK THE FILE
    lock_acquire(file->lock);

    // UNLOCK THE FILETABLE, SINCE THE FILE HAS BEEN LOCKED
    lock_release(curproc->p_filetable->lock);

    // CHECK IF THE FILE HAS BEEN OPENED AS WRITE ONLY.
    // IF SO, THE FILE CANNOT BE READ AND THE EBADF ERROR IS RETURNED
    if (file->flags & O_WRONLY) {
        lock_release(file->lock);
        kprintf("sys_read: %s\n", strerror(EBADF));
        return EBADF;
    }

    // ACCESS THE FILE OFFSET FROM THE OBTAINED OPENFILE STRUCT
    offset = file->offset;

    // SETUP A uio RECORD FOR THE TRANSFER, SO THAT THE KERNEL CAN READ FROM THE FILE
    // AND FILL THE BUFFER PROVIDED BY THE USER.
    // iov.iov_ubase        = USER BUFFER PROVIDED TO THE FUNCTION
    // iov.iov_len          = NUMBER OF BYTES TO BE TRANSFERRED, SO THE SIZE OF THE BUFFER
    // userio.uio_iov       = iovec STRUCT
    // userio.uio_iovcnt    = NUMBER OF iovec STRUCTS, SO 1
    // userio.uio_offset    = DESIRED OFFSET IN THE FILE, SO THE CURRENT FILE OFFSET
    // userio.uio_resid     = RESIDUAL NUMBER OF BYTES TO BE TRANSFERRED AND, HENCE, THE NUMBER OF BYTES TO READ
    // userio.uio_segflg    = ADDRESS SPACE OF THE USER BUFFER. SINCE THE BUFFER POINTER COMES FROM THE USER, IT IS
    //                        A USER POINTER. HENCE, THE BUFFER CONTAINS USER DATA AND THE ADDRESS SPACE IS UIO_USERSPACE
    // userio.uio_rw        = DATA TRANSFER DIRECTION. SINCE WE ARE READING FROM THE FILE, SOME DATA IS GOING FROM KERNEL
    //                        SPACE TO USER SPACE (buf). HENCE, THE DIRECTION IS UIO_READ
    // userio.uio_space     = ADDRESS SPACE OF THE CURRENT PROCESS
    iov.iov_ubase           = (userptr_t) buf;
    iov.iov_len             = buflen;
    userio.uio_iov          = &iov;
    userio.uio_iovcnt       = 1;
    userio.uio_offset       = offset;
    userio.uio_resid        = buflen;
    userio.uio_segflg       = UIO_USERSPACE;
    userio.uio_rw           = UIO_READ;
    userio.uio_space        = curproc->p_addrspace;

    // CALL VOP_READ.
    // THE ERROR MESSAGES ARE MANAGED BY VOP_READ()
    result = VOP_READ(file->vn, &userio);
    if (result) {
        lock_release(file->lock);
        kprintf("sys_read: %s\n", strerror(result));
        return result;
    }

    // UPDATE THE OFFSET IN THE OPENFILE STRUCT BASED ON THE AMOUNT OF BYTES READ, THAT IS buflen - userio.uio_resid.
    // IF ALL THE BYTES WERE READ, userio.uio_resid = 0 AND buflen - userio.uio_resid = buflen
    file->offset += (off_t) (buflen - userio.uio_resid);

    // UNLOCK THE FILE
    lock_release(file->lock);

    // UPDATE THE RETURNED NUMBER OF BYTES READ, THAT IS buflen - userio->uio_resid.
    // IF ALL THE BYTES WERE READ, userio->uio_resid = 0 AND buflen - userio->uio_resid = buflen
    *retval = (int) (userio.uio_offset - offset);

    return 0;

}

// WRITE TO A FILE.
// USING userptr_t FOR buf GENERATES ERROR, SO IT IS USED as void * AND THEN CASTED TO userptr_t IN ORDER ALSO
// TO ACCOMPLISH THE DEFINITION OF read() IN THE MAN PAGE.
// const void * LETS THE POINTER TO buf TO BE MODIFIED, BUT NOT THE CONTENT OF THE STRING
int sys_write(int fd, const void *buf, size_t nbytes, int *retval) {

    // DECLARE VARIABLES
    struct openfile *file;
    struct uio userio;
    struct iovec iov;
    int result;
    off_t offset;
    bool no_lock;

    // SINCE THE FILETABLE HAS TO BE LOCKED THROUGHOUT THE ENTIRE FUNCTION, THE LOCK IS
    // ACQUIRED HERE AND no_lock IS SET TO TRUE
    no_lock = true;
    lock_acquire(curproc->p_filetable->lock);

    // GET THE FILE FROM THE FILETABLE. 
    // ON ERROR IT RETURNS EBADF, WHICH MEANS THAT THE FILE DESCRIPTOR fd IS NOT VALID (NULL FILETABLE ENTRY
    // OR fd OUTSIDE THE FILETABLE BOUNDARIES 0 AND OPEN_MAX)
    result = filetable_get(curproc->p_filetable, fd, no_lock, &file); 
    if (result) {
        lock_release(curproc->p_filetable->lock);
        kprintf("sys_write: %s\n", strerror(result));
        return result;
    }

    // LOCK THE FILE
    lock_acquire(file->lock);

    // UNLOCK THE FILETABLE, SINCE THE FILE HAS BEEN LOCKED
    lock_release(curproc->p_filetable->lock);

    // CHECK IF THE FILE HAS BEEN OPENED AS READ ONLY
    if (file->flags & O_RDONLY) {
        lock_release(file->lock);
        kprintf("sys_write: %s\n", strerror(EBADF));
        return EBADF;
    }
    
    // ACCESS THE FILE OFFSET FROM THE OBTAINED OPENFILE STRUCT
    offset = file->offset;

    // SETUP A uio RECORD FOR THE TRANSFER, SO THAT THE KERNEL CAN FILL THE FILE
    // WITH THE DATA PROVIDED BY THE USER.
    // iov.iov_ubase        = USER BUFFER PROVIDED TO THE FUNCTION
    // iov.iov_len          = NUMBER OF BYTES TO BE TRANSFERRED, SO THE SIZE OF THE BUFFER
    // userio.uio_iov       = iovec STRUCT
    // userio.uio_iovcnt    = NUMBER OF iovec STRUCTS, SO 1
    // userio.uio_offset    = DESIRED OFFSET IN THE FILE, SO THE CURRENT FILE OFFSET
    // userio.uio_resid     = RESIDUAL NUMBER OF BYTES TO BE TRANSFERRED AND, HENCE, THE NUMBER OF BYTES TO WRITE
    // userio.uio_segflg    = ADDRESS SPACE OF THE USER BUFFER. SINCE THE BUFFER POINTER COMES FROM THE USER, IT IS
    //                        A USER POINTER. HENCE, THE BUFFER CONTAINS USER DATA AND THE ADDRESS SPACE IS UIO_USERSPACE 
    // userio.uio_rw        = DATA TRANSFER DIRECTION. SINCE WE ARE WRITING TO THE FILE, SOME DATA IS GOING FROM USER
    //                        SPACE (buf) TO KERNEL SPACE. HENCE, THE DIRECTION IS UIO_WRITE
    // userio.uio_space     = ADDRESS SPACE OF THE CURRENT PROCESS
    iov.iov_ubase           = (userptr_t) buf; 
	iov.iov_len             = nbytes;
	userio.uio_iov          = &iov;
	userio.uio_iovcnt       = 1;
	userio.uio_offset       = offset;
	userio.uio_resid        = nbytes;
	userio.uio_segflg       = UIO_USERSPACE;
	userio.uio_rw           = UIO_WRITE;
	userio.uio_space        = curproc->p_addrspace;

    // CALL VOP_WRITE.
    // THE ERROR MESSAGES ARE MANAGED BY VOP_WRITE()
    result = VOP_WRITE(file->vn, &userio);
    if (result) {
        lock_release(file->lock);
        kprintf("sys_write: %s\n", strerror(result));
        return result;
    }

    // UPDATE THE OFFSET IN THE OPENFILE STRUCT BASED ON THE AMOUNT OF BYTES WRITTEN, THAT IS nbytes - userio.uio_resid.
    // IF ALL THE BYTES WERE WRITTEN, userio.uio_resid = 0 AND nbytes - userio.uio_resid = nbytes
    file->offset += (off_t) (nbytes - userio.uio_resid); 

    // UNLOCK THE FILE
    lock_release(file->lock);

    // UPDATE THE RETURNED NUMBER OF BYTES WRITTEN, THAT IS nbytes - userio.uio_resid.
    // IF ALL THE BYTES WERE WRITTEN, userio.uio_resid = 0 AND nbytes - userio.uio_resid = nbytes
    *retval = (int) (userio.uio_offset - offset);

    return 0;

}

// ALTER THE CURRENT OFFSET OF A FILE BASED ON THE whence VALUE
// whence = SEEK_SET -> FINAL OFFSET = pos
// whence = SEEK_CUR -> FINAL OFFSET = CURRENT OFFSET + pos
// whence = SEEK_END -> FINAL OFFSET = EOF + pos
int sys_lseek(int fd, off_t pos, int whence, off_t *retval) {

    // DECLARE VARIABLES
    struct openfile *file;
    int result;
    bool no_lock;
    off_t seek_pos;
    struct stat *stat;
    off_t eof;

    // CHECK IF THE WHENCE VALUE IS VALID
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        kprintf("sys_lseek: %s\n", strerror(EINVAL));
        return EINVAL;
    }

    // SINCE THE FILETABLE HAS TO BE LOCKED THROUGHOUT THE ENTIRE FUNCTION, THE LOCK IS
    // ACQUIRED HERE AND no_lock IS SET TO TRUE
    no_lock = true;
    lock_acquire(curproc->p_filetable->lock);

    // GET THE FILE FROM THE FILETABLE
    result = filetable_get(curproc->p_filetable, fd, no_lock, &file);
    if (result) {
        lock_release(curproc->p_filetable->lock);
        kprintf("sys_lseek: %s\n", strerror(result));
        return result;
    }

    // LOCK THE FILE
    lock_acquire(file->lock);

    // UNLOCK THE FILETABLE, SINCE THE FILE HAS BEEN LOCKED
    lock_release(curproc->p_filetable->lock);

    // CKECK IF THE FILE IS SEEKABLE
    if (!VOP_ISSEEKABLE(file->vn)) {
        lock_release(file->lock);
        kprintf("sys_lseek: %s\n", strerror(ESPIPE));
        return ESPIPE;
    }

    // RETRIEVE THE OFFSET OF THE FILE
    seek_pos = file->offset;
   
    // CHECK THE WHENCE VALUE AND UPDATE
    switch (whence) {
        case SEEK_SET:
            seek_pos = pos;
            break;
        case SEEK_CUR:
            seek_pos += pos;
            break;
        case SEEK_END:
            // INITIALIZE THE stat STRUCT
            stat = kmalloc(sizeof(struct stat));
            if (stat == NULL) {
                kprintf("sys_open: %s\n", strerror(ENOMEM));
                return ENOMEM;
            }
            // GET THE STAT OF THE FILE AND THE OFFSET
            VOP_STAT(file->vn, stat);
            eof         = stat->st_size;
            seek_pos    = eof + pos;
            kfree(stat);
        default:
            break;
    }

    // CHECK IF THE FINAL OFFSET IS NEGATIVE
    if (seek_pos < 0) {
        lock_release(file->lock);
        kprintf("sys_lseek: %s\n", strerror(EINVAL));
        return EINVAL;
    }

    // UPDATE THE OFFSET IN THE OPENFILE STRUCT
    file->offset = seek_pos;
    
    // UNLOCK THE FILE
    lock_release(file->lock);

    // UPDATE THE RETURNED OFFSET.
    *retval = seek_pos;
    
    return 0;

}

// DUPLICATE AN OPENFILE STRUCT LOCATED AT oldfd INTO POSITION newfd
int sys_dup2(int oldfd, int newfd, int *retval) {

    // DECLARE VARIABLES
    struct openfile *file, *temp_file;
    int result;
    bool no_lock;

    // CHECK IF oldfd AND newfd ARE EQUAL. IF SO; NOTHING HAS TO BE DONE
    if (oldfd == newfd) {
        *retval = newfd;
        return 0;
    }

    // LOCK THE FILETABLE
    lock_acquire(curproc->p_filetable->lock);

    // CHECK IF newfd IS WITHIN THE FILETABLE BOUNDARIES
    result = is_valid(newfd);
    if (result) {
        lock_release(curproc->p_filetable->lock);
        kprintf("sys_dup2: %s\n", strerror(result));
        return result;
    }

    // GET THE FILE LOCATED AT oldfd FROM THE FILETABLE. 
    // THE FILETABLE IS BEING LOCKED EXTERNALLY, SO no_lock IS SET TO TRUE
    no_lock = true;
    result = filetable_get(curproc->p_filetable, oldfd, no_lock, &file);
    if (result) {
        lock_release(curproc->p_filetable->lock);
        kprintf("sys_dup2: %s\n", strerror(result));
        return result;
    }

    // CHECK IF newfd IS ALREADY IN USE. IF SO, CLOSE THE FILE AND FREE THE CORRESPONDING FILETABLE ENTRY.
    // IF result IS NOT 0, IT MEANS THAT newfd IS VALID BUT THE ENTRY IS IN USE.
    // NO FURTHER CHECKS ARE NEEDED ON THE OUTCOME OF filetable_get(), SINCE THE FILETABLE IS LOCKED AND 
    // newfd IS VALID AND FULL
    result = is_available(curproc->p_filetable, newfd);
    if (result) {
        // GET THE FILE FROM THE FILETABLE
        filetable_get(curproc->p_filetable, newfd, no_lock, &temp_file); 

        // CLOSE THE FILE
        vfs_close(temp_file->vn);
        
        // REMOVE THE FILE FROM THE FILETABLE
        filetable_remove(curproc->p_filetable, newfd, no_lock);
    } 

    // INCREASE THE REFERENCE COUNT OF THE struct vnode *vn FIELD OF THE OPENFILE STRUCT TO BE COPIED
    VOP_INCREF(file->vn);

    // COPY THE FILETABLE ENTRY FROM oldfd TO newfd
    result = filetable_add(curproc->p_filetable, file, newfd, no_lock);
    if (result) {
        lock_release(curproc->p_filetable->lock);
        kprintf("sys_dup2: %s\n", strerror(result));
        return result;
    }

    // UNLOCK THE FILETABLE
    lock_release(curproc->p_filetable->lock);

    // UPDATE THE RETURNED VALUE
    *retval = newfd; 

    return 0;

}

// CHANGE THE CURRENT WORKING DIRECTORY
int sys_chdir(const char *pathname) {

    // DECLARE VARIABLES
    int result;

    // CALL VFS_CHDIR
    result = vfs_chdir((char *) pathname);
    if (result) {
        kprintf("sys_chdir: %s\n", strerror(result));
        return result;
    }

    return 0;

}

// GET THE CURRENT WORKING DIRECTORY
int sys___getcwd(char *buf, size_t buflen, int *retval) {

    // DECLARE VARIABLES
    struct uio userio;
    struct iovec iov;
    int result;

    // SETUP A uio RECORD FOR THE TRANSFER, SO THAT THE KERNEL CAN FILL THE BUFFER 
    // PROVIDED BY THE USER WITH THE CURRENT WORKING DIRECTORY.
    // USE userio AND iov AS VARIABLES AND NOT AS POINTERS, OTHERWISE WE HAVE 
    // "error: 'iov' may be used uninitialized in this function"
    // iov.iov_ubase        = USER BUFFER PROVIDED TO THE FUNCTION
    // iov.iov_len          = NUMBER OF BYTES TO BE TRANSFERRED, SO THE SIZE OF THE BUFFER
    // userio.uio_iov       = iovec STRUCT
    // userio.uio_iovcnt    = NUMBER OF iovec STRUCTS, SO 1
    // userio.uio_offset    = DESIRED OFFSET, SO 0
    // userio.uio_resid     = RESIDUAL NUMBER OF BYTES TO BE TRANSFERRED AND, HENCE, THE NUMBER OF BYTES TO READ
    // userio.uio_segflg    = ADDRESS SPACE OF THE USER BUFFER. SINCE THE BUFFER POINTER COMES FROM THE USER, IT IS
    //                        A USER POINTER. HENCE, THE BUFFER CONTAINS USER DATA AND THE ADDRESS SPACE IS UIO_USERSPACE
    // userio.uio_rw        = DATA TRANSFER DIRECTION. SINCE WE ARE READING THE WORKING DIRETORY, SOME DATA IS GOING FROM KERNEL
    //                        SPACE TO USER SPACE (buf). HENCE, THE DIRECTION IS UIO_READ
    // userio.uio_space     = ADDRESS SPACE OF THE CURRENT PROCESS
    iov.iov_ubase           = (userptr_t) buf;
    iov.iov_len             = buflen;
    userio.uio_iov          = &iov;
    userio.uio_iovcnt       = 1;
    userio.uio_offset       = 0;
    userio.uio_resid        = buflen;
    userio.uio_segflg       = UIO_USERSPACE;
    userio.uio_rw           = UIO_READ;
    userio.uio_space        = curproc->p_addrspace; 

    // CALL VFS_GETCWD
    result = vfs_getcwd(&userio);
    if (result) {
        kprintf("sys__getcwd: %s\n", strerror(result));
        return result;
    }

    // UPDATE THE RETURNED NUMBER OF BYTES READ, THAT IS buflen - userio.uio_resid.
    *retval = buflen - userio.uio_resid;

    return 0;

}
    
// REMOVE A FILE
// GIVES ENOSYS, PROBABLY THROWN BY VOP_REMOVE() INTO vfs_remove() BECAUSE emufs_remove() IN emu.c IS NOT IMPLEMENTED
/* int sys_remove(const char *pathname) {

    // DECLARE VARIABLES
    int result;

    // CALL vfs_remove()
    result = vfs_remove((char *) pathname);

    kprintf("filename: %s\n", pathname);
    kprintf("result: %d\n", result);

    if (result) {
        kprintf("sys_remove: %s\n", strerror(result));
        return result;
    }

    return 0;

} */
    