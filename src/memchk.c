// Last Update:2019-06-25 17:03:54
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
#include <string.h>
#include <unistd.h>
#include "list.h"

#define BASIC() printf("[ %s %s() +%d ] ", __FILE__, __FUNCTION__, __LINE__ )
#define LOGI(args...) BASIC();printf(args)
#define LOGE(args...) LOGI(args)
//#define DEBUG
#ifdef DEBUG
#define DBG_TRACE_FUNC_IN() LOGI("---IN\n")
#define DBG_TRACE_FUNC_OUT() LOGI("---OUT\n")
#else
#define DBG_TRACE_FUNC_IN() 
#define DBG_TRACE_FUNC_OUT()
#endif

#define MAX_BLOCK_PER_CALLER (100)
#define MAX_CALLER_LIST ( 3000 )
#define GET_LR  __asm__ volatile("mov %0,lr" :"=r"(lr)::"lr")
#define SLEEP_TIME 10

typedef void *(*malloc_ptr) (size_t size);
typedef void (*free_ptr) (void *ptr);
typedef void *(*realloc_ptr) (void *ptr, size_t size);
typedef void *(*calloc_ptr) (size_t nmemb, size_t size);
typedef int (*pthread_create_ptr)(void *thread, const void *attr,
                          void *(*start_routine) (void *), void *arg);

typedef struct {
    void *addr;
    size_t size;
    struct list_head list;
} mem_block_t;

typedef struct {
    struct list_head block_list;
    int number;
    void *caller;
    struct list_head list;
    int total_size;
    int last_total_size;
    int last_diff;
    int increase_count;
} mem_caller_t;

static malloc_ptr real_malloc = NULL;
static free_ptr real_free = NULL;
static realloc_ptr real_realloc = NULL;
static calloc_ptr real_calloc = NULL;
static struct list_head mem_caller_list;
static int caller_index;
static pthread_mutex_t mutex;
static int total_blocks = 0;
static int total_size = 0;
static pthread_create_ptr real_pthread_create;

mem_caller_t *find_caller( void *caller );
static void *monitor_task( void *arg );

static void sig_hanlder( int signo )
{
    int i = 0, j = 0;
    mem_caller_t *mem_caller = NULL;
    mem_block_t *block = NULL;

    DBG_TRACE_FUNC_IN();
    if ( signo != SIGUSR1 ) {
        DBG_TRACE_FUNC_OUT();
        return;
    }

    if ( list_empty( &mem_caller_list) ) {
        return;
    }

    printf("dump all leak memory(%d callers %d blocks total : %d bytes ):\n", caller_index, total_blocks, total_size );
    list_for_each_entry( mem_caller, &mem_caller_list, list ) {
        if ( mem_caller  && mem_caller->number > 0 ) {
            printf("\tcaller : %p number leak: %d, total size : %d bytes\n",
                   mem_caller->caller, mem_caller->number, mem_caller->total_size );
            list_for_each_entry( block, &mem_caller->block_list, list ) {
                if ( block ) {
                    printf("\t\t%ld@%p\n", block->size, block->addr );
                }
            }
        }
    }
    DBG_TRACE_FUNC_OUT();
}

void record_block( void *ptr, size_t size, void *caller )
{
    mem_caller_t *mem_caller = NULL;
    mem_block_t *block = NULL;

    DBG_TRACE_FUNC_IN();
    pthread_mutex_lock( &mutex );
    if ( caller_index >= MAX_CALLER_LIST ) {
        LOGE("error, caller list full\n");
        pthread_mutex_unlock( &mutex );
        DBG_TRACE_FUNC_OUT();
        return;
    }

    mem_caller = find_caller( caller );
    if ( !mem_caller ) {
        mem_caller = (mem_caller_t *)real_malloc( sizeof(mem_caller_t) );
        if ( !mem_caller ) {
            pthread_mutex_unlock( &mutex );
            LOGE("malloc fail!!!\n");
            DBG_TRACE_FUNC_OUT();
            return;
        }
        memset( mem_caller, 0, sizeof(mem_caller_t) );
        list_add_tail( &mem_caller->list, &mem_caller_list );
        INIT_LIST_HEAD( &mem_caller->block_list );
        caller_index++;
    }
    mem_caller->caller = caller;
    block = (mem_block_t*)real_malloc( sizeof(mem_block_t) );
    if ( !block ) {
        LOGE("malloc fail!!!\n");
        pthread_mutex_unlock( &mutex );
        DBG_TRACE_FUNC_OUT();
        return;
    }
    memset( block, 0, sizeof(mem_block_t) );
    block->addr = ptr;
    block->size = size;
    mem_caller->total_size += block->size;
    total_size += block->size;
    mem_caller->number ++;
    total_blocks++;
    list_add_tail( &(block->list), &(mem_caller->block_list) );
    pthread_mutex_unlock( &mutex );
    DBG_TRACE_FUNC_OUT();
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
    DBG_TRACE_FUNC_IN();

    if ( real_malloc )
        ptr = real_malloc( size );

    record_block( ptr, size, (void *)lr );

    return ptr;
        
}

