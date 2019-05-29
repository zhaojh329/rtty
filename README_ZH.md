# rtty

[1]: https://img.shields.io/badge/license-MIT-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/rtty/pulls
[5]: https://img.shields.io/badge/Issues-welcome-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/rtty/issues/new
[7]: https://img.shields.io/badge/release-6.6.1-blue.svg?style=plastic
[8]: https://github.com/zhaojh329/rtty/releases
[9]: https://travis-ci.org/zhaojh329/rtty.svg?branch=master
[10]: https://travis-ci.org/zhaojh329/rtty

[![license][1]][2]
[![PRs Welcome][3]][4]
[![Issue Welcome][5]][6]
[![Release Version][7]][8]
[![Build Status][9]][10]

[Xterm.js]: https://github.com/xtermjs/xterm.js
[lrzsz]: https://ohse.de/uwe/software/lrzsz.html
[libev]: http://software.schmorp.de/pkg/libev.html
[libuwsc]: https://github.com/zhaojh329/libuwsc
[openssl]: https://github.com/openssl/openssl
[mbedtls(polarssl)]: https://github.com/ARMmbed/mbedtls
[CyaSSl(wolfssl)]: https://github.com/wolfSSL/wolfssl
[vue]: https://github.com/vuejs/vue
[iview]: https://github.com/iview/iview
[服务端]: https://github.com/zhaojh329/rttys

![](https://raw.githubusercontent.com/zhaojh329/rtty/doc/rtty.png)
![](https://raw.githubusercontent.com/zhaojh329/rtty/doc/screen.gif)
![](https://raw.githubusercontent.com/zhaojh329/rtty/doc/file.gif)

它由客户端和[服务端]组成。客户端采用纯C实现。[服务端]采用GO语言实现，前端界面采用[iview]和[vue]实现。

你可以在任何地方通过Web访问你的设备的终端。通过设备ID（如果不设置则使用设备的MAC地址）来区分你的不同的设备。

rtty非常适合远程维护你的或者你公司的部署在全球各地的成千上万的Linux设备。

# 特性
* 部署简单，使用方便
* 根据设备ID访问不同的设备
* 提供dashboard，直观的展示在线设备
* 基于[Xterm.js]的全功能终端
* 支持传输文件
* 支持SSL: openssl, mbedtls, CyaSSl(wolfssl)
* 支持设备认证
* 支持远程执行命令
* 客户端非常小，适合嵌入式Linux: rtty(20.1K) + libev(48.5K) + libuwsc(24.4K) = 93K. 如果你希望支持SSL，+libwolfssl(595.9K) = 688.9K

# 客户端依赖
* [libev] - 高性能的事件循环库
* [libuwsc] - 一个轻量的针对嵌入式Linux的基于libev的WebSocket客户端C库。
* [mbedtls(polarssl)]、[CyaSSl(wolfssl)]或者[openssl] - 如果你需要支持SSL

# [部署服务端](https://github.com/zhaojh329/rttys/blob/master/README_ZH.md)

# 如何安装和运行rtty客户端
## 针对Linux发行版：Ubuntu, Debian, ArchLinux, Centos
安装

    wget -qO- https://raw.githubusercontent.com/zhaojh329/rtty/master/tools/install.sh | sudo bash

查看命令行选项

    Usage: rtty [option]
      -i ifname    # Network interface name - Using the MAC address of
                          the interface as the device ID
      -I id        # Set an ID for the device(Maximum 63 bytes, valid character:letters
                          and numbers and underlines and short lines) - If set,
                          it will cover the MAC address(if you have specify the ifname)
      -h host      # Server host
      -p port      # Server port(Default is 5912)
      -a           # Auto reconnect to the server
      -v           # verbose
      -d           # Adding a description to the device(Maximum 126 bytes)
      -s           # SSL on
      -k keepalive # keep alive in seconds for this client. Defaults to 5
      -V           # Show version
      -D           # Run in the background
      -t token     # Authorization token

运行RTTY(将下面的参数替换为你自己的参数)

    sudo rtty -I 'My-device-ID' -h 'your-server' -p 5912 -a -v -s -d 'My Device Description'

如果你的rttys配置了一个token，请加上如下参数（将下面的token替换为你自己生成的）

    -t 34762d07637276694b938d23f10d7164

## [如何在OpenWRT中使用](/OPENWRT_ZH.md)

## [其它嵌入式Linux平台](/CROSS_COMPILE.md)

# 如何使用
使用你的Web浏览器访问你的服务器: `https://your-server-host:5912`，然后点击连接按钮。

你可以非常方便的将RTTY嵌入到你现有的平台： `https://your-server-host:5912/#/?id=your-id`

自动登录: `https://your-server:5912/#/?id=device-id&username=device-username&password=device-password`

## 传输文件
从本地传输文件到远程设备

    rtty -R

从远程设备传输文件到本地

    rtty -S test.txt

## [远程执行命令](/COMMAND_ZH.md)

# [捐赠](https://gitee.com/zhaojh329/rtty#project-donate-overview)

* 许玉善(Tim Xu) - 100¥
* シ乄BB~★ - 100¥
* just_doing - 20¥

# 贡献代码
如果你想帮助[rtty](https://github.com/zhaojh329/rtty)变得更好，请参考
[CONTRIBUTING_ZH.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING_ZH.md)。

# 技术交流
QQ群：153530783

