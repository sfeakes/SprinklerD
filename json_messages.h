#ifndef JSON_MESSAGES_H_
#define JSON_MESSAGES_H_

#include <stdbool.h>

#define DZ_OFF 0
#define DZ_ON  1
#define DZ_NULL_IDX 0
#define TEMP_UNKNOWN -1
#define DZ_SVALUE_LEN 20
#define JSON_MQTT_MSG_SIZE 50


bool parseJSONmqttrequest(const char *str, size_t len, int *idx, int *nvalue, char *svalue);
int build_dz_mqtt_status_JSON(char* buffer, int size, int idx, int nvalue, float tvalue);
int build_sprinkler_JSON(char* buffer, int size);
int build_sprinkler_cal_JSON(char* buffer, int size);
int build_advanced_sprinkler_JSON(char* buffer, int size);  





#endif /* JSON_MESSAGES_H_ */