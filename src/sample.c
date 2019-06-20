// Last Update:2019-06-20 14:34:29
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

int main()
{
    int i = 0;
    void *list[100] = { 0 };
    void *mem[100] = { 0 };

    LOGI("running...\n");

    for ( i=0; i<100; i++ ) {
        list[i] = my_malloc( 50 );
        mem[i] = new( 20 );
    }

    for ( i=0; i<80; i++ ) {
        my_free(list[i] );
        delete( mem[i] );
    }

    pause();
    return 0;
}

