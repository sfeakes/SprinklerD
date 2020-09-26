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


/*
* Note, all referances to pin in this code is the GPIO pin# not the physical pin#
*/

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <errno.h>

#include "GPIO_Pi.h"

#ifndef GPIO_ERR_LOG_DISABELED
  //#define LOG_ERROR(fmt, ...) log_message (true, fmt, ##__VA_ARGS__)
  #define LOG_ERROR(fmt, ...) log_message (true, "%s: " fmt, _GPIO_pi_NAME_,##__VA_ARGS__)
#else
  #define LOG_ERROR(...) {}
#endif

#ifdef GPIO_LOG_ENABLED
  //#define LOG_ERROR(fmt, ...) log_message (false, fmt, ##__VA_ARGS__)
  #define LOG(fmt, ...) log_message (false, "%s: " fmt, _GPIO_pi_NAME_,##__VA_ARGS__)
#else
  #define LOG(...) {}
#endif

#ifdef GPIO_DEBUG
  #define DEBUG(fmt, ...) log_message (false, "%s: (%s) " fmt, _GPIO_pi_NAME_,"DEBUG",##__VA_ARGS__)
#else
  #define DEBUG(...) {}
#endif


#ifdef GPIO_SYSFS_MODE
  #define GPIO_SYSFS_INTERRUPT
#endif

const char *_piModelNames [18] =
  {
    "Model A",    //  0
    "Model B",    //  1
    "Model A+",   //  2
    "Model B+",   //  3
    "Pi 2",       //  4
    "Alpha",      //  5
    "CM",         //  6
    "Unknown #07",// 07
    "Pi 3b",      // 08
    "Pi Zero",    // 09
    "CM3",        // 10
    "Unknown #11",// 11
    "Pi Zero-W",  // 12
    "Pi 3b+",     // 13
    "Pi 3a+",     // 14
    "Unknown #15",// 15
    "Pi CM3+",    // 16
    "Pi 4b",      // 17
  } ;

static bool _ever = false;
static bool _supressLogging = false;
//static bool _GPIO_setup = false;
static bool _usingGpioMem = false;   

/* RPI 4 has different pullup registers - we need to know if we have that type */
static bool _pud_type_rpi4 = false;
//static uint8_t _pud_type_rpi4 = 0;

/* RPI 4 has different pullup operation - make backwards compat */
//static uint8_t _pud_compat_setting = PUD_OFF;

//#define GPIOrunning(X)  ((X) <= (GPIO_MAX) ? ( ((X) >= (GPIO_MIN) ? (1) : (0)) ) : (0))
#define GPIOrunning()  (_ever)

void gpioDelay (unsigned int howLong);

void log_message(bool critical, char *format, ...)
{
  if (_supressLogging && !critical)
    return;

  va_list arglist;
  va_start( arglist, format );

  // if terminal don't log to syslod
  if ( ! isatty(1) ) {
    openlog(_GPIO_pi_NAME_, LOG_NDELAY, LOG_DAEMON);
    vsyslog(critical?LOG_ERR:LOG_INFO, format, arglist);
    closelog();
  } else if (critical == false) {
    vfprintf(stdout, format, arglist );
    fflush(stdout);
  }

  // Always send critical errors to stderr
  if (critical) {
    vfprintf(stderr, format, arglist );
  }
  
  va_end( arglist );
}

void printVersionInformation()
{
#ifdef GPIO_SYSFS_MODE
  LOG ("(sysfs) v%s\n",_GPIO_pi_VERSION_);
#else
  LOG ("v%s\n",_GPIO_pi_VERSION_);
#endif
}

void gpioDelay (unsigned int howLong) // Microseconds (1000000 = 1 second)
{
  struct timespec sleeper, dummy ;

  sleeper.tv_sec  = (time_t)(howLong / 1000) ;
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

  nanosleep (&sleeper, &dummy) ;
}

#ifndef GPIO_SYSFS_MODE

static volatile uint32_t  * _gpioReg = MAP_FAILED;

int piBoardId ()
{
  FILE *cpuFd ;
  char line [120] ;
  char *c ;
  unsigned int revision ;
  //int bRev, bType, bProc, bMfg, bMem, bWarranty;
	int bType;

  if ((cpuFd = fopen ("/proc/cpuinfo", "r")) == NULL)
    LOG_ERROR ( "Unable to open /proc/cpuinfo") ;

  while (fgets (line, 120, cpuFd) != NULL)
    if (strncmp (line, "Revision", 8) == 0)
      break ;

  fclose (cpuFd) ;

  if (strncmp (line, "Revision", 8) != 0)
    LOG_ERROR ( "Unable to determin pi Board \"Revision\" line") ;

// Chomp trailing CR/NL
  for (c = &line [strlen (line) - 1] ; (*c == '\n') || (*c == '\r') ; --c)
    *c = 0 ;
  
  DEBUG ( "pi Board Revision string: %s\n", line) ;

// Scan to the first character of the revision number

  for (c = line ; *c ; ++c)
    if (*c == ':')
      break ;

  if (*c != ':')
    LOG_ERROR ( "Unknown pi Board \"Revision\" line (no colon)") ;

// Chomp spaces

  ++c ;
  while (isspace (*c))
    ++c ;

  if (!isxdigit (*c))
    LOG_ERROR ( "Unknown pi Board \"Revision\" line (no hex digit at start of revision)") ;

  revision = (unsigned int)strtol (c, NULL, 16) ; // Hex number with no leading 0x
   
  if ((revision &  (1 << 23)) != 0)	// New way
  {
    /*
    bRev      = (revision & (0x0F <<  0)) >>  0 ;*/
    bType     = (revision & (0xFF <<  4)) >>  4 ;
		/*
    bProc     = (revision & (0x0F << 12)) >> 12 ;	// Not used for now.
    bMfg      = (revision & (0x0F << 16)) >> 16 ;
    bMem      = (revision & (0x07 << 20)) >> 20 ;
    bWarranty = (revision & (0x03 << 24)) != 0 ;
    */

    LOG ("pi Board Model: %s\n", _piModelNames[bType]) ;
    
    return bType;
  }

  LOG_ERROR ( "pi Board Model: UNKNOWN\n");
  return PI_MODEL_UNKNOWN;
}

