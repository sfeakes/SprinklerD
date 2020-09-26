
#ifndef _SD_GPIO_H_
#define _SD_GPIO_H_

/* Use sysfs for ALL gpio activity?
   comment out to use memory where we can
*/
//#define GPIO_SYSFS_MODE


#include <syslog.h>
#include <stdbool.h>

// check number is between 2 and 27
#ifndef USE_WIRINGPI
#define GPIO_MIN 2
#define GPIO_MAX 27
#else // WiringPI valid numbers
#define GPIO_MIN 0
#define GPIO_MAX 30
#endif

#define validGPIO(X)  ((X) <= (GPIO_MAX) ? ( ((X) >= (GPIO_MIN) ? (1) : (0)) ) : (0))

#ifndef USE_WIRINGPI // Don't include anything below this line if using wiringpi.

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
  #define GPIO_BASE_P2     0x3F000000
  #define GPIO_BASE_P1     0x20000000
  #define GPIO_OFFSET      0x200000
  //#define GPIO_BASE 0x20200000
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

  #define	PI_MODEL_UNKNOWN -1
  #define	PI_MODEL_A		    0
  #define	PI_MODEL_B		    1
  #define	PI_MODEL_AP		    2
  #define	PI_MODEL_BP		    3
  #define	PI_MODEL_2	      4
  #define	PI_ALPHA		      5
  #define	PI_MODEL_CM		    6
  #define	PI_MODEL_07		    7
  #define	PI_MODEL_3		    8
  #define	PI_MODEL_ZERO		  9
  #define	PI_MODEL_CM3		  10
  #define	PI_MODEL_ZERO_W		12
  #define PI_MODEL_3P       13

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

#endif /* WiringPI */
#endif /* _SD_GPIO_H_ */
