/*
 * Original evdev module:
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *
 *  itrax char devices, giving access to Intersense InterTrax 2 Headtracker.
 *  Copyright (c) 2000-2001 Jan-Friso Evers 
 *
 *  Ported to kernel 2.6 by Oliver Schneider
 *  Copyright (c) 2004
 *  Zentrum für Graphische Datenverarbeitung e.V.
 *  Digital Storytelling
 *  Fraunhoferstraße 5
 *  64283 Darmstadt
 *  Germany
*/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * 
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <evers@mip.informatik.uni-kiel.de>
 */

#define TRACKDEV_MINOR_BASE	128
#define TRACKDEV_MINORS		2

#include <linux/input.h>

#include "itrax.h"

#include <linux/poll.h>
#include <linux/major.h>
#include <linux/device.h>
#include <linux/devfs_fs_kernel.h>
//#include <linux/vermagic.h>
#include <linux/usb.h>


struct trackdev {
	int exist;
	int open;
	int minor;
	char name[16];
	struct input_handle handle;
	wait_queue_head_t wait; 
	struct trackdev_list *grab;
	struct list_head list;
};

struct trackdev_filter {
	int size;
	int type;  /* see itrax.h for types */
	int head;
	int lfv;  /*last filtered value */
	int *buffer;
	int *coeff;   /* coeff. for wighted filter */
	int buffer_full; /* ready to run ? */ 
};


struct trackdev_list {
	struct trackerposition position;
	int new;
	struct trackdev_filter filter[3]; 
	struct fasync_struct *fasync;
	struct trackdev *trackdev;
	struct list_head node;
};

static struct trackdev *trackdev_table[TRACKDEV_MINORS];

