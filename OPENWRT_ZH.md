# 直接在设备里面安装

    opkg update
    opkg list | grep rtty
    opkg install rtty-nossl

# 自己编译
## Openwrt master
更新feeds:

    ./scripts/feeds update -a
    ./scripts/feeds install -a

在menuconfig中选择rtty，然后重新编译固件。

    Utilities  --->
        Terminal  --->
            <*> rtty-mbedtls............................ A reverse proxy WebTTY (mbedtls)
            < > rtty-nossl............................... A reverse proxy WebTTY (NO SSL)
            < > rtty-openssl............................ A reverse proxy WebTTY (openssl)
            < > rtty-wolfssl............................ A reverse proxy WebTTY (wolfssl)

## [Openwrt 14.04](https://github.com/zhaojh329/rtty/blob/openwrt-14.04/README.md)

## [Openwrt 15.05](https://github.com/zhaojh329/rtty/blob/openwrt-15.05/README.md)

## [Lede](https://github.com/zhaojh329/rtty/blob/openwrt-lede/README.md)

# 配置
配置服务器参数

    uci add rtty rtty   # 如果是第一次配置
    uci set rtty.@rtty[0].host='your server host'
    uci set rtty.@rtty[0].port='your server port'

你可以给你的设备自定义一个ID。如果未指定，RTTY将使用指定的网络接口的MAC地址作为其ID

	uci set rtty.@rtty[0].id='your-device-id'

给你的设备添加一个描述

    uci set rtty.@rtty[0].description='My device'

使用SSL

    uci set rtty.@rtty[0].ssl='1'

保存配置并应用

    uci commit
    /etc/init.d/rtty restart