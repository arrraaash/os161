#include <types.h>
#include <proc.h>
#include <current.h>
#include <syscall.h>
#include <thread.h>
#include <proc_syscalls.h>
#include <addrspace.h>
#include <kern/errno.h>
#include <filetable.h>
#include <mips/trapframe.h>
#include <proc_table.h>
#include <kern/wait.h>
#include <wchan.h>
#include <synch.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <copyinout.h>


void
enter_usermode(void *data1, unsigned long data2);

static
int
setup_fork_trapframe(struct trapframe *old_tf, struct trapframe **new_tf);
//------------------------------GETPID---------------------------------

int sys_getpid(pid_t *retval) {
    //*retval = curproc->p_pid;
    *retval = ptable->process[curproc->p_pid]->p_pid;
    return 0;
}


//------------------------------FORK---------------------------------
static
int
setup_fork_trapframe(struct trapframe *old_tf, struct trapframe **new_tf)
{
    *new_tf = kmalloc(sizeof(struct trapframe));
	if (*new_tf == NULL) {
		return ENOMEM;
	}

	memcpy((void *) *new_tf, (const void *) old_tf, sizeof(struct trapframe));
	(*new_tf)->tf_v0 = 0;
	(*new_tf)->tf_v1 = 0;
	(*new_tf)->tf_a3 = 0;   
	(*new_tf)->tf_epc += 4;

	return 0;
}

void
enter_usermode(void *data1, unsigned long data2)
{
	(void) data2;
	void *tf = (void *) curthread->t_stack + 16;

	memcpy(tf, (const void *) data1, sizeof(struct trapframe));
	kfree((struct trapframe *) data1);

	as_activate();
	mips_usermode(tf);
}

int sys_fork(struct trapframe *tf, int32_t *retval)
{
    int err;
    struct proc *child_proc;

    child_proc = kmalloc(sizeof(*child_proc));
    struct trapframe *child_tf;
    //struct addrspace *child_addrs;
    /* child_tf = kmalloc(sizeof(child_tf));
    if (child_tf == NULL) {
        return ENOMEM;
    } */
    
    /* memcpy(child_tf, tf, sizeof(tf)); */
    child_proc = proc_create_runprogram(curproc->p_name);
    if (child_proc == NULL) {
        return ENOMEM;
    }
    
    //create a new address space for the child process and copy the parent's address space
    err = as_copy(curproc->p_addrspace, &child_proc->p_addrspace); 
    if (err) {
        return err;
    }
    //child_addrs = child_proc->p_addrspace;

    
    struct filetable *child_ft = curproc->p_filetable;
	lock_acquire(child_ft->lock);
	copy_filetable(curproc->p_filetable, &(child_proc->p_filetable)); // copy the file table
	lock_release(child_ft->lock);


    spinlock_acquire(&curproc->p_lock);  // copy the current working directory
        if (curproc->p_cwd != NULL) {
            VOP_INCREF(curproc->p_cwd);
            child_proc->p_cwd = curproc->p_cwd;
        }
    spinlock_release(&curproc->p_lock);

    setup_fork_trapframe(tf,&child_tf);

    err = thread_fork("new_thread", child_proc, enter_usermode,child_tf,1);
    // kprintf("err is %d\n", err);
    if (err) {
        proc_destroy(child_proc);
        free_pid(child_proc);
        kfree(child_tf);
        return err;
    }

    //child_proc->p_ppid = curproc->p_pid;
    //child_proc->p_pid = assign_pid(child_proc);
    //child_proc->exit = false;
    //child_proc->exit_status = false;
    * retval = child_proc->p_pid;
    return 0;
}

//------------------------------Exit---------------------------------


void
sys_exit (int status)
{
    KASSERT (curproc != NULL);
    ptable->process[curproc->p_pid]->exit = status;
    ptable->process[curproc->p_pid]->exit_status = true;
    thread_exit();
    proc_destroy(curproc);
    
}

//------------------------------waitpid---------------------------------
int 
sys_waitpid(pid_t pid,const struct __userptr * status,int32_t *retval, int32_t options) {
    

    // Check if the provided PID is valid
    int err = validity_check_pid(pid);
    if (err != 0) {
        
        *retval = -1;
        return err;
    }
    
    // Check if the options are valid
    if (options != 0) {
                *retval = -1;
        return EINVAL;
    }
    
    // Check if the status pointer is valid
    /* if (status == NULL) {
        *retval = -1;
        return EFAULT;
    } */
    

    // Acquire the process lock to access the process table
    //spinlock_acquire(&proc_table[proc_table[pid]->ppid]->p_lock);
    
    wait_func(pid);
    
    *retval = ptable->process[pid]->p_pid;
    //spinlock_release(&proc_table[proc_table[pid]->ppid]->p_lock);

    // Copy the exit status
    int ret = copyout(&ptable->process[pid]->exit, (userptr_t) status, sizeof(int32_t));
		if (ret){
			return ret;
		}
    // Return the PID of the terminated child process
    return 0;
}


//-----------------------------------SYS_EXECV---------------------------------


int
sys_execv(const char *program, char **args, int *retval){

    struct addrspace *as_new,*as_old;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
    int len;
	int result;
    int argc = 0;
    char *prog;


    if (program == NULL || args == NULL) {
		return EFAULT;
	}

	
	
    while (args[argc] != NULL) {
        argc++;
    }
	
    //allocate memory in kernel for the args, we pass args from user space to kernel space
	char **args_in = kmalloc((argc + 1) *sizeof(char *));
    if (args_in == NULL) {
        return ENOMEM;
    }

    for (int i=0; i < argc; i++) {
        len = strlen(args[i]) + 1;
        args_in[i] = kmalloc(len * sizeof(char));
        if (args_in[i] == NULL) {
            return ENOMEM;
        }

        //copy args[i] in kernel space
        result = copyinstr((const_userptr_t) args[i], args_in[i], len, NULL);
        if (result!=0) {
            kfree(args_in);
            return result;
        }
    }
    len = strlen(program) + 1;
    prog = kmalloc(len * sizeof(char));
    if (prog == NULL) {
        return ENOMEM;
    }
    result = copyinstr((const_userptr_t) program, prog, len, NULL);
    if (result!=0) {
        kfree(prog);
        kfree(args_in);
        return result;
    }
        
	/* Open the file. */
	result = vfs_open(prog, O_RDONLY, 0, &v);
	if (result) {
        vfs_close(v);
		kfree(prog);
        kfree(args_in);
        return result;
	}

	/* We should be a new process. */
	//KASSERT(proc_getas() == NULL);
    as_old = proc_getas();

	/* Create a new address space. */
	as_new = as_create();
	if (as_new == NULL) {
		vfs_close(v);
        kfree(prog);
        kfree(args_in);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
    //as_destroy(as_old);
	
    //proc_setas(NULL);
    as_deactivate();
    
    proc_setas(as_new);
	as_activate();
    //as_destroy(as_old);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
        //proc_setas(NULL);
        as_deactivate();
        proc_setas(as_old);
        as_activate();
        //as_destroy(as_new);
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	

	/* Define the user stack in the address space */
	result = as_define_stack(as_new, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
        //proc_setas(NULL);
        as_deactivate();
        proc_setas(as_old);
        as_activate();
        //as_destroy(as_new);
		return result;
	}

    vfs_close(v);
    *retval = 0;
	/* Warp to user mode. */
	enter_new_process( argc, (userptr_t)stackptr,NULL,stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;


}