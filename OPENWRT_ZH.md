# 直接在设备里面安装

    opkg update
    opkg list | grep rtty
    opkg install rtty-nossl

# 自己编译
## 更新feed

    ./scripts/feeds update packages
    ./scripts/feeds install -a -p packages

***如果您的openwrt中的rtty不是最新版本，您可以从这里获取最新的package***

    https://gitee.com/zhaojh329/rtty/tree/openwrt-package

## 在menuconfig中选择rtty，然后重新编译固件。

    Utilities  --->
	    Terminal  --->
	        <*> rtty-mbedtls................. Access your terminals from anywhere via the web
	        < > rtty-nossl................... Access your terminals from anywhere via the web
	        < > rtty-openssl................. Access your terminals from anywhere via the web
	        < > rtty-wolfssl................. Access your terminals from anywhere via the web

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

认证

    uci set rtty.@rtty[0].token='your-token'

保存配置并应用

    uci commit
    /etc/init.d/rtty restart
