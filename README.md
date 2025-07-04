# rtty([中文](/README_ZH.md))

**This project is officially supported by [GL.iNet](https://github.com/gl-inet).**

[1]: https://img.shields.io/badge/license-MIT-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/rtty/pulls
[5]: https://img.shields.io/badge/Issues-welcome-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/rtty/issues/new
[7]: https://img.shields.io/badge/release-9.0.0-blue.svg?style=plastic
[8]: https://github.com/zhaojh329/rtty/releases
[9]: https://github.com/zhaojh329/rtty/workflows/build/badge.svg
[10]: https://raw.githubusercontent.com/CodePhiliaX/resource-trusteeship/main/readmex.svg
[11]: https://readmex.com/zhaojh329/rtty
[12]: https://deepwiki.com/badge.svg
[13]: https://deepwiki.com/zhaojh329/rtty

[![license][1]][2]
[![PRs Welcome][3]][4]
[![Issue Welcome][5]][6]
[![Release Version][7]][8]
![Build Status][9]
[![ReadmeX][10]][11]
[![Ask DeepWiki][12]][13]
![visitors](https://visitor-badge.laobi.icu/badge?page_id=zhaojh329.rtty)

[Xterm.js]: https://github.com/xtermjs/xterm.js
[libev]: http://software.schmorp.de/pkg/libev.html
[openssl]: https://github.com/openssl/openssl
[mbedtls(polarssl)]: https://github.com/ARMmbed/mbedtls
[CyaSSl(wolfssl)]: https://github.com/wolfSSL/wolfssl
[vue]: https://github.com/vuejs/vue
[server]: https://github.com/zhaojh329/rttys

```mermaid
flowchart TB
s[rttys with public IP address]
u1["User(Web Browser)"] --> s
u2["User(Web Browser)"] --> s
u3["User(Web Browser)"] --> s
s --> c1["rtty(Linux Device)"]
s --> c2["rtty(Linux Device)"]
s --> c3["rtty(Linux Device)"]
```

![](/img/terminal.gif)
![](/img/file.gif)
![](/img/web.gif)

It is composed of a client and a [server]. The client is written in pure C. The [server] is written in go language
and the front-end is written in [Vue].

You can access your device's terminal from anywhere via the web. Distinguish your different device by device ID.

rtty is very suitable for remote maintenance your or your company's thousands of Linux devices deployed around
the world.

## Features
* The client is writen in C language, very small, suitable for embedded Linux
  - No SSL: rtty(32K) + libev(56K)
  - Support SSL: + libmbedtls(88K) + libmbedcrypto(241K) + libmbedx509(48k)
* Execute command remotely in a batch of devices 
* SSL support: openssl, mbedtls, CyaSSl(wolfssl)
* mTLS
* Very convenient to upload and download files
* Access different devices based on device ID
* Support HTTP Proxy - Access your device's Web
* Fully-featured terminal based on [Xterm.js]
* Simple to deployment and easy to use

## Who's using rtty
- [GL.iNet](https://www.gl-inet.com/)
- [Yunlianxin Technology](http://www.iyunlink.com/)
- [One IOT World](https://www.oneiotworld.com/)
- [bitswrt Communication Technology](http://bitswrt.com/)
- [Guangzhou Lingpai Technology](https://linkpi.cn/)
- ...

## Dependencies of the Client side
* [libev] - A full-featured and high-performance event loop
* [mbedtls(polarssl)] or [CyaSSl(wolfssl)] or [openssl] - If you want to support SSL

## Star History
[![Star History Chart](https://api.star-history.com/svg?repos=zhaojh329/rtty&type=Date)](https://www.star-history.com/#zhaojh329/rtty&Date)

## Contributing
If you would like to help making [rtty](https://github.com/zhaojh329/rtty) better,
see the [CONTRIBUTING.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING.md) file.
