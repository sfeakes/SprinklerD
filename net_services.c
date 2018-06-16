#include <libgen.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
//#include <sys/inotify.h>
#include <net/if.h>

#include <wiringPi.h>

#include "mongoose.h"
#include "utils.h"
#include "net_services.h"
#include "config.h"
#include "json_messages.h"
#include "zone_ctrl.h"
#include "sd_cron.h"


#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )


typedef enum {mqttstarting, mqttrunning, mqttstopped, mqttdisabled} mqttstatus;
static mqttstatus _mqtt_status = mqttstopped;
struct mg_connection *_mqtt_connection;
static time_t _mqtt_connected_time;

void start_mqtt(struct mg_mgr *mgr);

static int is_websocket(const struct mg_connection *nc) {
  return nc->flags & MG_F_IS_WEBSOCKET;
}
static int is_mqtt(const struct mg_connection *nc) {
  return nc->flags & MG_F_USER_1;
}
static void set_mqtt(struct mg_connection *nc) {
  nc->flags |= MG_F_USER_1; 
}
 // Find the first network interface with valid MAC and put mac address into buffer upto length
bool mac(char *buf, int len)
{
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  struct if_nameindex *if_nidxs, *intf;

  if_nidxs = if_nameindex();
  if (if_nidxs != NULL)
  {
    for (intf = if_nidxs; intf->if_index != 0 || intf->if_name != NULL; intf++)
    {
      strcpy(s.ifr_name, intf->if_name);
      if (0 == ioctl(fd, SIOCGIFHWADDR, &s))
      {
        int i;
        if ( s.ifr_addr.sa_data[0] == 0 &&
             s.ifr_addr.sa_data[1] == 0 &&
             s.ifr_addr.sa_data[2] == 0 &&
             s.ifr_addr.sa_data[3] == 0 &&
             s.ifr_addr.sa_data[4] == 0 &&
             s.ifr_addr.sa_data[5] == 0 ) {
          continue;
        }
        for (i = 0; i < 6 && i * 2 < len; ++i)
        {
          sprintf(&buf[i * 2], "%02x", (unsigned char)s.ifr_addr.sa_data[i]);
        }
        return true;
      }
    }
  }

  return false;
}

void urldecode(char *dst, const char *src)
{
  char a, b;
  while (*src)
  {
    if ((*src == '%') &&
        ((a = src[1]) && (b = src[2])) &&
        (isxdigit(a) && isxdigit(b)))
    {
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
    }
    else if (*src == '+')
    {
      *dst++ = ' ';
      src++;
    }
    else
    {
      *dst++ = *src++;
    }
  }
  *dst++ = '\0';
}

// Need to update network interface.
char *generate_mqtt_id(char *buf, int len) {
  extern char *__progname; // glibc populates this
  int i;
  strncpy(buf, basename(__progname), len);
  i = strlen(buf);

  if (i < len) {
    buf[i++] = '_';
    // If we can't get MAC to pad mqtt id then use PID
    if (!mac(&buf[i], len - i - 1)) {
      sprintf(&buf[i], "%.*d", (len-i-2), getpid());
    }
  }
  buf[len-1] = '\0';

  return buf;
}

void send_mqtt_msg(struct mg_connection *nc, char *toppic, char *message)
{
  static uint16_t msg_id = 0;

  if (msg_id >= 65535){msg_id=1;}else{msg_id++;}

  //mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_QOS(0), message, strlen(message));
  mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_RETAIN | MG_MQTT_QOS(1), message, strlen(message));

  logMessage(LOG_INFO, "MQTT: Published id=%d: %s %s\n", msg_id, toppic, message);
}

void publish_zone_mqtt(struct mg_connection *nc, struct GPIOcfg *gpiopin) {
  static char mqtt_topic[250];
  static char mqtt_msg[50];

  if (_gpioconfig_.enableMQTTaq == true) {
    // sprintf(mqtt_topic, "%s/%s", _gpioconfig_.mqtt_topic, gpiopin->name);
    sprintf(mqtt_topic, "%s/zone/%d", _gpioconfig_.mqtt_topic, gpiopin->zone);
    sprintf(mqtt_msg, "%s", (digitalRead(gpiopin->pin) == gpiopin->on_state ? MQTT_ON : MQTT_OFF));
    send_mqtt_msg(nc, mqtt_topic, mqtt_msg);
  }
  if (gpiopin->dz_idx > 0 && _gpioconfig_.enableMQTTdz == true) {
    build_dz_mqtt_status_JSON(mqtt_msg, 50, gpiopin->dz_idx, (digitalRead(gpiopin->pin) == 0 ? DZ_ON : DZ_OFF), TEMP_UNKNOWN);
    send_mqtt_msg(nc, _gpioconfig_.mqtt_dz_pub_topic, mqtt_msg);
  }
}