bool gpioSetup() {
	int fd;
	unsigned int piGPIObase = 0;
  unsigned int piGPIOlen = GPIO_LEN;

  printVersionInformation();

	switch ( piBoardId() )
  {
    case PI_MODEL_A:	
    case PI_MODEL_B:
    case PI_MODEL_AP:	
    case PI_MODEL_BP:
    case PI_ALPHA:	
    case PI_MODEL_CM:
    case PI_MODEL_ZERO:	
    case PI_MODEL_ZERO_W:
    //case PI_MODEL_UNKNOWN:
      piGPIObase = (GPIO_BASE_P1 + GPIO_OFFSET);
      piGPIOlen = GPIO_LEN;
      break ;

    case PI_MODEL_4B:
      piGPIObase = (GPIO_BASE_P4 + GPIO_OFFSET);
      piGPIOlen = GPIO_LEN_P4;
      _pud_type_rpi4 = 1;
      break ;

    default: // Pi 2 and 3
      piGPIObase = (GPIO_BASE_P2 + GPIO_OFFSET);
      piGPIOlen = GPIO_LEN;
      break ;
  }

   //fd = open("/dev/mem", O_RDWR | O_SYNC);

  if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0)
  {
    //LOG_ERROR ( "Failed to open '/dev/mem' for GPIO access (are we root?)\n"); 
    //return false;
    // Something odd going on with writes on /dev/gpiomem, need to fix this.
      if ((fd = open ("/dev/gpiomem", O_RDWR | O_SYNC | O_CLOEXEC) ) >= 0)	// We're using gpiomem
      {
        piGPIObase   = 0 ;
        _usingGpioMem = true ;
        LOG ("Using /dev/gpiomem!\n");
      } else {
			  LOG_ERROR ( "Failed to open '/dev/mem' or '/dev/gpiomem' for GPIO access (are we root?)\n"); 
        return false;
      }
   }

   _gpioReg = mmap
   (
      0,
      piGPIOlen,
      PROT_READ|PROT_WRITE|PROT_EXEC,
      MAP_SHARED|MAP_LOCKED,
      fd,
      piGPIObase);

   close(fd);

  _ever = true;
  //_GPIO_setup = true;

	return true;
}

int pinMode(unsigned int gpio, unsigned int mode) {
  int reg, shift;

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! GPIOrunning())
    return GPIO_ERR_NOT_SETUP;

  if (mode < INPUT || mode > IO_ALT3)
    return GPIO_NOT_IO_MODE;

  reg = gpio / 10;
  shift = (gpio % 10) * 3;

  _gpioReg[reg] = (_gpioReg[reg] & ~(7 << shift)) | (mode << shift);

  return GPIO_OK;
}

int getPinMode(unsigned int gpio) {
  int reg, shift;

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! GPIOrunning())
    return GPIO_ERR_NOT_SETUP;

  reg = gpio / 10;
  shift = (gpio % 10) * 3;

  //DEBUG("getPinMode read %d from GPIO %d\n",(*(_gpioReg + reg) >> shift) & 7,gpio);

  return (*(_gpioReg + reg) >> shift) & 7;
}

int digitalRead(unsigned int gpio) {
  unsigned int bank, bit;

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! GPIOrunning())
    return GPIO_ERR_NOT_SETUP;

  bank = gpio >> 5;
  bit = (1 << (gpio & 0x1F));

  if ((*(_gpioReg + GPLEV0 + bank) & bit) != LOW)
    return HIGH;
  else
    return LOW;
}

int digitalWrite(unsigned int gpio, unsigned int level) {
  unsigned int bank, bit;

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! GPIOrunning())
    return GPIO_ERR_NOT_SETUP;

  bank = gpio >> 5;
  bit = (1 << (gpio & 0x1F));

  DEBUG("Writing %d to GPIO %d\n",level==LOW?0:1,gpio);

  if (level == LOW)
    *(_gpioReg + GPCLR0 + bank) = bit;
  else
    *(_gpioReg + GPSET0 + bank) = bit;

  return GPIO_OK;
}

int setPullUpDown_Pi4(unsigned int gpio, unsigned int pud)
{
  if( _pud_type_rpi4 )
  {
    unsigned int pull, bit;

    LOG("Pi4 setPullUpDown() expermental support!");

    switch (pud)
    {
      case PUD_OFF:  pull = 0; break;
      case PUD_UP:   pull = 1; break;
      case PUD_DOWN: pull = 2; break;
      default:
        return GPIO_ERR_GENERAL;
      break;
    }
    int shift = (gpio & 0xf) << 1;

    bit = *(_gpioReg + GPPUPPDN0 + (gpio>>4));
    bit &= ~(3 << shift);
    bit |= (pull << shift);
    *(_gpioReg + GPPUPPDN0 + (gpio>>4)) = bit;

    return GPIO_OK;
  }

  return GPIO_ERR_GENERAL;
}

