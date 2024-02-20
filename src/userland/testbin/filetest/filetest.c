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

/*
 * filetest.c
 *
 * 	Tests the filesystem by opening, writing to and reading from a
 * 	user specified file.
 *
 * This should run (on SFS) even before the file system assignment is started.
 * It should also continue to work once said assignment is complete.
 * It will not run fully on emufs, because emufs does not support remove().
 */

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

static void lseek_test(char *writebuf, char *readbuf, const char *file) {

	int fd, rv;
	off_t pos;

	// OPEN IN WRITE-MODE WITH O_TRUNC FLAG, SO THAT THE FILE IS RESET 
	fd = open(file, O_WRONLY|O_TRUNC);
	if (fd < 0) {
		err(1, "%s: open for write", file);
	}

	// WRITE TEN TIMES THE WRITE BUFFER AND PUT THE EXPECTED OUTCOME "OS-PROJECT" AT LINE 1, 7 AND 13  
	for (int i=0; i<13; i++) {
		if (i == 6) {
			rv = write(fd, "OS-PROJECT-SEEK_SET\n", 20);
		} else if (i == 0) {
			rv = write(fd, "OS-PROJECT-SEEK_CUR\n", 20);
		} else if (i == 12) {
			rv = write(fd, "OS-PROJECT-SEEK_END\n", 20);
		} else {
			rv = write(fd, writebuf, 40);
		}
		if (rv < 0) {
			err(1, "%s: write", file);
		}
	}

	// CLOSE THE FILE
	rv = close(fd);
	if (rv < 0) {
		err(1, "%s: close (2nd time)", file);
	}

	// OPEN THE FILE IN READ-MODE
	fd = open(file, O_RDONLY);
	if (fd < 0) {
		err(1, "%s: open for read", file);
	}
	
	// SEEK TO THE BEGINNING OF THE 7TH LINE THROUGH THE SEEK_SET FLAG.
	// THE OFFSET IS CALCULATED AS FOLLOWS:
	// 	- 20 BYTES FOR FIRST LINE ("OS-PROJECT-SEEK_CUR\n")
	// 	- 40 BYTES FOR EACH OF THE 5 LINES IN BETWEEN ("Twiddle dee dee, Twiddle dum dum.......\n")
	printf("1) SEEK_SET\n");
	pos = lseek(fd, 20+(40*5), SEEK_SET);
	if (pos < 0) {
		err(1, "%s: lseek", file);
	}
	
	// READ THE 7TH LINE
	rv = read(fd, readbuf, 20);
	if (rv < 0) {
		err(1, "%s: read", file);
	}
	
	// ENSURE NULL TERMINATION
	readbuf[20] = 0;
	// printf("readbuf: %s\n", readbuf);
	
	// COMPARE THE READ BUFFER WITH THE EXPECTED OUTCOME
	if (strcmp(readbuf, "OS-PROJECT-SEEK_SET\n")) {
		errx(1, "BUFFER DATA MISMATCH WITH SEEK_SET!");
	}

	printf ("lseek() with SEEK_SET: PASSED\n");

	// SEEK TO THE BEGINNING OF THE FILE THROUGH THE SEEK_CUR FLAG.
	// NOW THE ACTUAL OFFSET OF THE FILE IS THE OFFSET OF THE PREVIOUS POINT (20+(40*5) = 220) PLUS THE OFFSET 
	// DUE TO THE LINE READ (20).
	// THE OFFSET IS THEN CALCULATED AS FOLLOWS:
	// 	- -20 BYTES FOR THE 7TH LINE ("OS-PROJECT-SEEK_SET\n")
	// 	- -40 BYTES FOR EACH OF THE 5 LINES IN BETWEEN ("Twiddle dee dee, Twiddle dum dum.......\n")
	// 	- -20 BYTES FOR THE FIRST LINE ("OS-PROJECT-SEEK_CUR\n")
	printf("2) SEEK_CUR\n");
	pos = lseek(fd, -(2*20)-(40*5), SEEK_CUR);
	if (pos < 0) {
		err(1, "%s: lseek", file);
	}

	// READ THE FIRST LINE
	rv = read(fd, readbuf, 20);
	if (rv < 0) {
		err(1, "%s: read", file);
	}

	// ENSURE NULL TERMINATION
	readbuf[20] = 0;
	// printf("readbuf: %s\n", readbuf);
	
	// COMPARE THE READ BUFFER WITH THE EXPECTED OUTCOME
	if (strcmp(readbuf, "OS-PROJECT-SEEK_CUR\n")) {
		errx(1, "BUFFER DATA MISMATCH WITH SEEK_CUR!");
	}

	printf ("lseek() with SEEK_CUR: PASSED\n");

	// SEEK TO THE BEGINNING OF THE 13TH LINE THROUGH THE SEEK_END FLAG.
	// THE OFFSET IS EQUAL TO THE SIZE OF THE LAST LINE TO BE READ, HENCE -20
	printf("3) SEEK_END\n");	
	pos = lseek(fd, -20, SEEK_END);
	if (pos < 0) {
		err(1, "%s: lseek", file);
	}

	// READ THE LAST LINE
	rv = read(fd, readbuf, 20);
	if (rv < 0) {
		err(1, "%s: read", file);
	}

	// ENSURE NULL TERMINATION
	readbuf[20] = 0;
	// printf("readbuf: %s\n", readbuf);

	// COMPARE THE READ BUFFER WITH THE EXPECTED OUTCOME
	if (strcmp(readbuf, "OS-PROJECT-SEEK_END\n")) {
		errx(1, "BUFFER DATA MISMATCH WITH SEEK_END!");
	}
	
	// CLOSE THE FILE
	rv = close(fd);
	if (rv < 0) {
		err(1, "%s: close (3rd time)", file);
	}
	
	printf ("lseek() with SEEK_END: PASSED\n");

}

