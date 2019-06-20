// Last Update:2019-06-20 18:28:40
/**
 * @file sample.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-06-19
 */

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>

#define BASIC() printf("[ %s %s() +%d ] ", __FILE__, __FUNCTION__, __LINE__ )
#define LOGI(args...) BASIC();printf(args)
#define LOGE(args...) LOGI(args)

void *my_malloc( size_t size )
{
    void *ptr = NULL;

    ptr = malloc( size );

    return ptr;
}

void *new( size_t size)
{
    void *ptr = NULL;

    ptr = malloc( size );

    return ptr;
}

void my_free( void *ptr )
{
    free( ptr );
}

void delete( void *ptr )
{
    free( ptr );
}

#define MAX_LEN 10

int main()
{
    int i = 0;
    void *list[MAX_LEN] = { 0 };
    void *mem[MAX_LEN] = { 0 };

    LOGI("running...\n");
    LOGI("my_malloc addr : %p\n", my_malloc );
    LOGI("my_free addr : %p\n", my_free );
    LOGI("new : %p\n", new );
    LOGI("delete : %p\n", delete );

    for ( i=0; i<MAX_LEN; i++ ) {
        list[i] = my_malloc( 50 );
        LOGI("list[%d] = %p\n", i, list[i] );
        mem[i] = new( 20 );
        LOGI("mem[%d] = %p\n", i, mem[i] );
    }

    for ( i=0; i<MAX_LEN-5; i++ ) {
        my_free(list[i] );
        delete( mem[i] );
    }

    pause();
    return 0;
}