mem_caller_t *find_caller( void *caller )
{
    int i = 0;
    mem_caller_t *mem_caller = NULL;

    DBG_TRACE_FUNC_IN();

    if ( list_empty( &mem_caller_list ) ) {
        LOGE("list empty");
        return NULL;
    }
    list_for_each_entry( mem_caller, &mem_caller_list, list ) {
        if ( mem_caller && mem_caller->caller == caller ) {
            DBG_TRACE_FUNC_OUT();
            return mem_caller;
        }
    }

    DBG_TRACE_FUNC_OUT();
    return NULL;
}

void delete_block( void *ptr, void *caller )
{
    mem_caller_t *mem_caller = NULL;
    mem_block_t *block = NULL;

    DBG_TRACE_FUNC_IN();
    if ( !ptr ) {
        DBG_TRACE_FUNC_OUT();
        return;
    }

    pthread_mutex_lock( &mutex );
    list_for_each_entry( mem_caller, &mem_caller_list, list ) {
        if ( mem_caller ) {
            list_for_each_entry( block, &mem_caller->block_list, list ) {
                if ( block && block->addr == ptr ) {
                    list_del( &block->list );
                    mem_caller->total_size -= block->size;
                    total_size -= block->size;
                    mem_caller->number--;
                    real_free( block );
                    total_blocks--;
                    pthread_mutex_unlock( &mutex );
                    DBG_TRACE_FUNC_OUT();
                    return;
                }
            }
        }
    }
    LOGE("!!! caller_index = %d, error not found block, ptr = %p\n", caller_index, ptr );
    pthread_mutex_unlock( &mutex );
    DBG_TRACE_FUNC_OUT();

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

    DBG_TRACE_FUNC_IN();
    if ( real_calloc )
        ptr = real_calloc( nmemb, size );
    //record_block( ptr, size, (void *)lr );

    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    void *p = NULL;
    unsigned long lr;

    GET_LR;
    DBG_TRACE_FUNC_IN();

    if ( real_realloc )
        p = real_realloc( ptr, size );

    delete_block( ptr, (void*)lr );
    record_block( p, size, (void*)lr );

    return p;
}


static void *monitor_task( void *arg )
{
    mem_caller_t *mem_caller = NULL;
    int diff;

    for (;;) {
        pthread_mutex_lock( &mutex );
        if ( list_empty(&mem_caller_list) ) {
            pthread_mutex_unlock( &mutex );
            sleep( SLEEP_TIME);
            continue;
        }
        list_for_each_entry( mem_caller, &mem_caller_list, list ) {
            if ( mem_caller ) {
                if ( mem_caller->last_total_size ) {
                    diff = mem_caller->total_size - mem_caller->last_total_size;
                    if ( mem_caller->last_diff > 0  && diff > 0 ) {
                        if ( diff > mem_caller->last_diff ) {
                            LOGI("warning : caller %p increasing %d bytes increase count : %d\n",
                                 mem_caller->caller, diff, ++mem_caller->increase_count );
                        } 
                    } else {
                        mem_caller->last_diff = diff;
                    }
                } else {
                    mem_caller->last_total_size = mem_caller->total_size;
                }
            }
        }
        pthread_mutex_unlock( &mutex );
        sleep( SLEEP_TIME );
    }

    return NULL;
}


void __attribute__ ((constructor)) memchk_load()
{
    pthread_t thread;

    INIT_LIST_HEAD( &mem_caller_list );
    real_malloc = dlsym( RTLD_NEXT, "malloc" );
    real_calloc = dlsym( RTLD_NEXT, "calloc" );
    real_free   = dlsym( RTLD_NEXT, "free" );
    real_realloc = dlsym( RTLD_NEXT, "realloc" );
    real_pthread_create = dlsym( RTLD_NEXT, "pthread_create");

    INIT_LIST_HEAD( &mem_caller_list );

    signal( SIGUSR1, &sig_hanlder );

    pthread_mutex_init( &mutex, NULL );
    pthread_create( &thread, NULL, monitor_task, NULL );
    LOGI("PreLoad Init Success, BuildTime: %s %s\r\n", __DATE__, __TIME__ );
    LOGI("killall -USR1 app to show leak info\n");
}

