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

#Can use below to track best api for location
#https://www.forecastadvisor.com/

# API Keys from the following are supported
# openweathermap.org weatherapi.com weatherbit.io climacell.co

openweatherAPIkey='-----'
weatherbitAPIkey='-----'
climacellAPIkey='-----'
weatherAPIkey='-----'
accuweatherAPI='-----'

lat='42.3601'
lon='-71.0589'
zip='11111'


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
openmeteoURL="https://api.open-meteo.com/v1/forecast?latitude=$lat&longitude=$lon&daily=precipitation_probability_max&timezone=America%2FChicago&forecast_days=1"
accuweatherURL="http://dataservice.accuweather.com/forecasts/v1/daily/1day/$zip?apikey=$accuweatherAPI&details=true"
pirateweatherURL="https://api.pirateweather.net/forecast/$pirateweatherAPI/$lat%2C$lon?exclude=minutely%2Chourly%2Calerts"


true=0
false=1

OUT_FIRST=1
OUT_MAX=2
OUT_MIN=3
OUT_AVERAGE=4
OUT_PRINT_ONLY=5

output=$OUT_FIRST

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

  if ! [[ $probability =~ ^[0-9]+([.][0-9]+)?$ ]]; then
     echoerr "Did not get a number from probability API result=$probability"
     return $false
  fi 

  probability=$(echo "$probability * $factor / 1" | bc )
  echo $probability
  return $true
}

# Test we have everything installed
command -v curl >/dev/null 2>&1 || { echoerr "curl is not installed.  Aborting!"; exit 1; }
command -v jq >/dev/null 2>&1 || { echoerr "jq is not installed.  Aborting!"; exit 1; }
command -v bc >/dev/null 2>&1 || { echoerr "bc not installed.  Aborting!"; exit 1; }

probArray=()

if [ "$#" -lt 1 ]; then
  echomsg "Pass at least one command line parameter (openweather, weatherbit, climacell, weatherapi, openmeteo, accuweather, pirateweather)"
  echomsg "if you pass more than 1 of above, can also use one of folowing -max -min -average -first"
  echomsg "Example:-"
  echomsg "            $0 openweather weatherapi -average"
  echomsg ""
  exit 1;
fi

for var in "$@"
do
  case "$var" in
    -max)
      output=$OUT_MAX
    ;;
    -min)
      output=$OUT_MIN
    ;;
    -average)
      output=$OUT_AVERAGE
    ;;
    -first)
      output=$OUT_FIRST
    ;;
    -print)
      output=$OUT_PRINT_ONLY
    ;;
    openweather)
      if [ "$openweatherAPIkey" == "-----" ]; then echomsg "missing OpenWeather API key" && continue; fi
      if probability=$(getURL "$openweatherURL" '.["daily"][0].pop' "100"); then
        echomsg "OpenWeather probability of rain today is $probability%"
        #pop=$(echo "if($probability>$pop)print $probability else print $pop" | bc -l)
        probArray+=($probability)
      fi
    ;;
    weatherbit)
      if [ "$weatherbitAPIkey" == "-----" ]; then echomsg "missing WeatherBit API key" && continue; fi
      if probability=$(getURL "$weatherbitURL" '.data[0].pop' "1"); then
        echomsg "WeatherBit probability of rain today is $probability%"
        #pop=$(echo "if($probability>$pop)print $probability else print $pop" | bc -l)
        probArray+=($probability)
      fi
    ;;
    climacell)
      if [ "$climacellAPIkey" == "-----" ]; then echomsg "missing ClimaCell API key" && continue; fi
      if probability=$(getURL "$climacellURL" '.data.timelines[0].intervals[0].values.precipitationProbability' "1"); then
        echomsg "ClimaCell probability of rain today is $probability%"
        probArray+=($probability)
      fi
    ;;
    weatherapi)
      if [ "$weatherAPIkey" == "-----" ]; then echomsg "missing WeatherAPI API key" && continue; fi
      if probability=$(getURL "$weatherAPIlURL" '.forecast.forecastday[0].day.daily_chance_of_rain' "1"); then
        echomsg "WeatherAPI probability of rain today is $probability%"
        probArray+=($probability)
      fi
    ;;
    openmeteo)
      if probability=$(getURL "$openmeteoURL" '.daily.precipitation_probability_max[]' "1"); then
        echomsg "Open Meteo probability of rain today is $probability%"
        probArray+=($probability)
      fi
    ;;
    accuweather)
      if probability=$(getURL "$accuweatherURL" '.DailyForecasts[0].Day.PrecipitationProbability' "1"); then
        echomsg "AccuWeather probability of rain today is $probability%"
        probArray+=($probability)
      fi
    ;;
    pirateweather)
      if probability=$(getURL "$pirateweatherURL" '.daily.data[0].precipProbability' "100"); then
        echomsg "PirateWeather probability of rain today is $probability%"
        probArray+=($probability)
      fi
    ;;
  esac
done


if [ ${#probArray[@]} -le 0 ]; then
  echoerr "No rain probability values found"
  exit 1
fi


prop=${probArray[0]}

case $output in
  $OUT_PRINT_ONLY)
    exit 0;
  ;;
  $OUT_FIRST)
    prop=${probArray[0]}
  ;;
  $OUT_MAX)
    for n in "${probArray[@]}" ; do
      ((n > prop)) && prop=$n
    done
  ;;
  $OUT_MIN)
    for n in "${probArray[@]}" ; do
      ((n < prop)) && prop=$n
    done
  ;;
  $OUT_AVERAGE)
    prop=0
    for n in "${probArray[@]}" ; do
      let prop=prop+n
    done
    let prop=prop/${#probArray[@]}
  ;;
esac

echomsg "Chance of rain $prop%"

curl -s "$sprinklerdProbability$prop" > /dev/null

if (( $(echo "$prop > $probabilityOver" | bc -l) )); then
  echomsg "Enabeling rain delay"
  curl -s "$sprinklerdEnableDelay" > /dev/null
fi

exit 0

