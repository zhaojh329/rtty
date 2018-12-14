#!/bin/sh

port=5912
host="localhost"

cnt=$1

[ -n "$cnt" ] || cnt=100

i=0

gen_mac() {
	m1=$(date +%s%N | md5sum | head -c 2)
	m2=$(date +%s%N | md5sum | head -c 2)
	m3=$(date +%s%N | md5sum | head -c 2)
	m4=$(date +%s%N | md5sum | head -c 2)
	m5=$(date +%s%N | md5sum | head -c 2)
	m6=$(date +%s%N | md5sum | head -c 2)

	echo "$m1$m2$m3$m4$m5$m6" | tr '[:lower:]' '[:upper:]'	
}

while [ $i -ne $cnt ]
do
    rtty -I "$(gen_mac)" -d "$(date +%s%N | md5sum | head -c 20)" -h $host -p $port -s &
    i=$((i+1))
done