int setPullUpDown(unsigned int gpio, unsigned int pud)
{
	unsigned int bank, bit;

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! GPIOrunning())
    return GPIO_ERR_NOT_SETUP;

  if (pud > PUD_UP || pud < PUD_OFF)
	  return GPIO_ERR_GENERAL;

  if( _pud_type_rpi4 ) {
    return setPullUpDown_Pi4(gpio, pud);
  }

  bank = gpio >> 5;
  bit = (1 << (gpio & 0x1F));

  *(_gpioReg + GPPUD) = pud;
  gpioDelay(1);
  *(_gpioReg + GPPUDCLK0 + bank) = bit;
  gpioDelay(1);
  *(_gpioReg + GPPUD) = 0;

  *(_gpioReg + GPPUDCLK0 + bank) = 0;

  return GPIO_OK;
}



#else  // If GPIO_SYSFS_MODE

// There is no need to setup GPIO memory in sysfs mode, so simply set setup state.
bool gpioSetup() {
  printVersionInformation();
  _ever = true;
  return true;
}

int setPullUpDown(unsigned int gpio, unsigned int pud) {
  LOG_ERROR("setPullUpDown() not supported in sysfs mode");
  return GPIO_ERR_GENERAL;
}

int pinMode (unsigned int pin, unsigned int mode)
{
  //printVersionInformation();
	//static const char s_directions_str[]  = "in\0out\0";

  if (! validGPIO(pin))
    return GPIO_ERR_BAD_PIN;

  if (! isExported(pin))
    return GPIO_NOT_EXPORTED;

   if (mode != INPUT && mode != OUTPUT)
    return GPIO_NOT_IO_MODE;

	char path[SYSFS_PATH_MAX];
	int fd;

 /*
  if ( pinExport(pin) != true) {
    LOG ("start pinMode (pinExport) failed\n");
	  return false;
  }
*/
	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio direction for writing!\n");
    LOG_ERROR ( "Failed to open gpio '%s' for writing!\n",path);
		return GPIO_ERR_IO;
	}
 
	//if (-1 == write(fd, &s_directions_str[INPUT == mode ? 0 : 3], INPUT == mode ? 2 : 3)) {
	if (-1 == write(fd, (INPUT==mode?"in\n":"out\n"),(INPUT==mode?3:4))) {
		//fprintf(stderr, "Failed to set direction!\n");
    LOG_ERROR ( "Failed to setup gpio input/output on '%s'!\n",path);
    LOG_ERROR ( "Error (%d) - %s\n",errno, strerror (errno));
		//displayLastSystemError("");
		return GPIO_ERR_IO;
	}
  
	close(fd);

	return GPIO_OK;
}

int getPinMode(unsigned int gpio) {
	char path[SYSFS_PATH_MAX];
	char value_str[SYSFS_READ_MAX];
	int fd;

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! isExported(gpio))
    return GPIO_NOT_EXPORTED;

  snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/direction", gpio);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio direction for writing!\n");
    LOG_ERROR ( "Failed to open gpio '%s' for reading!\n",path);
		return GPIO_ERR_IO;
	}

	if (-1 == read(fd, value_str, SYSFS_READ_MAX)) {
		//fprintf(stderr, "Failed to read value!\n");
    LOG_ERROR ( "Failed to read value on '%s'!\n",path);
    LOG_ERROR ( "Error (%d) - %s\n",errno, strerror (errno));
		//displayLastSystemError("");
		return GPIO_ERR_IO;
	}
 
	close(fd);

	if (strncasecmp(value_str, "out", 3)==0)
	  return OUTPUT;
	
	return INPUT;
}

int digitalRead (unsigned int gpio)
{
	char path[SYSFS_PATH_MAX];
	char value_str[SYSFS_READ_MAX];
	int fd;

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! isExported(gpio))
    return GPIO_NOT_EXPORTED;
 
	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/value", gpio);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio value for reading!\n");
    LOG_ERROR ( "Failed to open gpio '%s' for reading!\n",path);
		return GPIO_ERR_IO;
	}
 
	if (-1 == read(fd, value_str, SYSFS_READ_MAX)) {
		//fprintf(stderr, "Failed to read value!\n");
    LOG_ERROR ( "Failed to read value on '%s'!\n",path);
    LOG_ERROR ( "Error (%d) - %s\n",errno, strerror (errno));
		//displayLastSystemError("");
		return GPIO_ERR_IO;
	}
 
	close(fd);
 
	return(atoi(value_str));
}

int digitalWrite (unsigned int gpio, unsigned int value)
{
	//static const char s_values_str[] = "01";
 
	char path[SYSFS_PATH_MAX];
	int fd;

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! isExported(gpio))
    return GPIO_NOT_EXPORTED;

  if (getPinMode(gpio) != OUTPUT)
    return GPIO_NOT_OUTPUT;
 
	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/value", gpio);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio value for writing!\n");
    LOG_ERROR ( "Failed to open gpio '%s' for writing!\n",path);
		return GPIO_ERR_IO;
	}
 
	//if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
	if (1 != write(fd, (LOW==value?"0":"1"), 1)) {
		//fprintf(stderr, "Failed to write value!\n");
    LOG_ERROR ( "Failed to write value to '%s'!\n",path);
    LOG_ERROR ( "Error (%d) - %s\n",errno, strerror (errno));
    //displayLastSystemError("");
		return GPIO_ERR_IO;
	}
 
	close(fd);
	return GPIO_OK;
}
#endif  // GPIO_SYSFS_MODE

