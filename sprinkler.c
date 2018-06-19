#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <libgen.h>
#include <time.h>
#include <stdbool.h>
// We want a success/failure return value from 'wiringPiSetup()'
#define WIRINGPI_CODES      1
#include <wiringPi.h>
#include "mongoose.h"

#include "utils.h"
#include "config.h"
#include "net_services.h"
#include "sd_cron.h"
#include "version.h"

//#define PIDFILE     "/var/run/fha_daemon.pid"
#define PIDLOCATION "/var/run/"
//#define CFGFILE     "./config.cfg"
//#define HTTPD_PORT 80

// Use threads to server http requests
//#define PTHREAD

/* Function prototypes */
void Daemon_Stop (int signum);
void main_loop (void);
void event_trigger (struct GPIOcfg *);
void intHandler(int signum);

//extern _sdconfig_;

/* BS functions due to limitations in wiringpi of not supporting a pointer in callback event */
// Let's hope no one wants mroe than 24 zones
void event_trigger_0 (void) { event_trigger (&_sdconfig_.zonecfg[0]) ; }
void event_trigger_1 (void) { event_trigger (&_sdconfig_.zonecfg[1]) ; }
void event_trigger_2 (void) { event_trigger (&_sdconfig_.zonecfg[2]) ; }
void event_trigger_3 (void) { event_trigger (&_sdconfig_.zonecfg[3]) ; }
void event_trigger_4 (void) { event_trigger (&_sdconfig_.zonecfg[4]) ; }
void event_trigger_5 (void) { event_trigger (&_sdconfig_.zonecfg[5]) ; }
void event_trigger_6 (void) { event_trigger (&_sdconfig_.zonecfg[6]) ; }
void event_trigger_7 (void) { event_trigger (&_sdconfig_.zonecfg[7]) ; }
void event_trigger_8 (void) { event_trigger (&_sdconfig_.zonecfg[8]) ; }
void event_trigger_9 (void) { event_trigger (&_sdconfig_.zonecfg[9]) ; }
void event_trigger_10 (void) { event_trigger (&_sdconfig_.zonecfg[10]) ; }
void event_trigger_11 (void) { event_trigger (&_sdconfig_.zonecfg[11]) ; }
void event_trigger_12 (void) { event_trigger (&_sdconfig_.zonecfg[12]) ; }
void event_trigger_13 (void) { event_trigger (&_sdconfig_.zonecfg[13]) ; }
void event_trigger_14 (void) { event_trigger (&_sdconfig_.zonecfg[14]) ; }
void event_trigger_15 (void) { event_trigger (&_sdconfig_.zonecfg[15]) ; }
void event_trigger_16 (void) { event_trigger (&_sdconfig_.zonecfg[16]) ; }
void event_trigger_17 (void) { event_trigger (&_sdconfig_.zonecfg[17]) ; }
void event_trigger_18 (void) { event_trigger (&_sdconfig_.zonecfg[18]) ; }
void event_trigger_19 (void) { event_trigger (&_sdconfig_.zonecfg[19]) ; }
void event_trigger_20 (void) { event_trigger (&_sdconfig_.zonecfg[20]) ; }
void event_trigger_21 (void) { event_trigger (&_sdconfig_.zonecfg[21]) ; }
void event_trigger_22 (void) { event_trigger (&_sdconfig_.zonecfg[22]) ; }
void event_trigger_23 (void) { event_trigger (&_sdconfig_.zonecfg[23]) ; }


typedef void (*FunctionCallback)();
FunctionCallback callbackFunctions[] = {&event_trigger_0, &event_trigger_1, &event_trigger_2, 
                                        &event_trigger_3, &event_trigger_4, &event_trigger_5, 
                                        &event_trigger_6, &event_trigger_7, &event_trigger_8, 
                                        &event_trigger_9, &event_trigger_10, &event_trigger_11,
                                        &event_trigger_12, &event_trigger_13, &event_trigger_14,
                                        &event_trigger_15, &event_trigger_16, &event_trigger_17,
                                        &event_trigger_18, &event_trigger_19, &event_trigger_20,
                                        &event_trigger_21, &event_trigger_22, &event_trigger_23};

static int server_sock = -1;
 

