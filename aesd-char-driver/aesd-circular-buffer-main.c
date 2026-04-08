//
// Created by artb on 1/26/26.
//
#include <stdio.h>

#include "aesd-circular-buffer.h"

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

int main() {
    struct aesd_buffer_entry be1;
    struct aesd_buffer_entry be2;
    struct aesd_buffer_entry be3;
    struct aesd_buffer_entry be4;
    struct aesd_buffer_entry be5;
    struct aesd_buffer_entry be6;
    struct aesd_buffer_entry be7;
    struct aesd_buffer_entry be8;
    struct aesd_buffer_entry be9;
    struct aesd_buffer_entry be10;
    struct aesd_buffer_entry be11;

    be1.buffptr = "write1\n";
    be2.buffptr = "write2\n";
    be3.buffptr = "write3\n";
    be4.buffptr = "write4\n";
    be5.buffptr = "write5\n";
    be6.buffptr = "write6\n";
    be7.buffptr = "write7\n";
    be8.buffptr = "write8\n";
    be9.buffptr = "write9\n";
    be10.buffptr = "write10\n";
    be11.buffptr = "write11\n";

    be1.size = strlen(be1.buffptr);
    be2.size = strlen(be2.buffptr);
    be3.size = strlen(be3.buffptr);
    be4.size = strlen(be4.buffptr);
    be5.size = strlen(be5.buffptr);
    be6.size = strlen(be6.buffptr);
    be7.size = strlen(be7.buffptr);
    be8.size = strlen(be8.buffptr);
    be9.size = strlen(be9.buffptr);
    be10.size = strlen(be10.buffptr);
    be11.size = strlen(be11.buffptr);

    struct aesd_circular_buffer circBuffer;
    aesd_circular_buffer_init(&circBuffer);

    aesd_circular_buffer_add_entry(&circBuffer, &be1);
    aesd_circular_buffer_add_entry(&circBuffer, &be2);
    aesd_circular_buffer_add_entry(&circBuffer, &be3);
    aesd_circular_buffer_add_entry(&circBuffer, &be4);
    aesd_circular_buffer_add_entry(&circBuffer, &be5);
    aesd_circular_buffer_add_entry(&circBuffer, &be6);
    aesd_circular_buffer_add_entry(&circBuffer, &be7);
    aesd_circular_buffer_add_entry(&circBuffer, &be8);
    aesd_circular_buffer_add_entry(&circBuffer, &be9);
    aesd_circular_buffer_add_entry(&circBuffer, &be10);

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
    return 0;
}
