/* daemon.c */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#ifndef _UTILS_C_
#define _UTILS_C_
#endif

#include "utils.h"
#include "config.h"

#define MAXCFGLINE 265

#define DEBUGFILE     "/var/log/gpioctrld.log"

/*  _var = local
    _var_ = global
*/

bool _daemon_ = false;
//bool _debuglog_ = false;
bool _debug2file_ = false;

/*
* This function reports the error and
* exits back to the shell:
*/
void displayLastSystemError (const char *on_what)
{
  fputs (strerror (errno), stderr);
  fputs (": ", stderr);
  fputs (on_what, stderr);
  fputc ('\n', stderr);

  if (_daemon_ == true)
  {
    logMessage (LOG_ERR, "%d : %s", errno, on_what);
    closelog ();
  }
}

/*
From -- syslog.h --
#define	LOG_EMERG	0	// system is unusable 
#define	LOG_ALERT	1	// action must be taken immediately 
#define	LOG_CRIT	2	// critical conditions 
#define	LOG_ERR		3	// error conditions 
#define	LOG_WARNING	4	// warning conditions 
#define	LOG_NOTICE	5	// normal but significant condition 
#define	LOG_INFO	6	// informational 
#define	LOG_DEBUG	7	// debug-level messages 
*/

char *elevel2text(int level) 
{
  switch(level) {
    case LOG_ERR:
      return "Error:";
      break;
    case LOG_DEBUG:
      return "Debug:";
      break;
    case LOG_NOTICE:
      return "Notice:";
      break;
    case LOG_INFO:   
      return "Info:";
      break;
    case LOG_WARNING:
    default:
      return "Warning:";
      break;
  }
  
  return "";
}

int text2elevel(char* level)
{
  if (strcmp(level, "DEBUG") == 0) {
    return LOG_DEBUG;
  }
  else if (strcmp(level, "INFO") == 0) {
    return LOG_INFO;
  }
  else if (strcmp(level, "NOTICE") == 0) {
    return LOG_NOTICE;
  }
  else if (strcmp(level, "WARNING") == 0) {
    return LOG_WARNING;
  }
  else if (strcmp(level, "ERROR") == 0) {
    return LOG_ERR;
  }
  
 return  LOG_ERR; 
}

#define MXPRNT 512

void logMessage(int level, char *format, ...)
{
  //if (_debuglog_ == false && level == LOG_DEBUG)
  //  return;
  
  char buffer[MXPRNT];
  va_list args;
  va_start(args, format);
  strncpy(buffer, "         ", 8);
  int size = vsnprintf (&buffer[8], MXPRNT-10, format, args);
  va_end(args);

  //if (_sdconfig_.log_level < LOG_ERR || _sdconfig_.log_level > LOG_DEBUG) {
  if (_sdconfig_.log_level == -1) {
    fprintf (stderr, buffer);
    syslog (level, "%s", &buffer[8]);
    closelog ();
  } else if (level > _sdconfig_.log_level) {
    return;
  }

  if (size >= MXPRNT-10 ) {
    buffer[MXPRNT-0] = '\0';
    buffer[MXPRNT-1] = '\n';
    buffer[MXPRNT-2] = '.';
    buffer[MXPRNT-3] = '.';
    buffer[MXPRNT-4] = '.';
    buffer[MXPRNT-5] = '.';
  }

  if (_daemon_ == true)
  {
    syslog (level, "%s", &buffer[8]);
    closelog ();
  } 
  
  if (_daemon_ != true || level == LOG_ERR || _debug2file_ == true)
  {
    char *strLevel = elevel2text(level);

    strncpy(buffer, strLevel, strlen(strLevel));
    if ( buffer[strlen(buffer)-1] != '\n') { 
      buffer[strlen(buffer)+1] = '\0';
      buffer[strlen(buffer)] = '\n';
    }
    
    if (_debug2file_ == true) {
      int fp = open(DEBUGFILE, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
      if (fp != -1) {
        write(fp, buffer, strlen(buffer)+1 );
        close(fp);
      } else {
        fprintf (stderr, "Can't open debug log\n %s", buffer);
      }
    } else if (level != LOG_ERR) {
      printf ("%s", buffer);
    }
    
    if (level == LOG_ERR)
      fprintf (stderr, "%s", buffer);
  }

}

void daemonise (char *pidFile, void (*main_function) (void))
{
  FILE *fp = NULL;
  pid_t process_id = 0;
  pid_t sid = 0;

  _daemon_ = true;
  
  /* Check we are root */
  if (getuid() != 0)
  {
    logMessage(LOG_ERR,"Can only be run as root\n");
    exit(EXIT_FAILURE);
  }

  int pid_file = open (pidFile, O_CREAT | O_RDWR, 0666);
  int rc = flock (pid_file, LOCK_EX | LOCK_NB);
  if (rc)
  {
    if (EWOULDBLOCK == errno)
    ; // another instance is running
    //fputs ("\nAnother instance is already running\n", stderr);
    logMessage(LOG_ERR,"\nAnother instance is already running\n");
    exit (EXIT_FAILURE);
  }

  process_id = fork ();
  // Indication of fork() failure
  if (process_id < 0)
  {
    displayLastSystemError ("fork failed!");
    // Return failure in exit status
    exit (EXIT_FAILURE);
  }
  // PARENT PROCESS. Need to kill it.
  if (process_id > 0)
  {
    fp = fopen (pidFile, "w");

    if (fp == NULL)
    logMessage(LOG_ERR,"can't write to PID file %s",pidFile);
    else
    fprintf(fp, "%d", process_id);

    fclose (fp);
    logMessage (LOG_DEBUG, "process_id of child process %d \n", process_id);
    // return success in exit status
    exit (EXIT_SUCCESS);
  }
  //unmask the file mode
  umask (0);
  //set new session
  sid = setsid ();
  if (sid < 0)
  {
    // Return failure
    displayLastSystemError("Failed to fork process");
    exit (EXIT_FAILURE);
  }
  // Change the current working directory to root.
  chdir ("/");
  // Close stdin. stdout and stderr
  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);

  // this is the first instance
  (*main_function) ();

  return;
}

