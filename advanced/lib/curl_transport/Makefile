ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/curl_transport

include $(BLDDIR)/config.mk

default: all

MODS := xmlrpc_curl_transport curltransaction curlmulti

.PHONY: all
all: $(MODS:%=%.o) $(MODS:%=%.osh)

# Rules for the above dependencies are in common.mk,
# courtesy of TARGET_MODS.

TARGET_MODS = $(MODS)

OMIT_CURL_TRANSPORT_RULE=Y

include $(SRCDIR)/common.mk

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

CURL_VERSION := $(shell $(CURL_CONFIG) --vernum)

# Some time at or before Curl 7.12, <curl/types.h> became an empty file
# (no-op).  Some time after Curl 7.18, <curl/types.h> ceased to exist.
# curl-config --vernum yields a string of 6 digits; the first two are the
# major version in hexadecimal; the second two are the minor version number
# and so on.

# Version 071000 means 7.16.0.
NEED_CURL_TYPES_H := $(shell if test $(CURL_VERSION) '<' 071000; then echo Y; fi)

ifeq ($(NEED_CURL_TYPES_H),Y)
  CFLAGS_LOCAL += -DNEED_CURL_TYPES_H
endif

# Version 074100 means 7.65.0
HAVE_CURL_MULTI_POLL := $(shell if test $(CURL_VERSION) '>' 074100; then echo Y; fi)

ifeq ($(HAVE_CURL_MULTI_POLL),Y)
  CFLAGS_LOCAL += -DHAVE_CURL_MULTI_POLL
endif

# Version 071F00 means 7.31.0
HAVE_CURL_XFERINFOFUNCTION := $(shell if test $(CURL_VERSION) '>' 071F00; then echo Y; fi)

ifeq ($(HAVE_CURL_XFERINFOFUNCTION),Y)
  CFLAGS_LOCAL += -DHAVE_CURL_XFERINFOFUNCTION
endif

# Version 075400 means 7.84.0
CURL_DOES_OLD_OPENSSL := $(shell if test $(CURL_VERSION) '<' 075400; then echo Y; fi)

ifeq ($(CURL_DOES_OLD_OPENSSL),Y)
  CFLAGS_LOCAL += -DCURL_DOES_OLD_OPENSSL
endif

CURL_INCLUDES := $(shell $(CURL_CONFIG) --cflags)
# We expect that curl-config --cflags just gives us -I options, because
# we need just the -I options for 'make dep'.  Plus, it's scary to think
# of what any other compiler flag would do to our compile.

INCLUDES = \
  -I$(BLDDIR) \
  -I$(BLDDIR)/include \
  -Isrcdir/include \
  -Isrcdir/lib/util/include \
  $(CURL_INCLUDES)

.PHONY: clean
clean: clean-common

.PHONY: distclean
distclean: clean distclean-common

.PHONY: tags
tags: TAGS

.PHONY: distdir
distdir:

.PHONY: install
install:

.PHONY: uninstall
uninstall:

.PHONY: dep
dep: dep-common

include depend.mk

# Need this dependency for those who don't use depend.mk.
# Without it, version.h doesn't get created.
xmlrpc_curl_transport.o xmlrpc_curl_transport.osh: version.h
