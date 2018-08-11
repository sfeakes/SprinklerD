
#
# Change the URL's below to point to Meteohub server and SprinklerD server
#

meteohubRainTotal="http://hurricane/meteograph.cgi?text=day1_rain0_total_in"
#meteohubRainTotal="http://192.168.144.101/meteograph.cgi?text=last24h_rain0_total_in"
#meteohubRainTotal="http://192.168.144.101/meteograph.cgi?text=alltime_rain0_total_in"

sprinklerdSetRainTotal="http://localhost/?type=sensor&sensor=raintotal&value="

echoerr() { printf "%s\n" "$*" >&2; }
echomsg() { if [ -t 1 ]; then echo "$@" 1>&2; fi; }

command -v curl >/dev/null 2>&1 || { echoerr "curl is not installed.  Aborting!"; exit 1; }

rainTotal=$(curl -s "$meteohubRainTotal")

if [ $? -ne 0 ]; then
    echoerr "Error reading Metrohub URL, please check!"
    echoerr "Assuming rain sensor is not responding, not sending rain total to SprinklerD"
    exit 1;
fi

echomsg -n "Rain total is $rainTotal\" at `date`"


curl -s "$sprinklerdSetRainTotal$rainTotal" > /dev/null

echomsg ""
