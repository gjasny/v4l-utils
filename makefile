.PHONY: default configure distclean

ifeq ($(wildcard configure),)
default: configure
else
default: all
endif

configure:
	@set -ex; autoconf
	@rm -rf autom4te.cache
	@echo "*** You should now run ./configure or make ***"
distclean:
	-$(MAKE) -f Makefile clean
	-rm Makefile Make.rules configure config.h v4l-utils.spec

-include Makefile

ifeq ($(wildcard Makefile),)
all:
	./configure
	@$(MAKE) -f Makefile all
endif
