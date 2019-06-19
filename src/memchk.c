// Last Update:2019-06-19 21:05:45
/**
 * @file memchk.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-06-19
 */

#define __USE_GNU
#include <dlfcn.h>
#include <sys/syscall.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <pthread.h>
#include "memchk.h"

#define BASIC() printf("[ %s %s() +%d ] ", __FILE__, __FUNCTION__, __LINE__ )
#define LOGI(args...) BASIC();printf(args)
#define LOGE(args...) LOGI(args)

#define MAX_BLOCK_PER_CALLER (100)
#define MAX_CALLER_LIST (20000)
#define GET_LR  __asm__ volatile("mov %0,lr" :"=r"(lr)::"lr");

typedef void *(*malloc_ptr) (size_t size);
typedef void (*free_ptr) (void *ptr);
typedef void *(*realloc_ptr) (void *ptr, size_t size);
typedef void *(*calloc_ptr) (size_t nmemb, size_t size);

typedef struct {
    void *addr;
    size_t size;
    int used;
} mem_block_t;

typedef struct {
    mem_block_t blocks[MAX_BLOCK_PER_CALLER];
    int number;
    void *caller;
} mem_caller_t;

static malloc_ptr real_malloc = NULL;
static free_ptr real_free = NULL;
static realloc_ptr real_realloc = NULL;
static calloc_ptr real_calloc = NULL;
static mem_caller_t caller_list[MAX_CALLER_LIST];
static int caller_index;
static pthread_mutex_t mutex;

static void sig_hanlder( int signo )
{
}

mem_block_t *get_empty_block( mem_caller_t *mem_caller )
{
    int i = 0;

    for ( i=0; i<MAX_BLOCK_PER_CALLER; i++ ) {
        if ( !mem_caller->blocks[i].used ) {
            return &mem_caller->blocks[i];
        }
    }

    LOGE("error, block list full\n");
    return NULL;
}

void record_block( void *ptr, size_t size, void *caller )
{
    mem_caller_t *mem_caller = NULL;
    mem_block_t *block = NULL;

    pthread_mutex_lock( &mutex );
    if ( caller_index >= MAX_CALLER_LIST ) {
        LOGE("error, caller list full\n");
        return;
    }
    mem_caller = &caller_list[caller_index++];
    block = get_empty_block( mem_caller );
    if ( !block )
        return;
    block->addr = ptr;
    block->size = size;
    block->used = 1;
    mem_caller->number ++;
    pthread_mutex_unlock( &mutex );
}


void *malloc( size_t size )
{
    void *ptr = NULL;
    unsigned long lr;

    GET_LR;

    if ( real_malloc )
        ptr = real_malloc( size );

    record_block( ptr, size, lr );

    return ptr;
        
}

mem_caller_t *find_caller( void *caller )
{
    int i = 0;

    for ( i=0; i<MAX_CALLER_LIST; i++ ) {
        if ( caller_list[i].caler == caller ) {
            return &caller_list[i];
        }
    }

    LOGE("caller not found\n");
    return NULL;
}

mem_block_t *find_block( mem_caller_t *mem_caller, void *ptr )
{
    int i = 0;

    for ( i=0; i<MAX_BLOCK_PER_CALLER; i++ ) {
        if ( mem_caller->blocks[i].addr == ptr ) {
            return &mem_caller->blocks[i];
        }
    }

    LOGE("error, block not found\n");
    return NULL;
}

void delete_block( void *ptr, void *caller )
{
    mem_caller_t *mem_caller = NULL;
    mem_block_t *block = NULL;

    mem_caller = find_caller( caller );
    if ( !mem_caller )
        return;
    block = find_block( mem_caller, ptr );
    if ( !block )
        return;
    block->used = 0;
}

void free(void *ptr)
{
    unsigned long lr;

    GET_LR;

    if ( real_free )
        real_free( ptr );

    delete_block( ptr, lr );
}

void __attribute__ ((constructor)) memchk_load()
{
    real_malloc = dlsym( RTLD_NEXT, "malloc" );
    real_calloc = dlsym( RTLD_NEXT, "calloc" );
    real_free   = dlsym( RTLD_NEXT, "free" );
    real_realloc = dlsym( RTLD_NEXT, "realloc" );

    signal( SIGUSR1, &sig_hanlder );

    pthread_mutex_init( &mutex, NULL );
    LOGI("PreLoad Init Success, BuildTime: %s %s\r\n", __DATE__, __TIME__ );
    LOGI("killall -USR1 app to show leak info\n");
}
