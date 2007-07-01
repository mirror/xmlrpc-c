ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  LIBDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/wininet_transport

include $(BLDDIR)/Makefile.config

default: all

.PHONY: all
all: xmlrpc_wininet_transport.lo

# Rules for the above dependencies are in Makefile.common,
# courtesy of TARGET_MODS.

TARGET_MODS = xmlrpc_wininet_transport

OMIT_WININET_TRANSPORT_RULE=Y

include $(SRCDIR)/Makefile.common

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

INCLUDES = -I$(BLDDIR) -I$(SRCDIR)/include

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

.PHONY: dep
dep: dep-common

include Makefile.depend

# Need this dependency for those who don't use Makefile.depend.
# Without it, version.h doesn't get created.
xmlrpc_wininet_transport.o xmlrpc_wininet_transport.osh: version.h
