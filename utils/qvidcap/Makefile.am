bin_PROGRAMS = qvidcap
man_MANS = qvidcap.1

qvidcap_SOURCES = qvidcap.cpp qvidcap.h capture-win-gl.cpp capture-win-gl.h \
  v4l2-tpg-colors.c v4l2-tpg-core.c v4l-stream.c v4l2-info.cpp codec-fwht.c codec-v4l2-fwht.c
nodist_qvidcap_SOURCES = qrc_qvidcap.cpp moc_capture-win-gl.cpp v4l2-convert.h
qvidcap_LDADD = ../../lib/libv4l2/libv4l2.la ../../lib/libv4lconvert/libv4lconvert.la ../libv4l2util/libv4l2util.la \
  ../libmedia_dev/libmedia_dev.la
qvidcap_CPPFLAGS = -I../common

qvidcap_CPPFLAGS += $(QTGL_CFLAGS)
qvidcap_LDFLAGS = $(QTGL_LIBS)

qvidcap_CPPFLAGS += $(ALSA_CFLAGS)
qvidcap_LDFLAGS += $(ALSA_LIBS) -pthread

EXTRA_DIST = qvidcap_24x24.png qvidcap_64x64.png qvidcap.png qvidcap.svg \
  qvidcap_16x16.png qvidcap_32x32.png qvidcap.desktop \
  qvidcap.qrc qvidcap.pro qvidcap.1 v4l2-convert.glsl v4l2-convert.pl

clean-local:
	-rm -vf moc_*.cpp qrc_*.cpp qrc_*.o ui_*.h v4l2-convert.h formats.h

v4l2-convert.h: v4l2-convert.glsl v4l2-convert.pl
	perl v4l2-convert.pl v4l2-convert.glsl >$@

capture-win-gl.cpp: v4l2-convert.h

moc_capture-win-gl.cpp: $(srcdir)/capture-win-gl.h
	$(AM_V_GEN)$(MOC) -o $@ $(srcdir)/capture-win-gl.h

# Call the Qt resource compiler
qrc_qvidcap.cpp: $(srcdir)/qvidcap.qrc
	$(AM_V_GEN)$(RCC) -name qvidcap -o $@ $(srcdir)/qvidcap.qrc

install-data-local:
	$(INSTALL_DATA) -D -p "$(srcdir)/qvidcap.desktop"   "$(DESTDIR)$(datadir)/applications/qvidcap.desktop"
	$(INSTALL_DATA) -D -p "$(srcdir)/qvidcap_16x16.png" "$(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/qvidcap.png"
	$(INSTALL_DATA) -D -p "$(srcdir)/qvidcap_24x24.png" "$(DESTDIR)$(datadir)/icons/hicolor/24x24/apps/qvidcap.png"
	$(INSTALL_DATA) -D -p "$(srcdir)/qvidcap_32x32.png" "$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/qvidcap.png"
	$(INSTALL_DATA) -D -p "$(srcdir)/qvidcap_64x64.png" "$(DESTDIR)$(datadir)/icons/hicolor/64x64/apps/qvidcap.png"
	$(INSTALL_DATA) -D -p "$(srcdir)/qvidcap.svg"       "$(DESTDIR)$(datadir)/icons/hicolor/scalable/apps/qvidcap.svg"
