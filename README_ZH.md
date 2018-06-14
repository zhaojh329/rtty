# rtty

[1]: https://img.shields.io/badge/license-LGPL2-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/rtty/pulls
[5]: https://img.shields.io/badge/Issues-welcome-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/rtty/issues/new
[7]: https://img.shields.io/badge/release-5.0.2-blue.svg?style=plastic
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
[protobuf-c]: https://github.com/protobuf-c/protobuf-c

根据您的终端的macaddr，通过Web访问您的处在NAT或防火墙里面的终端。

它由客户端和[服务端](https://github.com/zhaojh329/rttys)组成。服务端采用GO语言实现，
以及使用了[vue]+[iview]。你可以基于你设置的设备ID（不设置则为设备的MAC地址）通过Web浏览器访问你的任意一台终端。

rtty非常适合远程维护你的或者你公司的部署在全球各地的成千上万的Linux设备。

**请保持关注以获取最新的项目动态**

**为了您的安全，强烈建议您使用SSL**

# 特性
* 部署简单，使用方便
* 反向代理
* 根据你设置的ID连接你的设备
* 基于[Xterm.js]的全功能终端
* 支持SSL: openssl, mbedtls, CyaSSl(wolfssl)
* 支持上传文件到设备
* 支持从设备下载文件
* 支持远程执行命令
* 跨平台: Linux, OpenWrt/LEDE

![](/rtty.svg)
![](/rtty.gif)
![](/upfile.gif)
![](/downfile.gif)

# 客户端依赖
* [libubox] - 用于OpenWrt的C工具函数库，但也可以用于其他Linux系统中.[参考](https://wiki.openwrt.org/doc/techref/libubox)
* [libuwsc] - 一个轻量的针对嵌入式Linux的基于libubox的WebSocket客户端C库。
* [protobuf-c]: - Protocol Buffers的C语言实现
* [ustream-ssl] - 如果你需要支持SSL
* [mbedtls] - 如果你选择mbedtls作为你的SSL后端
* [CyaSSl(wolfssl)] - 如果你选择wolfssl作为你的SSL后端
* [openssl] - 如果你选择openssl作为你的SSL后端

# [部署服务端](https://github.com/zhaojh329/rttys/blob/master/README_ZH.md)

# 如何安装和运行rtty客户端
## 针对Linux发行版：Ubuntu, Debian, ArchLinux, Centos
安装

    wget -qO- https://raw.githubusercontent.com/zhaojh329/rtty/master/tools/install.sh | sudo bash

运行RTTY(将下面的参数替换为你自己的参数)

    sudo rtty -I 'My-device-ID' -h 'your-server' -p 5912 -a -v -s -d 'My Device Description'

## 嵌入式Linux平台
你需要自行交叉编译

## [如何在OpenWRT中使用](/OPENWRT_ZH.md)

# 如何使用
使用你的Web浏览器访问你的服务器: `https://your-server-host:5912`，然后点击连接按钮。

你可以非常方便的将RTTY嵌入到你现有的平台： `https://your-server-host:5912/#/?id=your-id`

自动登录: `https://your-server:5912/#/?id=device-id&username=device-username&password=device-password`

## 其他功能
请点击鼠标右键

## 远程执行命令
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

# 贡献代码
如果你想帮助[rtty](https://github.com/zhaojh329/rtty)变得更好，请参考
[CONTRIBUTING_ZH.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING_ZH.md)。

# 技术交流
QQ群：153530783

# 如果该项目对您有帮助，请随手star，谢谢！
