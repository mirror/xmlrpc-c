ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)
endif
SUBDIR = .

include $(SRCDIR)/Makefile.config

SUBDIRS = src lib tools examples

# We're in a transition between the bloated, complex GNU
# Autoconf/Automake style of build, in which 'configure' creates all
# the make files, to simpler static make files.  Some directories have
# been converted; some haven't.  So we have the hack of putting
# 'xmlrpc_config.h' as the first dependency of 'all' to make sure
# 'configure runs before anything in the case that the user neglects
# to run 'configure' before doing 'make'.

default: xmlrpc_config.h all

.PHONY: all
all: xmlrpc-c-config xmlrpc-c-config.test $(SUBDIRS:%=%/all)

.PHONY: clean
clean: $(SUBDIRS:%=%/clean) clean-common

.PHONY: distclean
distclean: $(SUBDIRS:%=%/distclean) distclean-common

.PHONY: tags
tags: $(SUBDIRS:%=%/tags) TAGS

DISTFILES = 

.PHONY: distdir
distdir: distdir-common

.PHONY: install
install: $(SUBDIRS:%=%/install)

.PHONY: dep
dep: $(SUBDIRS:%=%/dep)

xmlrpc-c-config xmlrpc-c-config.test xmlrpc_config.h xmlrpc_amconfig.h \
	:%:%.in $(SRCDIR)/configure
	$(SRCDIR)/configure

include $(SRCDIR)/Makefile.common

