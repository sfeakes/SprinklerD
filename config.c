#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <sys/statvfs.h>

#ifdef USE_WIRINGPI
  #include <wiringPi.h>
  #define PIN_CFG_NAME "WPI_PIN"
#else
  //#include "sd_GPIO.h"
  #define PIN_CFG_NAME "GPIO_PIN"
#endif

#include "GPIO_Pi.h"
#include "minIni.h"
#include "utils.h"
#include "config.h"


#define sizearray(a)  (sizeof(a) / sizeof((a)[0]))

#define MAXCFGLINE 256

struct sprinklerdcfg _sdconfig_;

bool remount_root_ro(bool readonly) {
  // NSF Check if config is RO_ROOT set

  if (readonly) {
    logMessage(LOG_INFO, "reMounting root RO\n");
    mount (NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    return true;
  } else {
    struct statvfs fsinfo;
    statvfs("/", &fsinfo);
    if ((fsinfo.f_flag & ST_RDONLY) == 0) // We are readwrite, ignore
      return false;

    logMessage(LOG_INFO, "reMounting root RW\n");
    mount (NULL, "/", NULL, MS_REMOUNT, NULL);
    return true;
  }
}

void write_cache() {
  bool state = remount_root_ro(false);
  FILE *fp;
  int zone;

  fp = fopen(_sdconfig_.cache_file, "w");
  if (fp == NULL) {
    logMessage(LOG_ERR, "Open file failed '%s'\n", _sdconfig_.cache_file);
    remount_root_ro(true);
    return;
  }

  fprintf(fp, "%d\n", _sdconfig_.calendar);
  fprintf(fp, "%d\n", _sdconfig_.delay24h);
  fprintf(fp, "%li\n", _sdconfig_.delay24h_time);
  fprintf(fp, "%d\n", _sdconfig_.precipChanceDelay);
  fprintf(fp, "%.1f\n", _sdconfig_.precipInchDelay1day);
  fprintf(fp, "%.1f\n", _sdconfig_.precipInchDelay2day);
  fprintf(fp, "-1\n");
  for (zone=1; zone <= _sdconfig_.zones; zone ++) {
    fprintf(fp, "%d\n", _sdconfig_.zonecfg[zone].default_runtime);
  }
  fclose(fp);

  remount_root_ro(state);

  logMessage(LOG_INFO, "Wrote cache\n");
}

void read_cache() {
  FILE *fp;
  int i;
  float f;
  int c = 0;
  bool z = false;
  int zs = 0;

  fp = fopen(_sdconfig_.cache_file, "r");
  if (fp == NULL) {
    logMessage(LOG_ERR, "Open file failed '%s'\n", _sdconfig_.cache_file);
    return;
  }
  //while (EOF != fscanf (fp, "%d", &i)) {
  while (EOF != fscanf (fp, "%f", &f)) {
    if (f == -1) {
      z = true;
      zs = c;
      c++;
      continue;
    }
    i = (int)f;

    if (z == true) {
      _sdconfig_.zonecfg[ (c - zs) ].default_runtime = i;
      logMessage(LOG_DEBUG, "Read default_runtime '%d' for zone %d\n", i, (c - zs) );
    } else if (c == 0){
      _sdconfig_.calendar = i;
      logMessage(LOG_DEBUG, "Read Calendar '%s' from cache\n", _sdconfig_.calendar?"ON":"OFF");
    } else if (c == 1){
      _sdconfig_.delay24h = i;
      logMessage(LOG_DEBUG, "Read delay24h '%s' from cache\n", _sdconfig_.delay24h?"ON":"OFF");
    } else if (c == 2){
      _sdconfig_.delay24h_time = i;
       logMessage(LOG_DEBUG, "Read delay24h endtime '%li' from cache\n",_sdconfig_.delay24h_time);
    } else if (c == 3){
      _sdconfig_.precipChanceDelay = i;
       logMessage(LOG_DEBUG, "Read Chance of Rain delay '%d' from cache\n",_sdconfig_.precipChanceDelay);
    } else if (c == 4){
      _sdconfig_.precipInchDelay1day = f;
       logMessage(LOG_DEBUG, "Read Rain 1d delay '%f'in from cache\n",_sdconfig_.precipInchDelay1day);
    } else if (c == 5){
      _sdconfig_.precipInchDelay2day = f;
       logMessage(LOG_DEBUG, "Read Rain 2d delay '%f'in from cache\n",_sdconfig_.precipInchDelay2day);
    }/* else if ( c-1 <= _sdconfig_.zones) {
      _sdconfig_.gpiocfg[c-1].default_runtime = i;
      logMessage(LOG_DEBUG, "Read default_runtime '%d' for zone %d\n", i, c-1);
    }*/

    c++;
  }
  
  fclose (fp);
}

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
  return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}

