#
# Copyright (C) 2018 Jianhui Zhao
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libuwsc
PKG_VERSION:=3.3.2
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_VERSION:=v$(PKG_VERSION)
PKG_SOURCE_URL=https://github.com/zhaojh329/libuwsc/releases/download/v$(PKG_VERSION)
PKG_HASH:=0bfff3e11d075a125a4f4c486dd17f0cdfef546dd0581116578c0d41f0f121ee
CMAKE_INSTALL:=1

PKG_BUILD_DIR=$(BUILD_DIR)/$(PKG_NAME)-$(BUILD_VARIANT)/$(PKG_SOURCE_SUBDIR)

PKG_LICENSE:=LGPL-2.1
PKG_LICENSE_FILES:=LICENSE

PKG_MAINTAINER:=Jianhui Zhao <jianhuizhao329@gmail.com>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/libuwsc/Default
  TITLE:=A lightweight WebSocket client library based on libev
  SECTION:=libs
  CATEGORY:=Libraries
  SUBMENU:=Networking
  URL:=https://github.com/zhaojh329/libuwsc
  DEPENDS:=+libev $(2)
  VARIANT:=$(1)
  PROVIDES:=libuwsc
endef

Package/libuwsc-openssl=$(call Package/libuwsc/Default,openssl,+PACKAGE_libuwsc-openssl:libopenssl)
Package/libuwsc-wolfssl=$(call Package/libuwsc/Default,wolfssl,+PACKAGE_libuwsc-wolfssl:libwolfssl)
Package/libuwsc-mbedtls=$(call Package/libuwsc/Default,mbedtls,+PACKAGE_libuwsc-mbedtls:libmbedtls)
Package/libuwsc-nossl=$(call Package/libuwsc/Default,nossl)

ifeq ($(BUILD_VARIANT),openssl)
  CMAKE_OPTIONS += -DUWSC_USE_OPENSSL=ON
else ifeq ($(BUILD_VARIANT),wolfssl)
  CMAKE_OPTIONS += -DUWSC_USE_WOLFSSL=ON
else ifeq ($(BUILD_VARIANT),mbedtls)
  CMAKE_OPTIONS += -DUWSC_USE_MBEDTLS=ON
else
  CMAKE_OPTIONS += -DUWSC_SSL_SUPPORT=OFF
endif

define Package/libuwsc-$(BUILD_VARIANT)/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libuwsc.so* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libuwsc-openssl))
$(eval $(call BuildPackage,libuwsc-wolfssl))
$(eval $(call BuildPackage,libuwsc-mbedtls))
$(eval $(call BuildPackage,libuwsc-nossl))
