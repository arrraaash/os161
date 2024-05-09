/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <kern/types.h>
#include <file_syscalls.h>
#include <copyinout.h>
#include <proc_syscalls.h>


/*
 * System call dispatcher.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception-*.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. 64-bit arguments are passed in *aligned*
 * pairs of registers, that is, either a0/a1 or a2/a3. This means that
 * if the first argument is 32-bit and the second is 64-bit, a1 is
 * unused.
 *
 * This much is the same as the calling conventions for ordinary
 * function calls. In addition, the system call number is passed in
 * the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, or v0 and v1 if 64-bit. This is also like an ordinary
 * function call, and additionally the a3 register is also set to 0 to
 * indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/user/lib/libc/arch/mips/syscalls-mips.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * If you run out of registers (which happens quickly with 64-bit
 * values) further arguments must be fetched from the user-level
 * stack, starting at sp+16 to skip over the slots for the
 * registerized values, with copyin().
 */
void
syscall(struct trapframe *tf)
{
	int callno;
	int err;
	int whence;
	int high_seek_pos;
	int low_seek_pos;
	off_t seek_pos;

	// USE TWO VARIABLES TO STORE THE RETURNED VALUES. 
	// FOR ALL THE SYSTEM CALLS THAT RETURN A SINGLE 32-BIT VALUE, THE VALUE WILL BE 
	// STORED IN retval_low AND THE VALUE OF retval_high WILL BE 0.
	// FOR THE SYSTEM CALLS THAT RETURN A 64-BIT VALUE (e.g. SYS_lseek()), THE VALUE WILL BE STORED IN
	// retval_low AND retval_high.
	int32_t retval_low, retval_high;
	
	KASSERT(curthread != NULL);
	KASSERT(curthread->t_curspl == 0);
	KASSERT(curthread->t_iplhigh_count == 0);

	callno = tf->tf_v0;

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values,
	 * like write.
	 */

	retval_low 		= 0;
	retval_high 	= 0;
	err 			= 0;
	whence 			= 0;
	seek_pos 		= 0;
	high_seek_pos	= 0;
	low_seek_pos 	= 0;

	/* kprintf("\nsyscall.c:\n");
	kprintf("callno (tf->tf_v0): %d\n", tf->tf_v0);
	kprintf("tf->tf_a0: %d\n", tf->tf_a0);
	kprintf("tf->tf_a1: %d\n", tf->tf_a1);
	kprintf("tf->tf_a2: %d\n", tf->tf_a2); */

	switch (callno) {
	    
		case SYS_reboot:
			err = sys_reboot(tf->tf_a0);
			break;

	    case SYS___time:
			err = sys___time((userptr_t)tf->tf_a0, (userptr_t)tf->tf_a1);
			break;

	    /* Add stuff here */

		case SYS_open:
			// int sys_open(const char *filename, int flags, int *retval)
			err 		= sys_open((char *)tf->tf_a0, tf->tf_a1, &retval_high);
			retval_low  = 0;
			break;

		case SYS_close:
			// int sys_close(int fd)
			err = sys_close((int) tf->tf_a0);
			break;

		case SYS_read:
			// int sys_read(int fd, void *buf, size_t buflen, int *retval)
			err 		= sys_read((int) tf->tf_a0, (void *)tf->tf_a1, (size_t) tf->tf_a2, &retval_high);
			retval_low  = 0;
			break;
		
		case SYS_write:
			// int sys_write(int fd, const void *buf, size_t nbytes, int *retval)
			err 		= sys_write((int) tf->tf_a0, (void *)tf->tf_a1, (size_t) tf->tf_a2, &retval_high);
			retval_low  = 0;
			break;

		case SYS_lseek:
			// AS WRITTEN ABOVE, 64-BIT ARGUMENTS ARE PASSED IN ALIGNED PAIRS OF REGISTERS.
			// THIS MEANS THAT IF THE FIRST ARGUMENT, WHICH IS 32-BIT, WILL GO INTO THE a0 REGISTER,
			// AND THE SECOND ARGUMENT, WHICH IS 64-BIT SINCE IT IS AN off_t TYPE, WILL GO INTO THE a2-a3 REGISTERS
			// FOLLOWING THE BIG ENDIAN CONVENTION, WHICH LEADS TO THE FOLLOWING:
			// 	- THE LOWER 32-BITS IN a2
			// 	- THE HIGHER 32-BITS IN a3
			// THE a1 REGISTER WILL BE UNUSED AND FURTHER ARGUMENTS (whence) WILL BE FETCHED FROM THE STACK USING copyin()
			// AND STARTING AT sp+16 TO SKIP OVER THE SLOTS FOR THE REGISTERIZED VALUES.
			// int sys_lseek(int fd, off_t pos, int whence, int *retval_high, int *retval_low)
			copyin((const_userptr_t)tf->tf_sp+16, &whence, sizeof(int));
			high_seek_pos	= tf->tf_a2;
			low_seek_pos 	= tf->tf_a3;
			seek_pos 		= ((off_t)high_seek_pos << 32) | low_seek_pos;
			err 			= sys_lseek((int) tf->tf_a0, seek_pos, whence, &seek_pos);
			retval_high 	= seek_pos >> 32;
			retval_low 		= seek_pos & 0xFFFFFFFF;
			break;

		case SYS_dup2:
			// int sys_dup2(int oldfd, int newfd, int *retval)
			err 		= sys_dup2((int) tf->tf_a0, (int) tf->tf_a1, &retval_high);
			retval_low  = 0;
			break;

		case SYS_chdir:
			// int sys_chdir(const char *pathname)
			err = sys_chdir((char *)tf->tf_a0);
			break;
		
		case SYS___getcwd:
			// int sys___getcwd(char *buf, size_t buflen, int *retval)
			err 		= sys___getcwd((char *)tf->tf_a0, tf->tf_a1, &retval_high);
			retval_low  = 0;
			break;

		/* case SYS_remove:
			// int sys_remove(const char *pathname)
			err = sys_remove((char *)tf->tf_a0);
			break; */
		case SYS_getpid:
			err = sys_getpid(&retval_high);
			retval_low = 0;
			break;
		
		case SYS_fork:  
			err = sys_fork(tf, &retval_high);
			retval_low = 0;
			break;
			
		case SYS__exit:
			err = 0;
			sys_exit((int) tf->tf_a0);
			break;

		case SYS_waitpid:
			err = sys_waitpid((pid_t)tf->tf_a0,(const struct __userptr *) tf->tf_a1, (pid_t *)&retval_high, (uint32_t) tf->tf_a2);
			retval_low = 0;
			break;

		case SYS_execv:
			err = sys_execv((const char *)tf->tf_a0, (char **)tf->tf_a1, &retval_high);
			retval_low = 0;
			break;

	    default:
			kprintf("Unknown syscall %d\n", callno);
			err = ENOSYS;
			break;
	
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		// BEING THE ARCHITECTURE BIG ENDIAN, THE LOWER 32-BITS OF THE 64-BIT VALUE ARE STORED IN tf->tf_v0
		// AND THE HIGHER 32-BITS ARE STORED IN tf->tf_v1.
		tf->tf_v0 = retval_high;
		tf->tf_v1 = retval_low;
		tf->tf_a3 = 0;      /* signal no error */
	}

	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */

	tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
	KASSERT(curthread->t_curspl == 0);
	/* ...or leak any spinlocks */
	KASSERT(curthread->t_iplhigh_count == 0);
}

/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
/* void
enter_forked_process(struct trapframe *tf,unsigned long data2)
{	
	(void) data2;
	tf->tf_v0 = 0;
	tf->tf_a3 = 0;
	tf->tf_epc += 4;
	struct trapframe ntf=*tf;
	//kfree(tf);
	mips_usermode(&ntf);
} */

/* void
enter_forked_process(struct trapframe *tf)
{
	(void)tf;
} */