struct mg_mgr _mgr;

#ifdef PTHREAD
void *connection_handler(void *socket_desc)
{
    //logMessage (LOG_DEBUG, "connection_handler()");
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    accept_request(sock);
    close(sock);
    
    pthread_exit(NULL);
  //return 0;
}
#endif
 
/* ------------------ Start Here ----------------------- */
int main (int argc, char *argv[])
{
  int i;
  char *cfg = CFGFILE;
  //int port = -1;
  bool debuglog = false;
  // Default daemon to true to logging works correctly before we even start deamon process.
  _daemon_ = true;
  //_sdconfig_.port = HTTPD_PORT;

  for (i = 1; i < argc; i++)
  {
    if (strcmp (argv[i], "-h") == 0)
    {
      printf ("%s (options)\n -d do NOT run as daemon\n -v verbose\n -c [cfg file name]\n -f debug 2 file\n -h this", argv[0]);
      exit (EXIT_SUCCESS);
    }
    else if (strcmp (argv[i], "-d") == 0)
      _daemon_ = false;
    else if (strcmp (argv[i], "-v") == 0)
      debuglog = true;
    else if (strcmp (argv[i], "-f") == 0)
      _debug2file_ = true;
    else if (strcmp (argv[i], "-c") == 0)
      cfg = argv[++i];
    //else if (strcmp (argv[i], "-p") == 0)
    //  port = atoi(argv[++i]);
  }

  /* Check we are root */
  if (getuid() != 0)
  {
    logMessage(LOG_ERR,"%s Can only be run as root\n",argv[0]);
    fprintf (stderr, "%s Can only be run as root\n",argv[0]);
    exit(EXIT_FAILURE);
  }
  
  if (debuglog)
    _sdconfig_.log_level = LOG_DEBUG;
  else
    _sdconfig_.log_level = LOG_INFO;

  readCfg(cfg);

  logMessage(LOG_NOTICE,"Starting %s version %s\n",argv[0],SD_VERSION);

  _sdconfig_.system = true;
  _sdconfig_.currentZone.type = zcNONE;
  _sdconfig_.cron_update = 0;
  _sdconfig_.eventToUpdateHappened = false;
  read_cron();
  read_cache();

  /*
  if (port != -1)
    _sdconfig_.port = port; 
  */
  logMessage (LOG_DEBUG, "Running %s with options :- daemon=%d, verbose=%d, httpdport=%s, debug2file=%d configfile=%s\n", 
                         argv[0], _daemon_, debuglog, _sdconfig_.socket_port, _debug2file_, cfg);

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);
  signal(SIGSEGV, intHandler);

  if (_daemon_ == false)
  {
    main_loop ();
  }
  else
  {
    char pidfile[256];
    sprintf(pidfile, "%s/%s.pid",PIDLOCATION, basename(argv[0]));
    daemonise (pidfile, main_loop);
  }

  exit (EXIT_SUCCESS);
}