void broadcast_zonestate(struct mg_connection *nc, struct GPIOcfg *gpiopin) 
{
  //logMessage(LOG_DEBUG, "broadcast_gpiopinstate()\n"); 
  //static int mqtt_count=0;
  struct mg_connection *c;

  check_net_services(nc->mgr);

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (is_websocket(c)) {
      //ws_send(c, data);
      logMessage(LOG_ERR, "Hit part of code that's not complete  - broadcast_gpiopinstate() - websocket\n"); 
    } else if (is_mqtt(c)) {
      publish_zone_mqtt(c, gpiopin);
    }
  }
  return;
}



void broadcast_sprinklerdstate(struct mg_connection *nc) 
{
  //static int mqtt_count=0;
  int i;
  struct mg_connection *c;
  static char mqtt_topic[250];
  static char mqtt_msg[50];

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    for (i=1; i <= _gpioconfig_.zones ; i++)
    {
      if (is_websocket(c)) {
        //ws_send(c, data);
        logMessage(LOG_ERR, "Hit part of code that's not complete  - broadcast_gpiopinstate() - websocket\n"); 
      } else if (is_mqtt(c)) {
        publish_zone_mqtt(c, &_gpioconfig_.gpiocfg[i]);
      }
    }
    if (_gpioconfig_.enableMQTTaq == true) {
      sprintf(mqtt_topic, "%s/system", _gpioconfig_.mqtt_topic);
      sprintf(mqtt_msg, "%s", (_gpioconfig_.system?MQTT_ON:MQTT_OFF) );
      send_mqtt_msg(c, mqtt_topic, mqtt_msg);
      sprintf(mqtt_topic, "%s/24hdelay", _gpioconfig_.mqtt_topic);
      sprintf(mqtt_msg, "%s", (_gpioconfig_.delay24h?MQTT_ON:MQTT_OFF) );
      send_mqtt_msg(c, mqtt_topic, mqtt_msg);
      sprintf(mqtt_topic, "%s/cycleallzones", _gpioconfig_.mqtt_topic);
      sprintf(mqtt_msg, "%s", (_gpioconfig_.currentZone.type==zcALL?MQTT_ON:MQTT_OFF) );
      send_mqtt_msg(c, mqtt_topic, mqtt_msg);
    }
    // NSF Need to publish to domoticz
  }
  return;
}

int is_value_ON(char *buf) {
  if (strncasecmp(buf, "on", 3) == 0 || strncmp(buf, "1", 1) == 0)
    return true;
  else if (strncasecmp(buf, "off", 3) == 0 || strncmp(buf, "0", 1) == 0)
    return false;

  return -1;
}

