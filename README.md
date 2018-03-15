# rtty([中文](/README_ZH.md))

[1]: https://img.shields.io/badge/license-LGPL2-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/rtty/pulls
[5]: https://img.shields.io/badge/Issues-welcome-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/rtty/issues/new
[7]: https://img.shields.io/badge/release-4.1.1-blue.svg?style=plastic
[8]: https://github.com/zhaojh329/rtty/releases
[9]: https://travis-ci.org/zhaojh329/rtty.svg?branch=master
[10]: https://travis-ci.org/zhaojh329/rtty

[![license][1]][2]
[![PRs Welcome][3]][4]
[![Issue Welcome][5]][6]
[![Release Version][7]][8]
[![Build Status][9]][10]

[Xterm.js]: https://github.com/xtermjs/xterm.js
[libubox]: https://git.openwrt.org/?p=project/libubox.git
[libuwsc]: https://github.com/zhaojh329/libuwsc
[ustream-ssl]: https://git.openwrt.org/?p=project/ustream-ssl.git
[openssl]: https://github.com/openssl/openssl
[mbedtls]: https://github.com/ARMmbed/mbedtls
[CyaSSl(wolfssl)]: https://github.com/wolfSSL/wolfssl
[vue]: https://github.com/vuejs/vue
[iview]: https://github.com/iview/iview

A reverse proxy WebTTY. It is composed of the client and the
[server](https://github.com/zhaojh329/rttys). The server is written in go language and uses the [vue]+[iview].
You can access any of your terminals through a web browser based on the device ID you set(If the ID is not set,
the MAC address of the device is used).

rtty is very suitable for remote maintenance your or your company's thousands of Linux devices deployed around the world.

**Keep Watching for More Actions on This Space**

**For your security, it is strongly recommended that you use SSL**

![](/rtty.svg)
![](/rtty.gif)
![](/upfile.gif)
![](/downfile.gif)

# Features
* Simple to deployment and easy to use
* Reverse Proxy
* Connect your device according to the ID you set up
* Fully-featured terminal based on [Xterm.js]
* SSL support: openssl, mbedtls, CyaSSl(wolfssl)
* Support upload file to device
* Support download file from devices
* Support Execute a command remote
* Cross platform: macOS, Linux, FreeBSD/OpenBSD, OpenWrt/LEDE

# Dependencies for Client side
* [libubox] - C utility functions for OpenWrt, but can also be used for the same purposes in other Linux systems.
[Reference](https://wiki.openwrt.org/doc/techref/libubox)
* [libuwsc] - A Lightweight and fully asynchronous WebSocket client C library based on libubox for Embedded Linux.
* [ustream-ssl] - If you need to support SSL
* [mbedtls] - If you choose mbedtls as your SSL backend
* [CyaSSl(wolfssl)] - If you choose wolfssl as your SSL backend
* [openssl] - If you choose openssl as your SSL backend

# Deploying the server side
# Install

    curl https://raw.githubusercontent.com/zhaojh329/rttys/master/install.sh | sudo sh

# Manual run

    rttys -cert /etc/rttys/rttys.crt -key /etc/rttys/rttys.key


# Run in background

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

    sudo rtty -I 'My-device-ID' -h 'your-server' -p 5912 -a -v -s -d 'My Device Description'

Query online devices

    curl -k https://your-server:5912/devs
    [{"id":"My-device-ID","description":"My device"}]

## For Embedded Linux Platform
You need to cross compiling by yourself

## For OpenWRT
Install

    opkg update
    opkg list | grep rtty
    opkg install rtty-nossl

If the install command fails, you can [compile it yourself](/BUILDOPENWRT.md).

Configuring the server parameter

    uci add rtty rtty   # If it's the first configuration
    uci set rtty.@rtty[0].host='your server host'
    uci set rtty.@rtty[0].port='your server port'

You can customize an ID for your device. If the ID is not configured, RTTY will use
the MAC address of the specified network interface as the ID.
The format of the MAC address as the ID is: 1A2A3A4A5A6A

    uci set rtty.@rtty[0].id='your-device-id'

You can add a description to your device

    uci set rtty.@rtty[0].description='My device'

Use SSL

    uci set rtty.@rtty[0].ssl='1'

Save configuration and apply

    uci commit
    /etc/init.d/rtty restart

# Usage
Use your web browser to access your server: `https://your-server-host:5912`, then click the connection button

You can easily embed RTTY into your existing platform: `https://your-server-host:5912?id=your-id`

Automatic login: `https://your-server:5912/?id=device-id&username=device-username&password=device-password`

## Upload file and download file
Open the context menu with the shortcut key: Ctrl+Shift+f

## Execute a command remote
`curl -k https://your-server:5912/cmd -d '{"devid":"test","username":"test","password":"123456","cmd":"ls","params":["/"],"env":[]}'`

# Contributing
If you would like to help making [rtty](https://github.com/zhaojh329/rtty) better,
see the [CONTRIBUTING.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING.md) file.
