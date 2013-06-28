#!/bin/sh

set -e

(cd src && ./gen-makefile-am.sh)

libtoolize -c
aclocal
autoheader
automake -a -c
autoconf