int serve_web_request(struct mg_connection *nc, struct http_message *http_msg, char *buffer, int size, bool *changedOption) {
  static int buflen = 50;
  char buf[buflen];
  //int action = -1;
  //int pin = -1;
  //int status = -1;
  int length = 0;
  //int i;
  int zone;
  int runtime;

  memset(&buffer[0], 0, size);
  

    /* Need to action the following
  /?type=firstload                         // Include cal schedule
  /?type=read                              // no need to include cal schedule

  // Cfg options
  /?type=option&option=24hdelay&state=off  // turn off 24h delay
  /?type=option&option=calendar&state=off  // turn off calendar

  // Calendar
  /?type=calcfg&day=3&zone=&time=07:00      // Use default water zone times
  /?type=calcfg&day=2&zone=1&time=7         // Change water zone time
  /?type=calcfg&day=3&zone=&time=           // Delete day schedule
  
  // Run options
  /?type=option&option=allz&state=on        // Run all zones default times
  /?type=zone&zone=2&state=on&runtime=3     // Run zone 2 for 3 mins (ignore 24h delay & calendar settings)
  /?type=zrtcfg&zone=2&time=10              // change zone 2 default runtime to 10
  /?type=cron&zone=1&runtime=12'            // Run zone 1 for 12 mins (calendar & 24hdelay settings overide this request)
  */
  mg_get_http_var(&http_msg->query_string, "type", buf, buflen);
  logMessage (LOG_DEBUG, "Request type %s\n",buf);

  if (strcmp(buf, "firstload") == 0) {
      logMessage(LOG_DEBUG, "WEB REQUEST Firstload %s\n",buf);
      length = build_sprinkler_cal_JSON(buffer, size);
  } else if (strcmp(buf, "read") == 0) {
      logMessage(LOG_DEBUG, "WEB REQUEST read %s\n",buf);
      length = build_sprinkler_JSON(buffer, size);
  } else if (strcmp(buf, "option") == 0) {
      //logMessage(LOG_DEBUG, "WEB REQUEST option %s\n",buf);
      mg_get_http_var(&http_msg->query_string, "option", buf, buflen);
      if (strncasecmp(buf, "system", 6) == 0 ) {
        mg_get_http_var(&http_msg->query_string, "state", buf, buflen);
        if ( is_value_ON(buf) )
          _gpioconfig_.system = true;
         else
          _gpioconfig_.system = false;
        length = build_sprinkler_JSON(buffer, size);
      } else if (strncasecmp(buf, "24hdelay", 8) == 0 ) {
        mg_get_http_var(&http_msg->query_string, "state", buf, buflen);
        enable_delay24h(is_value_ON(buf));
        length = build_sprinkler_JSON(buffer, size);
      } else if (strncasecmp(buf, "allz", 8) == 0 ) {
        mg_get_http_var(&http_msg->query_string, "state", buf, buflen);
        zc_zone(zcALL, 0, (is_value_ON(buf)?zcON:zcOFF), 0);
        length = build_sprinkler_JSON(buffer, size);
      } else {
        logMessage(LOG_WARNING, "Bad request unknown option\n");
        length = sprintf(buffer,  "{ \"error\": \"Bad request unknown option\" }");
      }
      *changedOption = true;
      //broadcast_sprinklerdstate(nc);
  } else if ( strcmp(buf, "zone") == 0 || strcmp(buf, "cron") == 0 ) {
      //logMessage(LOG_DEBUG, "WEB REQUEST zone %s\n",buf);
      zcRunType type = zcSINGLE;
      if (strcmp(buf, "cron") == 0)
        type = zcCRON;
      
      mg_get_http_var(&http_msg->query_string, "zone", buf, buflen);
      zone = atoi(buf);
      mg_get_http_var(&http_msg->query_string, "runtime", buf, buflen);
      runtime = atoi(buf);
      mg_get_http_var(&http_msg->query_string, "state", buf, buflen);
      //if ( (strncasecmp(buf, "off", 3) == 0 || strncmp(buf, "0", 1) == 0) && zone <= _gpioconfig_.zones) {
      if ( is_value_ON(buf) == true && zone <= _gpioconfig_.zones) {
        zc_zone(type, zone, zcON, runtime);
        length = build_sprinkler_JSON(buffer, size);
      //} else if ( (strncasecmp(buf, "on", 2) == 0 || strncmp(buf, "1", 1) == 0) && zone <= _gpioconfig_.zones) {
      } else if ( is_value_ON(buf) == false && zone <= _gpioconfig_.zones) {
        zc_zone(type, zone, zcOFF, runtime);
        length = build_sprinkler_JSON(buffer, size);
      } else {
        if (zone > _gpioconfig_.zones) {
          logMessage(LOG_WARNING, "Bad request unknown zone %d\n",zone);
          length = sprintf(buffer,  "{ \"error\": \"Bad request unknown zone %d\"}", zone);
        } else {
          logMessage(LOG_WARNING, "Bad request on zone %d, unknown state %s\n",zone, buf);
          length = sprintf(buffer,  "{ \"error\": \"Bad request on zone %d, unknown state %s\"}", zone, buf);
        }
      }
  } else if (strcmp(buf, "zrtcfg") == 0) {
      //logMessage(LOG_DEBUG, "WEB REQUEST cfg %s\n",buf);
      mg_get_http_var(&http_msg->query_string, "zone", buf, buflen);
      zone = atoi(buf);
      mg_get_http_var(&http_msg->query_string, "time", buf, buflen);
      runtime = atoi(buf);
      if (zone > 0 && zone <= _gpioconfig_.zones && runtime > 0) {
        _gpioconfig_.gpiocfg[zone].default_runtime = runtime;
        logMessage(LOG_DEBUG, "changed default runtime on zone %d, to %d\n",zone, runtime);
        length = build_sprinkler_JSON(buffer, size);
      } else
        length += sprintf(buffer,  "{ \"error\": \"bad request zone %d runtime %d\"}",zone,runtime);
  } else if (strcmp(buf, "calcfg") == 0) {
    mg_get_http_var(&http_msg->query_string, "day", buf, buflen);
    int day = atoi(buf);
    mg_get_http_var(&http_msg->query_string, "zone", buf, buflen);
    zone = atoi(buf);
    mg_get_http_var(&http_msg->query_string, "time", buf, buflen);
    runtime = atoi(buf);
    if (day >= 0 && day <= 6 && zone > 0 && time > 0) {  // zone runtime
      if (zone <= _gpioconfig_.zones)
        _gpioconfig_.cron[day].zruntimes[zone-1] = runtime;
      else
        length = sprintf(buffer,  "{ \"error\": \"bad zone calendar config request\"}");
    } else if (day >= 0 && day <= 6 && strlen(buf) > 0) { // add day to schedule
      _gpioconfig_.cron[day].hour = str2int(buf, 2);
      _gpioconfig_.cron[day].minute = str2int(&buf[3], 2);;
    } else if (day >= 0 && day <= 6) { // remove day from schedule
      _gpioconfig_.cron[day].hour = -1;
      _gpioconfig_.cron[day].minute = -1;
    } else {
      length = sprintf(buffer,  "{ \"error\": \"bad calendar config request\"}");
    }
    time(&_gpioconfig_.cron_update);
    length = build_sprinkler_cal_JSON(buffer, size);
  } else {
      length += sprintf(buffer,  "{ \"error\": \"unknown request\"}");
  }

  buffer[length] = '\0';
  //logMessage (LOG_DEBUG, "Web Return %d = '%.*s'\n",length, length, buffer);
  return strlen(buffer);
}


