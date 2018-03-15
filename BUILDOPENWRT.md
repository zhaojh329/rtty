Update feeds

    ./scripts/feeds update -a
    ./scripts/feeds install -a

Select rtty in menuconfig and compile new image.

    Utilities  --->
        Terminal  --->
            < > rtty-mbedtls............................ A reverse proxy WebTTY (mbedtls)
            <*> rtty-nossl............................... A reverse proxy WebTTY (NO SSL)
            < > rtty-openssl............................ A reverse proxy WebTTY (openssl)
            < > rtty-wolfssl............................ A reverse proxy WebTTY (wolfssl)