static void trackdev_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	struct trackdev *trackdev = handle->private;
	struct trackdev_list *list;
	struct trackdev_filter *filter;
	int i;
	
	if (trackdev->grab) {
		list = trackdev->grab;

		do_gettimeofday(&list->position.time);
		if (code < 3 || code > 5) 
			printk(KERN_ERR "itrax: Unknown eventcode : %d\n", code);
		else  {
			list->position.raw[code-3] =value ;
			list->position.tenthdegree[code-3] =(value * 1800) / 32768; 
			
	
			/*  normalize to 0..360, -80..+80, -90..+90 */
			if (code-3 == 2 || code -3 == 1) 
				if (list->position.tenthdegree[code-3] >1800) 
					list->position.tenthdegree[code-3] -=3600;
	
			/*  All stuff below is for filtering */
			switch (list->filter[code-3].type ) { 
			case ITRAX_FILTER_OFF:
				break;
			case ITRAX_FILTER_SLIDING_WINDOW:
				filter = &(list->filter[code-3]);
				/* replace oldest value with new unweighted value */
				filter->buffer[filter->head] = list->position.tenthdegree[code-3];
				/* increase pointer */
				if (filter->head < filter->size-1) {
					filter->head++;
				}
				else {
					filter->head = 0;
					filter->buffer_full = 1;
				}
				/* calculate filtered value */
				filter->lfv = 0.0;
				for (i=0 ; i<filter->size ; i++) {
					filter->lfv += (filter->buffer[i] * filter->coeff[i]);
				}
				list->position.tenthdegree[code-3] = filter->lfv;
				break;
				
			case ITRAX_FILTER_FASTMEAN:
				filter = &(list->filter[code-3]);
				/* subtract oldest  value from lfv */
				if (filter->buffer_full)
					filter->lfv -= filter->buffer[filter->head];
				/* replace oldest value with new weighted value */
				filter->buffer[filter->head] = list->position.tenthdegree[code-3] 
				/ filter->size ;
				/* add new  value  to  lfv */
				filter->lfv += filter->buffer[filter->head];
				/* increase pointer */
				if (filter->head < filter->size-1) {
					filter->head++;
				}
				else {
					filter->head = 0;
					filter->buffer_full = 1;
				}
				/* calculate filtered value */
				if (filter->buffer_full)  
					list->position.tenthdegree[code-3] = filter->lfv;
			break;
			}  /* end of switch (filtertype) */
	
			/* new data arrived */
			list->new = 1;
			kill_fasync(&list->fasync, SIGIO, POLL_IN);
		}
	} else
		list_for_each_entry(list, &trackdev->list, node) {

			do_gettimeofday(&list->position.time);
			if (code < 3 || code > 5) 
				printk(KERN_ERR "itrax: Unknown eventcode : %d\n",code);
			else  {
				list->position.raw[code-3] =value ;
				list->position.tenthdegree[code-3] =(value * 1800) / 32768; 
				
		
				/*  normalize to 0..360, -80..+80, -90..+90 */
				if (code-3 == 2 || code -3 == 1) 
					if (list->position.tenthdegree[code-3] >1800) 
						list->position.tenthdegree[code-3] -=3600;
		
				/*  All stuff below is for filtering */
				switch (list->filter[code-3].type ) { 
				case ITRAX_FILTER_OFF:
					break;
				case ITRAX_FILTER_SLIDING_WINDOW:
					filter = &(list->filter[code-3]);
					/* replace oldest value with new unweighted value */
					filter->buffer[filter->head] = list->position.tenthdegree[code-3];
					/* increase pointer */
					if (filter->head < filter->size-1) {
						filter->head++;
					}
					else {
						filter->head = 0;
						filter->buffer_full = 1;
					}
					/* calculate filtered value */
					filter->lfv = 0.0;
					for (i=0 ; i<filter->size ; i++) {
						filter->lfv += (filter->buffer[i] * filter->coeff[i]);
					}
					list->position.tenthdegree[code-3] = filter->lfv;
					break;
					
				case ITRAX_FILTER_FASTMEAN:
					filter = &(list->filter[code-3]);
					/* subtract oldest  value from lfv */
					if (filter->buffer_full)
						filter->lfv -= filter->buffer[filter->head];
					/* replace oldest value with new weighted value */
					filter->buffer[filter->head] = list->position.tenthdegree[code-3] 
					/ filter->size ;
					/* add new  value  to  lfv */
					filter->lfv += filter->buffer[filter->head];
					/* increase pointer */
					if (filter->head < filter->size-1) {
						filter->head++;
					}
					else {
						filter->head = 0;
						filter->buffer_full = 1;
					}
					/* calculate filtered value */
					if (filter->buffer_full)  
						list->position.tenthdegree[code-3] = filter->lfv;
				break;
				}  /* end of switch (filtertype) */
		
				/* new data arrived */
				list->new = 1;
				kill_fasync(&list->fasync, SIGIO, POLL_IN);
			}
		}

	wake_up_interruptible(&trackdev->wait);
}

static int trackdev_fasync(int fd, struct file *file, int on)
{
	int retval;
	struct trackdev_list *list = file->private_data;
	retval = fasync_helper(fd, file, on, &list->fasync);
	return retval < 0 ? retval : 0;
}

static void trackdev_free(struct trackdev *trackdev)
{
	devfs_remove("input/tracker%d", trackdev->minor);
	class_simple_device_remove(MKDEV(INPUT_MAJOR, TRACKDEV_MINOR_BASE + trackdev->minor));
	trackdev_table[trackdev->minor] = NULL;
	kfree(trackdev);
}

