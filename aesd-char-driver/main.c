/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/string.h> //string operations
#include <linux/mutex.h>
#include <linux/slab.h> //for kmalloc and kfree
// Dynamically includes container_of based on the active kernel version
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0)
    #include <linux/container_of.h>
#else
    #include <linux/kernel.h>
#endif

#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Arthur Brodsky"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("aesd module open");
    /**
     * TODO: handle open
     */
    struct aesd_dev* dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data=dev;
    PDEBUG("aesd module open - COMPLETE");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("aesd module release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("aesd module read %zu bytes with offset %lld",count, *f_pos);
    /**
     * TODO: handle read
     */
    size_t cur_buff_entry_fpos = 0;
    struct aesd_dev* dev = filp->private_data;
    if(mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    struct aesd_buffer_entry* be_ptr=aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circ_buff,
                                                                                    *f_pos,
                                                                                    &cur_buff_entry_fpos);
    if(be_ptr) {
        size_t read_chars = be_ptr->size-cur_buff_entry_fpos;
        read_chars = (read_chars>count ? count : read_chars);
        // PDEBUG("aesd module read %d bytes to read", read_chars);
        // PDEBUG("aesd module read position to read from cur_buff_entry_fpos=%zu", cur_buff_entry_fpos);
        // PDEBUG("aesd module read -> %s from buffptr directly", be_ptr->buffptr);
        // PDEBUG("aesd module read -> %s from buffptr+cur_buff_entry_fpos", be_ptr->buffptr+cur_buff_entry_fpos);
        if( copy_to_user(buf, be_ptr->buffptr+cur_buff_entry_fpos, read_chars) ) {
            retval=-EFAULT;
            goto error;
        }
        retval = read_chars;
        *f_pos+=read_chars;
    }
error:
    mutex_unlock(&dev->lock);
    PDEBUG("aesd module read %zu bytes with offset %lld - COMPLETE", count, *f_pos);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos) {
    ssize_t retval = -ENOMEM;
    PDEBUG("aesd module write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    //Buffer each command to a single buffer entry, until terminated command detected.
    struct aesd_dev* dev = filp->private_data;
    size_t buff_entry_curr_size = dev->buff_entry.size;
    if(mutex_lock_interruptible(&dev->lock))
        return -EFAULT;
    char* new_buff = (char*)kmalloc((count+buff_entry_curr_size)*sizeof(char), GFP_KERNEL);
    if (!new_buff) {
        goto error;
    }
    //Copy data (if exists) from buffer entry into new allocated buffer.
    if(dev->buff_entry.buffptr) {
        memcpy(new_buff, dev->buff_entry.buffptr, buff_entry_curr_size);
    }

    //Assign new allocated buffer to circular buffer entry and update buffer entry size.
    //Copy/concatenate data from user space buffer (buf) into new kernel space buffer.
    dev->buff_entry.buffptr = new_buff;
    if( (retval=copy_from_user(new_buff+buff_entry_curr_size, buf, count) )) {
        retval = -EFAULT;
        goto error;
    }
    retval = count;
    buff_entry_curr_size+= count;
    dev->buff_entry.size = buff_entry_curr_size;
    // PDEBUG("aesd module write copied from user %zu bytes",count);
    // PDEBUG("aesd module write copied string=%s of %zu bytes", dev->buff_entry.buffptr, count);
    // PDEBUG("aesd module write new buffer entry size=%zu bytes", dev->buff_entry.size);

    //If terminated command (with '\n') detected, add entry to the circ-buffer, reset buffer entry size to 0.
    if(dev->buff_entry.buffptr[buff_entry_curr_size-1]=='\n') {
        const char* old_buff = aesd_circular_buffer_add_entry(&dev->circ_buff, &dev->buff_entry);
        kfree(old_buff);
        dev->buff_entry.size = 0;
    }
error:
    mutex_unlock(&dev->lock);
    PDEBUG("aesd module write %zu bytes with offset %lld retval=%zu - COMPLETE",count,*f_pos, retval);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    PDEBUG("aesd module init");
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    PDEBUG("aesd module init - COMPLETE");
    return result;

}

void aesd_cleanup_module(void)
{
    PDEBUG("aesd module clean-up");
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    // PDEBUG("aesd module clean-up be - before kfree");
    kfree(aesd_device.buff_entry.buffptr);
    // PDEBUG("aesd module clean-up be - after kfree");
    for (ssize_t i=0; i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        PDEBUG("aesd module clean-up - free circ buffer entry %zu size=%zu bytes", i, aesd_device.circ_buff.entry[i].size);
        PDEBUG("aesd module clean-up - free circ buffer entry ptr=%px", aesd_device.circ_buff.entry[i].buffptr);
        if(NULL!=aesd_device.circ_buff.entry[i].buffptr) {
            kfree(&aesd_device.circ_buff.entry[i].buffptr);
            aesd_device.circ_buff.entry[i].buffptr = NULL;
        }
    }
    PDEBUG("aesd module clean-up - before mutex destroy");
    mutex_destroy(&aesd_device.lock);
    PDEBUG("aesd module clean-up - after mutex destroy");
    unregister_chrdev_region(devno, 1);
    PDEBUG("aesd module clean-up - COMPLETE");
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
