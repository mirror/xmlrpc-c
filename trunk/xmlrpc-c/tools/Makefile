SUBDIR=tools
ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/..
BUILDDIR = $(SRCDIR)
endif

include $(SRCDIR)/Makefile.config

SUBDIRS = binmode-rpc-kit turbocharger xml-rpc-api2cpp xmlrpc

.PHONY: all clean install

all: $(SUBDIRS:%=%/all)

clean: $(SUBDIRS:%=%/all)

install: $(SUBDIRS:%=%/all)

dep: $(SUBDIRS:%=%/dep)

include $(SRCDIR)/Makefile.common
