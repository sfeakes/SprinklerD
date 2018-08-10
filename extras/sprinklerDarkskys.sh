#
#  darkskyAPI            = your DarkSky API key below, it's free to get one 
#  location              = your location lat,long
#  probabilityOver       = Enable rain delay if probability of rain today is grater than this number. 
#                          Range is 0 to 1, so 0.5 is 50%
#  sprinklerdEnableDelay = the URL to SprinklerD
#
darkskyAPI='0123456789abcdef9876543210fedcba'
location='42.3601,-71.0589'
probabilityOver=0.5
sprinklerdEnableDelay="http://localhost/?type=option&option=24hdelay&state=reset"

echoerr() { printf "%s\n" "$*" >&2; }
echomsg() { if [ -t 1 ]; then echo "$@" 1>&2; fi; }

command -v jq >/dev/null 2>&1 || { echoerr "jq is not installed.  Aborting!"; exit 1; }
command -v bc >/dev/null 2>&1 || { echoerr "bc not installed.  Aborting!"; exit 1; }

probability=$(curl -s "https://api.darksky.net/forecast/"$darkskyAPI"/"$location | jq '.["daily"].data[0].precipProbability' 2>/dev/null)

if [ $? -ne 0 ]; then
    echoerr "Error reading DarkSkys URL, please check!"
    echoerr "Maybe you didn't configure your API and location?"
    exit 1;
fi

echomsg -n "Probability of rain today is "`echo "$probability * 100" | bc`"%"

if (( $(echo "$probability > $probabilityOver" | bc -l) )); then
  echomsg -n ", enabeling rain delay"
  curl -s "$sprinklerdEnableDelay" > /dev/null
fi

echomsg ""