#define CT_TEXT "Content-Type: text/plain"
#define CT_JSON "Content-Type: application/json"

void action_web_request(struct mg_connection *nc, struct http_message *http_msg){
  int bufsize = 1024 + (200 * _gpioconfig_.zones);
  char buf[bufsize];
  char *c_type;
  int size = 0;
  bool changedOption = false;

  logMessage(LOG_DEBUG, "action_web_request %.*s %.*s\n",http_msg->uri.len, http_msg->uri.p, http_msg->query_string.len, http_msg->query_string.p);

  if (http_msg->query_string.len > 0) {
      //size = serve_web_request(nc, http_msg, buf, sizeof(buf));
    size = serve_web_request(nc, http_msg, buf, bufsize, &changedOption);
    c_type = CT_JSON;
    
    if (size <= 0) {
      size = sprintf(buf,  "{ \"error\": \"unknown request\"}");
    }
    
    mg_send_head(nc, 200, size, c_type);
    mg_send(nc, buf, size);
      //logMessage (LOG_DEBUG, "Web Return %d = '%.*s'\n",size, size, buf);
    
    if (changedOption)
      broadcast_sprinklerdstate(nc);

    return;
  }
  
  struct mg_serve_http_opts opts;

  memset(&opts, 0, sizeof(opts)); // Reset all options to defaults
  opts.document_root = _gpioconfig_.docroot; // Serve files from the current directory
    // logMessage (LOG_DEBUG, "Doc root=%s\n",opts.document_root);
  mg_serve_http(nc, http_msg, opts);
}
/*
void action_websocket_request(struct mg_connection *nc, struct websocket_message *wm){
  logMessage(LOG_INFO, "action_websocket_request\n");
}
*/

