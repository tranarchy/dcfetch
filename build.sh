#!/bin/sh

SRC="src/main.c src/dict_util.c src/os.c src/rpc.c"
STDFLAGS="-Iinclude -Wall"
OUTPUT="dcfetch"

CC="cc"
OS=$(uname)
TARGET="$1"

if [ "$TARGET" = "" ]; then
	if [ $OS = "Linux" ]; then
		SRC="$SRC src/audio/linux.c"
	elif [ $OS = "Darwin" ]; then
		SRC="$SRC src/audio/macos.c"
	else
		SRC="$SRC src/audio/unsupported.c"
	fi

	set -x

	$CC $SRC $STDFLAGS -o $OUTPUT
elif [ "$TARGET" = "install" ]; then
	set -x
		
	mkdir -p /usr/local/bin
	chmod +x $OUTPUT
	cp -f $OUTPUT /usr/local/bin
fi
