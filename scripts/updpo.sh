#!/bin/sh
set -e

for po in po/*.po; do
	msgmerge "$po" "po/taur.pot" -o "$po"
done
