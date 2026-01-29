/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    struct aesd_buffer_entry* curr_buff_entry = NULL;
    // uint8_t start_index = 0;
    size_t overall_buff_chrs_len = 0;
    size_t chr_offset_mod = 0;
    uint8_t rd_pos = buffer->out_offs;
    for(uint8_t cur_rd_pos = rd_pos;
        cur_rd_pos<rd_pos+AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; cur_rd_pos++) {
        curr_buff_entry=&(buffer->entry[cur_rd_pos%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED]);
        if (curr_buff_entry!=NULL) {
            overall_buff_chrs_len+=curr_buff_entry->size;
            if(char_offset>=overall_buff_chrs_len) continue;
            if(char_offset==0) {
                *entry_offset_byte_rtn = 0;
            }else {
                chr_offset_mod = overall_buff_chrs_len % char_offset;
                *entry_offset_byte_rtn = (0==chr_offset_mod ? 0 : curr_buff_entry->size - chr_offset_mod);
            }
            return curr_buff_entry;
        }
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    if(buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        buffer->full=true;
        buffer->in_offs = 0;
        ++buffer->out_offs;
        buffer->out_offs %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    buffer->entry[buffer->in_offs] = *add_entry;
    ++buffer->in_offs;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}


