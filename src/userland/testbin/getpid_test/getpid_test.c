#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h> 
#include <stdlib.h>


int main(void){
    printf("This is a test program to test the getpid() system call\n");
    printf("The process ID of the current process is: %d\n", getpid());
    printf("getpid() system call test passed\n");
    exit(0);
}