void main_loop ()
{
  int i;

  /* Make sure the file '/usr/local/bin/gpio' exists */
  struct stat filestat;
  if (stat ("/usr/local/bin/gpio", &filestat) == -1)
  {
    logMessage(LOG_ERR,"The program '/usr/local/bin/gpio' is missing, exiting");
    exit (EXIT_FAILURE);
  }

  /* Initialize 'wiringPi' library */
  if (wiringPiSetup () == -1)
  {
    displayLastSystemError ("'wiringPi' library couldn't be initialized, exiting");
    exit (EXIT_FAILURE);
  }

  logMessage(LOG_DEBUG, "Setting up GPIO\n");

  //for (i=0; _sdconfig_.zonecfg[i].pin > -1 ; i++)
  for (i=(_sdconfig_.master_valve?0:1); i <= _sdconfig_.zones ; i++)
  {
    logMessage (LOG_DEBUG, "Setting up Zone %d\n", i);    
    
    if (_sdconfig_.zonecfg[i].input_output == OUTPUT) {
      digitalWrite(_sdconfig_.zonecfg[i].pin, digitalRead(_sdconfig_.zonecfg[i].pin));
      logMessage (LOG_DEBUG, "Pre Set pin %d set to current state to stop statup bounce when using output mode\n", _sdconfig_.zonecfg[i].pin);
    }
    

//sleep(5); 
    pinMode (_sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].input_output);

    logMessage (LOG_DEBUG, "Set pin %d set to %s\n", _sdconfig_.zonecfg[i].pin,(_sdconfig_.zonecfg[i].input_output==OUTPUT?"OUTPUT":"INPUT") );
    /*
    if (_sdconfig_.zonecfg[i].input_output == OUTPUT) {
      digitalWrite(_sdconfig_.zonecfg[i].pin, 1);
      logMessage (LOG_DEBUG, "Set pin %d set to high/off\n", _sdconfig_.zonecfg[i].pin);
    }
    */

    if ( _sdconfig_.zonecfg[i].startup_state == 0 || _sdconfig_.zonecfg[i].startup_state == 1 ) {
//sleep(5);
      logMessage (LOG_DEBUG, "Setting pin %d to state %d\n", _sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].startup_state);
      digitalWrite(_sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].startup_state);
    }

    if (_sdconfig_.zonecfg[i].set_pull_updown != NONE) {
//sleep(5);
      logMessage (LOG_DEBUG, "Set pin %d set pull up/down resistor to %d\n", _sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].set_pull_updown );
      pullUpDnControl (_sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].set_pull_updown);
    }

    if ( _sdconfig_.zonecfg[i].receive_mode != NONE) {
//sleep(5);
      if (wiringPiISR (_sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].receive_mode, callbackFunctions[i]) == -1)
      {
        displayLastSystemError ("Unable to set interrupt handler for specified pin, exiting");
        exit (EXIT_FAILURE);
      }
      
      logMessage (LOG_DEBUG, "Set pin %d for trigger with rising/falling mode %d\n", _sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].receive_mode );
      // Reset output mode if we are triggering on an output pin, as last call re-sets state for some reason
      if (_sdconfig_.zonecfg[i].input_output == OUTPUT) {
//sleep(5);
        pinMode (_sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].input_output);
        logMessage (LOG_DEBUG, "ReSet pin %d set to %s\n", _sdconfig_.zonecfg[i].pin,(_sdconfig_.zonecfg[i].input_output==OUTPUT?"OUTPUT":"INPUT") );
      }
      
    }
  }
/*
  for (i=0; i < _sdconfig_.pinscfgs ; i++)
  {
    if ( _sdconfig_.zonecfg[i].startup_state == 0 || _sdconfig_.zonecfg[i].startup_state == 1 ) {
      logMessage (LOG_DEBUG, "Setting pin %d to state %d\n", _sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].startup_state);
      digitalWrite(_sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].startup_state);
    }
  }
*/
  logMessage (LOG_DEBUG, "Pin setup complete\n");

  logMessage (LOG_DEBUG, "Starting HTTPD\n");
  
  if (!start_net_services(&_mgr)) {
    logMessage(LOG_ERR, "Can not start webserver on port %s.\n", _sdconfig_.socket_port);
    exit(EXIT_FAILURE);
  }
  
  i=0;
  while (true)
  {
    //logMessage (LOG_DEBUG, "mg_mgr_poll\n");
    mg_mgr_poll(&_mgr, 500);
    
    check_cron();
    if (zc_check() == true || check_delay24h() == true || _sdconfig_.eventToUpdateHappened) {
      _sdconfig_.eventToUpdateHappened = false;
      broadcast_sprinklerdstate(_mgr.active_connections);
    }
/*
    if (i >= 20) {
      i=0;
      if (_sdconfig_.currentZone.type != zcNONE)
        broadcast_sprinklerdactivestate(_mgr.active_connections);
    }
*/
    //logMessage (LOG_DEBUG, "check_net_services\n");
    if (check_net_services(&_mgr) == false) {
      sleep(1);
    }

    i++;
    //logMessage (LOG_DEBUG, "loop\n");
  }

}

