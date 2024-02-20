#ifndef FILETABLE_H
#define FILETABLE_H

#include <types.h>
#include <spinlock.h>
#include <limits.h>
#include <vnode.h>
#include <lib.h>

struct openfile {
    struct vnode *vn;   // TO BE OBTAINED WITH vfs_open()
    int flags;          // O_RDONLY, O_WRONLY, O_RDWR
    off_t offset;       // CURRENT OFFSET IN FILE
    struct lock *lock;  // LOCK FOR SYNCHRONIZATION
    
    // THE refcount FIELD HAS FIRST BEEN ADDED TO THE openfile STRUCTURE AND THEN REMOVED.
    // ITS UTILITY WOULD BE TO KEEP TRACK OF THE NUMBER OF TIMES A FILE IS OPENED, THAT IS, 
    // THE NUMBER OF openfile STRUCTURES THAT POINT TO THE SAME vnode, BUT THIS WOULD 
    // CAUSE THIS FIELD TO BE UPDATED OVER THE WHOLE FILETABLE EACH TIME A NEW open() IS CALLED,
    // IN ORDER TO TRACK SUCH OPERATIONS, AND WOULD NOT BE USEFUL FOR ANYTHING ELSE, GIVEN THAT:
    //  - ONCE A FILETABLE ENTRY LOCATED AT fd IS REMOVED, THE openfile STRUCTURE IS LOST
    //  - THE REFERENCE COUNT RELATED TO THE TIMES A vnode STRUCTURE IS OPENED IS ALREADY TRACKED
    //    BY THE VNODE ITSELF, THROUGH ITS vn_refcount FIELD  
    // int refcount;       // REFERENCE COUNT

};

struct filetable {
    struct openfile *entries[OPEN_MAX];
    struct lock *lock;
};

// INITIALIZE THE OPENFILE
int openfile_init(struct vnode *vn, int flags, struct openfile **ret);
// DESTROY THE OPENFILE
int openfile_destroy(struct openfile *of);
// INCREASE THE REFERENCE COUNT OF THE OPENFILE
// int openfile_addref(struct openfile *of, bool no_lock);
// DECREASE THE REFERENCE COUNT OF THE OPENFILE
// int openfile_decref(struct openfile *of);
// INITIALIZE THE FILETABLE
struct filetable *filetable_init(void);
// DESTROY THE FILETABLE
void filetable_destroy(struct filetable *ft);
// INITIALIZE THE STD DEVICES
int init_stdio(struct filetable *ft);
// GET THE FILETABLE ENTRY
int filetable_get(struct filetable *ft, int fd, bool no_lock, struct openfile **ret);
// ADD THE FILETABLE ENTRY TO THE FIRST AVAILABLE SLOT
int filetable_add_generic(struct filetable *ft, struct openfile *of, int *fd, bool no_lock);
// ADD THE FILETABLE ENTRY TO A SPECIFIC SLOT
int filetable_add(struct filetable *ft, struct openfile *of, int fd, bool no_lock);
// REMOVE THE FILETABLE ENTRY
int filetable_remove(struct filetable *ft, int fd, bool no_lock);
// CHECK IF THE FILETABLE ENTRY IS VALID
int is_valid(int fd);
// CHECK IF THE FILETABLE ENTRY IS AVAILABLE
int is_available(struct filetable *ft, int fd);
// COPY THE FILETABLE
int copy_filetable(struct filetable *old_ft, struct filetable **new_ft);


#endif /* FILETABLE_H */
