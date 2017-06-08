include $(TOPDIR)/rules.mk

PKG_NAME:=xterminal
PKG_VERSION:=2017-6-8
PKG_RELEASE:=$(PKG_SOURCE_VERSION)

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/zhaojh329/xterminal.git
PKG_SOURCE_VERSION:=2b53cd53ebcf1a8860bd0f9cbc4e60c5270e3bb1
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz

include $(INCLUDE_DIR)/package.mk

define Package/xterminal
  SECTION:=net
  CATEGORY:=Network
  TITLE:=xTerminal
  URL:=https://github.com/zhaojh329/xterminal
  DEPENDS:=+lua-cjson +evmongoose +luaposix
endef

define Package/xterminal/description
	Xterminal is a multi device remote web shell tool
endef

define Build/Compile
endef

define Package/xterminal/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/xterminal_device.lua $(1)/usr/sbin/xterminal_device
endef

$(eval $(call BuildPackage,xterminal))
