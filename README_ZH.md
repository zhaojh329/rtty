# xTerminal

![](https://img.shields.io/badge/license-GPLV3-brightgreen.svg?style=plastic "License")

xTerminal是一个多终端的远程Web Shell工具。你可以通过浏览器根据特定的设备MAC地址登录到你的Linux设备。
它基于[evmongoose](https://github.com/zhaojh329/evmongoose)实现，由客户端和服务器两部分构成。

![](https://github.com/zhaojh329/xterminal/blob/master/xterminal_zh.png)

# 安装
## 在Ubuntu上安装Server
### 安装依赖
* [evmongoose](https://github.com/zhaojh329/evmongoose/blob/master/README_ZH.md)

* lua-posix

		sudo apt install lua-posix
    
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

# 技术交流
QQ群：153530783

# 如果该项目对您有帮助，请随手star，谢谢！
