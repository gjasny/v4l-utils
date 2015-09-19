gconvdir = $(libdir)/gconv
gconv_LTLIBRARIES = ARIB-STD-B24.la EN300-468-TAB00.la
gconv_DATA = gconv-modules

gconv_ldflags = -module -shared -avoid-version -no-install --version-script=gconv.map

gconv_base_sources = iconv/skeleton.c iconv/loop.c

arib-std-b24.c, en300-468-tab00.c: $(gconv_base_sources)

ARIB_STD_B24_la_SOURCES = arib-std-b24.c jis0201.h jis0208.h jisx0213.h
ARIB_STD_B24_la_LDFLAGS = $(gconv_ldflags) -L@gconvsysdir@ -R @gconvsysdir@ -lJIS -lJISX0213

EN300_468_TAB00_la_SOURCES = en300-468-tab00.c
EN300_468_TAB00_la_LDFLAGS = $(gconv_ldflags)

EXTRA_DIST = $(gconv_base_sources) $(gconv_DATA) gconv.map
