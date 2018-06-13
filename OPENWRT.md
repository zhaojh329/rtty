# Install directly inside the device

    opkg update
    opkg list | grep rtty
    opkg install rtty-nossl

# Compile it yourself
## For Openwrt master
Update feeds

	./scripts/feeds update -a
	./scripts/feeds install -a

Select rtty in menuconfig and compile new image

    Utilities  --->
	    Terminal  --->
	        <*> rtty-mbedtls............................ A reverse proxy WebTTY (mbedtls)
	        < > rtty-nossl............................... A reverse proxy WebTTY (NO SSL)
	        < > rtty-openssl............................ A reverse proxy WebTTY (openssl)
	        < > rtty-wolfssl............................ A reverse proxy WebTTY (wolfssl)

## [For Openwrt 14.04](https://github.com/zhaojh329/rtty/blob/openwrt-14.04/README.md)

## [For Openwrt 15.05](https://github.com/zhaojh329/rtty/blob/openwrt-15.05/README.md)

## [For Lede](https://github.com/zhaojh329/rtty/blob/openwrt-lede/README.md)

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