#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
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

#include "utils.h"
#include "sd_GPIO.h"

const char *_piModelNames [16] =
  {
    "Model A",    //  0
    "Model B",    //  1
    "Model A+",   //  2
    "Model B+",   //  3
    "Pi 2",       //  4
    "Alpha",      //  5
    "CM",         //  6
    "Unknown07",  // 07
    "Pi 3",       // 08
    "Pi Zero",    // 09
    "CM3",        // 10
    "Unknown11",  // 11
    "Pi Zero-W",  // 12
    "Pi 3+",      // 13
    "Unknown New 14",       // 14
    "Unknown New 15",       // 15
  } ;

static bool _ever = false;
void gpioDelay (unsigned int howLong);

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
    logMessage (LOG_ERR, "piBoardId: Unable to open /proc/cpuinfo") ;

  while (fgets (line, 120, cpuFd) != NULL)
    if (strncmp (line, "Revision", 8) == 0)
      break ;

  fclose (cpuFd) ;

  if (strncmp (line, "Revision", 8) != 0)
    logMessage (LOG_ERR, "piBoardId: No \"Revision\" line") ;

// Chomp trailing CR/NL
  for (c = &line [strlen (line) - 1] ; (*c == '\n') || (*c == '\r') ; --c)
    *c = 0 ;
  
  logMessage (LOG_DEBUG, "piBoardId: Revision string: %s\n", line) ;

// Scan to the first character of the revision number

  for (c = line ; *c ; ++c)
    if (*c == ':')
      break ;

  if (*c != ':')
    logMessage (LOG_ERR, "piBoardId: Unknown \"Revision\" line (no colon)") ;

// Chomp spaces

  ++c ;
  while (isspace (*c))
    ++c ;

  if (!isxdigit (*c))
    logMessage (LOG_ERR, "piBoardId: Unknown \"Revision\" line (no hex digit at start of revision)") ;

  revision = (unsigned int)strtol (c, NULL, 16) ; // Hex number with no leading 0x
   
// Check for new way:

  if ((revision &  (1 << 23)) != 0)	// New way
  {/*
    bRev      = (revision & (0x0F <<  0)) >>  0 ;*/
    bType     = (revision & (0xFF <<  4)) >>  4 ;
		/*
    bProc     = (revision & (0x0F << 12)) >> 12 ;	// Not used for now.
    bMfg      = (revision & (0x0F << 16)) >> 16 ;
    bMem      = (revision & (0x07 << 20)) >> 20 ;
    bWarranty = (revision & (0x03 << 24)) != 0 ;
    */

    logMessage (LOG_DEBUG, "piBoard Model: %s\n", _piModelNames[bType]) ;
    
    return bType;
  }

  logMessage (LOG_ERR, "piBoard Model: UNKNOWN\n");
  return PI_MODEL_UNKNOWN;
}

bool gpioSetup() {
	int fd;
	unsigned int piGPIObase = 0;

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
      break ;

    default:
      piGPIObase = (GPIO_BASE_P2 + GPIO_OFFSET);
      break ;
  }

   fd = open("/dev/mem", O_RDWR | O_SYNC);

   if (fd<0)
   {
			logMessage (LOG_ERR, "Failed to open '/dev/mem' for GPIO access (are we root?)\n"); 
      return false;
   }

   _gpioReg = mmap
   (
      0,
      GPIO_LEN,
      PROT_READ|PROT_WRITE|PROT_EXEC,
      MAP_SHARED|MAP_LOCKED,
      fd,
      piGPIObase);

   close(fd);

  _ever = true;

	 return true;
}

int pinMode(unsigned gpio, unsigned mode) {
  int reg, shift;

  reg = gpio / 10;
  shift = (gpio % 10) * 3;

  _gpioReg[reg] = (_gpioReg[reg] & ~(7 << shift)) | (mode << shift);

  return true;
}

int getPinMode(unsigned gpio) {
  int reg, shift;

  reg = gpio / 10;
  shift = (gpio % 10) * 3;

  return (*(_gpioReg + reg) >> shift) & 7;
}

int digitalRead(unsigned gpio) {
  unsigned bank, bit;

  bank = gpio >> 5;

  bit = (1 << (gpio & 0x1F));

  if ((*(_gpioReg + GPLEV0 + bank) & bit) != 0)
    return 1;
  else
    return 0;
}

int digitalWrite(unsigned gpio, unsigned level) {
  unsigned bank, bit;

  bank = gpio >> 5;

  bit = (1 << (gpio & 0x1F));

  if (level == 0)
    *(_gpioReg + GPCLR0 + bank) = bit;
  else
    *(_gpioReg + GPSET0 + bank) = bit;

  return 0;
}

