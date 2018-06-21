
#ifndef _SD_GPIO_H_
#define _SD_GPIO_H_

/* Use sysfs for ALL gpio activity?
   comment out to use memory where we can
*/
//#define GPIO_SYSFS_MODE


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

#ifndef GPIO_SYSFS_MODE
  #define GPIO_BASE 0x20200000
  #define GPIO_LEN  0xB4

  #define GPSET0     7
  #define GPSET1     8

  #define GPCLR0    10
  #define GPCLR1    11

  #define GPLEV0    13
  #define GPLEV1    14

  // Not used yet.
  #define GPPUD     37
  #define GPPUDCLK0 38
  #define GPPUDCLK1 39
#endif



//#ifndef SYSFS_MODE
bool pinExport(unsigned pin);
bool pinUnexport(unsigned pin);
bool isExported(unsigned pin);
int pinMode(unsigned gpio, unsigned mode);
int getPinMode(unsigned gpio);
int digitalRead(unsigned gpio);
int digitalWrite(unsigned gpio, unsigned level);
int setPullUpDown(unsigned gpio, unsigned pud);
bool gpioSetup();
void gpioShutdown();
bool registerGPIOinterrupt(int pin, int mode, void (*function)(void *args), void *args );
/*
#else
bool pinExport(int pin);
bool pinUnexport(int pin);
bool pinMode (int pin, int mode);
int digitalRead (int pin);
bool digitalWrite (int pin, int value);
#endif
*/

#endif /* _SD_GPIO_H_ */
