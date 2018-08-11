
#ifdef USE_WIRINGPI
  #include <wiringPi.h>
#else
  #include "sd_GPIO.h"
#endif

#include "zone_ctrl.h"
#include "config.h"
#include "utils.h"

bool zc_start(int zone);
bool zc_stop(int zone);
void zc_master(zcState state);

int calc_timeleft() {
  if (_sdconfig_.currentZone.zone != -1) {
    // See if the duration time changed since we started.
    if ( _sdconfig_.currentZone.duration != _sdconfig_.zonecfg[_sdconfig_.currentZone.zone].default_runtime)
      _sdconfig_.currentZone.duration=_sdconfig_.zonecfg[_sdconfig_.currentZone.zone].default_runtime;
    
    time_t now;
    time(&now);
    _sdconfig_.currentZone.timeleft = (_sdconfig_.currentZone.duration * SEC2MIN) - difftime(now, _sdconfig_.currentZone.started_time);
  } else {
    _sdconfig_.currentZone.timeleft = 0;
  }

  //logMessage (LOG_DEBUG, "Zone %d time left %d %d min\n",_sdconfig_.currentZone.zone, _sdconfig_.currentZone.timeleft, (_sdconfig_.currentZone.timeleft / 60) +1);

  return _sdconfig_.currentZone.timeleft;
}

bool zc_check() {
  // check what's running, and if time now is greater than duration Stop if grater
  // Move onto next zone if (all) runtype

  if ( _sdconfig_.currentZone.type == zcNONE)
    return false;

  //time_t now;
  //time(&now);
  //int secleft = (_sdconfig_.currentZone.duration * SEC2MIN) - difftime(now, _sdconfig_.currentZone.started_time);
  //_sdconfig_.currentZone.timeleft = secleft / 60 + 1;
  logMessage (LOG_DEBUG, "Zone running is %d of type %d\n",_sdconfig_.currentZone.zone, _sdconfig_.currentZone.type);
  //if (difftime(now, _sdconfig_.currentZone.started_time) > _sdconfig_.currentZone.duration * SEC2MIN){
  if (calc_timeleft() <= 0){
    if (_sdconfig_.currentZone.type==zcALL && _sdconfig_.currentZone.zone < _sdconfig_.zones) {
      zc_stop(_sdconfig_.currentZone.zone);
      _sdconfig_.currentZone.zone++;
      zc_start(_sdconfig_.currentZone.zone);
      time(&_sdconfig_.currentZone.started_time);
      _sdconfig_.currentZone.duration=_sdconfig_.zonecfg[_sdconfig_.currentZone.zone].default_runtime;
      calc_timeleft();
    } else {
      zc_master(zcOFF);
      zc_stop(_sdconfig_.currentZone.zone);
      _sdconfig_.currentZone.type=zcNONE;
      _sdconfig_.currentZone.zone=-1;
      _sdconfig_.currentZone.timeleft = 0;
    }

    return true;
  }

  return false;
}

void zc_rain_delay_enabeled() {
  // Turn off any running zone that was enabeled by cron, leave on any zones that were manually turned on.

  if(_sdconfig_.currentZone.type==zcCRON && _sdconfig_.currentZone.zone > 0) {
    logMessage (LOG_INFO, "Turning off zone %d '%s' due to rain delay\n",_sdconfig_.currentZone.zone, _sdconfig_.zonecfg[_sdconfig_.currentZone.zone].name);
    zc_zone(zcSINGLE, _sdconfig_.currentZone.zone, zcOFF, 0);
  }
  
}

void zc_master(zcState state) {

  if (!_sdconfig_.master_valve)
    return;

  if (state == zcON && _sdconfig_.zonecfg[0].on_state != digitalRead (_sdconfig_.zonecfg[0].pin ))
    zc_start(0);
  else if (state == zcOFF && _sdconfig_.zonecfg[0].on_state == digitalRead (_sdconfig_.zonecfg[0].pin ))
    zc_stop(0);
}

