#!/bin/bash

LSB_ID=

INSTALL=
REMOVE=
UPDATE=

PKG_LIBEV="libev-dev"
PKG_SSL="libssl-dev"
PKG_LIBPROTOBUF_C="libprotobuf-c0-dev"

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
		REMOVE="apt remove -y"
		return 0
	}

	check_cmd apt-get && {
		UPDATE="apt-get update -q"
		INSTALL="apt-get install -y"
		REMOVE="apt-get remove -y"
		return 0
	}

	check_cmd yum && {
		UPDATE="yum update -yq"
		INSTALL="yum install -y"
		REMOVE="yum "
		PKG_LIBEV="libev-devel"
		PKG_SSL="openssl-devel"
		PKG_LIBPROTOBUF_C="protobuf-c-devel"
		return 0
	}

	check_cmd pacman && {
		UPDATE="pacman -Sy --noprogressbar"
		INSTALL="pacman -S --noconfirm --noprogressbar"
		REMOVE="pacman -R --noconfirm --noprogressbar"
		PKG_LIBEV="libev"
		PKG_SSL="openssl"
		PKG_LIBPROTOBUF_C="protobuf-c"
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
	path=$(find $search -type f -name $header 2>/dev/null)
	[ -n "$path" ] && {
		echo ${path%/*} | cut -d' ' -f1
		return
	}

	search="/usr/local/include/$prefix"
	path=$(find $search -type f -name $header 2>/dev/null)
	[ -n "$path" ] && {
		echo ${path%/*} | cut -d' ' -f1
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

check_lib_header ev.h > /dev/null || $INSTALL $PKG_LIBEV

LIBSSL_INCLUDE=$(check_lib_header opensslv.h)
[ -n "$LIBSSL_INCLUDE" ] || {
	$INSTALL $PKG_SSL
	LIBSSL_INCLUDE=$(check_lib_header opensslv.h)
}

PBC_INCLUDE=$(check_lib_header protobuf-c.h)
[ -n "$PBC_INCLUDE" ] || {
	$INSTALL $PKG_LIBPROTOBUF_C
	PBC_INCLUDE=$(check_lib_header protobuf-c.h)
}

PBC_VER=$(cat $PBC_INCLUDE/protobuf-c.h | grep PROTOBUF_C_VERSION_NUMBER | grep -o '[0-9x]\+')
[ -n "$PBC_VER" ] || PBC_VER="0000000"

rm -rf /tmp/rtty-build
mkdir /tmp/rtty-build
pushd /tmp/rtty-build

if [ $PBC_VER -lt 1002000 ];
then
	$REMOVE $PKG_LIBPROTOBUF_C

	check_tool autoconf
	check_tool libtool

	git clone https://github.com/protobuf-c/protobuf-c.git || {
		echo "Clone protobuf-c failed"
		exit 1
	}

	cd protobuf-c && ./autogen.sh && ./configure --disable-protoc
	[ $? -eq 0 ] || exit 1

	make && make install && cd -
	[ $? -eq 0 ] || exit 1
fi

git clone https://github.com/zhaojh329/libuwsc.git || {
	echo "Clone libuwsc failed"
	exit 1
}

sleep 5

git clone https://github.com/zhaojh329/rtty.git || {
	echo "Clone rtty failed"
	exit 1
}

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
