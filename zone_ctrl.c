
#include <wiringPi.h>

#include "zone_ctrl.h"
#include "config.h"
#include "utils.h"

bool zc_start(int zone);
bool zc_stop(int zone);
void zc_master(zcState state);

bool zc_check() {
  // check what's running, and if time now is greater than duration Stop if grater
  // Move onto next zone if (all) runtype

  if ( _gpioconfig_.currentZone.type == zcNONE)
    return false;

  time_t now;
  time(&now);
  logMessage (LOG_DEBUG, "Zone running is %d of type %d\n",_gpioconfig_.currentZone.zone, _gpioconfig_.currentZone.type);
  if (difftime(now, _gpioconfig_.currentZone.started_time) > _gpioconfig_.currentZone.duration * SEC2MIN){
    if (_gpioconfig_.currentZone.type==zcALL && _gpioconfig_.currentZone.zone < _gpioconfig_.zones) {
      zc_stop(_gpioconfig_.currentZone.zone);
      _gpioconfig_.currentZone.zone++;
      zc_start(_gpioconfig_.currentZone.zone);
      time(&_gpioconfig_.currentZone.started_time);
      _gpioconfig_.currentZone.duration=_gpioconfig_.gpiocfg[_gpioconfig_.currentZone.zone].default_runtime;
    } else {
      zc_master(zcOFF);
      zc_stop(_gpioconfig_.currentZone.zone);
      _gpioconfig_.currentZone.type=zcNONE;
      _gpioconfig_.currentZone.zone=-1;
    }

    return true;
  }

  // check if 12h delay needs to be canceled.
  return false;
}

void zc_master(zcState state) {

  if (!_gpioconfig_.master_valve)
    return;

  if (state == zcON && _gpioconfig_.gpiocfg[0].on_state != digitalRead (_gpioconfig_.gpiocfg[0].pin ))
    zc_start(0);
  else if (state == zcOFF && _gpioconfig_.gpiocfg[0].on_state == digitalRead (_gpioconfig_.gpiocfg[0].pin ))
    zc_stop(0);
}

bool zc_zone(zcRunType type, int zone, zcState state, int length) {

  int i;

  if (type == zcCRON && state == zcON && (_gpioconfig_.system == false || _gpioconfig_.delay24h == true) ) {
    logMessage (LOG_WARNING, "Request to turn zone %d on. Ignored due to %s!\n",zone,_gpioconfig_.delay24h?"Rain Delay active":"System off");
    return false;
    // Check cal & 24hdelay, return if cal=false or 24hdelay=true
  } else if (type == zcALL) {
    // If we are turning on we ned to start with all zones off, if off, turn them off anyway.
    for (i=1; i < _gpioconfig_.pinscfgs ; i++)
    {
      zc_master(zcOFF);
      if ( _gpioconfig_.gpiocfg[i].on_state == digitalRead(_gpioconfig_.gpiocfg[i].pin) )
        zc_stop(i); 
    }
    _gpioconfig_.currentZone.type=zcNONE;
    if (state == zcON) {
      zc_start(1);
      zc_master(zcON);
      _gpioconfig_.currentZone.zone=1;
      time(&_gpioconfig_.currentZone.started_time);
      _gpioconfig_.currentZone.duration=_gpioconfig_.gpiocfg[1].default_runtime;
      _gpioconfig_.currentZone.type=zcALL;
    }
    return true;
  }

  if (length == 0) {
     //get default length here
     length = _gpioconfig_.gpiocfg[zone].default_runtime;
  }

  // NSF need to Check the array is valid
  //if ( _gpioconfig_.gpiocfg[zone] == NULL) {
  //  return false;
  //}

  if (state == zcON) {
    for (i=1; i < _gpioconfig_.pinscfgs ; i++)
    {
      if ( _gpioconfig_.gpiocfg[i].on_state == digitalRead (_gpioconfig_.gpiocfg[i].pin)) {
        if (zone == i) {
          logMessage (LOG_DEBUG, "Request to turn zone %d on. Zone %d is already on, ignoring!\n",zone,zone);
          return false;
        }
        zc_stop(i);
      }
    } 
    zc_start(zone);
    zc_master(zcON);
    _gpioconfig_.currentZone.type=type;
    _gpioconfig_.currentZone.zone=zone;
    _gpioconfig_.currentZone.duration=length;
    time(&_gpioconfig_.currentZone.started_time);
    return true;
  } else if (state == zcOFF) {
    if (_gpioconfig_.currentZone.type == zcALL) { // If all zones, and told to turn off current, run next zone
      _gpioconfig_.currentZone.started_time = _gpioconfig_.currentZone.started_time - (_gpioconfig_.currentZone.duration * SEC2MIN) - 1;
      zc_check();
    } else {
      zc_master(zcOFF);
      zc_stop(zone);
      _gpioconfig_.currentZone.type=zcNONE;
      _gpioconfig_.currentZone.zone=-1;
    }
    return true;
  }

  return false;
}
bool zc_start(/*zcRunType type,*/ int zone) {
  // check anything on, turn off
  
  // Turn on master valve if we have one
  // turn on zone
  if ( _gpioconfig_.gpiocfg[zone].on_state == digitalRead (_gpioconfig_.gpiocfg[zone].pin)) {
    logMessage (LOG_DEBUG, "Request to turn zone %d on. Zone %d is already on, ignoring!\n",zone,zone);
    return false;
  }
  logMessage (LOG_INFO, "Turning on Zone %d\n",zone);
  digitalWrite(_gpioconfig_.gpiocfg[zone].pin, _gpioconfig_.gpiocfg[zone].on_state );

  // store what's running 

  return true;
}

bool zc_stop(/*zcRunType type,*/ int zone) {

  // Turn on master valve if we have one

  if ( _gpioconfig_.gpiocfg[zone].on_state != digitalRead (_gpioconfig_.gpiocfg[zone].pin)) {
    logMessage (LOG_DEBUG, "Request to turn zone %d off. Zone %d is already off, ignoring!\n",zone,zone);
    return false;
  }
  logMessage (LOG_INFO, "Turning off Zone %d\n",zone);
  digitalWrite(_gpioconfig_.gpiocfg[zone].pin, !_gpioconfig_.gpiocfg[zone].on_state );

  //_gpioconfig_.gpiocfg[zone]
  // turn off zone

  // delete what's running

  return true;
}
