# rtty([中文](/README_ZH.md))

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

[Xterm.js]: https://github.com/xtermjs/xterm.js
[libubox]: https://git.openwrt.org/?p=project/libubox.git
[libuwsc]: https://github.com/zhaojh329/libuwsc
[ustream-ssl]: https://git.openwrt.org/?p=project/ustream-ssl.git
[openssl]: https://github.com/openssl/openssl
[mbedtls]: https://github.com/ARMmbed/mbedtls
[CyaSSl(wolfssl)]: https://github.com/wolfSSL/wolfssl
[vue]: https://github.com/vuejs/vue
[iview]: https://github.com/iview/iview

A reverse proxy WebTTY. It is composed of the client and the server. It is composed of the client and the
[server](https://github.com/zhaojh329/rttys). The server is written in go language and uses the [vue]+[iview].
You can access any of your terminals through a web browser based on the device ID you set.

rtty is very suitable for remote maintenance your or your company's thousands of Linux devices deployed around the world.

`Keep Watching for More Actions on This Space`

![](/rtty.svg)
![](/rtty.gif)

# Features
* Simple to deployment and easy to use
* Reverse Proxy
* Connect your device according to the ID you set up
* Fully-featured terminal based on [Xterm.js]
* SSL support: openssl, mbedtls, CyaSSl(wolfssl)
* Cross platform: macOS, Linux, FreeBSD/OpenBSD, OpenWrt/LEDE

# Dependencies for Client side
* [libubox]
* [libuwsc]
* [ustream-ssl] - If you need to support SSL
* [mbedtls] - If you choose mbedtls as your SSL backend
* [CyaSSl(wolfssl)] - If you choose wolfssl as your SSL backend
* [openssl] - If you choose openssl as your SSL backend

# Deploying the server side
Install the GO language environment (if you haven't installed it)

    sudo apt-get install golang     # For Ubuntu

    sudo yum install golang         # For Centos

Set GOPATH environment variable(if you haven't set it)(since Go 1.8, default is $HOME/go)

    export GOPATH=$HOME/go

Install rtty server

    go get github.com/zhaojh329/rttys

Manual run

    $GOPATH/bin/rttys -port 5912

Install the automatic boot script

    cd $GOPATH/src/github.com/zhaojh329/rttys
    sudo ./install.sh
    sudo /etc/init.d/rttys start

# How to build and install the Client - rtty
## For Linux distribution, such as Ubuntu and Centos
Install build tools

    sudo apt install gcc cmake git      # For Ubuntu

    yum install gcc cmake git           # For Centos

Install dependent packages

    sudo apt install libjson-c-dev      # For Ubuntu

    sudo apt install json-c-devel       # For Centos

    git clone https://git.openwrt.org/project/libubox.git
    cd libubox && cmake -DBUILD_LUA=OFF . && sudo make install

    git clone https://github.com/zhaojh329/libuwsc.git
    cd libuwsc && cmake -DUWSC_SSL_SUPPORT=OFF . && sudo make install

Install RTTY
    
    git clone https://github.com/zhaojh329/rtty.git
    cd rtty && cmake . && sudo make install

Run RTTY
Replace the following parameters with your own parameters

    sudo rtty -I 'My-device-ID' -h 'jianhuizhao.f3322.net' -p 5912 -a -v -d 'My Device Description'

Query online devices

    curl http://jianhuizhao.f3322.net:5912/list
    [{"id":"My-device-ID","description":"My device"}]

## For Embedded Linux Platform
You need to cross compiling by yourself

## For OpenWRT
add new feed into "feeds.conf.default":

    src-git libuwsc https://github.com/zhaojh329/libuwsc-feed.git
    src-git rtty https://github.com/zhaojh329/rtty-feed.git


for chaos_calmer(15.05)

    src-git libuwsc https://github.com/zhaojh329/libuwsc-feed.git;for-15.05
    src-git rtty https://github.com/zhaojh329/rtty-feed.git;for-15.05

Install rtty packages:

    ./scripts/feeds update libuwsc rtty
    ./scripts/feeds install -a -p rtty

Select package rtty in menuconfig and compile new image.

    Utilities  --->
        Terminal  --->
            < > rtty-mbedtls............................ A reverse proxy WebTTY (mbedtls)
            <*> rtty-nossl............................... A reverse proxy WebTTY (NO SSL)
            < > rtty-openssl............................ A reverse proxy WebTTY (openssl)
            < > rtty-wolfssl............................ A reverse proxy WebTTY (wolfssl)

Configuring the server parameter

    uci set rtty.@server[0].host='your server host'
    uci set rtty.@server[0].port='your server port'

You can customize an ID for your device. If the ID is not configured, RTTY will use
the MAC address of the specified network interface as the ID.
The format of the MAC address as the ID is: 1A2A3A4A5A6A

    uci set rtty.@device[0].id='your-device-id'

You can add a description to your device

    uci set rtty.@device[0].description='My device'

Save configuration and apply

    uci commit
    /etc/init.d/rtty restart

# Usage
Query online devices: http://your-server-host:5912/list

Use your web browser to access your server: http://your-server-host:5912,
then enter the id of the terminal you want to access, and then click the connection button

You can easily embed RTTY into your existing platform: http://your-server-host:5912?id=your-id

Automatic login: http://your-server:5912/?id=device-id&username=device-username&password=device-password

# Contributing
If you would like to help making [rtty](https://github.com/zhaojh329/rtty) better,
see the [CONTRIBUTING.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING.md) file.
