#ifndef FS_H
#define FS_H

#include "state.h"


typedef struct inode_LockTable{
    int inode_numbers[INODE_TABLE_SIZE];
	int counter;
} LockTable;

void init_fs();
void destroy_fs();
int is_dir_empty(DirEntry *dirEntries);
int create(char *name, type nodeType);
int delete(char *name);
int lookup(char *name, int operation, LockTable *table);
void print_tecnicofs_tree(FILE *fp);
void lockAndAddToArray(pthread_rwlock_t *lock, LockTable *table, int inumber, int operation);
void unlockFromArray(LockTable *table);


#endif /* FS_H */
