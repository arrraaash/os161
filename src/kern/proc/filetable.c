// FILETABLE-RELATED FUNCTIONS

#include <filetable.h>
#include <vnode.h>
#include <limits.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <types.h>
#include <kern/unistd.h>
#include <synch.h>
#include <lib.h>
#include <vfs.h>

// INITIALIZE AN OPENFILE STRUCT
int openfile_init(struct vnode *vn, int flags, struct openfile **ret) {
    struct openfile *of;

    // BE SURE THE VNODE ISN'T NULL
    KASSERT(vn != NULL);

    // ALLOCATE MEMORY FOR THE OPENFILE
    of = kmalloc(sizeof(struct openfile));
    if (of == NULL) {
        kprintf("openfile_init: %s\n", strerror(ENOMEM));
        return ENOMEM;
    }

    of->vn          = vn;
    of->flags       = flags;
    of->offset      = 0;

    of->lock        = lock_create("openfile_lock");

    // BE SURE THE LOCK ISN'T NULL
    // IF SO, FREE THE OPENFILE AND RETURN -1
    if (of->lock == NULL) {
        kfree(of);
        return -1;
    }

    // SET THE RETURN VALUE
    *ret = of;

    return 0;
}

// DESTROY AN OPENFILE STRUCT
int openfile_destroy(struct openfile *of) {
    // BE SURE THE OPENFILE ISN'T NULL
    KASSERT(of != NULL);

    // DESTROY THE LOCK
    lock_destroy(of->lock);

    // FREE THE SPCE OF THE OPENFILE
    kfree(of);

    return 0;
}

// INCREMENT THE REFERENCE COUNT OF AN OPENFILE STRUCT
// int openfile_addref(struct openfile *of, bool no_lock) {
//     // BE SURE THE OPENFILE ISN'T NULL
//     KASSERT(of != NULL);

//     // ACQUIRE THE LOCK IF no_lock IS FALSE
//     if (!no_lock) {
//         lock_acquire(of->lock);
//     }

//     // INCREMENT THE REFERENCE COUNT
//     of->refcount++;

//     // RELEASE THE LOCK IF no_lock IS FALSE
//     if (!no_lock) {
//         lock_release(of->lock);
//     }
    
//     return 0;
// }

// DECREMENT THE REFERENCE COUNT OF AN OPENFILE STRUCT
// int openfile_decref(struct openfile *of) {
//     // BE SURE THE OPENFILE ISN'T NULL
//     KASSERT(of != NULL);

//     // ACQUIRE THE LOCK
//     lock_acquire(of->lock);

//     // DECREMENT THE REFERENCE COUNT
//     of->refcount--;

//     // RELEASE THE LOCK
//     lock_release(of->lock);

//     return 0;
// }

// INITIALIZE THE FILETABLE
struct filetable *filetable_init() {

    struct filetable *ft;

    // ALLOCATE MEMORY FOR THE FILETABLE
    ft = kmalloc(sizeof(struct filetable));
    if (ft == NULL) {
        kprintf("filetable_init: %s\n", strerror(ENOMEM));
        return NULL;
    }

    // INITIALIZE THE FILETABLE LOCK
    ft->lock = lock_create("filetable_lock");
    if (ft->lock == NULL) {
        kprintf("filetable_init: %s\n", strerror(ENOMEM));
        return NULL;
    }

    // INITIALIZE THE FILETABLE
    for (int i = 0; i < OPEN_MAX; i++) {
        ft->entries[i] = NULL;
    }

    return ft;
}

// INITIALIZE STDIN, STDOUT, AND STDERR IN THE FILETABLE
int init_stdio(struct filetable *ft) {
    struct vnode *stdio;
    struct openfile *temp_file;
    int result;
    const char *console = "con:";

    // INITIALIZE THE STDIN VNODE
    // USE kstrdup(console) TO WORK ON A COPY OF THE STRING, W/O MODIFYING THE ORIGINAL ONE
    result = vfs_open(kstrdup(console), O_RDONLY, 0, &stdio);
    if (result) {
        kprintf("init_stdio: %s\n", strerror(result));
        return result;
    }
    // ALLOCATE THE FILETABLE ENTRY FOR STDIN
    result = openfile_init(stdio, O_RDONLY, &temp_file);
    if (result) {
        kprintf("init_stdio: %s\n", strerror(result));
        return result;
    }
    // ADD THE OPENFILE TO THE FILETABLE
    ft->entries[STDIN_FILENO] = temp_file;

    // INITIALIZE THE STDOUT VNODE
    // USE kstrdup(console) TO WORK ON A COPY OF THE STRING, W/O MODIFYING THE ORIGINAL ONE
    result = vfs_open(kstrdup(console), O_WRONLY, 0, &stdio);
    if (result) {
        kprintf("init_stdio: %s\n", strerror(result));
        return result;
    }
    // ALLOCATE THE FILETABLE ENTRY FOR STDOUT
    result = openfile_init(stdio, O_WRONLY, &temp_file);
    if (result) {
        kprintf("init_stdio: %s\n", strerror(result));
        return result;
    }
    // ADD THE OPENFILE TO THE FILETABLE
    ft->entries[STDOUT_FILENO] = temp_file;

    // INITIALIZE THE STDERR VNODE
    // USE kstrdup(console) TO WORK ON A COPY OF THE STRING, W/O MODIFYING THE ORIGINAL ONE
    result = vfs_open(kstrdup(console), O_WRONLY, 0, &stdio);
    if (result) {
        kprintf("init_stdio: %s\n", strerror(result));
        return result;
    }
    // ALLOCATE THE FILETABLE ENTRY FOR STDERR
    result = openfile_init(stdio, O_WRONLY, &temp_file);
    if (result) {
        kprintf("init_stdio: %s\n", strerror(result));
        return result;
    }
    // ADD THE OPENFILE TO THE FILETABLE
    ft->entries[STDERR_FILENO] = temp_file;

    return 0;

}

