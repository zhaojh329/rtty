# Install directly in the device

    opkg update
    opkg list | grep rtty
    opkg install rtty-nossl

# Compile it yourself
## Update feed

    ./scripts/feeds update packages
    ./scripts/feeds install -a -p packages

***If the rtty is not the latest version in your openwrt, you can get the latest package from here***

    https://github.com/zhaojh329/rtty/tree/openwrt-package

## Select rtty in menuconfig and compile new image

    Utilities  --->
	    Terminal  --->
	        <*> rtty-mbedtls................. Access your terminals from anywhere via the web
	        < > rtty-nossl................... Access your terminals from anywhere via the web
	        < > rtty-openssl................. Access your terminals from anywhere via the web
	        < > rtty-wolfssl................. Access your terminals from anywhere via the web

# Configure
Configuring the server parameter

    uci add rtty rtty   # If it is configured for the first time
    uci set rtty.@rtty[0].host='your server host'
    uci set rtty.@rtty[0].port='your server port'

You can customize an ID for your device. If the ID is not configured, rtty will use
the MAC address of the specified network interface as the ID.

	uci set rtty.@rtty[0].id='your-device-id'

You can add a description to your device

    uci set rtty.@rtty[0].description='My device'

Use SSL

    uci set rtty.@rtty[0].ssl='1'

Authorization

    uci set rtty.@rtty[0].token='your-token'

Save configuration and apply

    uci commit
    /etc/init.d/rtty start
