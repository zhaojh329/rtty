#!/bin/bash

get_distribution() {
	lsb_dist=""
	# Every system that we officially support has /etc/os-release
	if [ -r /etc/os-release ]; then
		lsb_dist="$(. /etc/os-release && echo "$ID")"
	fi
	# Returning an empty string here should be alright since the
	# case statements don't act unless you provide an actual value
	echo "$lsb_dist"
}

# perform some very rudimentary platform detection
lsb_dist=$( get_distribution )
lsb_dist="$(echo "$lsb_dist" | tr '[:upper:]' '[:lower:]')"

case "$lsb_dist" in
	ubuntu)
		which pkg-config > /dev/null || apt-get -y install pkg-config
		which gcc > /dev/null || apt-get -y install gcc
		which make > /dev/null || apt-get -y install make
		which cmake > /dev/null || apt-get -y install cmake
		which git > /dev/null || apt-get -y install git
		which quilt > /dev/null || apt-get -y install quilt

		dpkg -L libjson-c-dev > /dev/null 2>&1 || apt-get -y install libjson-c-dev
		dpkg -L libssl-dev > /dev/null 2>&1 || apt-get -y install libssl-dev
	;;
	*)
		echo "Your platform is not supported by this installer script."
		exit 1
	;;
esac

rm -rf /tmp/rtty-build
mkdir /tmp/rtty-build
pushd /tmp/rtty-build

git clone https://git.openwrt.org/project/libubox.git
git clone https://git.openwrt.org/project/ustream-ssl.git
git clone https://github.com/zhaojh329/libuwsc.git
git clone https://github.com/zhaojh329/rtty.git

# libubox
cd libubox && cmake -DBUILD_LUA=OFF . && make install && cd -


# ustream-ssl
cd ustream-ssl
git checkout 189cd38b4188bfcb4c8cf67d8ae71741ffc2b906

SSL_VER_MAJOR=$(openssl version | cut -d' ' -f2 | cut -d. -f1)
SSL_VER_MINOR=$(openssl version | cut -d' ' -f2 | cut -d. -f2)
SSL_VER="${SSL_VER_MAJOR}${SSL_VER_MINOR}"

[ $SSL_VER -ge 11 ] && {
	quilt import ../rtty/tools/us-openssl_v1_1.patch
	quilt push -a
}

cmake . && make install && cd -


# libuwsc
cd libuwsc && cmake . && make install && cd -


# rtty
cd rtty && cmake . && make install

popd
rm -rf /tmp/rtty-build

ldconfig

rtty -V