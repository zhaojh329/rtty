# xTerminal([中文](https://github.com/zhaojh329/xterminal/blob/master/README_ZH.md))([github](https://github.com/zhaojh329/xterminal))

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

XTerminal is a remote web shell tool for multi terminal devices.You can login to your Linux device via the browser at the specified MAC address.
It suits the company to the company very much Thousands of Linux devices are deployed around the world for remote debugging.
XTerminal is based on [evmongoose](https://github.com/zhaojh329/evmongoose) implementation, It consists of two parts, server and client.

![](https://github.com/zhaojh329/image/blob/master/xterminal.png)

![](https://github.com/zhaojh329/image/blob/master/xterminal_demo.png)

# How To Install
## Install server on Ubuntu
### Install dependency
* [evmongoose](https://github.com/zhaojh329/evmongoose/blob/master/README.md)

* lua-posix mosquitto

		sudo apt install lua-posix mosquitto
    
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
# Query online device
	http://server:8000/list

# Connect to devic
In the browser, enter the server address, the default port number 8000, and then in the page appears to enter the the 
MAC address of then device to be connected to, MAC address format can be:
xx:xx:xx:xx:xx:xx, xx-xx-xx-xx-xx-xx, xxxxxxxxxxxxx

# How To Contribute
Feel free to create issues or pull-requests if you have any problems.

**Please read [contributing.md](https://github.com/zhaojh329/xterminal/blob/master/contributing.md)
before pushing any changes.**

# Thanks for the following project
* [evmongoose](https://github.com/zhaojh329/evmongoose)
* [xterm.js](https://github.com/sourcelair/xterm.js)

# If the project is helpful to you, please do not hesitate to star. Thank you!