// DESTROY THE FILETABLE
void filetable_destroy(struct filetable *ft) {

    // BE SURE THE FILETABLE ISN'T NULL
    KASSERT(ft != NULL);

    // DELETE THE FILETABLE AND ITS LOCK
    lock_destroy(ft->lock);
    kfree(ft);

}

// CHECK IF THE FILE DESCRIPTOR IS VALID
int is_valid(int fd) {

    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    return 0;

}

// CHECK IF A POSITION IN THE FILETABLE IS FREE
// IT IS SAFER THAN JUST DOING ft->entries[fd] == NULL BECAUSE OF LOCKS MANAGEMENT.
// no_lock TELLS IF LOCKING HAS TO BE DONE OR NOT, DEPENDING ON WHAT IS BEING PERFOMED FROM
// THE CALLING FUNCTION.
int is_available(struct filetable *ft, int fd) {
    
    // BE SURE THE FILE DESCRIPTOR IS VALID
    int result = is_valid(fd);
    if (result) {
        return result;
    }

    // BE SURE THE FILETABLE ENTRY ISN'T NULL
    // IF SO, RETURN 0, SINCE THE FILE DESCRIPTOR ISN'T IN USE
    if (ft->entries[fd] == NULL) {
        return 0;
    }

    // IF THE FILE DESCRIPTOR IS IN USE, RETURN -1
    return -1;

}

// ADD AN OPENFILE TO THE FILETABLE AT A SPECIFIC POSITION
// IT IS SAFER THAN JUST DOING ft->entries[fd] = of BECAUSE OF LOCKS MANAGEMENT.
// no_lock TELLS IF LOCKING HAS TO BE DONE OR NOT, DEPENDING ON WHAT IS BEING PERFOMED FROM 
// THE CALLING FUNCTION.
int filetable_add(struct filetable *ft, struct openfile *of, int fd, bool no_lock) {
    
    // BE SURE THE FILETABLE ISN'T NULL
    KASSERT(ft != NULL);

    // BE SURE THE OPENFILE ENTRY ISN'T NULL
    KASSERT(of != NULL);

    // ACQUIRE THE LOCK OF THE FILETABLE IF no_lock IS FALSE
    if (!no_lock) {
        lock_acquire(ft->lock);
    }

    // BE SURE THE FILE DESCRIPTOR IS VALID AND THAT THE POSITION IS AVAILABLE
    int result = is_available(ft, fd);
    if (result) {
        return result;
    }

    // ADD THE OPENFILE TO THE FILETABLE
    ft->entries[fd] = of;

    // RELEASE THE LOCK OF THE FILETABLE IF no_lock IS FALSE
    if (!no_lock) {
        lock_release(ft->lock);
    }

    return 0;

}

// ADD AN OPENFILE TO THE FILETABLE IN THE FIRST AVAILABLE POSITION
// no_lock TELLS IF LOCKING HAS TO BE DONE OR NOT, DEPENDING ON WHAT IS BEING PERFOMED FROM 
// THE CALLING FUNCTION.
int filetable_add_generic(struct filetable *ft, struct openfile *of, int *fd, bool no_lock) {
    
    // BE SURE THE FILETABLE ISN'T NULL
    KASSERT(ft != NULL);

    // BE SURE THE OPENFILE ENTRY ISN'T NULL
    KASSERT(of != NULL);

    // ACQUIRE THE LOCK OF THE FILETABLE IF no_lock IS FALSE
    if (!no_lock) {
        lock_acquire(ft->lock);
    }

    // FIND THE FIRST AVAILABLE FILE DESCRIPTOR
    for (int i = 3; i < OPEN_MAX; i++) {
        if (ft->entries[i] == NULL) {

            // ADD THE OPENFILE TO THE FILETABLE
            ft->entries[i] = of;

            // SET THE FILE DESCRIPTOR
            *fd = i;

            // RELEASE THE LOCK OF THE FILETABLE
            lock_release(ft->lock);

            return 0;
        }
    }

    // RELEASE THE LOCK OF THE FILETABLE IF no_lock IS FALSE
    if (!no_lock) {
        lock_release(ft->lock);
    }

    return EMFILE;

}

