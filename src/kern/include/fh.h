#ifndef _FH_H_
#define _FH_H_

#include <limits.h>
/* struct of the file handle */
//first -> it should have the data of file if it is write or read
//second -> it should prevent non verified operaton, if it is RDONLY so we shouldnt be able to write
//third -> should have the data that show if it is safe to destroy or not
//fourth -> synchronization , fh should be shared amoung many procs and should be able to handle E.G.read/write at the same time
struct vnode;
struct fh {
    // I_fd is the file object
    char name[NAME_MAX];
	struct vnode *I_fd;
    off_t offset;
    int flag;
    bool lock;
};

#endif