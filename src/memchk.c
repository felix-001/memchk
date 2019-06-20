// Last Update:2019-06-20 18:25:00
/**
 * @file memchk.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-06-19
 */

#include <stdio.h>
#include <unistd.h>
#define __USE_GNU
#include <dlfcn.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <malloc.h>
#include <pthread.h>
#include "memchk.h"

#define BASIC() printf("[ %s %s() +%d ] ", __FILE__, __FUNCTION__, __LINE__ )
#define LOGI(args...) BASIC();printf(args)
#define LOGE(args...) LOGI(args)

#define MAX_BLOCK_PER_CALLER (100)
#define MAX_CALLER_LIST (20000)
#define GET_LR  __asm__ volatile("mov %0,lr" :"=r"(lr)::"lr")

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

mem_caller_t *find_caller( void *caller );

static void sig_hanlder( int signo )
{
    int i = 0, j = 0;
    mem_caller_t *mem_caller = NULL;

    if ( signo != SIGUSR1 )
        return;

    printf("dump all leak memory:\n");
    for ( i=0; i<MAX_CALLER_LIST; i++ ) {
        if ( caller_list[i].number > 0 ) {
            printf("\tcaller : %p number leak: %d\n", caller_list[i].caller, caller_list[i].number );
            mem_caller = &caller_list[i]; 
            for ( j=0; j<MAX_BLOCK_PER_CALLER; j++ ) {
               if ( mem_caller->blocks[j].used ) {
                   printf("\t\t%ld@%p\n", mem_caller->blocks[j].size,  mem_caller->blocks[j].addr );
               }
            }
        }
    }
}

mem_block_t *get_empty_block( mem_caller_t *mem_caller )
{
    int i = 0;

    for ( i=0; i<MAX_BLOCK_PER_CALLER; i++ ) {
        if ( !(mem_caller->blocks[i].used) ) {
            return &(mem_caller->blocks[i]);
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
        pthread_mutex_unlock( &mutex );
        return;
    }
    if ( caller_index > 0 ) {
        mem_caller = find_caller( caller );
        if ( !mem_caller )
            mem_caller = &caller_list[caller_index++];
    } else {
        mem_caller = &caller_list[caller_index++];
    }
    mem_caller->caller = caller;
    block = get_empty_block( mem_caller );
    if ( !block ) {
        pthread_mutex_unlock( &mutex );
        return;
    }
    block->addr = ptr;
    block->size = size;
    block->used = 1;
    mem_caller->number ++;
    pthread_mutex_unlock( &mutex );
}

void dump( char *addr, int size )
{
    int i = 0;

    printf("dump of %p:\n", addr );
    for ( i=0; i<size; i++ ) {
        printf("0x%x ", *addr++ );
    }
    printf("\n");
}

void *malloc( size_t size )
{
    void *ptr = NULL;
    unsigned long lr;
    char *p = (char *)&ptr;
    int i = 0;

    GET_LR;

    if ( real_malloc )
        ptr = real_malloc( size );

    record_block( ptr, size, (void *)lr );

    return ptr;
        
}

mem_caller_t *find_caller( void *caller )
{
    int i = 0;

    for ( i=0; i<MAX_CALLER_LIST; i++ ) {
        if ( caller_list[i].caller == caller ) {
            return &caller_list[i];
        }
    }

    return NULL;
}

mem_block_t *find_block( mem_caller_t *mem_caller, void *ptr )
{
    int i = 0;

    for ( i=0; i<MAX_BLOCK_PER_CALLER; i++ ) {
        if ( (mem_caller->blocks[i].used) && ((mem_caller->blocks[i].addr) == ptr) ) {
            return &(mem_caller->blocks[i]);
        }
    }

    return NULL;
}

void delete_block( void *ptr, void *caller )
{
    mem_caller_t *mem_caller = NULL;
    mem_block_t *block = NULL;
    int i = 0;

    pthread_mutex_lock( &mutex );
    for ( i=0; i<MAX_CALLER_LIST; i++ ) {
        mem_caller = &caller_list[i];
        block = find_block( mem_caller, ptr );
        if ( !block ) {
            pthread_mutex_unlock( &mutex );
            continue;
        }
        block->used = 0;
        mem_caller->number--;
        break;
    }
    if ( i == MAX_CALLER_LIST ) {
        LOGE("can't find block\n");
    }
    pthread_mutex_unlock( &mutex );
}

void free(void *ptr)
{
    unsigned long lr;

    GET_LR;

    if ( real_free )
        real_free( ptr );

    delete_block( ptr, (void *)lr );
}


void *calloc(size_t nmemb, size_t size)
{
    void *ptr = NULL;
    unsigned long lr;

    GET_LR;

    if ( real_calloc )
        ptr = real_calloc( nmemb, size );
    record_block( ptr, size, (void *)lr );

    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    void *p = NULL;
    unsigned long lr;

    GET_LR;

    if ( real_realloc )
        p = real_realloc( ptr, size );

    delete_block( ptr, (void*)lr );
    record_block( p, size, (void*)lr );

    return p;
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