char *cleanalloc(char*str)
{
  char *result;
  str = trimwhitespace(str);
  
  result = (char*)malloc(strlen(str)+1);
  strcpy ( result, str );
  //printf("Result=%s\n",result);
  return result;
}

char *cleanallocindex(char*str, int index)
{
  char *result;
  int i;
  int found = 1;
  int loc1=0;
  int loc2=strlen(str);
  
  for(i=0;i<loc2;i++) {
    if ( str[i] == ';' ) {
      found++;
      if (found == index)
        loc1 = i;
      else if (found == (index+1))
        loc2 = i;
    }
  }
  
  if (found < index)
    return NULL;

  // Trim leading & trailing spaces
  loc1++;
  while(isspace(str[loc1])) loc1++;
  loc2--;
  while(isspace(str[loc2])) loc2--;
  
  // Allocate and copy
  result = (char*)malloc(loc2-loc1+2*sizeof(char));
  strncpy ( result, &str[loc1], loc2-loc1+1 );
  result[loc2-loc1+1] = '\0';

  return result;
}


int cleanint(char*str)
{
  if (str == NULL)
    return 0;
    
  str = trimwhitespace(str);
  return atoi(str);
}

bool text2bool(char *str)
{
  str = trimwhitespace(str);
  if (strcasecmp (str, "YES") == 0 || strcasecmp (str, "ON") == 0)
    return true;
  else
    return false;
}

