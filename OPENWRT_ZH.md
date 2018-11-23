# 直接在设备里面安装

    opkg update
    opkg list | grep rtty
    opkg install rtty-nossl

# 自己编译
## 添加feed（openwrt 14.04,15.05,Lede and openwrt 18）
Openwrt 14.04

    echo 'src-git rtty https://github.com/zhaojh329/rtty.git;openwrt-14-15' >> feeds.conf.default

Openwrt 15.05

    echo 'src-git rtty https://github.com/zhaojh329/rtty.git;openwrt-lede' >> feeds.conf.default

openwrt 18

    echo 'src-git rtty https://github.com/zhaojh329/rtty.git;openwrt-18' >> feeds.conf.default

## 安装feed（openwrt 14.04,15.05,Lede and openwrt 18）

    ./scripts/feeds uninstall -a
    ./scripts/feeds update rtty
    ./scripts/feeds install -a -f -p rtty
    ./scripts/feeds install -a

## 安装feed（master）

    ./scripts/feeds update -a
    ./scripts/feeds install -a

## 在menuconfig中选择rtty，然后重新编译固件。

    Utilities  --->
        Terminal  --->
            <*> rtty-mbedtls............................ A reverse proxy WebTTY (mbedtls)
            < > rtty-nossl............................... A reverse proxy WebTTY (NO SSL)
            < > rtty-openssl............................ A reverse proxy WebTTY (openssl)
            < > rtty-wolfssl............................ A reverse proxy WebTTY (wolfssl)

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
