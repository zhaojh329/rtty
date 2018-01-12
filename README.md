# rtty([中文](https://github.com/zhaojh329/rtty/blob/master/README_ZH.md))

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

[libubox]: https://git.openwrt.org/?p=project/libubox.git
[libuwsc]: https://github.com/zhaojh329/libuwsc

Access your terminal over the web browser. The 'r' in the name refers to the 'Reverse Proxy' or 'Remote'.
It is composed of the client and the server. You can access any of your terminals through a web browser based on the MAC address.

`Keep Watching for More Actions on This Space`

![](https://github.com/zhaojh329/rtty/blob/master/rtty.png)

# Dependencies for Server side
* [python3](https://www.python.org)
* [uvloop](https://github.com/MagicStack/uvloop)
* [aiohttp](https://github.com/aio-libs/aiohttp)

# Dependencies for Client side
* [libubox]
* [libuwsc]

# How to use on OpenWRT
add new feed into "feeds.conf.default":

    src-git libuwsc https://github.com/zhaojh329/libuwsc-feed.git
    src-git rtty https://github.com/zhaojh329/rtty-feed.git

Install rtty packages:

    ./scripts/feeds update libuwsc rtty
    ./scripts/feeds install -a -p rtty

Select package rtty in menuconfig and compile new image.

    Utilities  --->
        Terminal  --->
            <*> rtty................................... Share your terminal over the web

Configuring the server parameter

    uci set rtty.@server[0].host='your server host'
    uci set rtty.@server[0].port='your server port'
    uci commit
    /etc/init.d/rtty restart

# Deploying the server side
Install dependencies

	sudo apt install python3 python3-pip
	sudo pip3 install aiohttp uvloop

clone code

	git clone https://github.com/zhaojh329/rtty.git

Manual run

	cd rtty/server && ./rtty.py -p 5912

Install the automatic boot script

    sudo ./install.sh
    sudo /etc/init.d/rtty start

# Use your web browser to access your server, then enter the MAC address of the terminal you want to access, and then click the connection button
http://your-server-host:5912

# Contributing
If you would like to help making [rtty](https://github.com/zhaojh329/rtty) better,
see the [CONTRIBUTING.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING.md) file.
