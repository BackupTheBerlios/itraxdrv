#ifndef __TRACKDEV__
#define __TRACKDEV__

#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <sys/time.h>
#include <sys/ioctl.h>

#endif                   

/* these types of filters are implemented */
                                                            
#define ITRAX_FILTER_OFF 0
#define ITRAX_FILTER_SLIDING_WINDOW 1  /* number of "size" coeff. required */ 
#define ITRAX_FILTER_FASTMEAN 2     /* no coeff. required */


                        
struct filtercontrol {
  int axes;  /* itrax2: 0..2 */
  int type;
  int size;  
  float * coeff; 
};


#define VENDORID_ISENSE 0x0a94
#define PRODUCTID_ISENSE 0x1
/* IOCTL */

/*get driver version */
#define ITRAXIOCGVERSION           _IOR('E', 0x01, int)  

/* get device ID */ 
#define ITRAXIOCGID                _IOR('E', 0x02, short[4])  

/* get device name */
#define ITRAXIOCGNAME(len)         _IOC(_IOC_READ, 'E', 0x06, len)   

/* get abs value/limits */
#define ITRAXIOCGABS(abs)          _IOR('E', 0x40 + abs, int[5])  

/* set filter  */
#define ITRAXIOCSFILTER            _IOR('E', 0x45,struct filtercontrol *)

/* get filter  */
#define ITRAXIOCGFILTER            _IOR('E', 0x46,struct filtercontrol * ) 
 




struct trackerposition {
  struct timeval time;
  int raw[3];
  int tenthdegree[3];
};



#endif
