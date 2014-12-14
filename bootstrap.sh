#!/bin/bash

mkdir build-aux/ 2>/dev/null
touch build-aux/config.rpath libdvbv5-po/Makefile.in.in v4l-utils-po/Makefile.in.in
autoreconf -vfi

GETTEXTIZE=$(which gettextize)
if [ "GETTEXTIZE" != "" ]; then
	VER=$(gettextize --version|perl -ne 'print $1 if (m/(\d\.\d.*)/)')

	cp build-aux/config.rpath build-aux/config.rpath.bak

	sed "s,read dummy < /dev/tty,," < $GETTEXTIZE > ./gettextize
	chmod 755 ./gettextize

	echo "Generating locale v4l-utils-po build files for gettext version $VER"
	./gettextize --force --copy --no-changelog --po-dir=v4l-utils-po >/dev/null

	echo "Generating locale libdvbv5-po build files for gettext version $VER"
	./gettextize --force --copy --no-changelog --po-dir=libdvbv5-po >/dev/null

	for i in v4l-utils-po/Makefile.in.in libdvbv5-po/Makefile.in.in; do
		sed 's,rm -f Makefile,rm -f,' $i >a && mv a $i
	done
	sed 's,PACKAGE = @PACKAGE@,PACKAGE = @LIBDVBV5_DOMAIN@,' <libdvbv5-po/Makefile.in.in >a && mv a libdvbv5-po/Makefile.in.in

	mv build-aux/config.rpath.bak build-aux/config.rpath
fi
