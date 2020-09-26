/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/GPIO_pi
 */

/********************->  GPIO Pi v1.2  <-********************/



#define _GPIO_pi_NAME_ "GPIO Pi"
#define _GPIO_pi_VERSION_ "1.2"

#ifndef _GPIO_pi_H_
#define _GPIO_pi_H_

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

#define INPUT     0
#define OUTPUT    1
#define IO_ALT5   2 // PWM Circuit
#define IO_ALT4   3 // SPI Circuit
#define IO_ALT0   4 // PCM Audio Circuit (Also I2C)
#define IO_ALT1   5 // SMI (Secondary Memory Interface)
#define IO_ALT2   6 // Nothing
#define IO_ALT3   7 // BSC - SPI Circuit


#define LOW  0
#define HIGH 1

#define	INT_EDGE_SETUP		0
#define	INT_EDGE_FALLING	1
#define	INT_EDGE_RISING		2
#define	INT_EDGE_BOTH		3

#define	PUD_OFF			 0
#define	PUD_DOWN		 1
#define	PUD_UP			 2

#define SYSFS_PATH_MAX 35
#define SYSFS_READ_MAX 3

#ifndef GPIO_SYSFS_MODE
  #define GPIO_BASE_P4     0xFE000000  // Pi 4
  #define GPIO_BASE_P2     0x3F000000  // Pi 2 & 3
  #define GPIO_BASE_P1     0x20000000  // Pi 1 & Zero
  #define GPIO_OFFSET      0x200000

  //#define GPIO_BASE 0x20200000
  #define GPIO_LEN     0xB4
  #define GPIO_LEN_P4  0xF1  // Pi 4 has more registers BCM2711

  #define GPSET0     7
  #define GPSET1     8

  #define GPCLR0    10
  #define GPCLR1    11

  #define GPLEV0    13
  #define GPLEV1    14

  #define GPPUD     37
  #define GPPUDCLK0 38
  #define GPPUDCLK1 39

  /* Pi4 BCM2711 has different pulls */
  #define GPPUPPDN0 57
  #define GPPUPPDN1 58
  #define GPPUPPDN2 59
  #define GPPUPPDN3 60


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
  #define PI_MODEL_3AP      14
  #define PI_MODEL_CM3P     16
  #define PI_MODEL_4B       17

#endif

#define GPIO_ERR_GENERAL    -7
#define GPIO_NOT_IO_MODE    -6
#define GPIO_NOT_OUTPUT     -5
#define GPIO_NOT_EXPORTED   -4
#define GPIO_ERR_IO         -3
#define GPIO_ERR_NOT_SETUP  -2
#define GPIO_ERR_BAD_PIN    -1
#define GPIO_OK              0

//#ifndef SYSFS_MODE
int pinExport(unsigned int gpio);
int pinUnexport(unsigned int gpio);
bool isExported(unsigned int gpio);
int pinMode(unsigned int gpio, unsigned int mode);
int getPinMode(unsigned int gpio);
int digitalRead(unsigned int gpio);
int digitalWrite(unsigned int gpio, unsigned int level);
int setPullUpDown(unsigned int gpio, unsigned int pud);
int edgeSetup (unsigned int pin, unsigned int value);
bool gpioSetup();
void gpioShutdown();
int registerGPIOinterrupt(unsigned int gpio, unsigned int mode, void (*function)(void *args), void *args );

#ifndef GPIO_SYSFS_INTERRUPT
int unregisterGPIOinterrupt(unsigned int gpio);
#endif

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
#endif /* _GPIO_pi_H_ */
