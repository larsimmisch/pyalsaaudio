#!/bin/sh

# NOTE: ensure to use the correct package manager depending on the manylinux
# image used.

if which yum; then
	yum install -y alsa-lib-devel
	exit
fi

if which apk; then
	apk add alsa-lib-dev
	exit
fi

if which apt-get; then
	apt-get install -y libasound2-dev
	exit
fi

echo "Cannot install ALSA headers! No package manager detected"
exit 1