#ifdef GPIO_SYSFS_INTERRUPT

bool isExported(unsigned int pin)
{
	char path[SYSFS_PATH_MAX];
	struct stat sb;

	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/", pin);

  if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    return true;
  } else {
	  return false;
  }
}

int pinExport(unsigned int pin)
{

	char buffer[SYSFS_READ_MAX];
	ssize_t bytes_written;
	int fd;
 
  if (! validGPIO(pin))
    return GPIO_ERR_BAD_PIN;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open export for writing!\n");
    LOG_ERROR ( "Failed to open '/sys/class/gpio/export' for writing!\n"); 
		return GPIO_ERR_IO;
	}
 
	bytes_written = snprintf(buffer, SYSFS_READ_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return GPIO_OK;
}
 
int pinUnexport(unsigned int pin)
{
	char buffer[SYSFS_READ_MAX];
	ssize_t bytes_written;
	int fd;
 
  if (! validGPIO(pin))
    return GPIO_ERR_BAD_PIN;

  if (! isExported(pin))
    return GPIO_NOT_EXPORTED;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open unexport for writing!\n");
    LOG_ERROR ( "Failed to open '/sys/class/gpio/unexport' for writing!\n");
		return GPIO_ERR_IO;
	}
 
	bytes_written = snprintf(buffer, SYSFS_READ_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return GPIO_OK;
}

int edgeSetup (unsigned int pin, unsigned int value)
{
	//static const char s_values_str[] = "01";
 
	char path[SYSFS_PATH_MAX];
	int fd;
 
  if (! validGPIO(pin))
    return GPIO_ERR_BAD_PIN;

	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/edge", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio value for writing!\n");
    LOG_ERROR ( "Failed to open gpio '%s' for writing!\n",path);
		return GPIO_ERR_IO;
	}

  int rtn = 0;
  if (value==INT_EDGE_RISING)
		rtn = write(fd, "rising", 6);
	else if (value==INT_EDGE_FALLING)
		rtn = write(fd, "falling", 7);
	else if (value==INT_EDGE_BOTH)
		rtn = write(fd, "both", 4);
	else
	  rtn = write(fd, "none", 4);
 
  if (rtn <= 0) {
    LOG_ERROR ( "Failed to setup edge on '%s'!\n",path);
    LOG_ERROR ( "Error (%d) - %s\n",errno, strerror (errno));
    //displayLastSystemError("");
		return GPIO_ERR_IO;
	}
 
	close(fd);
	return GPIO_OK;
}

#include <poll.h>
#include <pthread.h> 
#include <sys/ioctl.h>

struct threadGPIOinterupt{
	void (*function)(void *args);
	void *args;
	unsigned int pin;
};
static pthread_mutex_t pinMutex ;

#define MAX_FDS 64
static unsigned int _sysFds [MAX_FDS] =
{
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
} ;

void pushSysFds(int fd)
{
	int i;
	for (i=0; i< MAX_FDS; i++) {
		if (_sysFds[i] == -1) {
      _sysFds[i] = fd;
			return;
		}
	}
}

void gpioShutdown() {
	int i;
	_ever = false;

	for (i=0; i< MAX_FDS; i++) {
		if (_sysFds[i] != -1) {
			//printf("Closing fd %d\n",i);
      close(_sysFds[i]);
			_sysFds[i] = -1;
		} else {
			break;
		}
	}
}

int waitForInterrupt (int pin, int mS, int fd)
{
	int x;
  uint8_t c ;
  struct pollfd polls ;

  // Setup poll structure
  polls.fd     = fd ;
  polls.events = POLLPRI | POLLERR | POLLHUP | POLLNVAL;

  // Wait for something ...
  
  x = poll (&polls, 1, mS) ;

  // If no error, do a dummy read to clear the interrupt
  //	A one character read appars to be enough.

  if (x > 0)
  {
    lseek (fd, 0, SEEK_SET) ;	// Rewind
    (void)read (fd, &c, 1) ;	// Read & clear
  }

  return x ;
}

static void *interruptHandler (void *arg)
{
  struct threadGPIOinterupt *stuff = (struct threadGPIOinterupt *) arg;
	int pin = stuff->pin;
	void (*function)(void *args) = stuff->function;
	void *args = stuff->args;
	stuff->pin = -1;

	char path[SYSFS_PATH_MAX];
  int fd, count, i ;
  uint8_t c ;

  sprintf(path, "/sys/class/gpio/gpio%d/value", pin);

  if ((fd = open(path, O_RDONLY)) < 0)
  {
    LOG_ERROR ( "Failed to open '%s'!\n",path);
    return NULL;
  }

	pushSysFds(fd);

  // Clear any initial pending interrupt
	ioctl (fd, FIONREAD, &count) ;
  for (i = 0 ; i < count ; ++i)
    read (fd, &c, 1);

  while (_ever == true) {
    if (waitForInterrupt (pin, -1, fd) > 0) {
			function(args);
		} else {
			LOG_ERROR ("interruptHandler failed for GPIO %d, resetting\n", pin);
			gpioDelay(1);
		}
	}

  LOG("interruptHandler ended for GPIO %d\n", pin);

  close(fd);
  return NULL ;
}

