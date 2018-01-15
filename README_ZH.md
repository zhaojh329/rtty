# rtty

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

[ttyd]: https://github.com/tsl0922/ttyd
[libubox]: https://git.openwrt.org/?p=project/libubox.git
[libuwsc]: https://github.com/zhaojh329/libuwsc

通过Web浏览器访问你的终端。项目名称里面的“r”是指“反向代理”或者“远程”。它由客户端和服务端组成。
你可以根据你设置的设备ID通过Web浏览器访问你的任意一台终端。

rtty非常适合远程维护你的或者你公司的部署在全球各地的成千上万的Linux设备。

`请保持关注以获取最新的项目动态`

![](/rtty.svg)

![](/rtty.gif)

# 服务端依赖
* [python3](https://www.python.org)
* [uvloop](https://github.com/MagicStack/uvloop)
* [aiohttp](https://github.com/aio-libs/aiohttp)

# 客户端依赖
* [libubox]
* [libuwsc]

# 如何在OpenWRT中使用
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
            <*> rtty................................... Share your terminal over the web

配置服务器参数

    uci set rtty.@server[0].host='your server host'
    uci set rtty.@server[0].port='your server port'

你可以给你的设备自定义一个ID。如果未指定，RTTY将使用指定的网络接口的MAC地址作为其ID，
以MAC地址作为ID的格式为：1A2A3A4A5A6A

    uci set rtty.@device[0].id='your-device-id'

保存配置并应用

    uci commit
    /etc/init.d/rtty restart

# 部署服务端
安装依赖

    sudo apt install python3 python3-pip
    sudo pip3 install aiohttp uvloop

克隆代码

    git clone https://github.com/zhaojh329/rtty.git

手动运行

    cd rtty/server && ./rtty.py -p 5912

安装自启动脚本，后台运行

    sudo ./install.sh
    sudo /etc/init.d/rtty start

# 如何使用
查询在线设备: http://your-server-host:5912/list

使用你的Web浏览器访问你的服务器，然后输入你要访问的终端的ID，然后点击连接按钮。

http://your-server-host:5912

# 贡献代码
如果你想帮助[rtty](https://github.com/zhaojh329/rtty)变得更好，请参考
[CONTRIBUTING_ZH.md](https://github.com/zhaojh329/rtty/blob/master/CONTRIBUTING_ZH.md)。

# 技术交流
QQ群：153530783
