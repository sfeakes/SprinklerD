#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
/*
#include <stdint.h>
#include <sys/time.h>
#include <poll.h>
#include <pthread.h>
*/
#include "sd_GPIO.h"
#include "utils.h"
 
bool pinExport(int pin)
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
 
bool pinUnexport(int pin)
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

bool pinMode (int pin, int mode)
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
 
int digitalRead (int pin)
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

bool digitalWrite (int pin, int value)
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

/*
int waitForInterrupt (int pin, int mS)
{
  char path[SYSFS_PATH_MAX];
  int fd, x ;
  uint8_t c ;
  struct pollfd polls ;
  //struct pollfd pfd;

  sprintf(path, "/sys/class/gpio/gpio%d/value", pin);

   if ((fd = open(path, O_RDONLY)) < 0)
   {
      logMessage (LOG_ERR, "Failed to open '%s'!\n",path);
      return -2;
   }

// Setup poll structure

  polls.fd     = fd ;
  polls.events = POLLPRI | POLLERR ;

// Wait for it ...

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


static volatile int    pinPass = -1 ;
static volatile void(*functionPass);
static volatile char(*argsPass);

static pthread_mutex_t pinMutex ;

static void *interruptHandler (void *arg)
{
  int myPin ;

  //(void)piHiPri (55) ;	// Only effective if we run as root

  myPin   = pinPass ;
  pinPass = -1 ;

  for (;;)
    if (waitForInterrupt (myPin, -1) > 0)
      printf("READ SOMETHING\n");
      //(*function)(arg);

  return NULL ;
}


bool registerGPIOinterrupt(int pin, int mode, void (*function)(char *), void (*args) ) 
{
  pthread_t threadId ;
  // Clear any initial pending interrupt

  pinPass = pin ;

  pthread_mutex_lock (&pinMutex) ;
    if (pthread_create (&threadId, NULL, interruptHandler, NULL) < 0) 
      return false;
    else {
      while (pinPass != -1)
      delay (1) ;
    }

  pthread_mutex_unlock (&pinMutex) ;

  return true ;
}
*/







/*
#include <stdarg.h>
#include <errno.h>
#include <string.h>

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

#define PIN  4
#define POUT  27
int main(int argc, char *argv[]) {
  int repeat = 10;

  // if (-1 == GPIOExport(POUT) || -1 == GPIOExport(PIN))
  //              return(1);

  if (-1 == pinMode(POUT, OUTPUT) || -1 == pinMode(PIN, INPUT))
    return (2);

  do {

		printf("I'm writing to GPIO %d (input)\n", digitalRead(PIN), PIN);
    if (-1 == digitalWrite(POUT, repeat % 2))
      return (3);


    printf("I'm reading %d in GPIO %d (input)\n", digitalRead(PIN), PIN);
    printf("I'm reading %d in GPIO %d (output)\n", digitalRead(POUT), POUT);

    usleep(500 * 1000);
  } while (repeat--);

  if (-1 == pinUnexport(POUT) || -1 == pinUnexport(PIN))
    return (4);

  return (0);
}
*/