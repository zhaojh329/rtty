# xTTYD([中文](https://github.com/zhaojh329/xttyd/blob/master/README_ZH.md))

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

[ttyd]: https://github.com/tsl0922/ttyd
[libubox]: https://git.openwrt.org/?p=project/libubox.git
[libuwsc]: https://github.com/zhaojh329/libuwsc

Share your terminal over the web. Unlike [ttyd], xTTYD consists of two parts of the client and the server.
xTTYD can share the shell of your device based on the MAC address you specify.

`Keep Watching for More Actions on This Space`

![](https://github.com/zhaojh329/xttyd/blob/master/xttyd.png)

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
    src-git xttyd https://github.com/zhaojh329/xttyd-feed.git

Install xttyd packages:

    ./scripts/feeds update libuwsc xttyd
    ./scripts/feeds install -a -p xttyd

Select package xttyd in menuconfig and compile new image.

    Utilities  --->
        Terminal  --->
            <*> xttyd....................................... xTTYD

# Deploying the server side

	sudo apt install python3 python3-pip
	sudo pip3 install aiohttp uvloop
	git clone https://github.com/zhaojh329/xttyd.git
	cd xttyd/server && ./xttyd.py

# Contributing
If you would like to help making [xttyd](https://github.com/zhaojh329/xttyd) better,
see the [CONTRIBUTING.md](https://github.com/zhaojh329/xttyd/blob/master/CONTRIBUTING.md) file.
