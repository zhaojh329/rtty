# xTerminal([github](https://github.com/zhaojh329/xterminal))

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

xTerminal是一个多终端的远程Web Shell工具。你可以通过浏览器根据特定的设备MAC地址登录到你的Linux设备。
它非常适合公司对公司部署在全球各地的成千上万的Linux设备进行远程调试。它基于[evmongoose](https://github.com/zhaojh329/evmongoose)实现，由客户端和服务器两部分构成。

![](https://github.com/zhaojh329/image/blob/master/xterminal_zh.png)

![](https://github.com/zhaojh329/image/blob/master/xterminal_demo.png)

# 安装
## 在Ubuntu上安装Server
### 安装依赖
* [evmongoose](https://github.com/zhaojh329/evmongoose/blob/master/README_ZH.md)

* lua-posix mosquitto

		sudo apt install lua-posix mosquitto
    
### 安装 xTerminal Server
    git clone https://github.com/zhaojh329/xterminal.git
    cd xterminal/ubuntu
	sudo make install

## 安装客户端 OpenWRT/LEDE
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

# 使用
# 查询在线设备
	http://server:8000/list
	
# 连接设备
在浏览器中输入服务器地址，默认端口号8000，然后在出现的页面中输入要连接的设备MAC地址，MAC地址的格式可以是：
xx:xx:xx:xx:xx:xx, xx-xx-xx-xx-xx-xx, xxxxxxxxxxxxx

# 贡献代码

xTerminal使用github托管其源代码，贡献代码使用github的PR(Pull Request)的流程，十分的强大与便利:

1. [创建 Issue](https://github.com/zhaojh329/xterminal/issues/new) - 对于较大的
	改动(如新功能，大型重构等)最好先开issue讨论一下，较小的improvement(如文档改进，bugfix等)直接发PR即可
2. Fork [xterminal](https://github.com/zhaojh329/xterminal) - 点击右上角**Fork**按钮
3. Clone你自己的fork: ```git clone https://github.com/$userid/xterminal.git```
4. 创建dev分支，在**dev**修改并将修改push到你的fork上
5. 创建从你的fork的**dev**分支到主项目的**dev**分支的[Pull Request] -  
	[在此](https://github.com/zhaojh329/xterminal)点击**Compare & pull request**
6. 等待review, 需要继续改进，或者被Merge!
	
## 感谢以下项目提供帮助
* [evmongoose](https://github.com/zhaojh329/evmongoose)
* [xterm.js](https://github.com/sourcelair/xterm.js)

# 技术交流
QQ群：153530783

# 如果该项目对您有帮助，请随手star，谢谢！
