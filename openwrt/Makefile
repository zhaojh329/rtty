include $(TOPDIR)/rules.mk

PKG_NAME:=xterminal
PKG_VERSION:=2017-7-3
PKG_RELEASE:=$(PKG_SOURCE_VERSION)

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/zhaojh329/xterminal.git
PKG_SOURCE_VERSION:=d0275e8c5715e2438f804ff07a41f404b10af220
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz

include $(INCLUDE_DIR)/package.mk

define Package/xterminal
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=multi device remote web shell tool 
  URL:=https://github.com/zhaojh329/xterminal
  DEPENDS:=+lua-cjson +evmongoose +luaposix +uclient-fetch +libustream-mbedtls
  SUBMENU:=Terminal
endef

define Package/xterminal/description
	Xterminal is a multi device remote web shell tool
endef

define Build/Compile
endef

define Package/xterminal/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/xterminal_device.lua $(1)/usr/sbin/xterminal_device
	$(INSTALL_BIN) ./files/xterminal.init $(1)/etc/init.d/xterminal
	$(INSTALL_CONF) ./files/xterminal.config $(1)/etc/config/xterminal
endef

$(eval $(call BuildPackage,xterminal))
