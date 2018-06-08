#!/bin/sh

port=5913
host="192.168.0.100"

cnt=$1

[ -n "$cnt" ] || cnt=100

i=0

while [ $i -ne $cnt ]
do
    rtty -I "test-$i" -d "description-$i" -h $host -p $port &
    i=$((i+1))
done
