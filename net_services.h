#ifndef NET_SERVICES_H_
#define NET_SERVICES_H_

#include <stdbool.h>

#include "mongoose.h"
#include "config.h"

#define GET_RTN_OK "Ok"
#define GET_RTN_UNKNOWN "Unknown command"
#define GET_RTN_NOT_CHANGED "Not Changed"

#define MQTT_ON "1"
#define MQTT_OFF "0"

#define MQTT_LWM_TOPIC "Alive"

//enum nsAction{nsNONE = 0, nsGPIO = 1, nsLED = 2, nsLCD = 3}; 

bool start_net_services(struct mg_mgr *mgr);
void broadcast_zonestate(struct mg_connection *nc, struct GPIOcfg *gpiopin);
void broadcast_sprinklerdstate(struct mg_connection *nc);
void broadcast_sprinklerdactivestate(struct mg_connection *nc);
//void broadcast_state(struct mg_connection *nc);
//void broadcast_state_error(struct mg_connection *nc, char *msg);
bool check_net_services(struct mg_mgr *mgr);

void send_mqtt_msg(struct mg_connection *nc, char *toppic, char *message);

//void broadcast_polled_devices(struct mg_connection *nc);
//void broadcast_polled_devices(struct mg_mgr *mgr);

#endif // WEB_SERVER_H_
