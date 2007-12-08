ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCDIR := $(call updir,$(CURDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools

include $(BLDDIR)/Makefile.config

SUBDIRS = binmode-rpc-kit turbocharger

ifeq ($(MUST_BUILD_CLIENT),yes)
  SUBDIRS += xmlrpc xmlrpc_transport

  ifeq ($(ENABLE_CPLUSPLUS),yes)
    SUBDIRS += xml-rpc-api2cpp xmlrpc_cpp_proxy
  endif
endif

.PHONY: all clean distclean install check dep

all: $(SUBDIRS:%=%/all)

clean: $(SUBDIRS:%=%/clean)

distclean: $(SUBDIRS:%=%/distclean)

install: $(SUBDIRS:%=%/install)

check:

dep: $(SUBDIRS:%=%/dep)

include $(SRCDIR)/Makefile.common