bool zc_zone(zcRunType type, int zone, zcState state, int length) {

  int i;
  // Check to see if duplicate request
  if (zone > 0 && zone <= _sdconfig_.zones) {
    zcState cstate = _sdconfig_.zonecfg[zone].on_state==digitalRead (_sdconfig_.zonecfg[zone].pin)?zcON:zcOFF;
    if (cstate == state) {
      logMessage (LOG_INFO, "Request to turn zone %d %s. Ignored, zone is already %s\n",zone,state==zcON?"ON":"OFF",cstate==zcON?"ON":"OFF");
      return false;
    }
  } else if (type == zcALL && ( (state == zcON && _sdconfig_.currentZone.type == zcALL) || (state == zcOFF && _sdconfig_.currentZone.type != zcALL)) ) {
    logMessage (LOG_INFO, "Ignore request to cycle all zones %s\n",state==zcON?"ON":"OFF");
    return false;
  }

  if (type == zcCRON && state == zcON && (_sdconfig_.calendar == false || _sdconfig_.delay24h == true) ) {
    logMessage (LOG_WARNING, "Request to turn zone %d on. Ignored due to %s!\n",zone,_sdconfig_.delay24h?"Rain Delay active":"Calendar off");
    return false;
    // Check cal & 24hdelay, return if cal=false or 24hdelay=true
  } else if (type == zcALL) {
    // If we are turning on we ned to start with all zones off, if off, turn them off anyway.
    for (i=1; i <= _sdconfig_.zones ; i++)
    {
      zc_master(zcOFF);
      if ( _sdconfig_.zonecfg[i].on_state == digitalRead(_sdconfig_.zonecfg[i].pin) )
        zc_stop(i); 
    }
    _sdconfig_.currentZone.type=zcNONE;
    if (state == zcON) {
      zc_start(1);
      zc_master(zcON);
      _sdconfig_.currentZone.zone=1;
      time(&_sdconfig_.currentZone.started_time);
      _sdconfig_.currentZone.duration=_sdconfig_.zonecfg[1].default_runtime;
      _sdconfig_.currentZone.type=zcALL;
      calc_timeleft();
    }
    return true;
  }

  if (length == 0) {
     //get default length here
     length = _sdconfig_.zonecfg[zone].default_runtime;
  }

  // NSF need to Check the array is valid
  //if ( _sdconfig_.zonecfg[zone] == NULL) {
  //  return false;
  //}

  if (state == zcON) {
    for (i=1; i <= _sdconfig_.zones ; i++)
    {
      if ( _sdconfig_.zonecfg[i].on_state == digitalRead (_sdconfig_.zonecfg[i].pin)) {
        if (zone == i) {
          logMessage (LOG_DEBUG, "Request to turn zone %d on. Zone %d is already on, ignoring!\n",zone,zone);
          return false;
        }
        zc_stop(i);
      }
    } 
    zc_start(zone);
    zc_master(zcON);
    _sdconfig_.currentZone.type=type;
    _sdconfig_.currentZone.zone=zone;
    _sdconfig_.currentZone.duration=length;
    time(&_sdconfig_.currentZone.started_time);
    calc_timeleft();
    return true;
  } else if (state == zcOFF) {
    if (_sdconfig_.currentZone.type == zcALL) { // If all zones, and told to turn off current, run next zone
      _sdconfig_.currentZone.started_time = _sdconfig_.currentZone.started_time - (_sdconfig_.currentZone.duration * SEC2MIN) - 1;
      zc_check();
    } else {
      zc_master(zcOFF);
      zc_stop(zone);
      _sdconfig_.currentZone.type=zcNONE;
      _sdconfig_.currentZone.zone=-1;
    }
    return true;
  }

  return false;
}
bool zc_start(/*zcRunType type,*/ int zone) {
  // check anything on, turn off
  
  // Turn on master valve if we have one
  // turn on zone
  if ( _sdconfig_.zonecfg[zone].on_state == digitalRead (_sdconfig_.zonecfg[zone].pin)) {
    logMessage (LOG_DEBUG, "Request to turn zone %d on. Zone %d is already on, ignoring!\n",zone,zone);
    return false;
  }
  logMessage (LOG_NOTICE, "Turning on Zone %d\n",zone);
  digitalWrite(_sdconfig_.zonecfg[zone].pin, _sdconfig_.zonecfg[zone].on_state );
  _sdconfig_.eventToUpdateHappened = true;
  // store what's running 

  return true;
}

bool zc_stop(/*zcRunType type,*/ int zone) {

  // Turn on master valve if we have one

  if ( _sdconfig_.zonecfg[zone].on_state != digitalRead (_sdconfig_.zonecfg[zone].pin)) {
    logMessage (LOG_DEBUG, "Request to turn zone %d off. Zone %d is already off, ignoring!\n",zone,zone);
    return false;
  }
  logMessage (LOG_NOTICE, "Turning off Zone %d\n",zone);
  digitalWrite(_sdconfig_.zonecfg[zone].pin, !_sdconfig_.zonecfg[zone].on_state );
  _sdconfig_.eventToUpdateHappened = true;
  //_sdconfig_.zonecfg[zone]
  // turn off zone

  // delete what's running

  return true;
}