int registerGPIOinterrupt(unsigned int pin, unsigned int mode, void (*function)(void *args), void *args ) 
{
  pthread_t threadId ;
	struct threadGPIOinterupt stuff;

  if (! validGPIO(pin))
    return false;

	// Check it's exported
	if (! isExported(pin))
	  pinExport(pin);

  // Setup pin if defined.
  if (mode != INT_EDGE_SETUP) {
  // if the pin is output, set as input to setup edge then reset to output.
    if (getPinMode(pin) == OUTPUT) {
		  pinMode(pin, INPUT);
		  edgeSetup(pin, mode);
		  pinMode(pin, OUTPUT);
	  } else {
	    edgeSetup(pin, mode);
	  }
  }
//#endif
  stuff.function = function;
	stuff.args = args;
	stuff.pin = pin;

  pthread_mutex_lock (&pinMutex) ;
    if (pthread_create (&threadId, NULL, interruptHandler, (void *)&stuff) < 0) {
      LOG_ERROR("Failed to start interruptHandler thread\n");
      return GPIO_ERR_GENERAL;
    } else {
			while (stuff.pin == pin)
			gpioDelay(1);
    }

  pthread_mutex_unlock (&pinMutex) ;

  return GPIO_OK ;
}


#else // GPIO_SYSFS_INTERRUPT

#include <pthread.h> 

struct GPIOinterupt{
	void (*function)(void *args);
	void *args;
	unsigned int pin;
  unsigned int bit;
  unsigned int bank;
  unsigned int lastValue;
  unsigned int edge;
  bool running;
  pthread_t threadId;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  struct GPIOinterupt *next;
};

#define POLL_DELAY 10

static struct GPIOinterupt *_GPIOinterupts = NULL;
static pthread_t _interruptHandlerThreadId = 0;
static pthread_mutex_t _gpioRegisterMutex;


static void *GPIOinterruptHandler (void *arg)
{
  struct GPIOinterupt *data = (struct GPIOinterupt *) arg;
  void (*function)(void *args) = data->function;
	void *args = data->args;
  data->running = true;

  DEBUG("Created Thread for GPIO interrupt %d\n",data->pin);

  pthread_mutex_lock(&data->mutex);

  while (_ever == true && data->running == true) {
    //printf("GPIOinterruptHandler() %d wait\n", data->pin);  
    pthread_cond_wait(&data->cond, &data->mutex);
    DEBUG("GPIO interrupt triggered for %d\n",data->pin);
    // recheck _ever incase this is a shutdown signal, and running incase we've been told to shutdown
    if (_ever == true && data->running == true) {
      // Check edge
      if ( (data->edge == INT_EDGE_BOTH) ||
           (data->edge == INT_EDGE_RISING && data->lastValue == HIGH) ||
           (data->edge == INT_EDGE_FALLING && data->lastValue == LOW)) 
      {
        //DEBUG("GPIO interrupt triggered for %d\n",data->pin);
        function(args);
      }
      //printf("GPIOinterruptHandler() called gpio %d\n",data->pin);
    }
  }

  pthread_mutex_unlock(&data->mutex);
  //printf("GPIOinterruptHandler() stopped gpio %d\n",data->pin);
  DEBUG("Stopping thread for GPIO interrupt %d\n",data->pin);

  free(arg);
  pthread_exit(0);
  return NULL;
}


static void *interruptHandler(void *arg)
{
  // Use the global var and not one that passed, just incase we change the global pointer in the future
  // to redefine starting position.
  //struct GPIOinterupt *GPIOinterupt = (struct GPIOinterupt *) arg;

  //unsigned int bank, bit;
  struct GPIOinterupt *i_ptr;

  while (_ever == true && _GPIOinterupts != NULL) {
    for (i_ptr = _GPIOinterupts; i_ptr != NULL; i_ptr = i_ptr->next) {
      //DEBUG ("Poll %d\n",i_ptr->pin);
      if (i_ptr->threadId <= 0) {
        if (pthread_create (&i_ptr->threadId, NULL, GPIOinterruptHandler, (void *)i_ptr) < 0) {
          LOG_ERROR("Can't create GPIO interrupt handler for GPIO %d\n",i_ptr->pin);
        }
      }

      if (i_ptr->lastValue != digitalRead(i_ptr->pin))
      //if (i_ptr->lastValue != ((*(_gpioReg + GPLEV0 + i_ptr->bank) & i_ptr->bit)==0?LOW:HIGH) )
      {
        i_ptr->lastValue = !i_ptr->lastValue;
        pthread_cond_signal(&i_ptr->cond);
      }
    }
    gpioDelay(POLL_DELAY);
  }

  // signel so thread can shutdown
  for (i_ptr = _GPIOinterupts; i_ptr != NULL; i_ptr = i_ptr->next) {
    if (i_ptr->threadId > 0) {
      pthread_cond_signal(&i_ptr->cond);
    }
  }

  DEBUG("interruptHandler stopped\n");
  _interruptHandlerThreadId = 0;
  pthread_exit(0);
  return NULL;
}

