#
# Copyright (C) 2018 Jianhui Zhao
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libuwsc
PKG_VERSION:=3.0.3
PKG_RELEASE:=2

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_VERSION:=v$(PKG_VERSION)
PKG_SOURCE_URL=https://codeload.github.com/zhaojh329/libuwsc/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=226e092d9b24b5aec6fbd637f2986893cee015e5ca5b20a89d7eb05b5fdc8730
CMAKE_INSTALL:=1

PKG_BUILD_DIR=$(BUILD_DIR)/$(PKG_NAME)-$(BUILD_VARIANT)/$(PKG_SOURCE_SUBDIR)

PKG_LICENSE:=LGPL-2.1
PKG_LICENSE_FILES:=LICENSE

PKG_MAINTAINER:=Jianhui Zhao <jianhuizhao329@gmail.com>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/libuwsc/default
  SECTION:=libs
  CATEGORY:=Libraries
  SUBMENU:=Networking
  TITLE:=Lightweight WebSocket client library based on libev
  DEPENDS:=+libev
endef

define Package/libuwsc-mbedtls
  $(Package/libuwsc/default)
  VARIANT:=mbedtls
  TITLE += (mbedtls)
  DEPENDS += +libmbedtls
  CONFLICTS:=libuwsc-nossl libuwsc-openssl libuwsc-wolfssl
endef

define Package/libuwsc-nossl
  $(Package/libuwsc/default)
  VARIANT:=nossl
  TITLE += (NO SSL)
  CONFLICTS:=libuwsc-openssl libuwsc-wolfssl
endef

define Package/libuwsc-openssl
  $(Package/libuwsc/default)
  TITLE += (openssl)
  VARIANT:=openssl
  DEPENDS += +libopenssl
  CONFLICTS:=libuwsc-wolfssl
endef

define Package/libuwsc-wolfssl
  $(Package/libuwsc/default)
  TITLE += (wolfssl)
  VARIANT:=wolfssl
  DEPENDS += +libwolfssl
endef

ifeq ($(BUILD_VARIANT),openssl)
  CMAKE_OPTIONS += -DUWSC_USE_OPENSSL=ON
else ifeq ($(BUILD_VARIANT),wolfssl)
  CMAKE_OPTIONS += -DUWSC_USE_WOLFSSL=ON
else ifeq ($(BUILD_VARIANT),mbedtls)
  CMAKE_OPTIONS += -DUWSC_USE_MBEDTLS=ON
else
  CMAKE_OPTIONS += -DUWSC_SSL_SUPPORT=OFF
endif

define Package/libuwsc/default/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/libuwsc.so* $(1)/usr/lib/
endef

Package/libuwsc-nossl/install = $(Package/libuwsc/default/install)
Package/libuwsc-openssl/install = $(Package/libuwsc/default/install)
Package/libuwsc-wolfssl/install = $(Package/libuwsc/default/install)
Package/libuwsc-mbedtls/install = $(Package/libuwsc/default/install)

$(eval $(call BuildPackage,libuwsc-nossl))
$(eval $(call BuildPackage,libuwsc-mbedtls))
$(eval $(call BuildPackage,libuwsc-wolfssl))
$(eval $(call BuildPackage,libuwsc-openssl))