// REMOVE AN OPENFILE FROM THE FILETABLE
// IT IS SAFER THAN JUST DOING ft->entries[fd] = NULL BECAUSE OF LOCKS MANAGEMENT.
// no_lock TELLS IF LOCKING HAS TO BE DONE OR NOT, DEPENDING ON WHAT IS BEING PERFOMED FROM 
// THE CALLING FUNCTION.
int filetable_remove(struct filetable *ft, int fd, bool no_lock) {
    
    // BE SURE THE FILETABLE ISN'T NULL
    KASSERT(ft != NULL);

    // BE SURE THE FILE DESCRIPTOR IS VALID
    int result = is_valid(fd);
    if (result) {
        return result;
    }

    // BE SURE THE FILETABLE ENTRY IS AVAILABLE
    // IF SO, RETURN 0, SINCE THE FILE DESCRIPTOR ISN'T IN USE
    result = is_available(ft, fd);
    if (!result) {
        return 0;
    }

    // ACQUIRE THE LOCK OF THE FILETABLE IF no_lock IS FALSE
    if (!no_lock) {
        lock_acquire(ft->lock);
    }

    // REMOVE THE FILETABLE ENTRY
    ft->entries[fd] = NULL;

    // RELEASE THE LOCK OF THE FILETABLE IF no_lock IS FALSE
    if (!no_lock) {
        lock_release(ft->lock);
    }

    return 0;
    
}

// GET AN OPENFILE FROM THE FILETABLE
// IT IS SAFER THAN JUST DOING *ret = ft->entries[fd] BECAUSE OF LOCKS MANAGEMENT.
// USING A POINTER TO A POINTER, ENSURES TO GET AND WORK ON THE ENTRY ITSELF.  
// no_lock TELLS IF LOCKING HAS TO BE DONE OR NOT, DEPENDING ON WHAT IS BEING PERFOMED FROM 
// THE CALLING FUNCTION.
int filetable_get(struct filetable *ft, int fd, bool no_lock, struct openfile **ret) {
    
    int result;
    struct openfile *of;

    // BE SURE THE FILETABLE ISN'T NULL
    KASSERT(ft != NULL);

    // BE SURE THE FILETABLE ENTRY ISN'T NULL. THE CORRECTNESS OF THE FILE DESCRIPTOR IS CHECKED
    // IN THE is_available() FUNCTION.
    // IF THE FILE DESCRIPTOR ISN'T IN USE, is_available() RETURNS 0, SO THE FUNCTION RETURNS EBADF
    // IF THE FILE DESCRIPTOR IS IN USE, is_available() -1
    result = is_available(ft, fd);
    if (!result) {
        return EBADF;
    }

    // ACQUIRE THE LOCK OF THE FILETABLE IF no_lock IS FALSE
    if (!no_lock) {
        lock_acquire(ft->lock);
    }

    // GET THE OPENFILE FROM THE FILETABLE
    of = ft->entries[fd];

    // RELEASE THE LOCK OF THE FILETABLE IF no_lock IS FALSE
    if (!no_lock) {
        lock_release(ft->lock);
    }

    // SET THE RETURN VALUE
    *ret = of;

    return 0;

}

// COPY A FILETABLE
int copy_filetable(struct filetable *old_ft, struct filetable **new_ft) {
    struct filetable *ft;

    // BE SURE THE FILETABLE ISN'T NULL
    KASSERT(old_ft != NULL);

    // ALLOCATE MEMORY FOR THE NEW FILETABLE
    ft = kmalloc(sizeof(struct filetable));
    if (ft == NULL) {
        kprintf("copy_filetable: %s\n", strerror(ENOMEM));
        return ENOMEM;
    }

    // INITIALIZE THE NEW FILETABLE
    ft = filetable_init();
    if (ft == NULL) {
        kprintf("copy_filetable: %s\n", strerror(ENOMEM));
        return ENOMEM;
    }

    // COPY THE FILETABLE
    lock_acquire(old_ft->lock);
    for (int i = 0; i < OPEN_MAX; i++) {
        ft->entries[i] = old_ft->entries[i];
    }
    lock_release(old_ft->lock);

    // SET THE RETURN VALUE
    *new_ft = ft;

    return 0;
    
}