void action_domoticz_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  int idx = -1;
  int nvalue = -1;
  int i;
  char svalue[DZ_SVALUE_LEN];

  if (parseJSONmqttrequest(msg->payload.p, msg->payload.len, &idx, &nvalue, svalue)) {
    for (i=0; i < _gpioconfig_.pinscfgs ; i++)
    {
      if ( _gpioconfig_.gpiocfg[i].dz_idx == idx ) {
        logMessage(LOG_INFO, "Domoticz MQT id %d matched Pin %d %s\n",idx, _gpioconfig_.gpiocfg[i].pin, _gpioconfig_.gpiocfg[i].name);
        if ( nvalue == DZ_ON )
          //switchPin(nc, &_gpioconfig_.gpiocfg[i], ON);
          logMessage(LOG_INFO, "ADD TURN ON CODE");
        else if ( nvalue == DZ_OFF )
          //switchPin(nc, &_gpioconfig_.gpiocfg[i], OFF);
          logMessage(LOG_INFO, "ADD TURN OFF CODE");
        else
          logMessage(LOG_WARNING, "Domoticz MQT id %d, unknown state to set %d\n",idx,nvalue); 
      }
    }
  }
}


void action_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg){
  //logMessage(LOG_INFO, "action_mqtt_message\n");
  int i;
  //char *pt1 = (char *)&msg->topic.p[strlen(_gpioconfig_.mqtt_topic)+1];
  char *pt2 = NULL;
  char *pt3 = NULL;
  char *pt4 = NULL;

  // NSF change 10 to strlen(_gpioconfig_.mqtt_topic)+1   **** BUT TEST.***
  for (i=10; i < msg->topic.len; i++) {
    if ( msg->topic.p[i] == '/' ) {
      if (pt2 == NULL) {
        pt2 = (char *)&msg->topic.p[++i];
      } else if (pt3 == NULL) {
        pt3 = (char *)&msg->topic.p[++i];
      } else if (pt4 == NULL) {
        pt4 = (char *)&msg->topic.p[++i];
        break;
      }
    }
  }

  /* Need to action the following
  1 = on

  //sprinkler/zone/1/set on
  //sprinkler/zone/all/set on
  //sprinkler/24hdelay/set on
  //sprinkler/system/set on

  */
  // All topics are on or off / 1 or 0
  zcState status = zcOFF;
  if ( strncasecmp(msg->payload.p, "on", 2) == 0 || strncmp(msg->payload.p, MQTT_ON, 1) == 0) {
    status = zcON;
  }

  logMessage(LOG_INFO, "MQTT: topic %.*s %.*s\n",msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);

  if (pt2 != NULL && pt3 != NULL && pt4 != NULL && strncmp(pt2, "zone", 4) == 0 && strncmp(pt4, "set", 3) == 0 ) {
    int zone = atoi(pt3);
    if (zone > 0 && zone <= _gpioconfig_.zones) {
      zc_zone(zcSINGLE, zone, status, 0);
      logMessage(LOG_DEBUG, "MQTT: Turn zone %d %s\n",zone, status==zcON?"ON":"OFF");
    } else {
      logMessage(LOG_WARNING, "MQTT: unknown zone %d\n",zone);
    }
  } else if (pt2 != NULL && pt3 != NULL && strncmp(pt2, "24hdelay", 8) == 0 && strncmp(pt3, "set", 3) == 0 ) {
    enable_delay24h(status==zcON?true:false);
    logMessage(LOG_DEBUG, "MQTT: Enable 24 hour delay %s\n",status==zcON?"YES":"NO");
  } else if (pt2 != NULL && pt3 != NULL && strncmp(pt2, "system", 6) == 0 && strncmp(pt3, "set", 3) == 0 ) {
    _gpioconfig_.system=status==zcON?true:false;
    logMessage(LOG_DEBUG, "MQTT: Turning System %s\n",status==zcON?"ON":"OFF");
  } else if (pt2 != NULL && pt3 != NULL && strncmp(pt2, "cycleallzones", 13) == 0 && strncmp(pt3, "set", 3) == 0 ) {
    zc_zone(zcALL, 0, status, 0);
    logMessage(LOG_DEBUG, "MQTT: Cycle all zones %s\n",status==zcON?"ON":"OFF");
  } else {
    logMessage(LOG_DEBUG, "MQTT: Unknown topic %.*s %.*s\n",msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);
    return;
  }
}

