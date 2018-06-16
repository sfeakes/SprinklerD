# sprinklerd


More detailes to come

Has web interface on port 80 (or what ever you configure it to)
```
API endpoints
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

  MQTT
  // Turn stuff on/off (1 is on, 0 is off, but can use txt if you want)
  /sprinklerd/zone/1/set 1
  /sprinklerd/zone/all/set 1
  /sprinklerd/24hdelay/set 1
  /sprinklerd/system/set 1

  // Status of stuff (message is 1=on 0=off)
  sprinklerd/zone/1
  /sprinklerd/zall
  /sprinklerd/24hdelay
  /sprinklerd/system
```

