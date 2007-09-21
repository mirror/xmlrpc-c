ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/..
endif
SUBDIR = lib

include $(SRCDIR)/Makefile.config

SUBDIRS =
ifeq ($(ENABLE_ABYSS_SERVER),yes)
  SUBDIRS += abyss
endif
ifeq ($(ENABLE_WININET_CLIENT),yes)
  SUBDIRS += wininet_transport
endif
ifeq ($(ENABLE_CURL_CLIENT),yes)
  SUBDIRS += curl_transport
endif
ifeq ($(ENABLE_LIBWWW_CLIENT),yes)
  SUBDIRS += libwww_transport
endif
ifneq ($(ENABLE_LIBXML2),yes)
  SUBDIRS += expat
endif

default: all

.PHONY: all
all: $(SUBDIRS:%=%/all)

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

include $(SRCDIR)/Makefile.common