int setPullUpDown(unsigned gpio, unsigned pud)
{
	unsigned bank, bit;

  bank = gpio >> 5;

  bit = (1 << (gpio & 0x1F));

	/*
   if (gpio > PI_MAX_GPIO)
      SOFT_ERROR(PI_BAD_GPIO, "bad gpio (%d)", gpio);
*/
   if (pud > PUD_UP || pud < PUD_OFF)
	   return false;
      //SOFT_ERROR(PI_BAD_PUD, "gpio %d, bad pud (%d)", gpio, pud);

   *(_gpioReg + GPPUD) = pud;
   gpioDelay(1);
   *(_gpioReg + GPPUDCLK0 + bank) = bit;
   gpioDelay(1);
   *(_gpioReg + GPPUD) = 0;

   *(_gpioReg + GPPUDCLK0 + bank) = 0;

   return true;
}
#else

bool gpioSetup() {return true;}

int pinMode (unsigned pin, unsigned mode)
{
	//static const char s_directions_str[]  = "in\0out\0";

	char path[SYSFS_PATH_MAX];
	int fd;
 /*
  if ( pinExport(pin) != true) {
    logMessage (LOG_DEBUG, "start pinMode (pinExport) failed\n");
	  return false;
  }
*/
	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio direction for writing!\n");
    logMessage (LOG_ERR, "Failed to open gpio '%s' for writing!\n",path);
		return false;
	}
 
	//if (-1 == write(fd, &s_directions_str[INPUT == mode ? 0 : 3], INPUT == mode ? 2 : 3)) {
	if (-1 == write(fd, (INPUT==mode?"in\n":"out\n"),(INPUT==mode?3:4))) {
		//fprintf(stderr, "Failed to set direction!\n");
    logMessage (LOG_ERR, "Failed to setup gpio input/output on '%s'!\n",path);
		displayLastSystemError("");
		return false;
	}
  
	close(fd);

	return true;
}

int getPinMode(unsigned gpio) {
	char path[SYSFS_PATH_MAX];
	char value_str[SYSFS_READ_MAX];
	int fd;

  snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/direction", gpio);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio direction for writing!\n");
    logMessage (LOG_ERR, "Failed to open gpio '%s' for reading!\n",path);
		return false;
	}

	if (-1 == read(fd, value_str, SYSFS_READ_MAX)) {
		//fprintf(stderr, "Failed to read value!\n");
    logMessage (LOG_ERR, "Failed to read value on '%s'!\n",path);
		displayLastSystemError("");
		return(-1);
	}
 
	close(fd);

	if (strcasecmp(value_str, "out")==0)
	  return OUTPUT;
	
	return INPUT;
}

int digitalRead (unsigned pin)
{
	char path[SYSFS_PATH_MAX];
	char value_str[SYSFS_READ_MAX];
	int fd;
 
	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio value for reading!\n");
    logMessage (LOG_ERR, "Failed to open gpio '%s' for reading!\n",path);
		return(-1);
	}
 
	if (-1 == read(fd, value_str, SYSFS_READ_MAX)) {
		//fprintf(stderr, "Failed to read value!\n");
    logMessage (LOG_ERR, "Failed to read value on '%s'!\n",path);
		displayLastSystemError("");
		return(-1);
	}
 
	close(fd);
 
	return(atoi(value_str));
}

int digitalWrite (unsigned pin, unsigned value)
{
	//static const char s_values_str[] = "01";
 
	char path[SYSFS_PATH_MAX];
	int fd;
 
	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio value for writing!\n");
    logMessage (LOG_ERR, "Failed to open gpio '%s' for writing!\n",path);
		return false;
	}
 
	//if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
	if (1 != write(fd, (LOW==value?"0":"1"), 1)) {
		//fprintf(stderr, "Failed to write value!\n");
    logMessage (LOG_ERR, "Failed to write value to '%s'!\n",path);
    displayLastSystemError("");
		return false;
	}
 
	close(fd);
	return true;
}
#endif

void gpioDelay (unsigned int howLong) // Microseconds (1000000 = 1 second)
{
  struct timespec sleeper, dummy ;

  sleeper.tv_sec  = (time_t)(howLong / 1000) ;
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

  nanosleep (&sleeper, &dummy) ;
}

bool isExported(unsigned pin)
{
	char path[SYSFS_PATH_MAX];
	struct stat sb;

	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/", pin);

  if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode))
    return true;
	else
	  return false;
}

bool pinExport(unsigned pin)
{

	char buffer[SYSFS_READ_MAX];
	ssize_t bytes_written;
	int fd;
 
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open export for writing!\n");
    logMessage (LOG_ERR, "Failed to open '/sys/class/gpio/export' for writing!\n"); 
		return false;
	}
 
	bytes_written = snprintf(buffer, SYSFS_READ_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return true;
}
 
bool pinUnexport(unsigned pin)
{
	char buffer[SYSFS_READ_MAX];
	ssize_t bytes_written;
	int fd;
 
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open unexport for writing!\n");
    logMessage (LOG_ERR, "Failed to open '/sys/class/gpio/unexport' for writing!\n");
		return false;
	}
 
	bytes_written = snprintf(buffer, SYSFS_READ_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return true;
}

