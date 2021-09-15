# rtty - 在任何地方通过Web访问您的设备的终端

[1]: https://img.shields.io/badge/开源协议-MIT-brightgreen.svg?style=plastic
[2]: /LICENSE
[3]: https://img.shields.io/badge/提交代码-欢迎-brightgreen.svg?style=plastic
[4]: https://github.com/zhaojh329/rtty/pulls
[5]: https://img.shields.io/badge/提问-欢迎-brightgreen.svg?style=plastic
[6]: https://github.com/zhaojh329/rtty/issues/new
[7]: https://img.shields.io/badge/发布版本-7.5.0-blue.svg?style=plastic
[8]: https://github.com/zhaojh329/rtty/releases
[9]: https://github.com/zhaojh329/rtty/workflows/build/badge.svg
[10]: https://img.shields.io/badge/支持rtty-赞助作者-blueviolet.svg
[11]: #贡献代码
[12]: https://img.shields.io/badge/技术交流群-点击加入：153530783-brightgreen.svg
[13]: https://jq.qq.com/?_wv=1027&k=5PKxbTV


[![license][1]][2]
[![PRs Welcome][3]][4]
[![Issue Welcome][5]][6]
[![Release Version][7]][8]
![Build Status][9]
![visitors](https://visitor-badge.laobi.icu/badge?page_id=zhaojh329.rtty)
[![Support rtty][10]][11]
[![Chinese Chat][12]][13]

[Xterm.js]: https://github.com/xtermjs/xterm.js
[libev]: http://software.schmorp.de/pkg/libev.html
[openssl]: https://github.com/openssl/openssl
[mbedtls(polarssl)]: https://github.com/ARMmbed/mbedtls
[CyaSSl(wolfssl)]: https://github.com/wolfSSL/wolfssl
[vue]: https://github.com/vuejs/vue
[服务端]: https://github.com/zhaojh329/rttys

![](https://raw.githubusercontent.com/zhaojh329/rtty/doc/diagram.png)
![](https://raw.githubusercontent.com/zhaojh329/rtty/doc/terminal.gif)
![](https://raw.githubusercontent.com/zhaojh329/rtty/doc/file.gif)
![](https://raw.githubusercontent.com/zhaojh329/rtty/doc/web.gif)

它由客户端和[服务端]组成。客户端采用纯C实现。[服务端]采用GO语言实现，前端界面采用[vue]实现。

您可以在任何地方通过Web访问您的设备的终端。通过设备ID来区分您的不同的设备。

rtty非常适合远程维护您的或者您的公司的部署在全球各地的成千上万的Linux设备。

# 特性
* 客户端 C 语言实现，非常小，适合嵌入式 Linux
  - 不支持 SSL: rtty(32K) + libev(56K)
  - 支持 SSL: + libmbedtls(88K) + libmbedcrypto(241K) + libmbedx509(48k)
* 远程批量执行命令
* 支持SSL: openssl, mbedtls, CyaSSl(wolfssl)
* SSL 双向认证(mTLS)
* 非常方便的上传和下载文件
* 根据设备ID访问不同的设备
* 支持 HTTP 代理 - 访问您的设备的 Web
* 基于[Xterm.js]的全功能终端
* 部署简单，使用方便

# 客户端依赖
* [libev] - 高性能的事件循环库
* [mbedtls(polarssl)]、[CyaSSl(wolfssl)]或者[openssl] - 如果您需要支持SSL

# [部署服务端](https://github.com/zhaojh329/rttys/blob/master/README_ZH.md)

# 如何安装rtty
## 针对Linux发行版
### 安装依赖

    sudo apt install -y libev-dev libssl-dev      # Ubuntu, Debian
    sudo pacman -S --noconfirm libev openssl      # ArchLinux
    sudo yum install -y libev-devel openssl-devel # Centos

### 克隆rtty代码

    git clone --recursive https://github.com/zhaojh329/rtty.git

### 编译

    cd rtty && mkdir build && cd build
    cmake .. && make install

## 如何在Buildroot中使用
在menuconfig中选中rtty然后编译

    Target packages  --->
        Shell and utilities  --->
            [*] rtty

## [如何在OpenWRT中使用](/OPENWRT_ZH.md)

## [其它嵌入式Linux平台](/CROSS_COMPILE.md)

# 命令行选项

    Usage: rtty [option]
        -I, --id=string          Set an ID for the device(Maximum 63 bytes, valid
                                 character:letter, number, underline and short line)
        -h, --host=string        Server's host or ipaddr(Default is localhost)
        -p, --port=number        Server port(Default is 5912)
        -d, --description=string Add a description to the device(Maximum 126 bytes)
        -a                       Auto reconnect to the server
        -s                       SSL on
        -C, --cacert             CA certificate to verify peer against"
        -c, --cert               Certificate file to use"
        -k, --key                Private key file to use"
        -D                       Run in the background
        -t, --token=string       Authorization token
        -f username              Skip a second login authentication. See man login(1) about the details
        -R                       Receive file
        -S file                  Send file
        -v, --verbose            verbose
        -V, --version            Show version
        --help                   Show usage

# 运行rtty
将下面的参数替换为您自己的参数

    sudo rtty -I 'My-device-ID' -h 'your-server' -p 5912 -a -v -d 'My Device Description'

如果您的 [rttys](https://gitee.com/zhaojh329/rttys) 配置了一个 token，请加上如下参数（将下面的 token 替换为您自己生成的）

    -t 34762d07637276694b938d23f10d7164

# 如何使用
使用您的 Web 浏览器访问您的服务器: `http://your-server-host:5913`，然后点击连接按钮。

## 直接连接设备，无需 Web 登录(需要在服务端配置设备白名单)
http://your-server-host:5913/connect/devid1

http://your-server-host:5913/connect/devid2

## 传输文件
从本地传输文件到远程设备

    rtty -R

从远程设备传输文件到本地

    rtty -S test.txt

## [远程执行命令](/COMMAND_ZH.md)

# 贡献代码
如果您想帮助[rtty](https://github.com/zhaojh329/rtty)变得更好，请参考
[CONTRIBUTING_ZH.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING_ZH.md)。

# 强烈推荐佐大的 OpenWrt 培训班
想学习 OpenWrt 开发，但是摸不着门道？自学没毅力？基础太差？怕太难学不会？快来参加<跟着佐大学 OpenWrt 开发入门培训班> 佐大助你能学有所成，培训班报名地址：http://forgotfun.org/2018/04/openwrt-training-2018.html