static void ev_handler(struct mg_connection *nc, int ev, void *p) {
  struct mg_mqtt_message *mqtt_msg;
  struct http_message *http_msg;
  //struct websocket_message *ws_msg;
  //static double last_control_time;

  // struct mg_mqtt_message *msg = (struct mg_mqtt_message *)p;
  (void)nc;

  // if (ev != MG_EV_POLL) logMessage(LOG_DEBUG, "MQTT handler got event %d\n", ev);
  switch (ev) {
  case MG_EV_CONNECT: {
    set_mqtt(nc);
    if (_gpioconfig_.mqtt_topic != NULL || _gpioconfig_.mqtt_dz_sub_topic != NULL) {
      struct mg_send_mqtt_handshake_opts opts;
      memset(&opts, 0, sizeof(opts));
      opts.user_name = _gpioconfig_.mqtt_user;
      opts.password = _gpioconfig_.mqtt_passwd;
      opts.keep_alive = 5;
      // opts.flags = 0x00;
      // opts.flags |= MG_MQTT_WILL_RETAIN;
      opts.flags |= MG_MQTT_CLEAN_SESSION; // NFS Need to readup on this
      mg_set_protocol_mqtt(nc);
      mg_send_mqtt_handshake_opt(nc, _gpioconfig_.mqtt_ID, opts);
      logMessage(LOG_INFO, "Connected to mqtt %s with id of: %s\n", _gpioconfig_.mqtt_address, _gpioconfig_.mqtt_ID);
      _mqtt_status = mqttrunning;
      _mqtt_connection = nc;
    }
    //last_control_time = mg_time();
  } break;

  case MG_EV_MQTT_CONNACK:
    mqtt_msg = (struct mg_mqtt_message *)p;
    if (mqtt_msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
      logMessage(LOG_WARNING, "Got mqtt connection error: %d\n", mqtt_msg->connack_ret_code);
      _mqtt_status = mqttstopped;
      _mqtt_connection = NULL;
      break;
    }
    time(&_mqtt_connected_time);
    char gp_topic[30];
    struct mg_mqtt_topic_expression topics[2];
    int qos = 0; // can't be bothered with ack, so set to 0
    snprintf(gp_topic, 29, "%s/#", _gpioconfig_.mqtt_topic);
    if (_gpioconfig_.enableMQTTaq == true && _gpioconfig_.enableMQTTdz == true) {
      topics[0].topic = gp_topic;
      topics[0].qos = qos;
      topics[1].topic = _gpioconfig_.mqtt_dz_sub_topic;
      topics[1].qos = qos;
      mg_mqtt_subscribe(nc, topics, 2, 42);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", gp_topic);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", _gpioconfig_.mqtt_dz_sub_topic);
    } else if (_gpioconfig_.enableMQTTaq == true) {
      topics[0].topic = gp_topic;
      topics[0].qos = qos;
      mg_mqtt_subscribe(nc, topics, 1, 42);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", gp_topic);
    } else if (_gpioconfig_.enableMQTTdz == true) {
      topics[0].topic = _gpioconfig_.mqtt_dz_sub_topic;
      topics[0].qos = qos;
      mg_mqtt_subscribe(nc, topics, 1, 42);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", _gpioconfig_.mqtt_dz_sub_topic);
    }
    broadcast_sprinklerdstate(nc);
    break;

  case MG_EV_MQTT_PUBACK:
    mqtt_msg = (struct mg_mqtt_message *)p;
    logMessage(LOG_DEBUG, "Message publishing acknowledged (msg_id: %d)\n", mqtt_msg->message_id);
    break;

  case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
    // nc->user_data = WS;
    logMessage(LOG_DEBUG, "++ Websocket joined\n");
    break;

  case MG_EV_CLOSE:
    if (is_websocket(nc)) {
      logMessage(LOG_DEBUG, "-- Websocket left\n");
    } else if (is_mqtt(nc)) {
      logMessage(LOG_WARNING, "MQTT Connection closed\n");
      _mqtt_status = mqttstopped;
      _mqtt_connection = NULL;
      // start_mqtt(nc->mgr);
    }
    break;

  case MG_EV_HTTP_REQUEST:
    http_msg = (struct http_message *)p;
    action_web_request(nc, http_msg);
    break;
/*
  case MG_EV_WEBSOCKET_FRAME:
    ws_msg = (struct websocket_message *)p;
    action_websocket_request(nc, ws_msg);
    break;
*/
  case MG_EV_MQTT_PUBLISH:
    mqtt_msg = (struct mg_mqtt_message *)p;

    // logMessage(LOG_DEBUG, "MQTT: (msg_id: %d) topic %.*s\n",mqtt_msg->message_id, mqtt_msg->topic.len, mqtt_msg->topic.p);
    { // BS since mongoose doesn;t seem to adhear to CLEAN_SESSION.  Simply ignore messages for the first session.
      time_t now;
      time(&now);
      // logMessage(LOG_DEBUG, "MQTT: time diff = %ld\n", now - _mqtt_connected_time);
      if ((now - _mqtt_connected_time) < 1) {
        logMessage(LOG_DEBUG, "MQTT: received (msg_id: %d), looks like cached message, ignoring\n", mqtt_msg->message_id);
        break;
      }
    }
    if (mqtt_msg->message_id != 0) {
      logMessage(LOG_DEBUG, "MQTT: received (msg_id: %d), looks like my own message, ignoring\n", mqtt_msg->message_id);
      break;
    }
    // action_mqtt_message(nc, mqtt_msg);
    if (_gpioconfig_.enableMQTTaq && strncmp(mqtt_msg->topic.p, _gpioconfig_.mqtt_topic, strlen(_gpioconfig_.mqtt_topic)) == 0) {
      action_mqtt_message(nc, mqtt_msg);
    }
    if (_gpioconfig_.enableMQTTdz && strncmp(mqtt_msg->topic.p, _gpioconfig_.mqtt_dz_sub_topic, strlen(_gpioconfig_.mqtt_dz_sub_topic)) == 0) {
      action_domoticz_mqtt_message(nc, mqtt_msg);
    }

    break;
  }
}

