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


int main(){
    static char writebuf[41] = "Twiddle dee dee, Twiddle dum dum.......\n";
    static char readbuf[PATH_MAX];
    const char *file;
    file = "testfile";

    write_test(writebuf, file);
    printf("File written successfully from execv\n");
    read_test(writebuf, readbuf, file);
    printf("File read successfully from execv\n");

    return 0;
}