#ifndef _PROC_TABLE_H_
#define _PROC_TABLE_H_

#include <types.h>
#include <proc.h>
#include <kern/errno.h>
#include <lib.h>
#include <current.h>

#define MAX_PROC_NUM 200
extern struct proc_table *ptable;


struct proc_table {
    struct proc *process[MAX_PROC_NUM];    // array of pointers to processes
    pid_t next_pid;
    int num_processes;  // number of active processes
};


#endif /* _PROC_TABLE_H_ */