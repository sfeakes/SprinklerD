#!/bin/bash
#
#  you MUST change at least one of the following API keys, and the lat lng variables.
#  openweatherAPI, weatherAPIkey, weatherbitAPIkey, climacellAPIkey
#  lat                   = your latitude
#  lon                   = your longitude
#  probabilityOver       = Enable rain delay if probability of rain today is grater than this number. 
#                          Range is 0 to 100, so 50 is 50%
#  sprinklerdEnableDelay = the URL to SprinklerD
#
# The below are NOT valid, you MUST change them to your information.

# API Keys from the following are supported
# openweathermap.org weatherapi.com weatherbit.io climacell.co

openweatherAPIkey='-----'
weatherbitAPIkey='-----'
climacellAPIkey='-----'
weatherAPIkey='-----'

lat='42.3601'
lon='-71.0589'

# 101 means don't set rain delay from this script, use the SprinklerD config (webUI) to decide if to set delay
probabilityOver=101 

# If you are not running this from the same host as SprinklerD, then change localhost in the below.
sprinklerdHost="http://localhost:80"



########################################################################################################################
#
# Should not need to edit below this line.
#
########################################################################################################################


sprinklerdEnableDelay="$sprinklerdHost/?type=option&option=24hdelay&state=reset"
sprinklerdProbability="$sprinklerdHost/?type=sensor&sensor=chanceofrain&value="

openweatherURL="https://api.openweathermap.org/data/2.5/onecall?lat=$lat&lon=$lon&appid=$openweatherAPIkey&exclude=current,minutely,hourly,alerts"
weatherbitURL="https://api.weatherbit.io/v2.0/forecast/daily?key=$weatherbitAPIkey&lat=$lat&lon=$lon"
climacellURL="https://data.climacell.co/v4/timelines?location=$lat%2C$lon&fields=precipitationProbability&timesteps=1d&units=metric&apikey=$climacellAPIkey"
weatherAPIlURL="http://api.weatherapi.com/v1/forecast.json?key=$weatherAPIkey&q=$lat,$lon&days=1&aqi=no&alerts=no"

true=0
false=1

echoerr() { printf "%s\n" "$*" >&2; }
echomsg() { if [ -t 1 ]; then echo "$@" 1>&2; fi; }

function getURL() {
  url=$1
  jq=$2
  factor=$3

  JSON=$(curl -s "$url")

  if [ $? -ne 0 ]; then
    echoerr "Error reading URL, '$1' please check!"
    echoerr "Maybe you didn't configure your API and location?"
    echo 0;
    return $false
  else
    probability=$(echo $JSON | jq "$jq" )
  fi

  probability=$(echo "$probability * $factor / 1" | bc )
  echo $probability
  return $true
}

# Test we have everything installed
command -v curl >/dev/null 2>&1 || { echoerr "curl is not installed.  Aborting!"; exit 1; }
command -v jq >/dev/null 2>&1 || { echoerr "jq is not installed.  Aborting!"; exit 1; }
command -v bc >/dev/null 2>&1 || { echoerr "bc not installed.  Aborting!"; exit 1; }

pop=-1

if [ "$#" -lt 1 ]; then
  echomsg "Pass at least one command line parameter (openweather, weatherbit, climacell, weatherapi)"
  exit 1;
fi

for var in "$@"
do
  case "$var" in
    openweather)
      if [ "$openweatherAPIkey" == "-----" ]; then echomsg "missing OpenWeather API key" && continue; fi
      if probability=$(getURL "$openweatherURL" '.["daily"][0].pop' "100"); then
        echomsg "OpenWeather probability of rain today is $probability%"
        pop=$(echo "if($probability>$pop)print $probability else print $pop" | bc -l)
      fi
    ;;
    weatherbit)
      if [ "$weatherbitAPIkey" == "-----" ]; then echomsg "missing WeatherBit API key" && continue; fi
      if probability=$(getURL "$weatherbitURL" '.data[0].pop' "1"); then
        echomsg "WeatherBit probability of rain today is $probability%"
        pop=$(echo "if($probability>$pop)print $probability else print $pop" | bc -l)
      fi
    ;;
    climacell)
      if [ "$climacellAPIkey" == "-----" ]; then echomsg "missing ClimaCell API key" && continue; fi
      if probability=$(getURL "$climacellURL" '.data.timelines[0].intervals[0].values.precipitationProbability' "1"); then
        echomsg "ClimaCell probability of rain today is $probability%"
        pop=$(echo "if($probability>$pop)print $probability else print $pop" | bc -l)
      fi
    ;;
    weatherapi)
      if [ "$weatherAPIkey" == "-----" ]; then echomsg "missing WeatherAPI API key" && continue; fi
      if probability=$(getURL "$weatherAPIlURL" '.forecast.forecastday[0].day.daily_chance_of_rain' "1"); then
        echomsg "WeatherAPI probability of rain today is $probability%"
        pop=$(echo "if($probability>$pop)print $probability else print $pop" | bc -l)
      fi
    ;;
  esac
done

# Remove all new line, carriage return, tab characters
# from the string, to allow integer comparison
pop="${pop//[$'\t\r\n ']}"

if [ "$pop" -lt 0 ]; then
  echoerr "Error reading Probability, please check!"
  exit 1
fi

echomsg "Chance of rain $pop%"

curl -s "$sprinklerdProbability$pop" > /dev/null

if (( $(echo "$pop > $probabilityOver" | bc -l) )); then
  echomsg "Enabeling rain delay"
  curl -s "$sprinklerdEnableDelay" > /dev/null
fi

exit 0
