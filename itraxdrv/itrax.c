/*
 * Original evdev module:
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *
 *  itrax char devices, giving access to Intersense InterTrax 2 Headtracker.
 *  Copyright (c) 2000-2001 Jan-Friso Evers 
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

#include <linux/poll.h>
#include <linux/malloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>

#ifdef K24
#include <linux/smp_lock.h>
#else
#include <linux/kcomp.h>
#endif

#include "itrax.h"


struct trackdev {
  int exist;
  int open;
  int minor;
  struct input_handle handle;
   wait_queue_head_t wait; 
  devfs_handle_t devfs;
  struct trackdev_list *list;
};




struct trackdev_filter {
  int size;
  int type;  /* see itrax.h for types */
  int head;
  double lfv;  /*last filtered value */
  double *buffer;
  double *coeff;   /* coeff. for wighted filter */
  int buffer_full; /* ready to run ? */ 
};





struct trackdev_list {
  struct trackerposition position;
  int new;
  struct trackdev_filter filter[3]; 
  struct fasync_struct *fasync;
  struct trackdev *trackdev;
  struct trackdev_list *next;
};





static struct trackdev *trackdev_table[TRACKDEV_MINORS] = { NULL, /* ... */ };





static void trackdev_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	struct trackdev *trackdev = handle->private;
	struct trackdev_list *list = trackdev->list;
	struct trackdev_filter *filter;
	int i;
	while (list) {
		get_fast_time(&list->position.time);
		if (code < 3 || code > 5) 
		     printk(KERN_ERR "itrax: Unknown eventcode : %d\n",code);
		else  {

		  list->position.raw[code-3] =((double)value * 180) / 32768;
		  /*  normalize to 0..360, -80..+80, -90..+90 */
		  if (code-3 == 2 || code -3 == 1) 
		    if (list->position.raw[code-3] >180) list->position.raw[code-3] -=360;

  		  switch (list->filter[code-3].type ) { 
		  case ITRAX_FILTER_OFF:
		    break;
		  case ITRAX_FILTER_SLIDING_WINDOW:
		    filter = &(list->filter[code-3]);
		    /* replace oldest value with new unweighted value */
		    filter->buffer[filter->head] = list->position.raw[code-3];
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
		    list->position.raw[code-3] = filter->lfv;
		    break;
		    
		  case ITRAX_FILTER_FASTMEAN:
		    filter = &(list->filter[code-3]);
		    /* subtract oldest  value from lfv */
		    if (filter->buffer_full)
		      filter->lfv -= filter->buffer[filter->head];
		    /* replace oldest value with new weighted value */
		    filter->buffer[filter->head] = list->position.raw[code-3] 
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
		    list->position.raw[code-3] = filter->lfv;
		    break;
		    
		  }  /* end of switch (filtertype) */

		  /* new data arrived */
		  list->new = 1;
#ifdef K24	     
		  kill_fasync(&list->fasync, SIGIO, POLL_IN);
#else
		  kill_fasync(list->fasync, SIGIO);  
#endif
		}
		list = list->next;
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





static int trackdev_release(struct inode * inode, struct file * file)
{
	struct trackdev_list *list = file->private_data;
	struct trackdev_list **listptr;
	int i;

#ifdef K24
	lock_kernel();
#endif
	listptr = &list->trackdev->list;
	trackdev_fasync(-1, file, 0);

	while (*listptr && (*listptr != list))
		listptr = &((*listptr)->next);
	*listptr = (*listptr)->next;
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
			input_unregister_minor(list->trackdev->devfs);
			trackdev_table[list->trackdev->minor] = NULL;
			kfree(list->trackdev);
		}
	}
	kfree(list);
#ifdef K24
	unlock_kernel();
#endif
	return 0;
}



static int trackdev_open(struct inode * inode, struct file * file)
{
	struct trackdev_list *list;
	int i = MINOR(inode->i_rdev) - TRACKDEV_MINOR_BASE;

	if (i >= TRACKDEV_MINORS || !trackdev_table[i])
		return -ENODEV;

	if (!(list = kmalloc(sizeof(struct trackdev_list), GFP_KERNEL)))
		return -ENOMEM;
	memset(list, 0, sizeof(struct trackdev_list));

	list->trackdev = trackdev_table[i];
	list->next = trackdev_table[i]->list;
	trackdev_table[i]->list = list;

	file->private_data = list;

	if (!list->trackdev->open++)
		if (list->trackdev->exist)
			input_open_device(&list->trackdev->handle);	

	return 0;
}





static ssize_t trackdev_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}





