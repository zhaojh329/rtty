# xTerminal([中文](https://github.com/zhaojh329/xterminal/blob/master/README_ZH.md))

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

XTerminal is a remote web shell tool for multi terminal devices.You can login to your Linux device via the browser at the specified MAC address.
XTerminal is based on [evmongoose](https://github.com/zhaojh329/evmongoose) implementation, It consists of two parts, server and client.

# How To Install
## Install server on Ubuntu
### Install dependency
* [evmongoose](https://github.com/zhaojh329/evmongoose/blob/master/README.md)

* lua-posix

		sudo apt install lua-posix
    
### Install xTerminal Server
    git clone https://github.com/zhaojh329/xterminal.git
    cd xterminal/ubuntu
	sudo make install

### Run Server on Ubuntu
	/etc/init.d/xterminal start

## Install Client on OpenWRT/LEDE
	git clone https://github.com/zhaojh329/evmongoose.git
	cp -r evmongoose/openwrt openwrt_dir/package/evmongoose
	
	git clone https://github.com/zhaojh329/xterminal.git
	cp -r xterminal/openwrt openwrt_dir/package/xterminal
	
	cd openwrt_dir
	./scripts/feeds update -a
	./scripts/feeds install -a
	
	make menuconfig
	Network  --->
	    <*> xterminal
	
	make package/xterminal/compile V=s

# How to use


# How To Contribute
Feel free to create issues or pull-requests if you have any problems.

**Please read [contributing.md](https://github.com/zhaojh329/xterminal/blob/master/contributing.md)
before pushing any changes.**

# Thanks for the following project
* [evmongoose](https://github.com/zhaojh329/evmongoose)

# If the project is helpful to you, please do not hesitate to star. Thank you!
