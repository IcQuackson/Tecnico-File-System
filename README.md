# Tecnico File System

The ultimate goal of the project is to develop a user-level file system (File System, FS) that stores its contents in primary memory. The file system is called TecnicoFS.

# Project Description

The ultimate goal of the project is to develop a user-level file system (File System, FS) that stores its contents in primary memory. The file system is called TecnicoFS.

## Exercise 1

The goal of this exercise is to create an initial parallel version of the TecnicoFS server. This version does not support interaction with client processes, but instead executes sequences of calls loaded from an input file. For debugging purposes, the server will display the total execution time on stdout and write the final directory content to an output file.

## Exercise 2

The goal of this exercise is to extend the solution developed in the first exercise with two important optimizations and support for a new operation. The requirements are:

- Fine synchronization of inodes to allow greater parallelism between commands that do not conflict.
- Incremental execution of commands while the input file is loaded in parallel.
- Support for a new operation (not specified in this PDF).
- Use of mutexes, read-write locks, and condition variables for synchronization.
- Avoidance of busy waiting as much as possible.