static ssize_t trackdev_read(struct file * file, char * buffer, size_t count, loff_t *ppos)
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
  struct filtercontrol filterc;
  int retval;
  int i;

  switch (cmd) {

  case ITRAXIOCGVERSION:
    return put_user(EV_VERSION, (int *) arg);

  case ITRAXIOCGID:
    if ((retval = put_user(dev->idbus,     ((short *) arg) + 0))) return retval;
    if ((retval = put_user(dev->idvendor,  ((short *) arg) + 1))) return retval;
    if ((retval = put_user(dev->idproduct, ((short *) arg) + 2))) return retval;
    if ((retval = put_user(dev->idversion, ((short *) arg) + 3))) return retval;
    return 0;

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

 /*     printk("type: %d size: %d\n",list->filter[filterc.axes].type,((struct filtercontrol *)arg)->size ); */
    /* malloc(size) if required */
    if (list->filter[filterc.axes].type > ITRAX_FILTER_OFF
	&& ((struct filtercontrol *)arg)->size >0 ) {
      
      list->filter[filterc.axes].size = ((struct filtercontrol *)arg)->size;
      
/*        printk("kmalloc %d float filtermem for axes: %d\n", */
/*  	     list->filter[filterc.axes].size ,filterc.axes); */
      /* allocate new buffer */
      list->filter[filterc.axes].buffer = 
	kmalloc(list->filter[filterc.axes].size * sizeof(double), GFP_KERNEL);
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
      if (!dev->name) return 0;
      len = strlen(dev->name) + 1;
      if (len > _IOC_SIZE(cmd)) len = _IOC_SIZE(cmd);
      return copy_to_user((char *) arg, dev->name, len) ? -EFAULT : len;
    }

    if ((_IOC_NR(cmd) & ~ABS_MAX) == _IOC_NR(ITRAXIOCGABS(0))) {

      int t = _IOC_NR(cmd) & ABS_MAX;

      if ((retval = put_user(dev->abs[t],     ((int *) arg) + 0))) return retval;
      if ((retval = put_user(dev->absmin[t],  ((int *) arg) + 1))) return retval;
      if ((retval = put_user(dev->absmax[t],  ((int *) arg) + 2))) return retval;
      if ((retval = put_user(dev->absfuzz[t], ((int *) arg) + 3))) return retval;
      if ((retval = put_user(dev->absflat[t], ((int *) arg) + 4))) return retval;

      return 0;
    }
  }
  return -EINVAL;
}





static struct file_operations trackdev_fops = {
#ifdef K24
	owner:		THIS_MODULE,
#endif
	read:		trackdev_read,
	write:		trackdev_write,
	poll:		trackdev_poll,
	open:		trackdev_open,
	release:	trackdev_release,
	ioctl:		trackdev_ioctl,
	fasync:		trackdev_fasync,
};





static struct input_handle *trackdev_connect(struct input_handler *handler, struct input_dev *dev)
{
	struct trackdev *trackdev;
	int minor;
	/* verify device can be a tracker */
 	if (!test_bit(EV_KEY, dev->evbit) || test_bit(EV_REL, dev->evbit) || 
	    !test_bit(ABS_RX, dev->absbit) || !test_bit(ABS_RY, dev->absbit) ||
	    !test_bit(ABS_RZ, dev->absbit))
	
		  return NULL; 

	if (dev->idvendor != VENDORID_ISENSE)
	  {
	    printk("itrax: unknown Vendor: %x\n",dev->idvendor);
	    return NULL;
	  }
	/* for debugging only */
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

	init_waitqueue_head(&trackdev->wait);

	trackdev->minor = minor;
	trackdev_table[minor] = trackdev;

	trackdev->handle.dev = dev;
	trackdev->handle.handler = handler;
	trackdev->handle.private = trackdev;

	trackdev->exist = 1;
	trackdev->devfs = input_register_minor("tracker%d", minor, TRACKDEV_MINOR_BASE);

	printk(KERN_INFO "tracker%d: %s for input%d\n", minor,dev->name, dev->number);

	return &trackdev->handle;
}





static void trackdev_disconnect(struct input_handle *handle)
{
	struct trackdev *trackdev = handle->private;

	trackdev->exist = 0;

	if (trackdev->open) {
		input_close_device(handle);
	} else {
		input_unregister_minor(trackdev->devfs);
		trackdev_table[trackdev->minor] = NULL;
		kfree(trackdev);
	}
}
	




static struct input_handler trackdev_handler = {
	event:		trackdev_event,
	connect:	trackdev_connect,
	disconnect:	trackdev_disconnect,
	fops:		&trackdev_fops,
	minor:		TRACKDEV_MINOR_BASE,
};


static void __exit trackdev_exit(void)

{
    input_unregister_handler(&trackdev_handler); 
}



static int __init trackdev_init(void)
{
  input_register_handler(&trackdev_handler);
  return 0;
}




module_init(trackdev_init);
module_exit(trackdev_exit);

MODULE_AUTHOR("Jan-Friso Evers <evers@mip.informatik.uni-kiel.de");
MODULE_DESCRIPTION("Intertrax2 Headtracker character device driver");

