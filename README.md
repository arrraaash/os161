# OS161 System Calls Implementation

This repository contains an implementation of system calls for the OS161 operating system. The project aims to extend the OS161 kernel by adding various system call functionalities, which are essential for user-space programs to interact with the operating system.

## Features

- **System Call Implementations**:
  - `fork()`
  - `execv()`
  - `waitpid()`
  - `getpid()`
  - `exit()`
  - And more...

- **User-space and Kernel-space Interaction**: Demonstrates the mechanism of system calls and how user-space programs invoke kernel services.

## File Structure

- `src/kern/syscall/`: Directory containing the system call implementations.
- `src/kern/include/syscall.h`: Header file for system call definitions.
- `src/userland/testbin/`: User-space programs for testing the implemented system calls.
