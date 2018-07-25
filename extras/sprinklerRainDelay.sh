#!/bin/bash

URL="http://sprinkler/?type=option&option=24hdelay&state=reset&time="


if [[ "$1" =~ ^[0-9]+$ ]]; then
  days=$1
else
  echo "No days listed, using 1 day" 
  days=1
fi

delaytime=$(/bin/date -d "now + $days days" "+%s")
cnt=0

while [ "$rtn" != "200" ]; do
  rtn=$(/usr/bin/curl -s -o /dev/null -w "%{http_code}" "$URL$delaytime")
  if [ "$rtn" != "200" ]; then
    cnt=$[$cnt + 1]
    if [ $cnt -lt 3 ]; then 
      sleep 10
    fi
  fi
done