int count_characters(const char *str, char character)
{
    const char *p = str;
    int count = 0;

    do {
        if (*p == character)
            count++;
    } while (*(p++));

    return count;
}

char * replace(char const * const original, char const * const pattern, char const * const replacement) 
{
  size_t const replen = strlen(replacement);
  size_t const patlen = strlen(pattern);
  size_t const orilen = strlen(original);

  size_t patcnt = 0;
  const char * oriptr;
  const char * patloc;

  // find how many times the pattern occurs in the original string
  for ( (oriptr = original); (patloc = strstr(oriptr, pattern)); (oriptr = patloc + patlen))
  {
    patcnt++;
  }

  {
    // allocate memory for the new string
    size_t const retlen = orilen + patcnt * (replen - patlen);
    char * const returned = (char *) malloc( sizeof(char) * (retlen + 1) );

    if (returned != NULL)
    {
      // copy the original string, 
      // replacing all the instances of the pattern
      char * retptr = returned;
      for ( (oriptr = original); (patloc = strstr(oriptr, pattern)); (oriptr = patloc + patlen))
      {
        size_t const skplen = patloc - oriptr;
        // copy the section until the occurence of the pattern
        strncpy(retptr, oriptr, skplen);
        retptr += skplen;
        // copy the replacement 
        strncpy(retptr, replacement, replen);
        retptr += replen;
      }
      // copy the rest of the string.
      strcpy(retptr, oriptr);
    }
    return returned;
  }
}

void run_external(char *command, int state)
{

  char *cmd = replace(command, "%STATE%", (state==1?"1":"0"));
  system(cmd);
  logMessage (LOG_DEBUG, "Ran command '%s'\n", cmd);
  free(cmd);
    
  return;
}


int str2int(const char* str, int len)
{
  int i;
  int ret = 0;
  for(i = 0; i < len; ++i)
  {
    if (isdigit(str[i]))
      ret = ret * 10 + (str[i] - '0');
    else
      break;
  }
  return ret;
}















void run_external_OLD(char *command, int state)
{
  //int status;
  // By calling fork(), a child process will be created as a exact duplicate of the calling process.
    // Search for fork() (maybe "man fork" on Linux) for more information.
  if(fork() == 0){ 
    // Child process will return 0 from fork()
    //printf("I'm the child process.\n");
    char *cmd = replace(command, "%STATE%", (state==1?"1":"0"));
    system(cmd);
    logMessage (LOG_DEBUG, "Ran command '%s'\n", cmd);
    free(cmd);
    exit(0);
  }else{
    // Parent process will return a non-zero value from fork()
    //printf("I'm the parent.\n");
    
  }

  return;
}


/*
void readCfg (char *cfgFile)
{
  FILE * fp ;
  char bufr[MAXCFGLINE];
  const char delim[2] = ";";
  char *token;
  int line = 1;

  if( (fp = fopen(cfgFile, "r")) != NULL){
    while(! feof(fp)){
      if (fgets(bufr, MAXCFGLINE, fp) != NULL)
      {
        if (bufr[0] != '#' && bufr[0] != ' ' && bufr[0] != '\n')
        {
          token = strtok(bufr, delim);
          while( token != NULL ) {
            if ( token[(strlen(token)-1)] == '\n') { token[(strlen(token)-1)] = '\0'; }
            printf( "Line %d - Token %s\n", line, token );
            token = strtok(NULL, delim);
          }
          line++;
        }
      }
    }
    fclose(fp);
  } else {
    displayLastSystemError(cfgFile);
    exit (EXIT_FAILURE);
  }
}
*/
