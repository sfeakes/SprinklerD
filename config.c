#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <sys/statvfs.h>

#include <wiringPi.h>

#include "minIni.h"
#include "utils.h"
#include "config.h"



#define sizearray(a)  (sizeof(a) / sizeof((a)[0]))

#define MAXCFGLINE 256

struct GPIOCONTROLcfg _gpioconfig_;

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

  fp = fopen(_gpioconfig_.cache_file, "w");
  if (fp == NULL) {
    logMessage(LOG_ERR, "Open file failed '%s'\n", _gpioconfig_.cache_file);
    remount_root_ro(true);
    return;
  }

  fprintf(fp, "%d\n", _gpioconfig_.system);
  fprintf(fp, "%d\n", _gpioconfig_.delay24h);
  for (zone=1; zone <= _gpioconfig_.zones; zone ++) {
    fprintf(fp, "%d\n", _gpioconfig_.gpiocfg[zone].default_runtime);
  }
  fclose(fp);

  remount_root_ro(state);

  logMessage(LOG_INFO, "Wrote cache\n");
}

void read_cache() {
  FILE *fp;
  int i;
  int c = 0;

  fp = fopen(_gpioconfig_.cache_file, "r");
  if (fp == NULL) {
    logMessage(LOG_ERR, "Open file failed '%s'\n", _gpioconfig_.cache_file);
    return;
  }
  while (EOF != fscanf (fp, "%d", &i)) {
    if (c == 0){
      _gpioconfig_.system = i;
      logMessage(LOG_DEBUG, "Read System '%s' from cache\n", _gpioconfig_.system?"ON":"OFF");
    } else if (c == 1){
      _gpioconfig_.delay24h = i;
      logMessage(LOG_DEBUG, "Read delay24h '%s' from cache\n", _gpioconfig_.delay24h?"ON":"OFF");
    } else if ( c-1 <= _gpioconfig_.zones) {
      _gpioconfig_.gpiocfg[c-1].default_runtime = i;
      logMessage(LOG_DEBUG, "Read default_runtime '%d' for zone %d\n", i, c-1);
    }
    c++;
  }
/*
  fscanf (fp, "%d", &i);
  if (i > -1 && i< 2) {
    _gpioconfig_.system = i;
    logMessage(LOG_DEBUG, "Read System '%s' from cache\n", _gpioconfig_.system?"ON":"OFF");
  }

  fscanf (fp, "%d", &i);
  if (i > -1 && i< 5000) {
    _gpioconfig_.delay24h = i;
    logMessage(LOG_DEBUG, "Read delay24h '%s' from cache\n", _gpioconfig_.delay24h?"ON":"OFF");
  }
*/
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
  if (_gpioconfig_.log_level != LOG_DEBUG)
    _gpioconfig_.log_level = text2elevel(str);

  ini_gets("SPRINKLERD", "NAME", "None", _gpioconfig_.name, sizearray(_gpioconfig_.name), inifile);
  ini_gets("SPRINKLERD", "PORT", "8888", _gpioconfig_.socket_port, sizearray(_gpioconfig_.socket_port), inifile);
  ini_gets("SPRINKLERD", "DOCUMENTROOT", "./", _gpioconfig_.docroot, sizearray(_gpioconfig_.docroot), inifile);
  ini_gets("SPRINKLERD", "CACHE", "/tmp/sprinklerd.cache", _gpioconfig_.cache_file, sizearray(_gpioconfig_.cache_file), inifile);
  ini_gets("SPRINKLERD", "MQTT_ADDRESS", NULL, _gpioconfig_.mqtt_address, sizearray(_gpioconfig_.mqtt_address), inifile);
  ini_gets("SPRINKLERD", "MQTT_USER", NULL, _gpioconfig_.mqtt_user, sizearray(_gpioconfig_.mqtt_user), inifile);
  ini_gets("SPRINKLERD", "MQTT_PASSWD", NULL, _gpioconfig_.mqtt_passwd, sizearray(_gpioconfig_.mqtt_passwd), inifile);
  
  if ( ini_gets("SPRINKLERD", "MQT_TOPIC", NULL, _gpioconfig_.mqtt_topic, sizearray(_gpioconfig_.mqtt_topic), inifile) > 0 )
    _gpioconfig_.enableMQTTaq = true;
  else
    _gpioconfig_.enableMQTTaq = false;

  if ( ini_gets("SPRINKLERD", "MQTT_DZ_PUB_TOPIC", NULL, _gpioconfig_.mqtt_dz_pub_topic, sizearray(_gpioconfig_.mqtt_dz_pub_topic), inifile) > 0 &&
       ini_gets("SPRINKLERD", "MQTT_DZ_SUB_TOPIC", NULL, _gpioconfig_.mqtt_dz_sub_topic, sizearray(_gpioconfig_.mqtt_dz_sub_topic), inifile) > 0)
    _gpioconfig_.enableMQTTdz = true;
  else
    _gpioconfig_.enableMQTTdz = false;

  if (_gpioconfig_.enableMQTTdz == true || _gpioconfig_.enableMQTTaq == true) {
    logMessage (LOG_DEBUG,"Config mqtt_topic '%s'\n",_gpioconfig_.mqtt_topic);
    logMessage (LOG_DEBUG,"Config mqtt_dz_pub_topic '%s'\n",_gpioconfig_.mqtt_dz_pub_topic);
    logMessage (LOG_DEBUG,"Config mqtt_dz_sub_topic '%s'\n",_gpioconfig_.mqtt_dz_sub_topic);
  } else {
    logMessage (LOG_DEBUG,"Config mqtt 'disabeled'\n");
  }

  _gpioconfig_.dzidx_system = ini_getl(str, "DZIDX_SYSTEM", 0, inifile);
  _gpioconfig_.dzidx_24hdelay = ini_getl(str, "DZIDX_24HDELAY", 0, inifile);
  _gpioconfig_.dzidx_allzones = ini_getl(str, "DZIDX_ALL_ZONES", 0, inifile);

  logMessage (LOG_INFO, "Name = %s\n", _gpioconfig_.name);
  logMessage (LOG_INFO, "Port = %s\n", _gpioconfig_.socket_port);
  logMessage (LOG_INFO, "Docroot = %s\n", _gpioconfig_.docroot);

  idx=0;
  int pin=-1;
  _gpioconfig_.zones = 0;

  // Caculate how many zones we have
  for (i=1; i <= 24; i++) // 24 = Just some arbutary number (max GPIO without expansion board)
  {
    sprintf(str, "ZONE:%d", i);
    pin = ini_getl(str, "GPIO_PIN", -1, inifile);
    if (pin == -1)
      break;
    else
      _gpioconfig_.zones = i;
  }
  
  logMessage (LOG_DEBUG, "Found %d ZONES\n", _gpioconfig_.zones);
  _gpioconfig_.master_valve = false;

  if ( _gpioconfig_.zones != 0) {
  //  n= _gpioconfig_.zones+1;
    _gpioconfig_.gpiocfg = malloc((_gpioconfig_.zones + 1) * sizeof(struct GPIOcfg));
    for (i=0; i <= _gpioconfig_.zones; i++)
    {
      sprintf(str, "ZONE:%d", i);
      int pin = ini_getl(str, "GPIO_PIN", -1, inifile);
      if (pin != -1) {
        logMessage (LOG_DEBUG, "ZONE = %d\n", i);
        if (i==0) {
          _gpioconfig_.master_valve = true;
        }
        _gpioconfig_.gpiocfg[i].input_output = OUTPUT; // Zone is always output 
        _gpioconfig_.gpiocfg[i].receive_mode = BOTH; // Zone always needs trigger on both (high or low)

        _gpioconfig_.gpiocfg[i].zone = i;
        _gpioconfig_.gpiocfg[i].pin = pin;
        _gpioconfig_.gpiocfg[i].on_state = ini_getl(str, "GPIO_ON_STATE", NO, inifile);
        _gpioconfig_.gpiocfg[i].startup_state = !_gpioconfig_.gpiocfg[i].on_state;
        _gpioconfig_.gpiocfg[i].shutdown_state = !_gpioconfig_.gpiocfg[i].on_state;

        _gpioconfig_.gpiocfg[i].set_pull_updown = ini_getl(str, "GPIO_PULL_UPDN", -1, inifile);
        _gpioconfig_.gpiocfg[i].dz_idx = ini_getl(str, "DOMOTICZ_IDX", -1, inifile);
        //_gpioconfig_.gpiocfg[i].master_valve = ini_getl(str, "MASTER_VALVE", NO, inifile);
        _gpioconfig_.gpiocfg[i].default_runtime = ini_getl(str, "DEFAULT_RUNTIME", 10, inifile);
        ini_gets(str, "NAME", NULL, _gpioconfig_.gpiocfg[idx].name, sizearray(_gpioconfig_.gpiocfg[idx].name), inifile);

         logMessage (LOG_DEBUG,"Zone Config        : %s\n%25s : %d\n%25s : %d\n%25s : %d\n%25s : %d\n%25s : %d\n",
              _gpioconfig_.gpiocfg[i].name,
              "PIN",_gpioconfig_.gpiocfg[i].pin,
              "Set pull up/down", _gpioconfig_.gpiocfg[i].set_pull_updown, 
              "ON state", _gpioconfig_.gpiocfg[i].on_state, 
              "Master valve", _gpioconfig_.gpiocfg[i].master_valve,
              "Domoticz IDX", _gpioconfig_.gpiocfg[i].dz_idx);
         idx++;
      } else {
        logMessage (LOG_DEBUG, "No Information for ZONE = %d\n", i);
        if (i > 0) // We may not have zone 0, but much have zone 1 and all zones in sequence
          break;
      }
    }
    
    _gpioconfig_.pinscfgs = idx;
    logMessage (LOG_DEBUG,"Config: total GPIO pins = %d\n",_gpioconfig_.pinscfgs);

    _gpioconfig_.cron[0].zruntimes = malloc(_gpioconfig_.zones * sizeof(int));
    _gpioconfig_.cron[1].zruntimes = malloc(_gpioconfig_.zones * sizeof(int));
    _gpioconfig_.cron[2].zruntimes = malloc(_gpioconfig_.zones * sizeof(int));
    _gpioconfig_.cron[3].zruntimes = malloc(_gpioconfig_.zones * sizeof(int));
    _gpioconfig_.cron[4].zruntimes = malloc(_gpioconfig_.zones * sizeof(int));
    _gpioconfig_.cron[5].zruntimes = malloc(_gpioconfig_.zones * sizeof(int));
    _gpioconfig_.cron[6].zruntimes = malloc(_gpioconfig_.zones * sizeof(int));

  } else {
    printf("ERROR no config zones set\n");
    logMessage (LOG_ERR," no config zones set\n");
    exit (EXIT_FAILURE);
  }
}
