#include <stdio.h>
#include <time.h>
#include "config.h"
#include "utils.h"

int main (int argc, char *argv[])
{
  struct tm * timeinfo;
  char buff[20];
   _sdconfig_.log_level = LOG_DEBUG;
   readCfg("/nas/data/Development/Raspberry/sprinklerd/release/sprinklerd.conf");
   sprintf(_sdconfig_.cache_file, "/tmp/test.cache");
  
   _sdconfig_.system = true;
   _sdconfig_.delay24h = true;

   time(&_sdconfig_.delay24h_time);

   timeinfo = localtime (&_sdconfig_.delay24h_time); 
   strftime(buff, 20, "%b %d %H:%M\n", timeinfo); 
   printf("%s",buff);

   write_cache();
   read_cache(); 

   timeinfo = localtime (&_sdconfig_.delay24h_time);
   strftime(buff, 20, "%b %d %H:%M\n", timeinfo);
   printf("%s",buff);
 
   return 0;
}