static void dup2_test(char *writebuf, char *readbuf, const char *file) {

	int fd, rv;
	off_t pos;

	// OPEN THE FILE IN READ/WRITE-MODE AND RESET IT (O_TRUNC).
	fd = open(file, O_RDWR|O_TRUNC);
	if (fd < 0) {
		err(1, "%s: open for read", file);
	}

	// DUPLICATE STDOUT TO THE FILE DESCRIPTOR AT POSITION 10
	rv = dup2(STDOUT_FILENO, 10);
	if (rv < 0) {
		err(1, "%s: dup2", file);
	}

	// DUPLICATE THE OPENED FILE TO STDOUT
	rv = dup2(fd, STDOUT_FILENO);
	if (rv < 0) {
		err(1, "%s: dup2", file);
	}

	// WRITE SOMETHING TO THE OPENED FILE THROUGH printf, THUS EXPLOITING STDOUT
	printf("OS-PROJECT-dup2()\n");

	// RESTORE STDOUT. 
	rv = dup2(10, STDOUT_FILENO);
	if (rv < 0) {
		err(1, "%s: dup2", file);
	}
	
	// SINCE dup2() MAKES ALL THE COPIES SHARE THE SAME SEEK POINTER, THE OFFSET OF THE FILE IS NOW AT THE END.
	// SET THE OFFSET OF THE FILE TO THE BEGINNING OF THE WRITTEN SENTENCE
	pos = lseek(fd, 0, SEEK_SET);
	if (pos < 0) {
		err(1, "%s: lseek", file);
	}

	// READ THE CONTENTS OF THE OPENED FILE
	rv = read(fd, readbuf, 18);
	if (rv < 0) {
		err(1, "%s: read", file);
	}

	// ENSURE NULL TERMINATION
	readbuf[18] = 0;

	// COMPARE THE READ BUFFER WITH THE EXPECTED OUTCOME
	if (strcmp(readbuf, "OS-PROJECT-dup2()\n")) {
		errx(1, "BUFFER DATA MISMATCH WITH dup2()!");
	}

	// DUPLICATE THE OPENED FILE TO THE FILE DESCRIPTOR AT POSITION 3.
	// SINCE oldfd = newfd = fd, dup2() WILL DO NOTHING AND RETURN fd.
	rv = dup2(fd, 3);
	if (rv != fd) {
		err(1, "%s: dup2", file);
	}

	// DUPLICATE THE OPENED FILE TO THE FILE DESCRIPTOR AT POSITION 4.
	rv = dup2(fd, 4);
	if (rv < 0) {
		err(1, "%s: dup2", file);
	}

	// WRITE THE CONTENTS OF THE WRITE BUFFER TO THE FILE THROUGH THE FILE DESCRIPTOR AT POSITION 4
	rv = write(4, writebuf, 40);
	if (rv < 0) {
		err(1, "%s: write", file);
	}

	// SET THE OFFSET OF THE FILE POINTED BY THE FILE DESCRIPTOR AT POSITION 4 TO THE BEGINNING OF THE 
	// WRITTEN SENTENCE
	pos = lseek(fd, -40, SEEK_END);
	if (pos < 0) {
		err(1, "%s: lseek", file);
	}

	// READ THE CONTENTS OF THE FILE THROUGH THE FILE DESCRIPTOR AT POSITION 3
	rv = read(3, readbuf, 40);
	if (rv < 0) {
		err(1, "%s: read", file);
	}

	// ENSURE NULL TERMINATION
	readbuf[40] = 0;

	// COMPARE THE READ BUFFER WITH THE EXPECTED OUTCOME
	if (strcmp(readbuf, writebuf)) {
		errx(1, "BUFFER DATA MISMATCH WITH dup2()!");
	}

	// CLOSE THE FILE AT POSITION 4
	rv = close(4);
	if (rv < 0) {
		err(1, "%s: close (4th time)", file);
	}

	// CLOSE THE FILE AT POSITION 3
	rv = close(fd);
	if (rv < 0) {
		err(1, "%s: close (5th time)", file);
	}

}

