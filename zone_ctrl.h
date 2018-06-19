#ifndef ZONE_CTRL_H_
#define ZONE_CTRL_H_

#include <time.h>
#include <stdbool.h>

//#define SEC2MIN 1
#define SEC2MIN 60

typedef enum {zcNONE, zcSINGLE, zcALL, zcCRON} zcRunType;
typedef enum {zcON, zcOFF} zcState;

struct szRunning {
  zcRunType type;
  time_t started_time;
  int duration;
  //time_t *end_time;
  int timeleft;
  int zone;
};

bool zc_check();
bool zc_zone(zcRunType type, int zone, zcState state, int length);

#endif 