int registerGPIOinterrupt(unsigned int gpio, unsigned int mode, void (*function)(void *args), void *args ) 
{

  if (! validGPIO(gpio))
    return GPIO_ERR_BAD_PIN;

  if (! GPIOrunning())
    return GPIO_ERR_NOT_SETUP;

  if (mode == INT_EDGE_SETUP) {
    LOG("INT_EDGE_SETUP is only valid for sysfs mode");
    mode = INT_EDGE_BOTH;
  }

  // Lock the function, this isn't thread safe.
  pthread_mutex_lock (&_gpioRegisterMutex);

  struct GPIOinterupt *i_ptr;
  struct GPIOinterupt *interupt = calloc(1, sizeof(struct GPIOinterupt));
  
  interupt->function = function;
  interupt->args = args;
  interupt->pin = gpio;
  interupt->edge = mode;
  interupt->lastValue = digitalRead(gpio);
  interupt->threadId = 0;
  interupt->running = false;
  interupt->next = NULL;

  interupt->bank = gpio >> 5;
  interupt->bit = (1 << (gpio & 0x1F));
  //interupt->lastValue = (*(_gpioReg + GPLEV0 + i_ptr->bank) & i_ptr->bit)==0?0:1)

  if (_GPIOinterupts == NULL) {
    _GPIOinterupts = interupt;
  } else {
    for (i_ptr = _GPIOinterupts; i_ptr->next != NULL; i_ptr = i_ptr->next) {} // Simply run to the end of the list
    i_ptr->next = interupt;
  }

  if ( _interruptHandlerThreadId <= 0 ) {
    if (pthread_create (&_interruptHandlerThreadId, NULL, interruptHandler, (void *)_GPIOinterupts) < 0) {
      LOG_ERROR("Couldn't start GPIO interrupt handler\n");
      return GPIO_ERR_GENERAL;
    } else {
      DEBUG("interruptHandler started\n");
		//while (_interupts[_numInterupts-1].running == false)
		//gpioDelay(1);
    }
  }

  pthread_mutex_unlock (&_gpioRegisterMutex) ;

  return GPIO_OK;
}

int unregisterGPIOinterrupt(unsigned int gpio) 
{
  struct GPIOinterupt *i_ptr;
  struct GPIOinterupt *d_ptr;

  // Lock the function, this isn't thread safe.
  pthread_mutex_lock (&_gpioRegisterMutex);

  // Is it the first pin in the list
  if (_GPIOinterupts->pin == gpio) {
    DEBUG("First pin ADD CODE");
    d_ptr = _GPIOinterupts;
    _GPIOinterupts = d_ptr->next;
    d_ptr->running = false;
    pthread_cond_signal(&d_ptr->cond); // Wake up the thread so it can exit
    // Let's be safe, and let's hope we call this before the thread cleans itself up.
    pthread_mutex_unlock(&d_ptr->mutex);
  } else {
    for (i_ptr = _GPIOinterupts; i_ptr->next != NULL && i_ptr->next->pin != gpio; i_ptr = i_ptr->next) {}
    if (i_ptr->next != NULL) {
      d_ptr = i_ptr->next;
      DEBUG("Removing listner on pin %d\n", d_ptr->pin); 
      i_ptr->next = i_ptr->next->next;
      d_ptr->running = false;
      pthread_cond_signal(&d_ptr->cond); // Wake up the thread so it can exit
      // Let's be safe, and let's hope we call this before the thread cleans itself up.
      pthread_mutex_unlock(&d_ptr->mutex);
    } else {
      LOG_ERROR("GPIO %d not registered as listner\n",gpio);
      return GPIO_ERR_BAD_PIN;
    }
  }

  pthread_mutex_unlock (&_gpioRegisterMutex) ;

  return GPIO_OK;
}

void gpioShutdown() {
	//int i;
	_ever = false;
	// Wait enough time for interupt handler poll cycle to catch the shutdown
  gpioDelay(POLL_DELAY);
}

#endif //GPIO_SYSFS_INTERUPT


#if defined(TEST_HARNESS) || defined(GPIO_MONITOR) || defined(GPIO_RW) || defined(GPIO_TOOL)

#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

struct pin_info {
  int pin;
  int GPIO;
  char *name;
};

const struct pin_info _pinDetails[40] = {
  {1,  -1, "3.3v"},   {2, -1, "5v"},
  {3,   2, "GPIO2"},  {4, -1, "5v"},
  {5,   3, "GPIO3"},  {6, -1, "GND"},
  {7,   4, "GPIO4"},  {8,  14, "GPIO14"},
  {9,  -1, "GND"},    {10, 15, "GPIO15"},
  {11, 17, "GPIO17"}, {12, 18, "GPIO18"},
  {13, 27, "GPIO27"}, {14, -1, "GND"},
  {15, 22, "GPIO22"}, {16, 23, "GPIO23"},
  {17, -1, "3.3v"},   {18, 24, "GPIO24"},
  {19, 10, "GPIO10"}, {20, -1, "GND"},
  {21,  9, "GPIO9"},  {22, 25, "GPIO25"},
  {23, 11, "GPIO11"}, {24, 8, "GPIO8"},
  {25, -1, "GND"},    {26, 7, "GPIO7"},
  {27, -1, "DNC"},    {28, -1, "DNC"},
  {29,  5, "GPIO5"},  {30, -1, "GND"},
  {31,  6, "GPIO6"},  {32, 12, "GPIO12"},
  {33, 13, "GPIO13"}, {34, -1, "GND"},
  {35, 19, "GPIO19"}, {36, 16, "GPIO16"},
  {37, 26, "GPIO26"}, {38, 20, "GPIO20"},
  {39, -1, "GND"},    {40, 21, "GPIO21"}
};


int GPIO2physicalPin(int gpio)
{ 
  int i;
  
  for(i=0; i<40; i++) {
    if (_pinDetails[i].GPIO == gpio)
      return _pinDetails[i].pin;
  }
  return -1;
}