bool check_net_services(struct mg_mgr *mgr) {

  if (_mqtt_status == mqttstopped)
    logMessage (LOG_NOTICE, "MQTT client stopped\n");

  if (_mqtt_status == mqttstopped && _mqtt_status != mqttdisabled) 
  {
    start_mqtt(mgr);
    return false;
  }
  return true;
}

void start_mqtt(struct mg_mgr *mgr) {

  //if (_gpioconfig_.enableMQTT == false) {
  if( _gpioconfig_.enableMQTTaq == false && _gpioconfig_.enableMQTTdz == false ) {
    logMessage (LOG_NOTICE, "MQTT client is disabled, not stating\n");
    _mqtt_status = mqttdisabled;
    return;
  }

  //generate_mqtt_id(_gpioconfig_.mqtt_ID, sizeof(_gpioconfig_.mqtt_ID)-1);
  if (strlen(_gpioconfig_.mqtt_ID) < 1) {
    logMessage (LOG_DEBUG, "MQTTaq %d | MQTTdz %d\n", _gpioconfig_.enableMQTTaq, _gpioconfig_.enableMQTTdz);
    generate_mqtt_id(_gpioconfig_.mqtt_ID, MQTT_ID_LEN-1);
    logMessage (LOG_DEBUG, "MQTTaq %d | MQTTdz %d\n", _gpioconfig_.enableMQTTaq, _gpioconfig_.enableMQTTdz);
  }

  logMessage (LOG_NOTICE, "Starting MQTT client to %s\n", _gpioconfig_.mqtt_address);
  if (mg_connect(mgr, _gpioconfig_.mqtt_address, ev_handler) == NULL) {
      logMessage (LOG_ERR, "Failed to create MQTT listener to %s\n", _gpioconfig_.mqtt_address);
  } else {
    /* NSF NEED TO PASS GPIO STATE HERE
    int i;
    for (i=0; i < TOTAL_BUTTONS; i++) {
      _last_mqtt_aqualinkdata.aqualinkleds[i].state = LED_S_UNKNOWN;
    }
    */
    _mqtt_status = mqttstarting;
  }
}

bool start_net_services(struct mg_mgr *mgr) {
  //static struct mg_serve_http_opts s_http_server_opts;
  struct mg_connection *nc;

  mg_mgr_init(mgr, NULL);
  logMessage (LOG_NOTICE, "Starting web server on port %s\n", _gpioconfig_.socket_port);
  nc = mg_bind(mgr, _gpioconfig_.socket_port, ev_handler);
  if (nc == NULL) {
    logMessage (LOG_ERR, "Failed to create listener\n");
    return false;
  }

  // Set up HTTP server parameters
  mg_set_protocol_http_websocket(nc);

  // Start MQTT
  start_mqtt(mgr);

  

  return true;
}