static int trackdev_release(struct inode * inode, struct file * file)
{
	struct trackdev_list *list = file->private_data;
	int i;
	
	if (list->trackdev->grab == list) {
		input_release_device(&list->trackdev->handle);
		list->trackdev->grab = NULL;
	}

	trackdev_fasync(-1, file, 0);
	list_del(&list->node);

	if (!--list->trackdev->open) {
		if (list->trackdev->exist) {
			/* free filtermem */
			for (i=0; i<3; i++) {
				if (list->filter[i].size > 0) {
#ifdef ITRAX_DEBUG
					printk("trackdev_release() freeing %d floats filtermem for axes: %d\n",
					list->filter[i].size ,i);
#endif
					kfree(list->filter[i].buffer);
				}
				if (list->filter[i].size > 0 
					&& list->filter[i].type == ITRAX_FILTER_SLIDING_WINDOW) {
#ifdef ITRAX_DEBUG
					printk("trackdev_release() freeing %d floats coeff for axes: %d\n",
					list->filter[i].size ,i);
#endif
					kfree(list->filter[i].coeff);
				}
			}  /*   end of free filtermem */
			input_close_device(&list->trackdev->handle);	
		} else {
			trackdev_free(list->trackdev);
		}
	}
	kfree(list);
	return 0;
}



static int trackdev_open(struct inode * inode, struct file * file)
{
	struct trackdev_list *list;
	int i = iminor(inode) - TRACKDEV_MINOR_BASE;
	int accept_err;

	if (i >= TRACKDEV_MINORS || !trackdev_table[i] || !trackdev_table[i]->exist)
		return -ENODEV;

	if ((accept_err = input_accept_process(&(trackdev_table[i]->handle), file)))
		return accept_err;

	if (!(list = kmalloc(sizeof(struct trackdev_list), GFP_KERNEL)))
		return -ENOMEM;
	memset(list, 0, sizeof(struct trackdev_list));

	list->trackdev = trackdev_table[i];
	list_add_tail(&list->node, &trackdev_table[i]->list);

	file->private_data = list;

	if (!list->trackdev->open++)
		if (list->trackdev->exist)
			input_open_device(&list->trackdev->handle);	
	
	return 0;
}