char *GPIOmode2txt(unsigned int mode) {
  switch (mode) {
    case INPUT:
      return "IN";
    break;
    case OUTPUT:
      return "OUT";
    break;
    case IO_ALT0:
      return "ALT0";
    break;
    case IO_ALT1:
      return "ALT1";
    break;
    case IO_ALT2:
      return "ALT2";
    break;
    case IO_ALT3:
      return "ALT3";
    break;
    case IO_ALT4:
      return "ALT4";
    break;
    case IO_ALT5:
      return "ALT5";
    break;
    default:
      return "---";
    break;
  }
}

void printGPIOstatus(int pin)
{
  printf ("GPIO %2d (Pin %2d|%-4s) = %d\n", pin ,GPIO2physicalPin(pin),GPIOmode2txt(getPinMode(pin)) , digitalRead(pin));
}

#endif //TEST_HARNESS || GPIO_MONITOR

#ifdef GPIO_TOOL

void errorParms(char *fname)
{
  printVersionInformation();
  printf("Missing Parameters:-\n\t[read|write] <pin> <value>\t- read/write to GPIO\n"
                               "\t[input|output] <pin>\t\t- set GPIO mode to input or output\n"
#ifdef GPIO_SYSFS_MODE
                               "\t[export|unexport] <pin>\t\t- (un)export GPIO, needed for sysfs mode\n"
#else
                               "\tmode <pin> <value>\t\t- set GPIO mode advanced value=0 to 7\n"
                               "\t[pud_off|pud_up|pud_down] <pin>\t- set pull up / down resistor\n"
#endif
                               "\treadall\t\t\t\t- Print information on every GPIO\n"
                               "\t\t-q\t\t\t- Optional LAST parameter to just output result\n"
                               "\teg :- %s write 17 1 -q\n",fname);
  exit(1);
}

char *GPIOitoa(int val, char *rbuf, int base) {
  static char buf[32] = {0};
  if (val < 0) {
    sprintf(rbuf,"-");
  } else if (val == 0) {
    sprintf(rbuf,"0");
  } else {
    int i=30;
    for(; val && i; --i, val /= base)
      buf[i] = "0123456789abcdef"[val % base];

    sprintf(rbuf, "%s", &buf[i+1]);
  }

  return rbuf;
}

void readAllGPIOs()
{
  int i;
  char buf1[10];
  char buf2[10];
  char buf3[10];
  char buf4[10];
  printf("-----------------------------------------------------------------\n");
  printf("| GPIO |  Name  | Mode | V |  Pin #  | V | Mode |  Name  | GPIO |\n");
  printf("+------+--------+------+---+---------+---+------+--------+------+\n");
  for (i=0; i <= 38; i+=2) {
#ifdef GPIO_SYSFS_MODE
    if (!isExported(_pinDetails[i].GPIO)) {
      pinExport(_pinDetails[i].GPIO);
      //gpioDelay(1);
    }
    if (!isExported(_pinDetails[i+1].GPIO)) {
      pinExport(_pinDetails[i+1].GPIO);
      //gpioDelay(1);
    }
#endif 
    printf("| %4s | %6s | %4s |%2s | %2d | %2d |%2s | %4s | %6s | %4s |\n",
          GPIOitoa(_pinDetails[i].GPIO,buf1,10), 
          _pinDetails[i].name, 
          _pinDetails[i].GPIO==-1?"-":GPIOmode2txt(getPinMode(_pinDetails[i].GPIO)), 
          GPIOitoa(digitalRead(_pinDetails[i].GPIO),buf2,10), 
          _pinDetails[i].pin,
          _pinDetails[i+1].pin, 
          GPIOitoa(digitalRead(_pinDetails[i+1].GPIO),buf3,10), 
          _pinDetails[i+1].GPIO==-1?"-":GPIOmode2txt(getPinMode(_pinDetails[i+1].GPIO)),
          _pinDetails[i+1].name, 
          GPIOitoa(_pinDetails[i+1].GPIO,buf4,10));
  }
  printf("-----------------------------------------------------------------\n");
}

