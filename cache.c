#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"
#include "cache.h"

#define MAX_CACHE_SIZE 1049000
#define CACHE_OBJS_COUNT 10
#define MAX_OBJECT_SIZE 102400

cache_block *caches[CACHE_OBJS_COUNT];

void init_cache()
{  
    //Allocate the cache space
   for (int i = 0; i < CACHE_OBJS_COUNT; i++) {
        caches[i] = (cache_block *)malloc(CACHE_OBJS_COUNT * sizeof(cache_block));
        caches[i]->uri = malloc(sizeof(char) * MAXLINE);
        strcpy(caches[i]->uri,"");
        caches[i]->data = malloc(sizeof(char) * MAX_OBJECT_SIZE);
        memset(caches[i]->data,0,MAX_OBJECT_SIZE);
        pthread_rwlock_init(&caches[i]->rwlock,NULL);
    }
}

int read_cache(char *uri,int fd){
    printf("read cache %s \n", uri);
    cache_block *block;
    int cache_num = 0;
    //find the cache block with the uri
    for (; cache_num < CACHE_OBJS_COUNT; cache_num++) {
        block  = caches[cache_num];
        if(strcmp(uri,block->uri) == 0) break;  
    }
    
    //if all the block has no correspond uri, then return 0
    if(cache_num >= CACHE_OBJS_COUNT){
        return 0;
    }
    
    //read lock
    pthread_rwlock_rdlock(&block->rwlock);
    if(strcmp(uri,block->uri) != 0){
        pthread_rwlock_unlock(&block->rwlock);
        return 0;
    }
    pthread_rwlock_unlock(&block->rwlock);
    if (!pthread_rwlock_trywrlock(&block->rwlock)) {
        pthread_rwlock_unlock(&block->rwlock); 
    }
    pthread_rwlock_rdlock(&block->rwlock);

    //get the cache and write into client
    Rio_writen(fd,block->data,sizeof(block->data));
    pthread_rwlock_unlock(&block->rwlock);
    return 1;
}

void write_cache(char *uri, char *data, int size){
    int cache_num = 0;
    for (;cache_num < CACHE_OBJS_COUNT && size > MAX_OBJECT_SIZE; cache_num++);

    cache_block *block = caches[cache_num];
    
    //add write lock
    pthread_rwlock_wrlock(&block->rwlock);
    //write into cache
    memcpy(block->uri,uri,MAXLINE);
    memcpy(block->data,data,size);
    pthread_rwlock_unlock(&block->rwlock);

}

void free_cache() {
    //free the memory of cache clock
    for (int i = 0; i < CACHE_OBJS_COUNT; i++) {
        free(caches[i]->uri);
        free(caches[i]->data);
        pthread_rwlock_destroy(&caches[i]->rwlock);
        free(caches[i]);
    }
}