static ssize_t trackdev_write(struct file * file, const char __user * buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t trackdev_read(struct file * file, char __user * buffer, size_t count, loff_t *ppos)
{
	struct trackdev_list *list = file->private_data;
	int retval = 0;

	if (copy_to_user(buffer + retval, &(list->position),
		sizeof(struct trackerposition))) return -EFAULT;
	retval += sizeof(struct trackerposition);
	list->new = 0;
	
	return retval;
}

static unsigned int trackdev_poll(struct file *file, poll_table *wait)
{
	struct trackdev_list *list = file->private_data;
	poll_wait(file, &list->trackdev->wait, wait);
	if (list->new) 
		return POLLIN | POLLRDNORM;
	return 0; 
}

static int trackdev_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct trackdev_list *list = file->private_data;
	struct trackdev *trackdev = list->trackdev;
	struct input_dev *dev = trackdev->handle.dev;
	struct input_absinfo abs;
	struct filtercontrol filterc;
	void __user *p = (void __user *)arg;
	int i;
	
	switch (cmd) {
	
		case ITRAXIOCGVERSION:
			return put_user(EV_VERSION, (int *) arg);
			
		case ITRAXIOCGID:
			return copy_to_user(p, &dev->id, sizeof(struct input_id)) ? -EFAULT : 0;

		case ITRAXIOCSFILTER :
			if (((struct filtercontrol *)arg)->axes >2)
				return -EINVAL;
			filterc.axes = ((struct filtercontrol *)arg)->axes;   
    
			/* free coeff. if required */
			if (list->filter[filterc.axes].type == ITRAX_FILTER_SLIDING_WINDOW) {
#ifdef ITRAX_DEBUG
				printk("Freeing %d floats coeff for axes: %d\n",
					list->filter[filterc.axes].size ,filterc.axes);
#endif
				kfree(list->filter[filterc.axes].coeff);
			}
			/* free old buffer */
			if (list->filter[filterc.axes].size >0 ) {
#ifdef ITRAX_DEBUG
				printk("Freeing %d floats filtermem for axes: %d\n",
					list->filter[filterc.axes].size ,filterc.axes);
#endif
				kfree(list->filter[filterc.axes].buffer);
				list->filter[filterc.axes].size = 0;
			}   
		
			list->filter[filterc.axes].type = ((struct filtercontrol *)arg)->type;
		
			/* printk("type: %d size: %d\n",list->filter[filterc.axes].type,((struct filtercontrol *)arg)->size ); */
			/* malloc(size) if required */
			if (list->filter[filterc.axes].type > ITRAX_FILTER_OFF
				&& ((struct filtercontrol *)arg)->size >0 ) {
			
				list->filter[filterc.axes].size = ((struct filtercontrol *)arg)->size;
			
				/* printk("kmalloc %d float filtermem for axes: %d\n", */
				/* list->filter[filterc.axes].size ,filterc.axes); */
				/* allocate new buffer */
				list->filter[filterc.axes].buffer = 
				kmalloc(list->filter[filterc.axes].size * sizeof(float), GFP_KERNEL);
				list->filter[filterc.axes].head = 0;
				list->filter[filterc.axes].buffer_full = 0;
				
				/* init new coeff. */
				if (list->filter[filterc.axes].type == ITRAX_FILTER_SLIDING_WINDOW) {
#ifdef ITRAX_DEBUG
					printk("kmalloc %d float coeff. for axes: %d\n",
					list->filter[filterc.axes].size ,filterc.axes); 
#endif
					list->filter[filterc.axes].coeff = 
					kmalloc(list->filter[filterc.axes].size * sizeof(float), GFP_KERNEL);
					/* copy coeff. */
					for (i=0; i<list->filter[filterc.axes].size; i++) {
						list->filter[filterc.axes].coeff[i]=
							((struct filtercontrol *)arg)->coeff[i];
#ifdef ITRAX_DEBUG
						printk("coeff: %d in percent\n",
							(int)(list->filter[filterc.axes].coeff[i]*100));
#endif
					}
		/*  	if (retval >0 ) return -EINVAL; */
				}
			}
			return 0;
			
		case ITRAXIOCGFILTER :
			/* to be improved */
			filterc.axes = ((struct filtercontrol *)arg)->axes;
			filterc.size = list->filter[filterc.axes].size;
			filterc.type = list->filter[filterc.axes].type;
			return copy_to_user((int *)arg , &filterc , sizeof(filterc)); 

		default:
	
			if (_IOC_TYPE(cmd) != 'E' || _IOC_DIR(cmd) != _IOC_READ)
				return -EINVAL;

			if (_IOC_NR(cmd) == _IOC_NR(ITRAXIOCGNAME(0))) {
				int len;
				if (!dev->name) return -ENOENT;
				len = strlen(dev->name) + 1;
				if (len > _IOC_SIZE(cmd)) len = _IOC_SIZE(cmd);
				return copy_to_user(p, dev->name, len) ? -EFAULT : len;
			}
			if ((_IOC_NR(cmd) & ~ABS_MAX) == _IOC_NR(ITRAXIOCGABS(0))) {

				int t = _IOC_NR(cmd) & ABS_MAX;

				if (copy_from_user(&abs, p, sizeof(struct input_absinfo)))
					return -EFAULT;

				dev->abs[t] = abs.value;
				dev->absmin[t] = abs.minimum;
				dev->absmax[t] = abs.maximum;
				dev->absfuzz[t] = abs.fuzz;
				dev->absflat[t] = abs.flat;

				return 0;
			}
	}
	return -EINVAL;
}

static struct file_operations trackdev_fops = {
	.owner =	THIS_MODULE,
	.read =		trackdev_read,
	.write =	trackdev_write,
	.poll =		trackdev_poll,
	.open =		trackdev_open,
	.release =	trackdev_release,
	.ioctl =	trackdev_ioctl,
	.fasync =	trackdev_fasync,
};

