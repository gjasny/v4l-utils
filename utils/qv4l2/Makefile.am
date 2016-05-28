bin_PROGRAMS = qv4l2
man_MANS = qv4l2.1

qv4l2_SOURCES = qv4l2.cpp general-tab.cpp ctrl-tab.cpp vbi-tab.cpp capture-win.cpp tpg-tab.cpp \
  capture-win-qt.cpp capture-win-qt.h capture-win-gl.cpp capture-win-gl.h alsa_stream.c alsa_stream.h \
  raw2sliced.cpp qv4l2.h capture-win.h general-tab.h vbi-tab.h raw2sliced.h \
  v4l2-tpg-core.c v4l2-tpg-colors.c
nodist_qv4l2_SOURCES = moc_qv4l2.cpp moc_general-tab.cpp moc_capture-win.cpp moc_vbi-tab.cpp qrc_qv4l2.cpp
qv4l2_LDADD = ../../lib/libv4l2/libv4l2.la ../../lib/libv4lconvert/libv4lconvert.la \
  ../libv4l2util/libv4l2util.la ../libmedia_dev/libmedia_dev.la
qv4l2_CPPFLAGS = -I../common

if WITH_QTGL
qv4l2_CPPFLAGS += $(QTGL_CFLAGS)
qv4l2_LDFLAGS = $(QTGL_LIBS)
else
qv4l2_CPPFLAGS += $(QT_CFLAGS)
qv4l2_LDFLAGS = $(QT_LIBS)
endif

qv4l2_CPPFLAGS += $(ALSA_CFLAGS)
qv4l2_LDFLAGS += $(ALSA_LIBS) -pthread

EXTRA_DIST = enterbutt.png exit.png fileopen.png qv4l2_24x24.png qv4l2_64x64.png qv4l2.png qv4l2.svg \
  snapshot.png video-television.png fileclose.png qv4l2_16x16.png qv4l2_32x32.png qv4l2.desktop \
  qv4l2.qrc saveraw.png qv4l2.pro qv4l2.1 fullscreenexit.png fullscreen.png start.png step.png

clean-local:
	-rm -vf moc_*.cpp qrc_*.cpp qrc_*.o ui_*.h

# Call the moc preprocessor
moc_qv4l2.cpp: $(srcdir)/qv4l2.h
	$(AM_V_GEN)$(MOC) -o $@ $(srcdir)/qv4l2.h

moc_general-tab.cpp: $(srcdir)/general-tab.h
	$(AM_V_GEN)$(MOC) -o $@ $(srcdir)/general-tab.h

moc_vbi-tab.cpp: $(srcdir)/vbi-tab.h
	$(AM_V_GEN)$(MOC) -o $@ $(srcdir)/vbi-tab.h

moc_capture-win.cpp: $(srcdir)/capture-win.h
	$(AM_V_GEN)$(MOC) -o $@ $(srcdir)/capture-win.h

# Call the Qt resource compiler
qrc_qv4l2.cpp: $(srcdir)/qv4l2.qrc
	$(AM_V_GEN)$(RCC) -name qv4l2 -o $@ $(srcdir)/qv4l2.qrc

install-data-local:
	$(INSTALL_DATA) -D -p "$(srcdir)/qv4l2.desktop"   "$(DESTDIR)$(datadir)/applications/qv4l2.desktop"
	$(INSTALL_DATA) -D -p "$(srcdir)/qv4l2_16x16.png" "$(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/qv4l2.png"
	$(INSTALL_DATA) -D -p "$(srcdir)/qv4l2_24x24.png" "$(DESTDIR)$(datadir)/icons/hicolor/24x24/apps/qv4l2.png"
	$(INSTALL_DATA) -D -p "$(srcdir)/qv4l2_32x32.png" "$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/qv4l2.png"
	$(INSTALL_DATA) -D -p "$(srcdir)/qv4l2_64x64.png" "$(DESTDIR)$(datadir)/icons/hicolor/64x64/apps/qv4l2.png"
	$(INSTALL_DATA) -D -p "$(srcdir)/qv4l2.svg"       "$(DESTDIR)$(datadir)/icons/hicolor/scalable/apps/qv4l2.svg"
