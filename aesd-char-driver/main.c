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
#include <linux/container_of.h>

#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Arthur Brodsky"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev* dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data=dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    size_t cur_buff_entry_fpos = 0;
    struct aesd_dev* dev = filp->private_data;
    if(mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    struct aesd_buffer_entry* be_ptr=aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circ_buff, *f_pos, &cur_buff_entry_fpos);
    if(be_ptr) {
        size_t read_chars = ( (count>(be_ptr->size-cur_buff_entry_fpos) ) ? be_ptr->size-cur_buff_entry_fpos : count );
        if( copy_to_user(buf, be_ptr->buffptr+cur_buff_entry_fpos, read_chars) ) {
            retval=-EFAULT;
            goto error;
        }
        retval = read_chars;
        *f_pos+=read_chars;
    }
error:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos) {
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    //Buffer each command to a single buffer entry, until terminated command detected.
    struct aesd_dev* dev = filp->private_data;
    size_t buff_entry_curr_size = dev->buff_entry.size;
    if(mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    char* new_buff = (char*)kmalloc((count+buff_entry_curr_size)*sizeof(char), GFP_KERNEL);
    if (!new_buff) {
        goto error;
    }
    //Copy data (if exists) from buffer entry into new allocated buffer. Release old ptr from the entry.
    if(dev->buff_entry.buffptr) {
        memcpy(new_buff, dev->buff_entry.buffptr, buff_entry_curr_size);
        kfree(dev->buff_entry.buffptr);
    }

    //Assign new allocated buffer to circular buffer entry and update buffer entry size.
    //Copy/concatenate data from user space buffer (buf) into new kernel space buffer.
    dev->buff_entry.buffptr = new_buff;
    if( retval=copy_from_user(new_buff+buff_entry_curr_size, buf, count) ) {
        retval = -EFAULT;
        goto error;
    }
    buff_entry_curr_size+= count;
    dev->buff_entry.size = buff_entry_curr_size;

    //If terminated command (with '\n') detected, add entry to the circ-buffer, reset buffer entry size to 0.
    if(dev->buff_entry.buffptr[buff_entry_curr_size-1]=='\n') {
        const char* old_buff = aesd_circular_buffer_add_entry(&dev->circ_buff, &dev->buff_entry);
        if (old_buff) kfree(old_buff);
        dev->buff_entry.size = 0;
    }
error:
    mutex_unlock(&dev->lock);
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
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
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
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    if(aesd_device.buff_entry.buffptr) kfree(aesd_device.buff_entry.buffptr);
    for (ssize_t i=0; i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if(aesd_device.circ_buff.entry[i].buffptr!=NULL) kfree(aesd_device.circ_buff.entry[i].buffptr);
    }
    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
