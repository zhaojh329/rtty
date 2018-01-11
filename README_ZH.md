# xTTYD

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

[ttyd]: https://github.com/tsl0922/ttyd
[libubox]: https://git.openwrt.org/?p=project/libubox.git
[libuwsc]: https://github.com/zhaojh329/libuwsc

通过Web分享你的终端。与[ttyd]不同的是，xTTYD由服务端和客户端两部分组成。
xTTYD可以根据你指定的MAC地址分享你的设备的Shell。

`请保持关注以获取最新的项目动态`

![](https://github.com/zhaojh329/xttyd/blob/master/xttyd.png)

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
    src-git xttyd https://github.com/zhaojh329/xttyd-feed.git

Install xttyd packages:

    ./scripts/feeds update libuwsc xttyd
    ./scripts/feeds install -a -p xttyd

Select package xttyd in menuconfig and compile new image.

    Utilities  --->
        Terminal  --->
            <*> xttyd....................................... xTTYD

# 部署服务端

	sudo apt install python3 python3-pip
	sudo pip3 install aiohttp uvloop
	git clone https://github.com/zhaojh329/xttyd.git
	cd xttyd/server && ./rttyd.py

# 贡献代码
如果你想帮助[xttyd](https://github.com/zhaojh329/xttyd)变得更好，请参考
[CONTRIBUTING_ZH.md](https://github.com/zhaojh329/xttyd/blob/master/CONTRIBUTING_ZH.md)。

# 技术交流
QQ群：153530783
