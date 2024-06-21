

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>


#include "mongoose.h"
#include "config.h"
#include "net_services.h"
#include "version.h"

#define HASS_DEVICE "\"identifiers\": " \
                        "[\"SprinklerD\"]," \
                        " \"sw_version\": \"" SD_VERSION "\"," \
                        " \"model\": \"Sprinkler Daemon\"," \
                        " \"name\": \"SprinklerD\"," \
                        " \"manufacturer\": \"SprinklerD\"," \
                        " \"suggested_area\": \"pool\""

#define HASS_AVAILABILITY "\"payload_available\" : \"1\"," \
                          "\"payload_not_available\" : \"0\"," \
                          "\"topic\": \"%s/" MQTT_LWM_TOPIC "\""

const char *HASSIO_TEXT_SENSOR_DISCOVER = "{"
   "\"device\": {" HASS_DEVICE "},"
   "\"availability\": {" HASS_AVAILABILITY "},"
   "\"type\": \"sensor\","
   "\"unique_id\": \"%s\","
   "\"name\": \"%s\","
   "\"state_topic\": \"%s/%s\","
   "\"icon\": \"mdi:card-text\""
"}";

const char *HASSIO_SWITCH_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"switch\","
    "\"unique_id\": \"%s\","
    "\"name\": \"%s\","
    "\"state_topic\": \"%s/%s\","
    "\"command_topic\": \"%s/%s/set\","
    "\"payload_on\": \"1\","
    "\"payload_off\": \"0\","
    "\"qos\": 1,"
    "\"retain\": false"
"}";

const char *HASSIO_VALVE_DISCOVER = "{"
    "\"device\": {" HASS_DEVICE "},"
    "\"availability\": {" HASS_AVAILABILITY "},"
    "\"type\": \"valve\","
    "\"device_class\": \"water\","
    "\"unique_id\": \"%s\","  //  sprinklerd_zone_1
    "\"name\": \"Zone %d (%s)\","            //  1 island
    "\"state_topic\": \"%s/zone%d\","      // 1
    "\"command_topic\": \"%s/zone%d/set\","  // 1
    "\"value_template\": \"{%% set values = { '0':'closed', '1':'open'} %%}{{ values[value] if value in values.keys() else 'closed' }}\","
    "\"payload_open\": \"1\","
    "\"payload_close\": \"0\","
    "\"qos\": 1,"
    "\"retain\": false"
"}";

void publish_mqtt_hassio_discover(struct mg_connection *nc)
{
  char msg[2048];
  char topic[256];
  char id[128];
  int i;


  sprintf(id,"sprinklerd_status");
  sprintf(topic, "%s/sensor/sprinklerd/%s/config", _sdconfig_.mqtt_ha_dis_topic, id);
  sprintf(msg, HASSIO_TEXT_SENSOR_DISCOVER,
               _sdconfig_.mqtt_topic,
               id,
               "Status",
               _sdconfig_.mqtt_topic, "status" );
  send_mqtt_msg(nc, topic, msg);

  sprintf(id,"sprinklerd_active_zone");
  sprintf(topic, "%s/sensor/sprinklerd/%s/config", _sdconfig_.mqtt_ha_dis_topic, id);
  sprintf(msg, HASSIO_TEXT_SENSOR_DISCOVER,
               _sdconfig_.mqtt_topic,
               id,
               "Active Zone",
               _sdconfig_.mqtt_topic, "active" );
  send_mqtt_msg(nc, topic, msg);



  sprintf(id,"sprinklerd_calendar");
  sprintf(topic, "%s/switch/sprinklerd/%s/config", _sdconfig_.mqtt_ha_dis_topic, id);
  sprintf(msg, HASSIO_SWITCH_DISCOVER,
               _sdconfig_.mqtt_topic,
               id,
               "Calendar Schedule",
               _sdconfig_.mqtt_topic, "calendar",
               _sdconfig_.mqtt_topic, "calendar" );
  send_mqtt_msg(nc, topic, msg);

  sprintf(id,"sprinklerd_24hdelay");
  sprintf(topic, "%s/switch/sprinklerd/%s/config", _sdconfig_.mqtt_ha_dis_topic, id);
  sprintf(msg, HASSIO_SWITCH_DISCOVER,
               _sdconfig_.mqtt_topic,
               id,
               "24 Hour rain delay",
               _sdconfig_.mqtt_topic, "24hdelay",
               _sdconfig_.mqtt_topic, "24hdelay" );
  send_mqtt_msg(nc, topic, msg);

  sprintf(id,"sprinklerd_cycleallzones");
  sprintf(topic, "%s/switch/sprinklerd/%s/config", _sdconfig_.mqtt_ha_dis_topic, id);
  sprintf(msg, HASSIO_SWITCH_DISCOVER,
               _sdconfig_.mqtt_topic,
               id,
               "Cycle All Zones",
               _sdconfig_.mqtt_topic, "cycleallzones",
               _sdconfig_.mqtt_topic, "cycleallzones" );
  send_mqtt_msg(nc, topic, msg);




  //for (i=(_sdconfig_.master_valve?0:1); i <= _sdconfig_.zones ; i++)
  // Don't publish zome0/master valve to ha
  for (i=1; i <= _sdconfig_.zones ; i++)
  {
    sprintf(id,"sprinklerd_zone_%d", _sdconfig_.zonecfg[i].zone);
    sprintf(topic, "%s/valve/sprinklerd/%s/config", _sdconfig_.mqtt_ha_dis_topic, id);
    sprintf(msg, HASSIO_VALVE_DISCOVER,
            _sdconfig_.mqtt_topic,
            id,
            _sdconfig_.zonecfg[i].zone,
            _sdconfig_.zonecfg[i].name,
            _sdconfig_.mqtt_topic,_sdconfig_.zonecfg[i].zone,
            _sdconfig_.mqtt_topic,_sdconfig_.zonecfg[i].zone
    );

    send_mqtt_msg(nc, topic, msg);
/*
    length += sprintf(buffer+length,  "{\"type\" : \"zone\", \"zone\": %d, \"name\": \"%s\", \"state\": \"%s\", \"duration\": %d, \"id\" : \"zone%d\" },",
                                      _sdconfig_.zonecfg[i].zone,
                                      _sdconfig_.zonecfg[i].name,
                                      (digitalRead(_sdconfig_.zonecfg[i].pin)==_sdconfig_.zonecfg[i].on_state?"on":"off"),
                                      _sdconfig_.zonecfg[i].default_runtime * 60,
                                      _sdconfig_.zonecfg[i].zone);
                                      */
  }

}