更新feeds:

    ./scripts/feeds update -a
    ./scripts/feeds install -a

如果你使用的是chaos_calmer(15.05), 你需要修改Makefile: feeds/packages/utils/rtty/Makefile

    PKG_SOURCE_PROTO:=git
    PKG_SOURCE_VERSION:=v$(PKG_VERSION)
    PKG_SOURCE_URL=https://github.com/zhaojh329/rtty.git
    # Add the following two lines
    PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
    PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_SOURCE_VERSION).tar.gz
    # And comment the line below
    #PKG_MIRROR_HASH:=23a203351fdd47acfd16d3c3b3e3d51dd65a5d9e8ca89d4b1521d40c40616102

在menuconfig中选择rtty，然后重新编译固件。

    Utilities  --->
        Terminal  --->
            < > rtty-mbedtls............................ A reverse proxy WebTTY (mbedtls)
            <*> rtty-nossl............................... A reverse proxy WebTTY (NO SSL)
            < > rtty-openssl............................ A reverse proxy WebTTY (openssl)
            < > rtty-wolfssl............................ A reverse proxy WebTTY (wolfssl)