#!/bin/sh

update-rc.d rtty remove
rm -rf /var/www/rtty

mkdir -p /var/www/rtty
cp -r www/* /var/www/rtty
cp rtty.py /usr/sbin/
cp rtty.init /etc/init.d/rtty
update-rc.d rtty defaults
