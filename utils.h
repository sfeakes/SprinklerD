
#include <syslog.h>
#include <stdbool.h>

#ifndef UTILS_H_
#define UTILS_H_

#ifndef EXIT_SUCCESS
  #define EXIT_FAILURE 1
  #define EXIT_SUCCESS 0
#endif


void daemonise ( char *pidFile, void (*main_function)(void) );
//void debugPrint (char *format, ...);
void displayLastSystemError (const char *on_what);
void logMessage(int level, char *format, ...);
int count_characters(const char *str, char character);
void run_external(char *command, int state);
int str2int(const char* str, int len);
//void readCfg (char *cfgFile);
int text2elevel(char* level);
char *elevel2text(int level);


//#ifndef _UTILS_C_
  extern bool _daemon_;
  //extern bool _debuglog_;
  extern bool _debug2file_;
//#endif

#endif /* UTILS_H_ */
