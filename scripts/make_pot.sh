#!/bin/sh
# original https://github.com/Morganamilo/paru/blob/master/scripts/mkpot
set -e

xgettext \
	-d taur \
	--msgid-bugs-address https://github.com/BurntRanch/TabAUR \
	--package-name=taur\
	--default-domain=taur\
	--package-version="$(awk -F '= ' '/^VERSION/ {print $2}' Makefile | sed 's/\"//g')" \
	-k_ \
	-o po/taur.pot \
	src/*.cpp include/*.hpp --c++
