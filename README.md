# Add feeds
    
    echo 'src-git rtty https://github.com/zhaojh329/rtty.git;openwrt-18' >> feeds.conf.default

# Update feeds

    ./scripts/feeds uninstall -a
    ./scripts/feeds update rtty
    ./scripts/feeds install -a -f -p rtty
    ./scripts/feeds install -a

# Select rtty in menuconfig and compile new image.

    Utilities  --->
        Terminal  --->
            <*> rtty-mbedtls................... Access your terminals from anywhere via the web (mbedtls)
            < > rtty-nossl..................... Access your terminals from anywhere via the web (NO SSL)
            < > rtty-openssl................... Access your terminals from anywhere via the web (openssl)
            < > rtty-wolfssl................... Access your terminals from anywhere via the web (wolfssl)
