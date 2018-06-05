#!/bin/bash

LSB_ID=

INSTALL=
UPDATE=

PKG_JSON="libjson-c-dev"
PKG_SSL="libssl-dev"

show_distribution() {
	local pretty_name=""

	if [ -f /etc/os-release ];
	then
		. /etc/os-release
		pretty_name="$PRETTY_NAME"
		LSB_ID="$(echo "$ID" | tr '[:upper:]' '[:lower:]')"
	elif [ -f /etc/redhat-release ];
	then
		pretty_name=$(cat /etc/redhat-release)
		LSB_ID="$(echo "$pretty_name" | tr '[:upper:]' '[:lower:]')"
		echo "$LSB_ID" | grep centos > /dev/null && LSB_ID=centos
	fi

	LSB_ID=$(echo "$LSB_ID" | tr '[:upper:]' '[:lower:]')

	echo "Platform: $pretty_name"
}

check_cmd() {
	which $1 > /dev/null 2>&1
}

detect_pkg_tool() {
	check_cmd apt && {
		UPDATE="apt update -q"
		INSTALL="apt install -y"
		return 0
	}

	check_cmd apt-get && {
		UPDATE="apt-get update -q"
		INSTALL="apt-get install -y"
		return 0
	}

	check_cmd yum && {
		UPDATE="yum update -yq"
		INSTALL="yum install -y"
		PKG_JSON="json-c-devel"
		PKG_SSL="openssl-devel"
		return 0
	}

	check_cmd pacman && {
		UPDATE="pacman -Sy --noprogressbar"
		INSTALL="pacman -S --noconfirm --noprogressbar"
		PKG_JSON="json-c"
		PKG_SSL="openssl"
		return 0
	}

	return 1
}

check_tool() {
	local tool=$1

	check_cmd $tool || $INSTALL $tool
}

check_lib_header() {
	local header=$1
	local prefix=$2
	local path search

	search="/usr/include/$prefix"
	path=$(find $search -name $header 2>/dev/null)
	[ -n "$path" ] && {
		echo ${path%/*}
		return
	}

	search="/usr/local/include/$prefix"
	path=$(find $search -name $header 2>/dev/null)
	[ -n "$path" ] && {
		echo ${path%/*}
		return
	}
}

show_distribution

detect_pkg_tool || {
	echo "Your platform is not supported by this installer script."
	exit 1
}

$UPDATE

[ "$LSB_ID" = "centos" ] && $INSTALL epel-release

check_tool pkg-config
check_tool gcc
check_tool make
check_tool cmake
check_tool git
check_tool quilt

check_lib_header json.h json-c > /dev/null || $INSTALL $PKG_JSON

LIBSSL_INCLUDE=$(check_lib_header opensslv.h)
[ -n "$LIBSSL_INCLUDE" ] || {
	$INSTALL $PKG_SSL
	LIBSSL_INCLUDE=$(check_lib_header opensslv.h)
}

rm -rf /tmp/rtty-build
mkdir /tmp/rtty-build
pushd /tmp/rtty-build

git clone https://git.openwrt.org/project/libubox.git
git clone https://git.openwrt.org/project/ustream-ssl.git
git clone https://github.com/zhaojh329/libuwsc.git
git clone https://github.com/zhaojh329/rtty.git

# libubox
cd libubox && cmake -DBUILD_LUA=OFF . && make install && cd -
[ $? -eq 0 ] || exit 1

# ustream-ssl
cd ustream-ssl
git checkout 189cd38b4188bfcb4c8cf67d8ae71741ffc2b906

LIBSSL_VER=$(cat $LIBSSL_INCLUDE/opensslv.h | grep OPENSSL_VERSION_NUMBER | grep -o '[0-9x]\+')
LIBSSL_VER=$(echo ${LIBSSL_VER:2:6})

# > 1.1
if [ $LIBSSL_VER -ge 101000 ];
then 
	quilt import ../rtty/tools/us-openssl_v1_1.patch
	quilt push -a
elif [ $LIBSSL_VER -le 100020 ];
then
    # < 1.1.2
    quilt import ../rtty/tools/us-openssl_v1_0_1.patch
    quilt push -a
fi

cmake . && make install && cd -
[ $? -eq 0 ] || exit 1

# libuwsc
cd libuwsc && cmake . && make install && cd -
[ $? -eq 0 ] || exit 1

# rtty
cd rtty && cmake . && make install
[ $? -eq 0 ] || exit 1

popd
rm -rf /tmp/rtty-build

ldconfig

case "$LSB_ID" in
	centos|arch)
		echo "/usr/local/lib" > /etc/ld.so.conf.d/rtty
		ldconfig -f /etc/ld.so.conf.d/rtty
	;;
esac

rtty -V
