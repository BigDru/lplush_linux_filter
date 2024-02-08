#!/bin/bash
g++ -std=c++20 -Wall filter.cpp -static-libgcc -static-libstdc++ \
	/usr/lib/aarch64-linux-gnu/libcups.a \
	/usr/lib/aarch64-linux-gnu/libcupsimage.a \
	/usr/lib/aarch64-linux-gnu/libcupsfilters.a \
	/usr/lib/aarch64-linux-gnu/libtasn1.a \
	/usr/lib/aarch64-linux-gnu/libavahi-client.a \
	/usr/lib/aarch64-linux-gnu/libavahi-common.a \
	/usr/lib/aarch64-linux-gnu/libdbus-1.a \
	/usr/lib/aarch64-linux-gnu/libidn2.a \
	/usr/lib/aarch64-linux-gnu/libunistring.a \
	/usr/lib/aarch64-linux-gnu/libz.a \
	-lgnutls \
	-lgmp \
	-lnettle \
	-lgssapi_krb5 \
	-lp11-kit \
	-lsystemd \
