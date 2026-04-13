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

int main() {
    struct aesd_buffer_entry bes[11];
    char* buff=NULL;
    //Init 11 circular buffer entries
    for(int i=0; i < 11; i++) {
        buff = (char*)calloc(24, sizeof(char));
        snprintf(buff, 24, "%s%d\n", "write", i);
        bes[i].size = strlen(buff);
        bes[i].buffptr = buff;
    }
    //Write entries to circular buffer
    struct aesd_circular_buffer circBuffer;
    aesd_circular_buffer_init(&circBuffer);
    for(int i=0; i < 10; i++) {
        aesd_circular_buffer_add_entry(&circBuffer, &bes[i]);
    }

    //Perform some operations
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

    // aesd_circular_buffer_add_entry(&circBuffer, &be11);
    // ret = aesd_circular_buffer_find_entry_offset_for_fpos(&circBuffer, 0, &entry_offset_byte_ret );
    // if(ret) {
    //     printf("found fpos %ld\n", entry_offset_byte_ret);
    //     printf("entry string %s\n", ret->buffptr);
    //     printf("entry string at fpos %s\n", &ret->buffptr[entry_offset_byte_ret]);
    // }

    for(int i=0; i < 11; i++) {
        free((char*)bes[i].buffptr);
    }
    return 0;
}
