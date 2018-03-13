# rtty

[1]: https://img.shields.io/badge/license-LGPL2-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/rtty/pulls
[5]: https://img.shields.io/badge/Issues-welcome-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/rtty/issues/new
[7]: https://img.shields.io/badge/release-4.1.0-blue.svg?style=plastic
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

一个反向代理WebTTY。它由客户端和[服务端](https://github.com/zhaojh329/rttys)组成。服务端采用GO语言实现，
以及使用了[vue]+[iview]。你可以基于你设置的设备ID（不设置则为设备的MAC地址）通过Web浏览器访问你的任意一台终端。

rtty非常适合远程维护你的或者你公司的部署在全球各地的成千上万的Linux设备。

`请保持关注以获取最新的项目动态`

![](/rtty.svg)
![](/rtty.gif)
![](/upfile.gif)
![](/downfile.gif)

# 特性
* 部署简单，使用方便
* 反向代理
* 根据你设置的ID连接你的设备
* 基于[Xterm.js]的全功能终端
* 支持SSL: openssl, mbedtls, CyaSSl(wolfssl)
* 支持上传文件到设备
* 支持从设备下载文件
* 跨平台: macOS, Linux, FreeBSD/OpenBSD, OpenWrt/LEDE

# 客户端依赖
* [libubox] - 用于OpenWrt的C工具函数库，但也可以用于其他Linux系统中.[参考](https://wiki.openwrt.org/doc/techref/libubox)
* [libuwsc] - 一个轻量的针对嵌入式Linux的基于libubox的WebSocket客户端C库。
* [ustream-ssl] - 如果你需要支持SSL
* [mbedtls] - 如果你选择mbedtls作为你的SSL后端
* [CyaSSl(wolfssl)] - 如果你选择wolfssl作为你的SSL后端
* [openssl] - 如果你选择openssl作为你的SSL后端

# 部署服务端
安装GO语言环境（如果您还未安装）

    sudo apt-get install golang     # For ubuntu

    sudo yum install golang         # For Centos

设置环境变量GOPATH（如果您未设置）(从Go 1.8开始, 默认为$HOME/go)

    export GOPATH=$HOME/go

安装rtty server

    go get -u github.com/zhaojh329/rttys

手动运行

    $GOPATH/bin/rttys -port 5912

安装自启动脚本，后台运行(debian/ubuntu)

    cd $GOPATH/src/github.com/zhaojh329/rttys
    sudo ./install.sh
    sudo /etc/init.d/rttys start

# 如何编译和安装 rtty客户端
## 针对Linux发行版, 例如Ubuntu和Centos
安装编译工具

    sudo apt install gcc cmake git      # For Ubuntu

    yum install gcc cmake git           # For Centos

编译和安装依赖软件包

    sudo apt install libjson-c-dev      # For Ubuntu

    sudo apt install json-c-devel       # For Centos

    git clone https://git.openwrt.org/project/libubox.git
    cd libubox && cmake -DBUILD_LUA=OFF . && sudo make install

    git clone https://github.com/zhaojh329/libuwsc.git
    cd libuwsc && cmake -DUWSC_SSL_SUPPORT=OFF . && sudo make install

编译和安装RTTY
    
    git clone https://github.com/zhaojh329/rtty.git
    cd rtty && cmake . && sudo make install

运行RTTY
将下面的参数替换为你自己的参数

    sudo rtty -I 'My-device-ID' -h 'your-server' -p 5912 -a -v -d 'My Device Description'

查询在线设备列表

    curl http://your-server:5912/devs
    [{"id":"My-device-ID","description":"My device"}]

## 嵌入式Linux平台
你需要自行交叉编译

## 如何在OpenWRT中使用
安装

    opkg update
    opkg list | grep rtty
    opkg install rtty-nossl

如果安装失败，你可以[自己编译](/BUILDOPENWRT_ZH.md)。

配置服务器参数

    uci add rtty rtty   # If it's the first configuration
    uci set rtty.@rtty[0].host='your server host'
    uci set rtty.@rtty[0].port='your server port'

你可以给你的设备自定义一个ID。如果未指定，RTTY将使用指定的网络接口的MAC地址作为其ID，
以MAC地址作为ID的格式为：1A2A3A4A5A6A

    uci set rtty.@rtty[0].id='your-device-id'

保存配置并应用

    uci commit
    /etc/init.d/rtty restart

# 如何使用
使用你的Web浏览器访问你的服务器: `http://your-server-host:5912`，然后点击连接按钮。

你可以非常方便的将RTTY嵌入到你现有的平台： `http://your-server-host:5912?id=your-id`

自动登录: `http://your-server:5912/?id=device-id&username=device-username&password=device-password`

## 上传文件和下载文件
使用快捷键打开菜单: Ctrl+Shift+f

## 远程执行命令
`curl http://your-server:5912/cmd -d '{"devid":"test","cmd":"ls","username":"test","password":"123456"}'`

# 贡献代码
如果你想帮助[rtty](https://github.com/zhaojh329/rtty)变得更好，请参考
[CONTRIBUTING_ZH.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING_ZH.md)。

# 技术交流
QQ群：153530783
