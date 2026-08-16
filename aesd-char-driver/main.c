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
#include "aesd_ioctl.h"

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
        if( copy_to_user(buf, be_ptr->buffptr+cur_buff_entry_fpos, read_chars) ) {
            retval=-EFAULT;
            goto error;
        }
        PDEBUG("aesd module read, read data %s", be_ptr->buffptr+cur_buff_entry_fpos);
        retval = read_chars;
        *f_pos+=read_chars;
    }
error:
    mutex_unlock(&dev->lock);
    PDEBUG("aesd module read, read %zu bytes, new f_pos=%lld - COMPLETE", retval, *f_pos);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos) {
    ssize_t retval = -ENOMEM;
    PDEBUG("aesd module write %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle write
     */
    //Buffer each command to a single buffer entry, until terminated command detected.
    struct aesd_dev* dev = filp->private_data;
    if(mutex_lock_interruptible(&dev->lock))
        return -EFAULT;
    size_t buff_entry_curr_size = dev->buff_entry.size;
    char* new_buff = (char*)kmalloc((count+buff_entry_curr_size)*sizeof(char), GFP_KERNEL);
    if (!new_buff) {
        goto error;
    }
    PDEBUG("aesd module write - kmalloc new buff ptr=%px", new_buff);

    //Copy data (if exists) from buffer entry into new allocated buffer.
    if(dev->buff_entry.buffptr) {
        memcpy(new_buff, dev->buff_entry.buffptr, buff_entry_curr_size);
        PDEBUG("aesd module write - Copy and kfree previous buff entry str=%s, ptr=%px", dev->buff_entry.buffptr, dev->buff_entry.buffptr);
        kfree(dev->buff_entry.buffptr);
        dev->buff_entry.buffptr = NULL;
    }

    //Copy/concatenate data from user space buffer (buf) into new kernel space buffer.
    //Assign new allocated buffer to circular buffer entry and update buffer entry size.
    if( copy_from_user(new_buff+buff_entry_curr_size, buf, count) ) {
        retval = -EFAULT;
        kfree(new_buff);
        goto error;
    }
    dev->buff_entry.buffptr = new_buff;
    retval = count;
    buff_entry_curr_size+= count;
    dev->buff_entry.size = buff_entry_curr_size;
    *f_pos+=count;

    //If terminated command (with '\n') detected, add entry to the circ-buffer, reset buffer entry size to 0.
    if(dev->buff_entry.buffptr[buff_entry_curr_size-1]=='\n') {
        const char* old_buff = aesd_circular_buffer_add_entry(&dev->circ_buff, &dev->buff_entry);
        PDEBUG("aesd module write - Added new entry to circ. buffer str=%s, size=%zu, ptr=%px", dev->buff_entry.buffptr, dev->buff_entry.size, dev->buff_entry.buffptr);
        kfree(old_buff);
        dev->buff_entry.size = 0;
        dev->buff_entry.buffptr = NULL;
    }
error:
    mutex_unlock(&dev->lock);
    PDEBUG("aesd module write %zu bytes, new f_pos %lld, retval=%zu - COMPLETE",count,*f_pos, retval);
    return retval;
}

/**
 *Adjust the file offset (f_pos) of @param filp based on the location that specified by
 *@param write_cmd (the 0-referenced command to locate)
 * @param write_cmd_offset - the 0-referenced offset into the command
 * @return 0 on success, negative if error occured:
 *  -EINVAL if write command or cmd_offset was out of range
 *  -ERESTARTSYS if mutex couldn't be obtained
 */
int aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset) {

    PDEBUG("aesd module ioctl seek into cmd=%zu cmd_offset=%zu", write_cmd, write_cmd_offset);
    if (write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) return -EINVAL;

    PDEBUG("aesd module ioctl seek into, cmd=%zu is valid", write_cmd);

    struct aesd_dev* dev = filp->private_data;
    struct aesd_buffer_entry be = dev->circ_buff.entry[write_cmd];
    if (NULL==be.buffptr) return -EINVAL;

    PDEBUG("aesd module ioctl seek into, cmd=%zu returns valid buffer entry", write_cmd);

    if (write_cmd_offset > be.size) return -EINVAL;

    PDEBUG("aesd module ioctl seek into, cmd_offset=%zu is valid", write_cmd_offset);

    bool isBufferFull= dev->circ_buff.full;
    uint8_t out_offs = dev->circ_buff.out_offs;
    //If buffer is NOT full, seek new f_pos is relative to cmd-0 until requested @param write_cmd+write_cmd_offset
    uint8_t from = ( isBufferFull ? out_offs : 0 );
    uint8_t until = (isBufferFull ?
                    (out_offs<write_cmd ? write_cmd : write_cmd+AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
                                                    : write_cmd);
    const char* isBuffFull = (isBufferFull ? "full" : "not full");
    PDEBUG("aesd module ioctl seek from cmd=%zu until cmd=%zu, buffer is %s", from, until, isBuffFull);

    size_t write_cmd_start_pos = 0;
    for ( ; from < until; ) {
        write_cmd_start_pos+=dev->circ_buff.entry[from % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
        from++;
    }

    filp->f_pos = write_cmd_start_pos + write_cmd_offset;
    PDEBUG("aesd module ioctl seek set file pointer to new f_pos=%zu", filp->f_pos);
    return 0;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    int retval = 0;
    struct aesd_dev* dev = filp->private_data;
    PDEBUG("aesd module ioctl - begin");

    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;

    switch(cmd) {
    case AESDCHAR_IOCSEEKTO:
        struct aesd_seekto seekto;
        if(mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;
        if ( copy_from_user(&seekto, (const void __user*)arg, sizeof(seekto))!=0 ) {
            retval = -EFAULT;
        } else {
            retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
        }
        mutex_unlock(&dev->lock);
        break;
    default:
        return -ENOTTY;
    }
    PDEBUG("aesd module ioctl retval=%d - COMPLETE", retval);
    return  retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry* buffer_entry = NULL;
    ssize_t i = 0;
    ssize_t buff_size = 0;
    loff_t newpos = 0;
    PDEBUG("aesd module llseek - begin");

    if(mutex_lock_interruptible(&dev->lock))
        return -EFAULT;
    AESD_CIRCULAR_BUFFER_FOREACH(buffer_entry, &dev->circ_buff, i){
        buff_size+=buffer_entry->size;
    }
    newpos = fixed_size_llseek(filp, off, whence, buff_size);
    mutex_unlock(&dev->lock);
    PDEBUG("aesd module llseek - COMPLETE");

    return newpos;
}

struct file_operations aesd_fops = {
    .owner  =   THIS_MODULE,
    .llseek =   aesd_llseek,
    .read   =   aesd_read,
    .write  =   aesd_write,
    .unlocked_ioctl = aesd_ioctl,
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
    kfree(aesd_device.buff_entry.buffptr);
    aesd_device.buff_entry.size = 0;
    struct aesd_buffer_entry* buffer_entry = NULL;
    ssize_t i = 0;
    AESD_CIRCULAR_BUFFER_FOREACH(buffer_entry, &aesd_device.circ_buff, i){
        kfree(buffer_entry->buffptr);
    }
    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
    PDEBUG("aesd module clean-up - COMPLETE");
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
