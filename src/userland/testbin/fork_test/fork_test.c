#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h> 
#include <stdlib.h>




static void write_test(char *writebuf, const char *file) {

	int fd, rv;
	
	// OPEN A FILE IN WRITE MODE WITH THE O_CREAT (CREATE A FILE IF NOT PRESENT) AND O_TRUNC ("RESET" THE FILE WHEN
	// OPENED) FLAGS. NOT USING THE O_CREAT FLAG WILL CAUSE THE OPEN CALL TO FAIL IF THE FILE DOES NOT EXIST.
	// THE THIRD ARGUMENT IS THE mode ONE; WHICH INDICATES THE PERMISSIONS OF THE FILE. IT CAN BE IGNORED IN OS/161.
	fd = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (fd < 0) {
		err(1, "%s: open for write", file);
	}
	
	// WRITE THE CONTENTS OF THE WRITE BUFFER TO THE FILE
	rv = write(fd, writebuf, 40);
	if (rv < 0) {
		err(1, "%s: write", file);
	}

	// CLOSE THE FILE
	rv = close(fd);
	if (rv < 0) {
		err(1, "%s: close (1st time)", file);
	}

}
static void read_test(char *writebuf, char *readbuf, const char *file) {

	int fd, rv;

	// OPEN THE FILE IN READ MODE
	fd = open(file, O_RDONLY);
	if (fd < 0) {
		err(1, "%s: open for read", file);
	}

	// READ THE CONTENTS OF THE FILE INTO THE READ BUFFER
	rv = read(fd, readbuf, 40);
	if (rv < 0) {
		err(1, "%s: read", file);
	}

	// CLOSE THE FILE
	rv = close(fd);
	if (rv < 0) {
		err(1, "%s: close (2nd time)", file);
	}

	// ENSURE NULL TERMINATION TO THE READ BUFFER
	readbuf[40] = 0;

	// COMPARE THE ORIGINAL AND READ BUFFERS
	if (strcmp(readbuf, writebuf)) {
		errx(1, "Buffer data mismatch!");
	}

}



int main(void){
    printf("\nThis is a test program to test the fork() system call");
    printf("\nThe current process ID is: %d\n", getpid());

    static char writebuf[41] = "Twiddle dee dee, Twiddle dum dum.......\n";
    static char readbuf[PATH_MAX];
    const char *file;
    file = "testfile";

    int pid = fork();
    //printf("pid is: %d\n",pid);
    
    if (pid < 0) {
        err(1, "fork failed");
    }

    if (pid == 0) {
        printf("\nThis is the child process and the id is %d",getpid()); 
       
        printf("\nTEST 1 - WRITE TO A FILE in chile_proc\n");
        write_test(writebuf, file);
        printf("write(): PASSED\n");
        exit(0);
    } 
    if (pid>0) {
        int status;
        int x= waitpid(pid, &status,0);
        printf("\nChild process exited with pid %d", x);
        printf("\nChild process exited with status %d", status);
        printf("\nThis is the parent process and the id is %d\n",getpid());
        printf("\nTEST 2 - READ FROM A FILE in parent proc\n");
	    read_test(writebuf, readbuf, file);
	    printf("read(): PASSED\n");
        const char *args[] = {NULL} ;
        int argc=0;
        while (args[argc] != NULL) {
            argc++;
        }
        char *argv[argc+1];
        for (int i = 0; i < argc; i++) {
            argv[i] = (char *)args[i];
        }
        argv[argc] = NULL;
        printf("\nExecv is called\n");
        execv("testbin/short_filetest", argv);
    }
    return 0;
}