bool edgeSetup (unsigned pin, unsigned value)
{
	//static const char s_values_str[] = "01";
 
	char path[SYSFS_PATH_MAX];
	int fd;
 
	snprintf(path, SYSFS_PATH_MAX, "/sys/class/gpio/gpio%d/edge", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		//fprintf(stderr, "Failed to open gpio value for writing!\n");
    logMessage (LOG_ERR, "Failed to open gpio '%s' for writing!\n",path);
		return false;
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
    logMessage (LOG_ERR, "Failed to setup edge on '%s'!\n",path);
    displayLastSystemError("");
		return false;
	}
 
	close(fd);
	return true;
}

#include <poll.h>
#include <pthread.h> 
#include <sys/ioctl.h>

struct threadGPIOinterupt{
	void (*function)(void *args);
	void *args;
	unsigned pin;
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
			printf("Closing fd\n");
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
    logMessage (LOG_ERR, "Failed to open '%s'!\n",path);
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
			printf("SOMETHING FAILED, reset\n");
			gpioDelay(1);
		}
	}

  printf("interruptHandler ended\n");

  close(fd);
  return NULL ;
}


bool registerGPIOinterrupt(int pin, int mode, void (*function)(void *args), void *args ) 
{
  pthread_t threadId ;
	struct threadGPIOinterupt stuff;
	// Check it's exported
	if (! isExported(pin))
	  pinExport(pin);

  // if the pin is putput, set as input to setup edge then reset to output.
  if (getPinMode(pin) == OUTPUT) {
		pinMode(pin, INPUT);
		edgeSetup(pin, mode);
		pinMode(pin, OUTPUT);
	} else {
	  edgeSetup(pin, mode);
	}

  stuff.function = function;
	stuff.args = args;
	stuff.pin = pin;

  pthread_mutex_lock (&pinMutex) ;
    if (pthread_create (&threadId, NULL, interruptHandler, (void *)&stuff) < 0) 
      return false;
    else {
			while (stuff.pin == pin)
			gpioDelay(1);
    }

  pthread_mutex_unlock (&pinMutex) ;

  return true ;
}


//#define TEST_HARNESS

#ifdef TEST_HARNESS

#define GPIO_OFF   0x00005000  /* Offset from IO_START to the GPIO reg's. */

/* IO_START and IO_BASE are defined in hardware.h */

#define GPIO_START (IO_START_2 + GPIO_OFF) /* Physical addr of the GPIO reg. */
#define GPIO_BASE_NEW  (IO_BASE_2  + GPIO_OFF) /* Virtual addr of the GPIO reg. */

#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>

void *myCallBack(void * args) {
  printf("Ping\n");
	//struct threadGPIOinterupt *stuff = (struct threadGPIOinterupt *) args;
	//printf("Pin is %d\n",stuff->pin);
}

void logMessage(int level, char *format, ...)
{
  //if (_debuglog_ == false && level == LOG_DEBUG)
  //  return;
  
  char buffer[256];
  va_list args;
  va_start(args, format);
  strncpy(buffer, "         ", 8);
  int size = vsnprintf (&buffer[8], 246-10, format, args);
  va_end(args);

	fprintf (stderr, buffer);
}
void displayLastSystemError (const char *on_what)
{
  fputs (strerror (errno), stderr);
  fputs (": ", stderr);
  fputs (on_what, stderr);
  fputc ('\n', stderr);

  logMessage (LOG_ERR, "%d : %s", errno, on_what);

}

#define PIN  17
#define POUT  27
int main(int argc, char *argv[]) {
  int repeat = 3;

  // if (-1 == GPIOExport(POUT) || -1 == GPIOExport(PIN))
  //              return(1);
  gpioSetup();
/*
	pinUnexport(POUT);
	pinUnexport(PIN);
	pinExport(POUT);
	pinExport(PIN);
*/
  sleep(1);

	//edgeSetup(POUT, INT_EDGE_BOTH);

  if (-1 == pinMode(POUT, OUTPUT) || -1 == pinMode(PIN, INPUT))
    return (2);

  //edgeSetup(PIN, INT_EDGE_RISING);
  //edgeSetup(POUT, INT_EDGE_RISING);

  if (pinExport(POUT) != true) 
		printf("Error exporting pin\n");
	/*
  if (registerGPIOinterrupt(POUT, INT_EDGE_RISING, (void *)&myCallBack, (void *)&repeat ) != true)
	  printf("Error registering interupt\n");

	if (registerGPIOinterrupt(PIN, INT_EDGE_RISING, (void *)&myCallBack, (void *)&repeat ) != true)
	  printf("Error registering interupt\n");
	*/

  do {

		printf("Writing %d to GPIO %d\n", repeat % 2, POUT);
    if (-1 == digitalWrite(POUT, repeat % 2))
      return (3);


    printf("Read %d from GPIO %d (input)\n", digitalRead(PIN), PIN);
    printf("Read %d from GPIO %d (output)\n", digitalRead(POUT), POUT);

    usleep(500 * 1000);
  } while (repeat--);

  gpioShutdown();
	
  sleep(1);

  if (-1 == pinUnexport(POUT) || -1 == pinUnexport(PIN))
    return (4);

  sleep(1);

  return (0);
}
#endif