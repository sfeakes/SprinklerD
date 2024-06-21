
# SprinklerD

More details & pics to come

linux daemon to control sprinklers.<br> 
* <b>This daemon is designed to be small, efficient and fail safe.</b>

The sprinkler zone control is done through GPIO pins, so simply connect a relay board to the GPIO pins, and the realys to your sprinkler valves & power supply. Provides web UI, MQTT client & HTTP API endpoints. So you can control your pool equiptment from any phone/tablet or computer, and should work with just about Home control systems, including Apple HomeKit, Samsung, Alexa, Google, etc home hubs.
It is not designed to be a feature rich solution with elaborate UI, but rather a solution that can be completley controlled from smart hubs. It can run completley self contained without any automation hub as it does have a web UI and calendar for scheduling zone runtimes, rain day delay etc. But advanced features like integrated rain censors and web forecast delays should be done through your smart hub, or the supplied scripts.
It does support a master valve or pump. (ie turn on a master device with every zone).

### It does not provide any layer of security. NEVER directly expose the device running this software to the outside world, only indirectly through the use of Home Automation hub's or other securty measures, e.g. VPNs.

## Builtin WEB Interface.
<img src="extras/IMG_0236.png?raw=true" width="350"></img>
<br>
<img src="extras/CalendarUI.PNG?raw=true" width="600"></img>

## In Apple Home app.
<img src="extras/IMG_0246.png?raw=true" width="350"></img>
<img src="extras/IMG_0247.png?raw=true" width="350"></img>
<img src="extras/IMG_0248.png?raw=true" width="350"></img>
* Full support for Irrigation Valve in Homekit.

## The Setup I use. 
### Pi Zero W, 8 channel relay board & 2 channel relay board.
All rain delays are set directly from my home automation hub, and not any locally connected sensors. (see included scripts in the config section below) ie :-
* if rain sensor detects rain, cancel any running zones & delay 24h.
* if rain sensor accumulates more than 5mm rain in 24hours, delay sprinklers for 48 hours.
* Poll DarySkys API if rain forecast is higher than 50% enable 24h delay.

<img src="extras/IMG_0243.jpg?raw=true" width="700"></img>

Valves for each zone are connected to the relays, and relays connected to a 24vac power adapter, similar to wiring in the image below.

<img src="extras/example valves.png?raw=true" width="700"></img>

# TL;DR Install
## Quick instal if you are using Raspberry PI
* There is a chance the pre-compiled binary will run, copy the git repo and run the install.sh script from the release directory. ie from the install directory `sudo ./release/install.sh`
* try to run it with :-
    * `sudo sprinklerd -v -d -c /etc/sprinklerd.conf`
    * If it runs, then start configuring it to your setup.
    * If it fails, try making the execuatable, below are some instructions.

## Making & Install
* Get a copy of the repo using git.
* run make
* run sudo make install
* edit `/etc/sprinklerd.conf` to your setup
* Test every works :-
    * `sudo sprinklerd -d -c /etc/sprinklerd.conf`
    * point a web browser to the IP of the box running sprinklerd
* If all is good enable as service
    * sudo `systemctl start sprinklerd`
## More Advanced make
* sprinklerd uses it's own GPIO interaction. This may not be enough for all types of relay boards, especially ones that require the use of the GPIO's internal pull-up or pull-down resistor. So sprkinlerd can make use or WiringPi which is a far more powerful GPIO library with tons of support. 
* To use WiringPi :-
* install WiringPI first `http://wiringpi.com/download-and-install/`
* Make sprinklerd with the below flag
* `make USE_WIRINGPI=1`
* Make sure to use the WPI_PIN and not GPIO_PIN in the configuration for the pin numbers.
* you can also use GPIO_PULL_UPDN option in the configuration.


