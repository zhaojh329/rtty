include $(TOPDIR)/rules.mk

PKG_NAME:=xterminal
PKG_VERSION:=2017-6-11
PKG_RELEASE:=$(PKG_SOURCE_VERSION)

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/zhaojh329/xterminal.git
PKG_SOURCE_VERSION:=69942efe0b30624dd4923f66c4458aa01c9ad27b
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz

include $(INCLUDE_DIR)/package.mk

define Package/xterminal
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=multi device remote web shell tool 
  URL:=https://github.com/zhaojh329/xterminal
  DEPENDS:=+lua-cjson +evmongoose +luaposix
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
