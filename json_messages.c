#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_WIRINGPI
  #include <wiringPi.h>
#else
  #include "GPIO_Pi.h"
#endif

#include "json_messages.h"
#include "config.h"
//#include "aq_programmer.h"
#include "utils.h"
//#include "web_server.h"

int build_sprinkler_cal_JSON(char* buffer, int size)
{
  int day;
  int zone;
  int length = build_sprinkler_JSON(buffer, size);

  length = length-1;

  for (zone=1; zone <= _sdconfig_.zones ; zone++)
  {
        length += sprintf(buffer+length,  ", \"z%d-runtime\" : %d, \"z%d-name\" : \"%s\" ", 
                   //_sdconfig_.zonecfg[zone].zone, 
                  //(digitalRead(_sdconfig_.zonecfg[zone].pin)==_sdconfig_.zonecfg[zone].on_state?"on":"off"),
                   _sdconfig_.zonecfg[zone].zone,
                   _sdconfig_.zonecfg[zone].default_runtime,
                   _sdconfig_.zonecfg[zone].zone,
                   _sdconfig_.zonecfg[zone].name);
        //logMessage(LOG_DEBUG, "Zone %d, length %d limit %d\n",i,length,size);
  }


  for (day=0; day <= 6; day++) {
    if (_sdconfig_.cron[day].hour >= 0 && _sdconfig_.cron[day].minute >= 0) {
      length += sprintf(buffer+length, ", \"d%d-starttime\" : \"%.2d:%.2d\" ",day,_sdconfig_.cron[day].hour,_sdconfig_.cron[day].minute);
      for (zone=0; zone < _sdconfig_.zones; zone ++) {
        if (_sdconfig_.cron[day].zruntimes[zone] >= 0) {
          length += sprintf(buffer+length, ", \"d%dz%d-runtime\" : %d",day,zone+1,_sdconfig_.cron[day].zruntimes[zone]);
          //logMessage(LOG_DEBUG, "Zone %d, length %d limit %d\n",zone,length,size);
        }
      }
    }
  }

  length += sprintf(buffer+length, "}");

  buffer[length] = '\0';
  return strlen(buffer);
}

int build_sprinkler_JSON(char* buffer, int size)
{
  int i;
  memset(&buffer[0], 0, size);
  int length = 0;
  char status[50];

  if(_sdconfig_.currentZone.type!=zcNONE)
    sprintf(status,"Active: Zone %d : time left %02d:%02d",_sdconfig_.currentZone.zone, _sdconfig_.currentZone.timeleft / 60, _sdconfig_.currentZone.timeleft % 60 );
  else if (_sdconfig_.todayRainChance > 0 || _sdconfig_.todayRainTotal > 0)
    sprintf(status, "Today:- Chance of rain: %d%%,  Rain total: %.2f\\\"", _sdconfig_.todayRainChance, _sdconfig_.todayRainTotal);
  else
    status[0] = '\0';

  length += sprintf(buffer+length,  "{ \"title\" : \"%s\",\"calendar\" : \"%s\", \"24hdelay\" : \"%s\", \"allz\" : \"%s\", \"zones\" : \"%d\", \"24hdelay-offtime\" : %li, \"status\" : \"%s\", \"raindelaychance\" : \"%d\", \"raindelaytotal1\" : \"%.1f\", \"raindelaytotal2\" : \"%.1f\", \"todaychanceofrain\" : \"%d\", \"todayraintotal\" : \"%.2f\"", 
                                    _sdconfig_.name, 
                                    _sdconfig_.calendar?"on":"off",  
                                    _sdconfig_.delay24h?"on":"off", 
                                    _sdconfig_.currentZone.type==zcALL?"on":"off", 
                                    _sdconfig_.zones,
                                    _sdconfig_.delay24h_time,
                                    status,
                                    _sdconfig_.precipChanceDelay,
                                    _sdconfig_.precipInchDelay1day,
                                    _sdconfig_.precipInchDelay2day,
                                    _sdconfig_.todayRainChance,
                                    _sdconfig_.todayRainTotal);
      
      for (i=1; i <= _sdconfig_.zones ; i++)
      {
        length += sprintf(buffer+length,  ", \"z%d\" : \"%s\" ", 
                   _sdconfig_.zonecfg[i].zone, 
                  (digitalRead(_sdconfig_.zonecfg[i].pin)==_sdconfig_.zonecfg[i].on_state?"on":"off"));
                   //_sdconfig_.zonecfg[i].zone,
                   //_sdconfig_.zonecfg[i].default_runtime,
                   //_sdconfig_.zonecfg[i].zone,
                   //_sdconfig_.zonecfg[i].name);
        //logMessage(LOG_DEBUG, "Zone %d, length %d limit %d\n",i,length,size);
      }
      
      length += sprintf(buffer+length, "}");

  buffer[length] = '\0';
  return strlen(buffer);
}

