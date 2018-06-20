
#ifndef _SD_GPIO_H_
#define _SD_GPIO_H_

#include <syslog.h>
#include <stdbool.h>

#define INPUT  0
#define OUTPUT 1
 
#define LOW  0
#define HIGH 1
 
#define SYSFS_PATH_MAX 35
#define SYSFS_READ_MAX 3


/* Not used, just here for compile */
#define	PUD_OFF			 0
#define	PUD_DOWN		 1
#define	PUD_UP			 2

#define	INT_EDGE_SETUP		0
#define	INT_EDGE_FALLING	1
#define	INT_EDGE_RISING		2
#define	INT_EDGE_BOTH		3

bool pinExport(int pin);
bool pinUnexport(int pin);
bool pinMode (int pin, int mode);
int digitalRead (int pin);
bool digitalWrite (int pin, int value);



#endif /* _SD_GPIO_H_ */