void Daemon_Stop (int signum)
{
  int i;
  /* 'SIGTERM' was issued, system is telling this daemon to stop */
  //syslog (LOG_INFO, "Stopping daemon");
  logMessage (LOG_INFO, "Stopping!\n");
  //close(server_sock);

  //(_sdconfig_.master_valve?0:1)
  for (i=(_sdconfig_.master_valve?0:1); i <= _sdconfig_.zones ; i++)
  {
    if ( _sdconfig_.zonecfg[i].shutdown_state == 0 || _sdconfig_.zonecfg[i].shutdown_state == 1 ) {
      logMessage (LOG_DEBUG, "Turning off Zone %d. Setting pin %d to state %d\n", i, _sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].shutdown_state);
      digitalWrite(_sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].shutdown_state);
    }
  }
  /*
#ifdef PTHREAD  
  logMessage (LOG_INFO, "Stopping httpd threads!\n"); 
  pthread_exit(NULL);
#endif
  */
  write_cache();
  logMessage (LOG_INFO, "Exit!\n"); 
  exit (EXIT_SUCCESS);
}

void intHandler(int signum) {
  int i;
  //syslog (LOG_INFO, "Stopping");
  logMessage (LOG_INFO, "Stopping!\n");

  for (i=(_sdconfig_.master_valve?0:1); i <= _sdconfig_.zones ; i++)
  {
    if ( _sdconfig_.zonecfg[i].shutdown_state == 0 || _sdconfig_.zonecfg[i].shutdown_state == 1 ) {
      logMessage (LOG_DEBUG, "Turning off Zone %d. Setting pin %d to state %d\n", i, _sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].shutdown_state);
      digitalWrite(_sdconfig_.zonecfg[i].pin, _sdconfig_.zonecfg[i].shutdown_state);
    }
  }
  
  close(server_sock);
  write_cache();
  /*
#ifdef PTHREAD
  logMessage (LOG_INFO, "Stopping httpd threads!\n"); 
  pthread_exit(NULL);
#endif
*/
  exit (EXIT_SUCCESS);
}

void event_trigger (struct GPIOcfg *gpiopin)
{
  //int out_state_toset; 
  int in_state_read;
  time_t rawtime;
  struct tm * timeinfo;
  char timebuffer[20];
  //bool changed=false;
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (timebuffer,20,"%T",timeinfo);


 //printf("Received trigger %d - at %s - last trigger %d\n",digitalRead (gpioconfig->pin), timebuffer, gpioconfig->last_event_state);
 
  logMessage (LOG_DEBUG,"%s Received input change on pin %d - START\n",timebuffer, gpiopin->pin);
  
  if ( (rawtime - gpiopin->last_event_time) < 1 && gpiopin->input_output == INPUT)
  {
    logMessage (LOG_DEBUG,"        ignoring, time between triggers too short (%d-%d)=%d\n",rawtime, gpiopin->last_event_time, (rawtime - gpiopin->last_event_time));
    return;
  }
  gpiopin->last_event_time = rawtime;
  //logMessage (LOG_DEBUG,"Diff between last event %d (%d, %d)\n",rawtime - gpioconfig->last_event_time, rawtime, gpioconfig->last_event_time);
  
  /* Handle button pressed interrupts */
  
  in_state_read = digitalRead (gpiopin->pin);
  //logMessage (LOG_DEBUG, "        Init pin state %d, previous state %d\n", in_state_read, gpiopin->last_event_state);
  
  //if ( gpioconfig->receive_state == BOTH || in_state_read == gpioconfig->receive_state)
  //{
  gpiopin->last_event_state = in_state_read;
    
   /* 
    if (gpiopin->ext_cmd != NULL && strlen(gpiopin->ext_cmd) > 0) {
      //logMessage (LOG_DEBUG, "command '%s'\n", gpioconfig->ext_cmd);
      run_external(gpiopin->ext_cmd, in_state_read);
    } 
   */ 
    //sleep(1);
  //} else {
  //  logMessage (LOG_DEBUG,"        ignoring, reseived state does not match cfg\n");
  //  return;
  //}
  
  // Sleep for 1 second just to limit duplicte events
  //sleep(1);
  
  logMessage (LOG_DEBUG,"%s Receive input change on pin %d - END\n",timebuffer, gpiopin->pin);
  //if (changed ==true )
  if (_mgr.active_connections != NULL) {
    _sdconfig_.eventToUpdateHappened = true;
    //broadcast_zonestate(_mgr.active_connections, gpiopin);
  }
}


