ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  EXPATDIR := $(call updir,$(CURDIR))
  LIBDIR := $(call updir,$(EXPATDIR))
  SRCDIR := $(call updir,$(LIBDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := lib/expat/xmlparse

XMLTOKDIR = $(SRCDIR)/lib/expat/xmltok
UTILDIR = $(SRCDIR)/lib/util

include $(BLDDIR)/Makefile.config

default: all

all: libxmlrpc_xmlparse.la

LIBXMLRPC_XMLPARSE_OBJS = xmlparse.lo

INCLUDES = -I$(BLDDIR) -I$(UTILDIR)/include -I$(XMLTOKDIR)

LIBLDFLAGS = $(LDFLAGS_VERSINFO) -L../xmltok/.libs $(RPATH) $(LADD)

libxmlrpc_xmlparse.la: $(LIBXMLRPC_XMLPARSE_OBJS) $(LIBXMLRPC_XMLTOK)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) \
	  $(LIBXMLRPC_XMLPARSE_OBJS) -lxmlrpc_xmltok

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

$(LIBXMLRPC_XMLPARSE_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(CFLAGS) $<

LTLIBRARIES_TO_INSTALL = libxmlrpc_xmlparse.la

.PHONY: install
install: install-common

.PHONY: clean distclean
clean: clean-common

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

# This 'Makefile.common' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/Makefile.common: srcdir

OMIT_XMLPARSE_LIB_RULE=Y

include $(SRCDIR)/Makefile.common

include Makefile.depend
