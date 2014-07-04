#ifndef __MEMDEV_H__
#define __MEMDEV_H__

#include <linux/ioctl.h>

#define MEMDEV_IOC_MAGIC 'k'

#define PWM_IOCTL_SET_FREQ _IO(MEMDEV_IOC_MAGIC,1) 
#define PWM_IOCTL_STOP     _IOR(MEMDEV_IOC_MAGIC,2,int)
#define PWM_IOCTL_SET_DC   _IOW(MEMDEV_IOC_MAGIC,3,int)

#define MEMDEV_IOC_MAXNR 3
#endif 
