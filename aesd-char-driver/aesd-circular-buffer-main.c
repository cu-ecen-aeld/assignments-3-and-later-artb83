//
// Created by artb on 1/26/26.
//

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "aesd-circular-buffer.h"

void read_buffer(struct aesd_circular_buffer* circBuffer);
void write_buffer_entries(struct aesd_circular_buffer* circBuffer, int nEntries);
void releaseBuffer(struct aesd_circular_buffer* circBuffer);
void write_buffer_single_entry(struct aesd_circular_buffer* circBuffer, const char* bePtr);

void read_buffer(struct aesd_circular_buffer* circBuffer) {
    struct aesd_buffer_entry* be=NULL;
    int index=0;
    size_t entry_offset_byte_ret = 0;
    size_t fpos = 0;
    printf("Read buffer entries\n");
    AESD_CIRCULAR_BUFFER_FOREACH(be, circBuffer, index) {
        be = aesd_circular_buffer_find_entry_offset_for_fpos(circBuffer, fpos, &entry_offset_byte_ret );
        if(be && be->buffptr) {
            printf("%s",be->buffptr);
            fpos+=be->size;
        }
    }
}

void write_buffer_entries(struct aesd_circular_buffer* circBuffer, int nEntries) {
    struct aesd_buffer_entry be;
    char* buff=NULL;
    printf("Write %d buffer entries\n", nEntries);
    for(int i=0; i < nEntries; i++) {
        buff = (char*)calloc(24, sizeof(char));
        snprintf(buff, 24, "%s%d\n", "write", i+1);
        be.size = strlen(buff);
        be.buffptr = buff;
        const char* oldPtr=aesd_circular_buffer_add_entry(circBuffer, &be);
        if(oldPtr) free((char*)oldPtr);
    }
}

void write_buffer_single_entry(struct aesd_circular_buffer* circBuffer, const char* bePtr) {
    struct aesd_buffer_entry be;
    char* buff=NULL;
    printf("Write buffer single entry\n");
    buff = (char*)calloc(24, sizeof(char));
    snprintf(buff, 24, "%s", bePtr);
    be.size = strlen(buff);
    be.buffptr = buff;
    const char* oldPtr=aesd_circular_buffer_add_entry(circBuffer, &be);
    if(oldPtr) free((char*)oldPtr);
}

void releaseBuffer(struct aesd_circular_buffer* circBuffer) {
    struct aesd_buffer_entry* be=NULL;
    int index=0;
    printf("\nRelease buffer entries\n");
    AESD_CIRCULAR_BUFFER_FOREACH(be, circBuffer, index) {
        if(be && be->buffptr) free((char*)be->buffptr);
    }
}

int main() {

    //Init circular buffer
    struct aesd_circular_buffer circBuffer;
    aesd_circular_buffer_init(&circBuffer);

    //Write/read entries to/from circular buffer

    write_buffer_entries(&circBuffer, 10);
    read_buffer(&circBuffer);
    write_buffer_single_entry(&circBuffer, "write11\n");
    read_buffer(&circBuffer);
    write_buffer_entries(&circBuffer, 10);
    read_buffer(&circBuffer);

    //Debug assignment 8 - aesdchar driver
     // snprintf((char*)bes[0].buffptr, 24, "%s\n", "hello");
     // bes[0].size = strlen(bes[0].buffptr);
     // snprintf((char*)bes[1].buffptr, 24, "%s\n", "Arthur");
     // bes[1].size = strlen(bes[1].buffptr);
     //
     // for(int i=0; i < 2; i++) {
     //     aesd_circular_buffer_add_entry(&circBuffer, &bes[i]);
     // }

    //Perform some operations
    /*
    size_t entry_offset_byte_ret = 0;
    ssize_t char_offset = 63;
    printf("find entry %ld\n", char_offset);
    struct aesd_buffer_entry *ret = aesd_circular_buffer_find_entry_offset_for_fpos(&circBuffer, char_offset, &entry_offset_byte_ret );
    if(ret) {
        printf("found fpos %ld\n", entry_offset_byte_ret);
        printf("entry string %s\n", ret->buffptr);
        printf("entry string at fpos %s\n", &ret->buffptr[entry_offset_byte_ret]);
    }
    else printf("entry not found\n");
    */

    releaseBuffer(&circBuffer);
    return 0;
}
