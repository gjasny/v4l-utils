#!/bin/bash

autoreconf -vfi

GETTEXTIZE=$(which gettextize)
if [ "GETTEXTIZE" != "" ]; then

	cp build-aux/config.rpath build-aux/config.rpath.bak

	sed "s,read dummy < /dev/tty,," < $GETTEXTIZE > ./gettextize
	chmod 755 ./gettextize

	./gettextize --force --copy --no-changelog --po-dir=v4l-utils-po
	./gettextize --force --copy --no-changelog --po-dir=libdvbv5-po

	for i in v4l-utils-po/Makefile.in.in libdvbv5-po/Makefile.in.in; do
		sed 's,rm -f Makefile, rm -f,' $i >a && mv a $i
	done
	sed 's,rm -f Makefile , rm -f ,' $i >a && mv a $i
	sed 's,PACKAGE = @PACKAGE@,PACKAGE = @LIBDVBV5_DOMAIN@,' <libdvbv5-po/Makefile.in.in >a && mv a libdvbv5-po/Makefile.in.in

	mv build-aux/config.rpath.bak build-aux/config.rpath
fi