int main(int argc, char *argv[]) {

  int pin = 0;
  int value = 0;

  if (argc < 2) {
    errorParms(argv[0]);
  } else if (strcmp (argv[argc-1], "-q") == 0) {
    _supressLogging = true;
  }


  if (! gpioSetup()) {
    LOG_ERROR ( "Failed to setup GPIO\n");
    return 1;
  }

 
  if (strcmp (argv[1], "read") == 0) {
    if (argc < 3)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    (_supressLogging?printf("%d\n",digitalRead(pin)):printGPIOstatus(pin));
  } else if (strcmp (argv[1], "write") == 0) {
    if (argc < 4)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    value = atoi(argv[3]);
    int pmode = getPinMode(pin);
    pinMode (pin, OUTPUT);
    digitalWrite(pin, value);
    if (pmode != OUTPUT) {
      if (!_supressLogging){printGPIOstatus(pin);}
      usleep(500 * 1000); // Allow any triggers to read value before reset back to origional mode
      if (!_supressLogging){printf("Resetting to input mode\n");}
      pinMode (pin, pmode);
    }
    (_supressLogging?printf("%d\n",digitalRead(pin)):printGPIOstatus(pin));
  } else if (strcmp (argv[1], "input") == 0) {
    if (argc < 3)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    int rtn = pinMode(pin, INPUT);
    (_supressLogging?printf("%d\n",rtn):printGPIOstatus(pin));
  } else if (strcmp (argv[1], "output") == 0) {
    if (argc < 3)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    int rtn = pinMode(pin, OUTPUT);
     (_supressLogging?printf("%d\n",rtn):printGPIOstatus(pin));
#ifdef GPIO_SYSFS_MODE
   } else if (strcmp (argv[1], "export") == 0) {
    if (argc < 3)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    int rtn = pinExport(pin);
    (_supressLogging?printf("%d\n",rtn):printGPIOstatus(pin));
   } else if (strcmp (argv[1], "unexport") == 0) {
    if (argc < 3)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    int rtn = pinUnexport(pin);
    printf("%d\n",rtn);
#else
  } else if (strcmp (argv[1], "mode") == 0) {
    if (argc < 4)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    int mode = atoi(argv[3]);
    int rtn = pinMode(pin, mode);
    (_supressLogging?printf("%d\n",rtn):printGPIOstatus(pin));
  } else if (strcmp (argv[1], "pud_off") == 0) {
    if (argc < 3)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    int rtn = setPullUpDown(pin, PUD_OFF);
    (_supressLogging?printf("%d\n",rtn):printGPIOstatus(pin));
  } else if (strcmp (argv[1], "pud_up") == 0) {
    if (argc < 3)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    int rtn = setPullUpDown(pin, PUD_UP);
    (_supressLogging?printf("%d\n",rtn):printGPIOstatus(pin));
  } else if (strcmp (argv[1], "pud_down") == 0) {
    if (argc < 3)
      errorParms(argv[0]);
    pin = atoi(argv[2]);
    int rtn = setPullUpDown(pin, PUD_DOWN);
    (_supressLogging?printf("%d\n",rtn):printGPIOstatus(pin));
#endif
  } else if (strcmp (argv[1], "readall") == 0) {
    readAllGPIOs();
  } else 
    errorParms(argv[0]);


  gpioShutdown();

  return 0;
}
#endif

#ifdef GPIO_RW

void errorParms()
{
  printf("Missing Parameters:-\n\t[read|write] pin <value>\n\tgpio write 17 1\n");
  exit(1);
}
int main(int argc, char *argv[]) {

  bool isWrite=false;
  int pin = 0;
  int value = 0;

  _log_level = LOG_ERR;

  if (argc < 3) {
    errorParms();
  }

  if (strcmp (argv[1], "read") == 0)
  {
    isWrite = false;
  } else if (strcmp (argv[1], "write") == 0) {
    isWrite = true;
    if (argc < 4)
      errorParms();
  } else {
    errorParms();
  }

  pin = atoi(argv[2]);
  
  if (! gpioSetup()) {
    logMessage (LOG_ERR, "Failed to setup GPIO\n");
    return 1;
  }

  if (isWrite) {
    value = atoi(argv[3]);
    int pmode = getPinMode(pin);
    pinMode (pin, OUTPUT);
    digitalWrite(pin, value);
    //if (pmode != OUTPUT)
    //  pinMode (pin, pmode);
  }
  
  printf ("%d\n", digitalRead(pin));
  

  return 0;
}
#endif

#ifdef GPIO_MONITOR

bool FOREVER = true;

void errorParms(char *fname)
{
  printVersionInformation();
  printf("Parameters:-\n\t<list of numbers>\t- GPIO's to monitor\n"\
                               "\t-q\t\t\t- Quiet mode, (must be last param) just print whats been triggered, no startup state\n"\
                               "\t-h\t\t\t- This\n"\
                               "\t<no numbers>\t\t- Monitor everything\n"\
                               "eg :- %s 2 17 27 -q\n",fname);
  exit(1);
}

void intHandler(int signum) {
  static int called=0;
  LOG ( "Stopping! - signel(%d)\n",signum);
  gpioShutdown();
/*
  gpioDelay(100);
*/
  FOREVER = false;
  called++;
  if (called > 3)
    exit(1);
}


void event_trigger (int pin)
{
  //printf("Pin %d triggered, state=%d\n",pin,digitalRead(pin));
  printGPIOstatus(pin);
}

void setup_monitor(int pin)
{
#ifdef GPIO_SYSFS_MODE
  if (! isExported(pin)) {
    pinExport(pin);
    gpioDelay(10); // 0.1 second
  }
#endif

  if (!_supressLogging){printGPIOstatus(pin);}
  if (registerGPIOinterrupt(pin, INT_EDGE_BOTH, (void *)&event_trigger, (void *)pin) != GPIO_OK)
  {
    LOG_ERROR ( "Unable to set interrupt handler for specified pin. Error (%d) - %s\n",errno, strerror (errno));
  }
   //gpioDelay(100); // REMOVE THIS WHEN DONE
}

int main(int argc, char *argv[]) {

  int i;

  if (strcmp (argv[argc-1], "-q") == 0) {
    _supressLogging = true;
    argc--;
  } else if (strcmp (argv[argc-1], "-h") == 0) {
    errorParms(argv[0]);
  } 

  if (! gpioSetup()) {
    LOG_ERROR ( "Failed to setup GPIO\n");
    return 1;
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);
  signal(SIGSEGV, intHandler);

  if (argc > 1) {
    for (i=1; i < argc; i++)
     setup_monitor( atoi(argv[i]));
  } else {
    for (i=GPIO_MIN; i <= GPIO_MAX; i++)
      setup_monitor(i);
  }

  while(FOREVER) {
    sleep(100);
  }

  return 0;
}

#endif //GPIO_MONITOR