int build_advanced_sprinkler_JSON(char* buffer, int size)
{
  int i, day;
  memset(&buffer[0], 0, size);
  int length = 0;
  bool cal = false;
/*
  length += sprintf(buffer+length,  "{ \"title\" : \"%s\",\"calendar\" : \"%s\", \"24hdelay\" : \"%s\", \"allz\" : \"%s\", \"#zones\" : %d, \"24hdelay-offtime\" : %li", 
                                    _sdconfig_.name, 
                                    _sdconfig_.calendar?"on":"off",  
                                    _sdconfig_.delay24h?"on":"off", 
                                    _sdconfig_.currentZone.type==zcALL?"on":"off", 
                                    _sdconfig_.zones,
                                    _sdconfig_.delay24h_time);
*/
  length += sprintf(buffer+length,  "{ \"title\" : \"%s\",\"calendar\" : \"%s\", \"24hdelay\" : \"%s\", \"allz\" : \"%s\", \"zones\" : \"%d\", \"24hdelay-offtime\" : %li, \"raindelaychance\" : \"%d\", \"raindelaytotal1\" : \"%.1f\", \"raindelaytotal2\" : \"%.1f\", \"todaychanceofrain\" : \"%d\", \"todayraintotal\" : \"%.2f\"", 
                                    _sdconfig_.name, 
                                    _sdconfig_.calendar?"on":"off",  
                                    _sdconfig_.delay24h?"on":"off", 
                                    _sdconfig_.currentZone.type==zcALL?"on":"off", 
                                    _sdconfig_.zones,
                                    _sdconfig_.delay24h_time,
                                    _sdconfig_.precipChanceDelay,
                                    _sdconfig_.precipInchDelay1day,
                                    _sdconfig_.precipInchDelay2day,
                                    _sdconfig_.todayRainChance,
                                    _sdconfig_.todayRainTotal);
  
  length += sprintf(buffer+length,  ", \"zones\": {");
  for (i=1; i <= _sdconfig_.zones ; i++)
  {
    length += sprintf(buffer+length,  "\"zone %d\": {\"number\": %d, \"name\": \"%s\", \"state\": \"%s\", \"runtime\": %d },",
                                      _sdconfig_.zonecfg[i].zone,
                                      _sdconfig_.zonecfg[i].zone,
                                      _sdconfig_.zonecfg[i].name,
                                      (digitalRead(_sdconfig_.zonecfg[i].pin)==_sdconfig_.zonecfg[i].on_state?"on":"off"),
                                      _sdconfig_.zonecfg[i].default_runtime
                                      );
  }
  if (_sdconfig_.currentZone.type != zcNONE)
    length += sprintf(buffer+length,  "\"active\": {\"zone\": %d, \"name\": \"%s\"},",_sdconfig_.currentZone.zone, _sdconfig_.zonecfg[_sdconfig_.currentZone.zone].name);

  length -= 1;
  length += sprintf( buffer+length ,  "}, \"calendar\": {" );

  for (day=0; day <= 6; day++) {
    if (_sdconfig_.cron[day].hour >= 0 && _sdconfig_.cron[day].minute >= 0) {
      cal = true;
      length += sprintf(buffer+length, "\"day %d\" : { \"start time\" : \"%.2d:%.2d\", ",day,_sdconfig_.cron[day].hour,_sdconfig_.cron[day].minute);
      for (i=1; i < _sdconfig_.zones; i ++) {
        if (_sdconfig_.cron[day].zruntimes[i] >= 0) {
          length += sprintf(buffer+length, "\"Zone %d\" : %d,",i+1,_sdconfig_.cron[day].zruntimes[i]);
          //logMessage(LOG_DEBUG, "Zone %d, length %d limit %d\n",i+1,length,size);
        }
      }
      length -= 1;
      length += sprintf(buffer+length,  "},");
    }
  }
  if (cal) {
    length -= 1;
  }
  length += sprintf(buffer+length, "}}");
  

  buffer[length] = '\0';
  return strlen(buffer);
}

