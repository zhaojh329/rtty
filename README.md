# rtty([中文](/README_ZH.md))

[1]: https://img.shields.io/badge/license-LGPL2-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/rtty/pulls
[5]: https://img.shields.io/badge/Issues-welcome-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/rtty/issues/new
[7]: https://img.shields.io/badge/release-6.1.0-blue.svg?style=plastic
[8]: https://github.com/zhaojh329/rtty/releases
[9]: https://travis-ci.org/zhaojh329/rtty.svg?branch=master
[10]: https://travis-ci.org/zhaojh329/rtty

[![license][1]][2]
[![PRs Welcome][3]][4]
[![Issue Welcome][5]][6]
[![Release Version][7]][8]
[![Build Status][9]][10]

[Xterm.js]: https://github.com/xtermjs/xterm.js
[libev]: http://software.schmorp.de/pkg/libev.html
[libuwsc]: https://github.com/zhaojh329/libuwsc
[openssl]: https://github.com/openssl/openssl
[mbedtls(polarssl)]: https://github.com/ARMmbed/mbedtls
[CyaSSl(wolfssl)]: https://github.com/wolfSSL/wolfssl
[vue]: https://github.com/vuejs/vue
[iview]: https://github.com/iview/iview
[protobuf-c]: https://github.com/protobuf-c/protobuf-c

Access your terminal behind a NAT or firewall over the web based on your terminal's macaddr.

It is composed of the client and the [server](https://github.com/zhaojh329/rttys). The server is written in go
language and uses the [vue]+[iview]. You can access any of your terminals through a web browser based on the
device ID you set(If the ID is not set, the MAC address of the device is used).

rtty is very suitable for remote maintenance your or your company's thousands of Linux devices deployed around
the world.

**Keep Watching for More Actions on This Space**

**For your security, it is strongly recommended that you use SSL**

# Features
* Simple to deployment and easy to use
* Reverse Proxy
* Connect your device according to the ID you set up
* Fully-featured terminal based on [Xterm.js]
* SSL support: openssl, mbedtls, CyaSSl(wolfssl)
* Support upload file to device
* Support download file from devices
* Support Execute a command remote
* Cross platform: Linux, OpenWrt/LEDE

![](/rtty.svg)
![](/rtty.f30806d.gif)

# Dependencies for Client side
* [libev] - A full-featured and high-performance event loop
* [libuwsc] - A Lightweight and fully asynchronous WebSocket client library based on libev
* [protobuf-c]: - Protocol Buffers implementation in C
* [mbedtls(polarssl)] - If you choose mbedtls as your SSL backend
* [CyaSSl(wolfssl)] - If you choose wolfssl as your SSL backend
* [openssl] - If you choose openssl as your SSL backend

# [Deploying the server side](https://github.com/zhaojh329/rttys)

# How to install and run the Client - rtty
## For Linux distribution: Ubuntu, Debian, ArchLinux, Centos
Install

    wget -qO- https://raw.githubusercontent.com/zhaojh329/rtty/master/tools/install.sh | sudo bash

Run RTTY(Replace the following parameters with your own parameters)

    sudo rtty -I 'My-device-ID' -h 'your-server' -p 5912 -a -v -s -d 'My Device Description'

## For Embedded Linux Platform
You need to cross compiling by yourself

## [For OpenWRT](/OPENWRT.md)

# Usage
Use your web browser to access your server: `https://your-server-host:5912`, then click the connection button

You can easily embed RTTY into your existing platform: `https://your-server-host:5912/#/?id=your-id`

Automatic login: `https://your-server:5912/#/?id=device-id&username=device-username&password=device-password`

## Other functions
Please click the right mouse button

## Execute a command remote
### Shell

    curl -k https://your-server:5912/cmd -d '{"devid":"test","username":"test","password":"123456","cmd":"ls","params":["/"],"env":{}}'

    {"Err":0,"msg":"","code":0,"stdout":"bin\ndev\netc\nlib\nmnt\noverlay\nproc\nrom\nroot\nsbin\nsys\ntmp\nusr\nvar\nwww\n","stderr":""}

### Jquery

    var data = {devid: 'test', username: 'test', password: '123456', cmd: 'ls', params: ['/'], env: {}};
    $.post('https://your-server:5912/cmd', JSON.stringify(data), function(r) {console.log(r)});


### Axios

    var data = {devid: 'test', username: 'test', password: '123456', cmd: 'ls', params: ['/'], env: {}};
    axios.post('https://your-server:5912/cmd', JSON.stringify(data)).then(function (response) {
        console.log(response.data);
    }).catch(function (error) {
        console.log(error);
    });

# [Donate](https://gitee.com/zhaojh329/rtty#project-donate-overview)

# Contributing
If you would like to help making [rtty](https://github.com/zhaojh329/rtty) better,
see the [CONTRIBUTING.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING.md) file.

# QQ group: 153530783

# If the project is helpful to you, please do not hesitate to star. Thank you!