### Note:-
The install script is designed for systemd / systemctl to run as a service or daemon. If you are using init / init-d then don't run the install script, install manually and the init-d script to use is in the xtras directory.
Manual install for init-d systems
* copy ./release/sprinklerd to /usr/local/bin
* copy ./release/sprinklerd.conf to /etc
* copy ./release/sprinklerd.service.avahi to /etc/avahi/services/sprinklerd.service
* copy ./extras/sprinklerd.init.d to /etc/init-d/sprinklerd
* copy recuesivley ./web/* to /var/www/sprinklerd/
* sudo update-rc.d sprinklerd defaults


## Hardware
You will need relays connected to the PI. (add some crap about how to do that here) 

Raspberry Pi Zero is the perfect device for this software. But all Raspberry PI's are inherently unstable devices to be running 24/7/365 in default Linux configrations. This is due to the way they use CF card, a power outage will generally cause CF card coruption. My recomendation is to use what's calles a "read only root" installation of Linux. Converting Raspbian to this is quite simple, but will require some Linux knoladge. There are two methods, one uses overlayfs and if you are not knolagable in Linux this is the easiest option. There are a some downsides to this method on a PI, so my prefered method is to simply use tmpfs on it's own without overlayfs ontop, this is easier to setup initially, but will probably require a few custom things to get right as some services will fail. Once you are up and running, You should search this topic, and there are a plenty of resources, and even some scripts the will do everything for you.  But below are two links that explain the process.

[Good overlayfs tutorial on PI Forums](https://www.raspberrypi.org/forums/viewtopic.php?t=161416)

[My prefered way to impliment](https://hallard.me/raspberry-pi-read-only/)

I have my own scripts to do this for me, and probably won't ever document or publish them, but thay are very similar to the 2nd link above.

## sprinklerd Configuration
Please see the [sprinklerd.conf](https://github.com/sfeakes/sprinklerd/blob/master/release/sprinklerd.conf) 
example in the release directory.  Many things are turned off by default, and you may need to enable or configure them for your setup.
Specifically, make sure you configure your MQTT, Pool Equiptment Labels & Domoticz ID's in there, looking at the file it should be self explanatory. 

## Included scripts in extras directory.

[sprinklerRainProbability.sh](https://github.com/sfeakes/sprinklerd/blob/master/extras/sprinklerRainProbability.sh)

Script to check the chance of rain from forecast OpenWeather, WeaterBit, Climacell, WeatherAPI, if it's greater than a configured percentage then enable the 24h rainDelay on SprinklerD.
Simply edit the scrips with your values, and use cron to run it 30mins before your sprinkelrs are due to turn on each day. If the chance of rain is over your configured % it will enable SprinklerD's 24h rain delay, which will stop the sprinklers running that day, and the 24h delay will timeout before the sprinklers are due to run the next day. (or be enabeled again if the chance of rain is high).
You will need to edit this script for your API keys.

Example cron entry  
`0 5 * * * ~/bin/sprinklerRainProbability.sh weatherapi`

<!--
[sprinklerDarkskys.sh](https://github.com/sfeakes/sprinklerd/blob/master/extras/sprinklerDarkskys.sh)
Script to check the chance of rain from darkskys forecast API, if it's greater than a configured percentage then enable the 24h rainDelay on SprinklerD.
Simply edit the scrips with your values, and use cron to run it 30mins before your sprinkelrs are due to turn on each day. If the chance of rain is over your configured % it will enable SprinklerD's 24h rain delay, which will stop the sprinklers running that day, and the 24h delay will timeout before the sprinklers are due to run the next day. (or be enabeled again if the chance of rain is high)
You can also use this script to simply sent the 'chance or rain today' to SprinklerD, and allow SprinklerD to make the decision if to enable rain delay, depending on options configured in the UI.

[sprinklerOpenWeather.sh](https://github.com/sfeakes/sprinklerd/blob/master/extras/sprinklerOpenWeather.sh)
Exactly the same as above (sprinklerDarkskys.sh) but uses OpenWeather forecast API.

[sprinklerMeteohub.sh](https://github.com/sfeakes/sprinklerd/blob/master/extras/sprinklerMeteohub.sh)
Script to pull daily rain total from Meteohub and send it to SprinklerD.  SprinklerD will then turn on rain dalys (14h or 48h) depending on rain threshold.

[sprinklerRainDelay.sh](https://github.com/sfeakes/sprinklerd/blob/master/extras/sprinklerRainDelay.sh)
Script to enable extended rain delays. i.e. I have my weatherstation triger this script when it detects rain. The script will cancel any running zones and enable a rain delay.  You can use a number on command line parameter to enable long delays, (number represents number of days).  So if my rainsensor logs over 5mm rain in any 24h period it will call this script with a 2 day delay, or 3 day delay if over 15mm of rain. 
-->

# Configuration with home automation hubs

## Apple HomeKit (prefered way)

You need to install 3 things MQTT broker, Homebridge & Homebridge-sprinklerd.If you already have any of them install, just skip that section.

### Install MQTT Broker (mosquitto)
```
sudo apt-get install mosquitto
```
### Install Homebridge
Follow the instructions in this URL :-
https://github.com/nfarina/homebridge
### Install HomeBridge-SprinklerD
See https://github.com/sfeakes/homebridge-sprinklerd for package content

```
sudo npm install -g homebridge-sprinklerd
```

Configure as you would any other homebridge accessory. i.e Add the platform details to homebridge config.json
```
"platforms": [
        {
            "platform": "sprinklerd",
            "name": "sprinklerd",
            "server": "my-server-or-ip-running-sprinklerd",
            "port": "80",
            "mqtt": {
              "host": "my-mqtt-server",
              "port": 1883,
              "topic": "sprinklerd"
            }
       }
    ],
```
* See bottom for old homekit integration using Homebridge2MQTT.

## All other hubs

Two interfaces are offered, MQTT and WEB APIs, take your pick as the the best option for your own setup, details of each and some generic setups are below.

## MQTT
sprinklerd supports generic MQTT implimentations, as well as specific Domoticz one described above.
To enable, simply configure the main topic in `sprinklerd.conf`. 

```mqtt_topic = sprinklerd```

Then status messages will be posted to the sub topics listed below, with appropiate information.
```
  Status of stuff (message is 1=on 0=off)
  sprinklerd/zone/1
  sprinklerd/zall
  sprinklerd/24hdelay
  sprinklerd/calendar
```

To turn something on, or set information, simply add `set` to the end of the above topics, and post 1 or 0 in the message for a button. Topics sprinklerd will act on.
```
Turn stuff on/off (1 is on, 0 is off, but can use txt if you want)
sprinklerd/zone/1/set 1
sprinklerd/zone/zall/set 1   // Cycle all zones using default runtimes.
sprinklerd/24hdelay/set 1
sprinklerd/calendar/set 1
sprinklerd/chanceofrain/set 29  // % chance of rain for today
sprinklerd/raintotal/set 0.3    // Set rain total in inches.
```


## All other hubs (excluding Apple HomeKit) Amazon,Samsung,Google etc
Obviously details will be diferent on each device, and I won't document them all. Basic idea is create a switch for item you want to control (zone(s), 24hdelay, calendar schedule etc). Then add the following URL to program the switching of each device.
If you use a hub on you local lan like smartthings, then this is super simple. If you use cloud only device, like Alexa then you need to make a connection from your lan to amazon cloud since you should not put this web interface on the internet (since there is no security). There are 1001 ways to do this, but a MQTT bridge may be the easiest, search `MQTT to <my cloud-based hub> bridge` and pick one you like.
```
http://sprinklerd.ip.address:port?type=option&option=24hdelay&state=off
```
```
* // Info options
<host>?type=firstload               // JSON status Include cal schedule
<host>?type=read                    // JSON status no need to include cal schedule
<host>?type=json                    // JSON full array style, need full parser to pass.

*  // Cfg options
<host>?type=option&option=calendar&state=off    // turn off calendar
<host>?type=option&option=24hdelay&state=off    // turn off 24h delay
<host>?type=option&option=24hdelay&state=reset  // reset time on 24h delay
<host>?type=option&option=24hdelay&state=reset&time=1529328782  // reset custom time on delay (utime in seconds)

*  // Calendar
<host>?type=calcfg&day=3&zone=&time=07:00      // Add day schedule and use default water zone times
<host>?type=calcfg&day=2&zone=1&time=7         // Change water zone time (0 time is off)
<host>?type=calcfg&day=3&zone=&time=           // Delete day schedule
  
*  // Run options
<host>?type=option&option=allz&state=on        // Run all zones default times (ignore 24h delay & calendar settings)
<host>?type=zone&zone=2&state=on&runtime=3     // Run zone 2 for 3 mins (ignore 24h delay & calendar settings)
<host>?type=zone&zone=2&state=off              // Turn off zone 2
<host>?type=zone&zone=2&state=flip             // Flip state of zone 2, Turn off if on, Turn on if off 
<host>?type=zrtcfg&zone=2&time=10              // change zone 2 default runtime to 10
<host>?type=cron&zone=1&runtime=12&state=on'   // Run zone 1 for 12 mins (calendar & 24hdelay settings overide this request)

* // Sensor config options
<host>?type=config&option=raindelaychance&value=50  // Enable 24h rain delay is % change or rain today is higher than this value
<host>?type=config&option=raindelaytotal1&value=0.5 // Enable 24h rain delay is total rain was greater than this value
<host>?type=config&option=raindelaytotal2&value=1.5 // Enable 48h rain delay is total rain was greater than this value

* // Sensor options (remote sensors posting to SprinklerD)
* // These are used to determin if to enable rain delay and cancel running zones.
<host>?type=sensor&sensor=chanceofrain&value=87  // Set % chance of rain for today to 87%
<host>?type=sensor&sensor=raintotal&value=0.34   // Set rain total for todaye to 0.34"
```
The JSON that's returned is completley flat, this is so it can be passed with really small lightweight passers, even grep and awk. If you want a full JSON with arrays that's easier to use with full json parsers you can use the below
```
<host>?type=json
```


## Apple HomeKit (old way)
For the moment, native Homekit support has been removed, it will be added back in the future under a different implimentation. 
Recomended option for HomeKit support is to make use of the MQTT interface and use [HomeKit2MQTT](https://www.npmjs.com/package/homekit2mqtt) to bridge between sprinklerd and you Apple (phone/tablet/tv & hub).
* If you don't already have an MQTT broker Installed, install one. Mosquitto is recomended, this can usually be installed with apt-get
* Install [HomeKit2MQTT](https://www.npmjs.com/package/homekit2mqtt). (see webpage for install)
* Then copy the [homekit2mqtt configuration](https://github.com/sfeakes/sprinklerd/blob/master/extras/homekit2mqtt.json) file found in the extras directory `homekit2mqtt.json`


You can of course use a myriad of other HomeKit bridges with the URL endpoints listed in the `All other hubs section`, or MQTT topics listed in the `MQTT` section. The majority of them (including HomeBridge the most popular) use Node and HAP-Node.JS, neither of which I am a fan of for the RaspberryPI. But HomeKit2MQTT seemed to have the least overhead of them all. So that's why the recomendation.

## Domoticz
Does naivety support domoticz, need to add documentation.
* enable MQTT in domoticz
* add virtual censor for every zone and buton
* add the domoticz ID's in he sprinklerd config.


# License
## Non Commercial Project
All non commercial projects can be run using our open source code under GPLv2 licensing. As long as your project remains in this category there is no charge.
See License.md for more details.