void readCfg(char *inifile)
{
  char str[100];
  //long n;
  int i;
  int idx=0;

  ini_gets("SPRINKLERD", "LOG_LEVEL", "WARNING", str, sizearray(str), inifile);
  if (_sdconfig_.log_level != LOG_DEBUG)
    _sdconfig_.log_level = text2elevel(str);

  ini_gets("SPRINKLERD", "NAME", "None", _sdconfig_.name, sizearray(_sdconfig_.name), inifile);
  ini_gets("SPRINKLERD", "PORT", "8888", _sdconfig_.socket_port, sizearray(_sdconfig_.socket_port), inifile);
  ini_gets("SPRINKLERD", "DOCUMENTROOT", "./", _sdconfig_.docroot, sizearray(_sdconfig_.docroot), inifile);
  //ini_gets("SPRINKLERD", "CACHE", "/tmp/sprinklerd.cache", _sdconfig_.cache_file, sizearray(_sdconfig_.cache_file), inifile);
  ini_gets("SPRINKLERD", "CACHE", "/tmp/sprinklerd.cache", _sdconfig_.cache_file, 512, inifile);
  ini_gets("SPRINKLERD", "MQTT_ADDRESS", NULL, _sdconfig_.mqtt_address, sizearray(_sdconfig_.mqtt_address), inifile);
  ini_gets("SPRINKLERD", "MQTT_USER", NULL, _sdconfig_.mqtt_user, sizearray(_sdconfig_.mqtt_user), inifile);
  ini_gets("SPRINKLERD", "MQTT_PASSWD", NULL, _sdconfig_.mqtt_passwd, sizearray(_sdconfig_.mqtt_passwd), inifile);

  if ( ini_gets("SPRINKLERD", "MQT_TOPIC", NULL, _sdconfig_.mqtt_topic, sizearray(_sdconfig_.mqtt_topic), inifile) > 0 )
    _sdconfig_.enableMQTTaq = true;
  else
    _sdconfig_.enableMQTTaq = false;

  //if ( ini_gets("SPRINKLERD", "MQTT_HA_DIS_TOPIC", NULL, _sdconfig_.mqtt_ha_dis_topic, sizearray(_sdconfig_.mqtt_ha_dis_topic), inifile) > 0 )
  if ( ini_gets("SPRINKLERD", "MQTT_DZ_PUB_TOPIC", NULL, _sdconfig_.mqtt_dz_pub_topic, sizearray(_sdconfig_.mqtt_dz_pub_topic), inifile) > 0 &&
       ini_gets("SPRINKLERD", "MQTT_DZ_SUB_TOPIC", NULL, _sdconfig_.mqtt_dz_sub_topic, sizearray(_sdconfig_.mqtt_dz_sub_topic), inifile) > 0)
    _sdconfig_.enableMQTTdz = true;
  else
    _sdconfig_.enableMQTTdz = false;

  if ( ini_gets("SPRINKLERD", "MQTT_HA_DIS_TOPIC", NULL, _sdconfig_.mqtt_ha_dis_topic, sizearray(_sdconfig_.mqtt_ha_dis_topic), inifile) > 0 )
    _sdconfig_.enableMQTTha = true;
  else
    _sdconfig_.enableMQTTha = false;


  if (_sdconfig_.enableMQTTaq == true ) {
    logMessage (LOG_DEBUG,"Config mqtt_topic '%s'\n",_sdconfig_.mqtt_topic);
    if (_sdconfig_.enableMQTTdz == true) {
      logMessage (LOG_DEBUG,"Config mqtt_dz_pub_topic '%s'\n",_sdconfig_.mqtt_dz_pub_topic);
      logMessage (LOG_DEBUG,"Config mqtt_dz_sub_topic '%s'\n",_sdconfig_.mqtt_dz_sub_topic);
    }
    if (_sdconfig_.enableMQTTha == true) {
      logMessage (LOG_DEBUG,"Config mqtt_ha_dis_topic '%s'\n",_sdconfig_.mqtt_ha_dis_topic);
    }
  } else {
    logMessage (LOG_DEBUG,"Config mqtt 'disabeled'\n");
  }

  _sdconfig_.dzidx_calendar = ini_getl("SPRINKLERD", "DZIDX_CALENDAR", 0, inifile);
  _sdconfig_.dzidx_24hdelay = ini_getl("SPRINKLERD", "DZIDX_24HDELAY", 0, inifile);
  _sdconfig_.dzidx_allzones = ini_getl("SPRINKLERD", "DZIDX_ALL_ZONES", 0, inifile);
  _sdconfig_.dzidx_status = ini_getl("SPRINKLERD", "DZIDX_STATUS", 0, inifile);
  _sdconfig_.dzidx_rainsensor = ini_getl("SPRINKLERD", "DZIDX_RAINSENSOR", 0, inifile);

  logMessage (LOG_INFO, "Name = %s\n", _sdconfig_.name);
  logMessage (LOG_INFO, "Port = %s\n", _sdconfig_.socket_port);
  logMessage (LOG_INFO, "Docroot = %s\n", _sdconfig_.docroot);

  idx=0;
  int pin=-1;
  _sdconfig_.zones = 0;

  // Caculate how many zones we have
  for (i=1; i <= 24; i++) // 24 = Just some arbutary number (max GPIO without expansion board)
  {
    sprintf(str, "ZONE:%d", i);
    pin = ini_getl(str, PIN_CFG_NAME, -1, inifile);
    if (pin == -1)
      break;
    else
      _sdconfig_.zones = i;
  }
  
  logMessage (LOG_DEBUG, "Found %d ZONES\n", _sdconfig_.zones);
  _sdconfig_.master_valve = false;

  //_sdconfig_.dz_cache = malloc((_sdconfig_.zones + NON_ZONE_DZIDS + 1) * sizeof(struct DZcache)); 
  _sdconfig_.dz_cache = calloc((_sdconfig_.zones + NON_ZONE_DZIDS + 1), sizeof(struct DZcache)); 
  if ( _sdconfig_.zones != 0) {
  //  n= _sdconfig_.zones+1;
    _sdconfig_.zonecfg = malloc((_sdconfig_.zones + 1) * sizeof(struct GPIOcfg));
    for (i=0; i <= _sdconfig_.zones; i++)
    {
      sprintf(str, "ZONE:%d", i);
      pin = ini_getl(str, PIN_CFG_NAME, -1, inifile);
      if (pin != -1) {
        logMessage (LOG_DEBUG, "ZONE = %d\n", i);
        if (i==0) {
          _sdconfig_.master_valve = true;
        }
        _sdconfig_.zonecfg[i].input_output = OUTPUT; // Zone is always output 
        _sdconfig_.zonecfg[i].receive_mode = BOTH; // Zone always needs trigger on both (high or low)

        _sdconfig_.zonecfg[i].zone = i;
        _sdconfig_.zonecfg[i].pin = pin;
        _sdconfig_.zonecfg[i].on_state = ini_getl(str, "GPIO_ON_STATE", NO, inifile);
        _sdconfig_.zonecfg[i].startup_state = !_sdconfig_.zonecfg[i].on_state;
        _sdconfig_.zonecfg[i].shutdown_state = !_sdconfig_.zonecfg[i].on_state;

        _sdconfig_.zonecfg[i].set_pull_updown = ini_getl(str, "GPIO_PULL_UPDN", -1, inifile);
        _sdconfig_.zonecfg[i].dz_idx = ini_getl(str, "DOMOTICZ_IDX", -1, inifile);
        //_sdconfig_.zonecfg[i].master_valve = ini_getl(str, "MASTER_VALVE", NO, inifile);
        _sdconfig_.zonecfg[i].default_runtime = ini_getl(str, "DEFAULT_RUNTIME", 10, inifile);
        //ini_gets(str, "NAME", NULL, _sdconfig_.zonecfg[idx].name, sizearray(_sdconfig_.zonecfg[idx].name), inifile);
	      ini_gets(str, "NAME", NULL, _sdconfig_.zonecfg[i].name, sizearray(_sdconfig_.zonecfg[i].name), inifile);
//#ifndef USE_WIRINGPI
        if ( ! validGPIO(pin) ) {
          logMessage (LOG_ERR, "GPIO pin %d is not valid, found in ZONE:%d of configuration file %s \n",pin, i, inifile);
          pin = -1;
          sprintf(_sdconfig_.zonecfg[i].name, "ERROR");
        }
//#endif
        /*
         logMessage (LOG_DEBUG,"Zone Config        : %s\n%25s : %d\n%25s : %d\n%25s : %d\n%25s : %d\n%25s : %d\n",
              _sdconfig_.zonecfg[i].name,
              "PIN",_sdconfig_.zonecfg[i].pin,
              "Set pull up/down", _sdconfig_.zonecfg[i].set_pull_updown, 
              "ON state", _sdconfig_.zonecfg[i].on_state, 
              "Master valve", _sdconfig_.zonecfg[i].master_valve,
              "Domoticz IDX", _sdconfig_.zonecfg[i].dz_idx);
         */
          logMessage (LOG_DEBUG,"Zone Config        : %s\n%25s : %d\n%25s : %d\n%25s : %d\n%25s : %d\n",
              _sdconfig_.zonecfg[i].name,
              "PIN",_sdconfig_.zonecfg[i].pin,
              "Set pull up/down", _sdconfig_.zonecfg[i].set_pull_updown,
              "ON state", _sdconfig_.zonecfg[i].on_state,
              "Domoticz IDX", _sdconfig_.zonecfg[i].dz_idx);
         idx++;
      } else {
        logMessage (LOG_DEBUG, "No Information for ZONE = %d\n", i);
        if (i > 0) // We may not have zone 0, but much have zone 1 and all zones in sequence
          break;
      }
    }
    
    //_sdconfig_.pinscfgs = idx;
    //logMessage (LOG_DEBUG,"Config: total GPIO pins = %d\n",_sdconfig_.pinscfgs);

    _sdconfig_.cron[0].zruntimes = malloc(_sdconfig_.zones * sizeof(int));
    _sdconfig_.cron[1].zruntimes = malloc(_sdconfig_.zones * sizeof(int));
    _sdconfig_.cron[2].zruntimes = malloc(_sdconfig_.zones * sizeof(int));
    _sdconfig_.cron[3].zruntimes = malloc(_sdconfig_.zones * sizeof(int));
    _sdconfig_.cron[4].zruntimes = malloc(_sdconfig_.zones * sizeof(int));
    _sdconfig_.cron[5].zruntimes = malloc(_sdconfig_.zones * sizeof(int));
    _sdconfig_.cron[6].zruntimes = malloc(_sdconfig_.zones * sizeof(int));

  } else {
    printf("ERROR no config zones set\n");
    logMessage (LOG_ERR," no config zones set\n");
    exit (EXIT_FAILURE);
  }

  _sdconfig_.inputs = 0;
  // Caculate how many inputs we have
  for (i=1; i <= 24; i++) // 24 = Just some arbutary number (max GPIO without expansion board)
  {
    sprintf(str, "INPUT:%d", i);
    pin = ini_getl(str, PIN_CFG_NAME, -1, inifile);
    if (pin == -1)
      break;
    else
      _sdconfig_.inputs = i;
  }
  
  logMessage (LOG_DEBUG, "Found %d INPUTS\n", _sdconfig_.inputs);
 
  if ( _sdconfig_.inputs != 0) {
  //  n= _sdconfig_.zones+1;
    _sdconfig_.inputcfg = malloc((_sdconfig_.inputs + 1) * sizeof(struct GPIOcfg));
    for (i=0; i < _sdconfig_.inputs; i++)
    {
      sprintf(str, "INPUT:%d", i+1);
      pin = ini_getl(str, PIN_CFG_NAME, -1, inifile);
      if (! validGPIO(pin) ) {
        logMessage (LOG_ERR, "GPIO pin %d is not valid, found in INPUT:%d of configuration file %s \n",pin, i+1, inifile);
        continue;
      }
      logMessage (LOG_DEBUG, "INPUT = %d\n", i+1);

      _sdconfig_.inputcfg[i].input_output = INPUT; // Zone is always input 
      _sdconfig_.inputcfg[i].receive_mode = BOTH; // Zone always needs trigger on both (high or low)
      _sdconfig_.inputcfg[i].zone = i+1;
      _sdconfig_.inputcfg[i].pin = pin;
      _sdconfig_.inputcfg[i].on_state = ini_getl(str, "GPIO_ON_STATE", NO, inifile);
      //_sdconfig_.inputcfg[i].startup_state = !_sdconfig_.inputcfg[i].on_state;
      //_sdconfig_.inputcfg[i].shutdown_state = !_sdconfig_.inputcfg[i].on_state;
      
      _sdconfig_.inputcfg[i].set_pull_updown = ini_getl(str, "GPIO_PULL_UPDN", -1, inifile);
      _sdconfig_.inputcfg[i].dz_idx = ini_getl(str, "DOMOTICZ_IDX", -1, inifile); // Not used at the moment.
      ini_gets(str, "NAME", NULL, _sdconfig_.inputcfg[i].name, sizearray(_sdconfig_.inputcfg[i].name), inifile);

      _sdconfig_.inputcfg[i].command_on = malloc(COMMAND_SIZE * sizeof(char));
      _sdconfig_.inputcfg[i].command_off = malloc(COMMAND_SIZE * sizeof(char));
      ini_gets(str, "COMMAND_ON", NULL, _sdconfig_.inputcfg[i].command_on, COMMAND_SIZE, inifile);
      ini_gets(str, "COMMAND_OFF", NULL, _sdconfig_.inputcfg[i].command_off, COMMAND_SIZE, inifile);

      logMessage (LOG_DEBUG,"Input Config        : %s\n%25s : %d\n%25s : %d\n%25s : %d\n%25s : %d\n",
              _sdconfig_.inputcfg[i].name,
              "PIN",_sdconfig_.inputcfg[i].pin,
              "Set pull up/down", _sdconfig_.inputcfg[i].set_pull_updown,
              "ON state", _sdconfig_.inputcfg[i].on_state,
              "Domoticz IDX", _sdconfig_.inputcfg[i].dz_idx);
    }
  }

  _sdconfig_.runBeforeCmds = 0;
  // Caculate how many external command to run
  for (i=1; i <= 10; i++) // 10 = Just some arbutary number 
  {
    sprintf(str, "EXTERNAL:%d", i);
    pin = ini_getl(str, "RUNB4STARTTIME", -1, inifile);
    if (pin == -1)
      break;
    else
      _sdconfig_.runBeforeCmds = i;
  }

  if ( _sdconfig_.runBeforeCmds != 0) {
  //  n= _sdconfig_.zones+1;
    _sdconfig_.runBeforeCmd = malloc((_sdconfig_.runBeforeCmds) * sizeof(struct RunB4CalStart));
    for (i=0; i < _sdconfig_.runBeforeCmds; i++)
    {
      sprintf(str, "EXTERNAL:%d", i+1);
      _sdconfig_.runBeforeCmd[i].mins = ini_getl(str, "RUNB4STARTTIME", -1, inifile);
      ini_gets(str, "COMMAND", NULL, _sdconfig_.runBeforeCmd[i].command, sizearray(_sdconfig_.runBeforeCmd[i].command), inifile);
      //if (_sdconfig_.runBeforeCmd[i].mins <= 0 || _sdconfig_.runBeforeCmd[i].mins > 120) {
      //  logMessage (LOG_ERR, "RUNB4STARTTIME %d is not valid, found in EXTERNAL:%d of configuration file %s \n",_sdconfig_.runBeforeCmd[i].mins, i+1, inifile);
      //  continue;
      //}
      logMessage (LOG_DEBUG,"Run external command '%s', %d mins before start time\n",_sdconfig_.runBeforeCmd[i].command, _sdconfig_.runBeforeCmd[i].mins);
    }
  }


/*
  idx=0;
  pin=-1;
  // Caculate how many gpio cfg we have
  for (i=1; i <= 24; i++) // 24 = Just some arbutary number (max GPIO without expansion board)
  {
    sprintf(str, "GPIO:%d", i);
    pin = ini_getl(str, PIN_CFG_NAME, -1, inifile);
    if (pin == -1)
      break;
    else
      _sdconfig_.pincfgs = i;
  }
  
  logMessage (LOG_DEBUG, "Found %d GPIO PINS\n", _sdconfig_.pincfgs);

  if ( _sdconfig_.pincfgs != 0) {
  //  n= _sdconfig_.zones+1;
    _sdconfig_.gpiocfg = malloc((_sdconfig_.pincfgs + 1) * sizeof(struct GPIOcfg));
    for (i=0; i <= _sdconfig_.pincfgs; i++)
    {
      sprintf(str, "GPIO:%d", i);
      int pin = ini_getl(str, PIN_CFG_NAME, -1, inifile);

      if (pin != -1) {
        logMessage (LOG_DEBUG, "ZONE = %d\n", i);
        _sdconfig_.pincfgs[i].input_output = OUTPUT; // Zone is always output 
        _sdconfig_.pincfgs[i].receive_mode = BOTH; // Zone always needs trigger on both (high or low)

        ini_gets(str, "NAME", NULL, _sdconfig_.pincfgs[idx].name, sizearray(_sdconfig_.pincfgs[idx].name), inifile);
        _sdconfig_.pincfgs[i].pin = pin;
        _sdconfig_.pincfgs[i].on_state = ini_getl(str, "GPIO_ON_STATE", NO, inifile);
        _sdconfig_.pincfgs[i].startup_state = !_sdconfig_.pincfgs[i].on_state;
        _sdconfig_.pincfgs[i].shutdown_state = !_sdconfig_.pincfgs[i].on_state;

        _sdconfig_.pincfgs[i].set_pull_updown = ini_getl(str, "GPIO_PULL_UPDN", -1, inifile);
      }
    }
*/
}
