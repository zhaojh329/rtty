# Install directly in the device(master)

    opkg update
    opkg list | grep rtty
    opkg install rtty-nossl

# Compile it yourself
## Add feed for openwrt 14.04,15.05,Lede and openwrt 18
Openwrt 14.04

    echo 'src-git rtty https://github.com/zhaojh329/rtty.git;openwrt-14-15' >> feeds.conf.default

Openwrt 15.05

    echo 'src-git rtty https://github.com/zhaojh329/rtty.git;openwrt-lede' >> feeds.conf.default

openwrt 18

    echo 'src-git rtty https://github.com/zhaojh329/rtty.git;openwrt-18' >> feeds.conf.default

## install feed for openwrt 14.04,15.05,Lede and openwrt 18

    ./scripts/feeds uninstall -a
    ./scripts/feeds update rtty
    ./scripts/feeds install -a -f -p rtty
    ./scripts/feeds install -a

## install feed for master

	./scripts/feeds update -a
	./scripts/feeds install -a

## Select rtty in menuconfig and compile new image

    Utilities  --->
	    Terminal  --->
	        <*> rtty-mbedtls............................ A reverse proxy WebTTY (mbedtls)
	        < > rtty-nossl............................... A reverse proxy WebTTY (NO SSL)
	        < > rtty-openssl............................ A reverse proxy WebTTY (openssl)
	        < > rtty-wolfssl............................ A reverse proxy WebTTY (wolfssl)

# Configure
Configuring the server parameter

    uci add rtty rtty   # If it is configured for the first time
    uci set rtty.@rtty[0].host='your server host'
    uci set rtty.@rtty[0].port='your server port'

You can customize an ID for your device. If the ID is not configured, RTTY will use
the MAC address of the specified network interface as the ID.

	uci set rtty.@rtty[0].id='your-device-id'

You can add a description to your device

    uci set rtty.@rtty[0].description='My device'

Use SSL

    uci set rtty.@rtty[0].ssl='1'

Save configuration and apply

    uci commit
    /etc/init.d/rtty restart
