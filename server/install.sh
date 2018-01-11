#!/bin/sh

update-rc.d xttyd remove
rm -rf /var/www/xttyd

mkdir -p /var/www/xttyd
cp -r www/* /var/www/xttyd
cp xttyd.py /usr/sbin/
cp xttyd.init /etc/init.d/xttyd
update-rc.d xttyd defaults
