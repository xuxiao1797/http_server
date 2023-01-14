#ifndef COMP2310_CACHE_H
#define COMP2310_CACHE_H

#include <stdlib.h>
#include "csapp.h"

typedef struct cache_block{
    char* uri;   //uri of cache
    char* data;  //data to cache
    pthread_rwlock_t rwlock;  //read/write lock
} cache_block;


void init_cache();
int read_cache(char* url, int fd);
void write_cache(char* url, char* data, int size);
void free_cache();

#endif