static void chdir_test(const char *file) {

	char readbuf[2001], checkbuf[2001];
	int rv, fd;
	char chdir_name[] = "include";

	// CHANGE TO THE include DIRECTORY
	rv = chdir(chdir_name);
	if (rv < 0) {
		err(1, "%s: chdir", chdir_name);
	}

	// OPEN THE err.h FILE IN READ MODE
	fd = open("err.h", O_RDONLY);
	if (fd < 0) {
		err(1, "%s: open for read", file);
	}

	// READ THE FIRST 2000 BYTES OF THE err.h FILE
	rv = read(fd, readbuf, 2000);
	if (rv < 0) {
		err(1, "%s: read", file);
	}

	// ENSURE NULL TERMINATION
	readbuf[2000] = 0;

	// CLOSE THE FILE
	rv = close(fd);
	if (rv < 0) {
		err(1, "%s: close (6th time)", file);
	}

	// RESTORE THE CURRENT DIRECTORY
	rv = chdir("..");
	if (rv < 0) {
		err(1, "%s: chdir", "..");
	}

	// OPEN THE err.h FILE IN READ MODE WITHOUT chdir()
	fd = open("include/err.h", O_RDONLY);
	if (fd < 0) {
		err(1, "%s: open for read", file);
	}

	// READ THE FIRST 2000 BYTES OF THE err.h FILE
	rv = read(fd, checkbuf, 2000);
	if (rv < 0) {
		err(1, "%s: read", file);
	}

	// ENSURE NULL TERMINATION
	checkbuf[2000] = 0;

	// CLOSE THE FILE
	rv = close(fd);
	if (rv < 0) {
		err(1, "%s: close (7th time)", file);
	}

	// COMPARE THE TWO READ BUFFERS
	if (strcmp(readbuf, checkbuf)) {
		errx(1, "BUFFER DATA MISMATCH WITH chdir()!");
	}

}

static void getcwd_test() {

	// THE ONLY WAY TO TEST __getcwd() IS TO CHECK THE CURRENT WORKING DIRECTORY, GIVEN THAT
	// THE FUNCTION DOES NOT WORK IN OTHER DIRECTORIES BUT THE root ONE.
	// AS A PROOF, ISSUING cd TO WHATEVER OTHER FOLDER INTO THE MENU AND CALLING pwd CAUSES
	// THE SYSTEM TO CRASH WITH ERROR "Menu command failed: Function not implemented".

	char buf[PATH_MAX];
	int rv;

	// GET THE CURRENT WORKING DIRECTORY
	rv = __getcwd(buf, PATH_MAX);
	if (rv < 0) {
		err(1, "__getcwd");
	}

	// ENSURE NULL TERMINATION
	buf[rv] = 0;

	// COMPARE THE READ BUFFER WITH THE EXPECTED OUTCOME
	if (strcmp(buf, "emu0:")) {
		errx(1, "BUFFER DATA MISMATCH WITH __getcwd()!");
	}

}

int main(int argc, char *argv[]) {

	static char writebuf[41] = "Twiddle dee dee, Twiddle dum dum.......\n";
	static char readbuf[PATH_MAX];

	const char *file;

	if (argc == 0) {
		warnx("No arguments - running on \"testfile\"");
		file = "testfile";
	}
	else if (argc == 2) {
		file = argv[1];
	}
	else {
		errx(1, "Usage: filetest <filename>");
	}

	/* TEST 1 - WRITE TO A FILE */
	printf("\nTEST 1 - WRITE TO A FILE\n");
	write_test(writebuf, file);
	printf("write(): PASSED\n");

	/*TEST 2 - READ FROM A FILE */
	printf("\nTEST 2 - READ FROM A FILE\n");
	read_test(writebuf, readbuf, file);
	printf("read(): PASSED\n");

	/* TEST 3 - TEST lseek() */
	printf("\nTEST 3 - TEST lseek()\n");
	lseek_test(writebuf, readbuf, file);
	printf("lseek(): PASSED\n");

	/* TEST 4 - TEST dup2() */
	printf("\nTEST 4 - TEST dup2()\n");
	dup2_test(writebuf, readbuf, file);
	printf ("dup2(): PASSED\n");

	/* TEST 5 - TEST chdir() */
	printf("\nTEST 5 - TEST chdir()\n");
	chdir_test(file);
	printf("chdir(): PASSED\n");

	/* TEST 6 - TEST __getcwd() */
	printf("\nTEST 6 - TEST __getcwd()\n");
	getcwd_test();
	printf("__getcwd(): PASSED\n");

	printf("\nFILETEST PASSED.\n\n");

	return 0;

}