static struct input_handle *trackdev_connect(struct input_handler *handler, struct input_dev *dev, struct input_device_id *id)
{
	struct trackdev *trackdev;
	int minor;
	/* verify device can be a tracker */
 	if (!test_bit(EV_KEY, dev->evbit) || test_bit(EV_REL, dev->evbit) || 
	    !test_bit(ABS_RX, dev->absbit) || !test_bit(ABS_RY, dev->absbit) ||
	    !test_bit(ABS_RZ, dev->absbit))
	
		return NULL; 

/*	if (dev->idvendor != VENDORID_ISENSE)
	  {
	    printk("itrax: unknown Vendor: %x\n",dev->idvendor);
	    return NULL;
	  }
*/	/* for debugging only */
/*  	printk(" dev->evbit[%d] %ld\n",0,dev->evbit[0]); */
/*  	printk(" dev->keybit[%d] %ld\n",0,dev->keybit[0]); */
/*  	printk(" dev->absbit[%d] %ld\n",0,dev->absbit[0]); */
/*  	printk(" dev->relbit[%d] %ld\n",0,dev->relbit[0]); */
	
	
	for (minor = 0; minor < TRACKDEV_MINORS && trackdev_table[minor]; minor++);
	if (minor == TRACKDEV_MINORS) {
		printk(KERN_ERR "itrax: no more free tracker devices\n");
		return NULL;
	}

	if (!(trackdev = kmalloc(sizeof(struct trackdev), GFP_KERNEL)))
		return NULL;
	memset(trackdev, 0, sizeof(struct trackdev));

	INIT_LIST_HEAD(&trackdev->list);
	init_waitqueue_head(&trackdev->wait);

	trackdev->minor = minor;
	trackdev_table[minor] = trackdev;

	trackdev->handle.dev = dev;
	trackdev->handle.name = trackdev->name;
	trackdev->handle.handler = handler;
	trackdev->handle.private = trackdev;
	sprintf(trackdev->name, "tracker%d", minor);

	trackdev->exist = 1;
//	trackdev->devfs = input_register_minor("tracker%d", minor, TRACKDEV_MINOR_BASE);

	devfs_mk_cdev(MKDEV(INPUT_MAJOR, TRACKDEV_MINOR_BASE + minor),
			S_IFCHR|S_IRUGO|S_IWUSR, "input/tracker%d", minor);
	class_simple_device_add(input_class,
				MKDEV(INPUT_MAJOR, TRACKDEV_MINOR_BASE + minor),
				dev->dev, "tracker%d", minor);
	
//	printk(KERN_INFO "tracker%d: %s "/*for input%d\n"*/, minor, dev->name/*, dev->number*/);

	return &trackdev->handle;
}

static void trackdev_disconnect(struct input_handle *handle)
{
	struct trackdev *trackdev = handle->private;

	trackdev->exist = 0;

	if (trackdev->open) {
		input_close_device(handle);
	} else {
		trackdev_free(trackdev);
	}
}


static struct input_device_id trackdev_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, trackdev_ids);

static struct input_handler trackdev_handler = {
	.event =	trackdev_event,
	.connect =	trackdev_connect,
	.disconnect =	trackdev_disconnect,
	.fops =		&trackdev_fops,
	.minor =	TRACKDEV_MINOR_BASE,
	.name =		"trackdev",
	.id_table =	trackdev_ids,
};


static int __init trackdev_init(void)
{
  input_register_handler(&trackdev_handler);
  return 0;
}


static void __exit trackdev_exit(void)

{
    input_unregister_handler(&trackdev_handler); 
}


module_init(trackdev_init);
module_exit(trackdev_exit);

//MODULE_AUTHOR("Jan-Friso Evers <evers@mip.informatik.uni-kiel.de");
MODULE_AUTHOR("Oliver Schneider <Oliver.Schneider@zgdv.de>");
MODULE_DESCRIPTION("Intertrax2 Headtracker character device driver for kernel 2.6");
MODULE_SUPPORTED_DEVICE("Intertrax2 Headtracker");
MODULE_LICENSE("GPL");