int build_homebridge_sprinkler_JSON(char* buffer, int size)
{
  int i;
  memset(&buffer[0], 0, size);
  int length = 0;

  length += sprintf(buffer+length,  "{ \"title\" : \"%s\", ", _sdconfig_.name);

  length += sprintf(buffer+length,  " \"devices\": [");
  for (i=(_sdconfig_.master_valve?0:1); i <= _sdconfig_.zones ; i++)
  {
    length += sprintf(buffer+length,  "{\"type\" : \"zone\", \"zone\": %d, \"name\": \"%s\", \"state\": \"%s\", \"duration\": %d, \"id\" : \"zone%d\" },",
                                      _sdconfig_.zonecfg[i].zone,
                                      _sdconfig_.zonecfg[i].name,
                                      (digitalRead(_sdconfig_.zonecfg[i].pin)==_sdconfig_.zonecfg[i].on_state?"on":"off"),
                                      _sdconfig_.zonecfg[i].default_runtime * 60,
                                      _sdconfig_.zonecfg[i].zone);
  }
  
  length += sprintf(buffer+length,  "{\"type\" : \"zone\", \"zone\": %d, \"name\": \"Cycle all zones\", \"state\": \"%s\", \"duration\": 0, \"id\" : \"cycleallzones\" },",
                                    _sdconfig_.zones+1,
                                    _sdconfig_.currentZone.type==zcALL?"on":"off");
  length += sprintf(buffer+length,  "{\"type\" : \"switch\", \"zone\": -1, \"name\": \"24h Rain Delay\", \"state\": \"%s\", \"duration\": 0, \"id\" : \"24hdelay\" },",
                                    _sdconfig_.delay24h?"on":"off");
  length += sprintf(buffer+length,  "{\"type\" : \"switch\", \"zone\": -1, \"name\": \"Run on calendar schedule\", \"state\": \"%s\", \"duration\": 0, \"id\" : \"calendar\" }",
                                    _sdconfig_.calendar?"on":"off");


  length += sprintf(buffer+length, "]}");

  buffer[length] = '\0';
  return strlen(buffer);

}

int build_dz_mqtt_status_JSON(char* buffer, int size, int idx, int nvalue, float tvalue)
{
  memset(&buffer[0], 0, size);
  int length = 0;

  if (tvalue == TEMP_UNKNOWN) {
    length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"\"}", idx, nvalue);
  } else {
    length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"%.2f\"}", idx, nvalue, tvalue);
  }

  buffer[length] = '\0';
  return strlen(buffer);
}

int build_dz_status_message_JSON(char* buffer, int size, int idx, int nvalue, char *svalue)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  //json.htm?type=command&param=udevice&idx=IDX&nvalue=LEVEL&svalue=TEXT

  length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"%s\"}", idx, nvalue, svalue);

  buffer[length] = '\0';
  return strlen(buffer);
}

bool parseJSONmqttrequest(const char *str, size_t len, int *idx, int *nvalue, char *svalue, const char *svalue_str) {
  int i = 0;
  int found = 0;
  
  svalue[0] = '\0';

  for (i = 0; i < len && str[i] != '\0'; i++) {
    if (str[i] == '"') {
      if (strncmp("\"idx\"", (char *)&str[i], 5) == 0) {
        i = i + 5;
        for (; str[i] != ',' && str[i] != '\0'; i++) {
          if (str[i] == ':') {
            *idx = atoi(&str[i + 1]);
            found++;
          }
        }
        //if (*idx == 45) 
        //  printf("%s\n",str);
      } else if (strncmp("\"nvalue\"", (char *)&str[i], 8) == 0) {
        i = i + 8;
        for (; str[i] != ',' && str[i] != '\0'; i++) {
          if (str[i] == ':') {
            *nvalue = atoi(&str[i + 1]);
            found++;
          }
        }
      //} else if (strncmp("\"svalue1\"", (char *)&str[i], 9) == 0) {
      } else if (strncmp(svalue_str, (char *)&str[i], 9) == 0) {
        i = i + 9;
        for (; str[i] != ',' && str[i] != '\0'; i++) {
          if (str[i] == ':') {
            while(str[i] == ':' || str[i] == ' ' || str[i] == '"' || str[i] == '\'') i++;
            int j=i+1;
            while(str[j] != '"' && str[j] != '\'' && str[j] != ',' && str[j] != '}') j++;
            strncpy(svalue, &str[i], ((j-i)>DZ_SVALUE_LEN?DZ_SVALUE_LEN:(j-i)));
            svalue[((j-i)>DZ_SVALUE_LEN?DZ_SVALUE_LEN:(j-i))] = '\0'; // Simply force the last termination
            found++;
          }
        }
      } 
      if (found >= 4) {
        return true;
      }
    }
  }
  // Just incase svalue is not found, we really don;t care for most devices.
  if (found >= 2) {
    return true;
  }
  return false;
}
