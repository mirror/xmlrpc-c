ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/xmlrpc

default: all

include $(BLDDIR)/Makefile.config

PROGRAMS_TO_INSTALL = xmlrpc

include $(SRCDIR)/tools/Makefile.common

INCLUDES = \
  -I$(BLDDIR) \
  -I$(BLDDIR)/include \
  -I$(SRCDIR)/include \
  -I$(SRCDIR)/lib/util/include

CFLAGS = $(CFLAGS_COMMON) $(INCLUDES) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(LADD)

all: xmlrpc

UTIL_OBJS = cmdline_parser.o getoptx.o casprintf.o
UTILS = $(UTIL_OBJS:%=$(UTIL_DIR)/%)

# These are the Libtool .la files.  We use them only for make dependencies.
# We'd like to use these in the link rule, by using libtool --link, but
# Libtool adds -l options to the link to cover the dependencies that are
# recorded inside the shared libraries.  And it doesn't add the necessary
# -L options (it can't), so we end up with a messy mixture of the two
# forms of specifying libraries.

LIBS = $(LIBXMLRPC_CLIENT) $(LIBXMLRPC) $(LIBXMLRPC_XML) $(LIBXMLRPC_UTIL)

XMLRPC_OBJS = xmlrpc.o dumpvalue.o

xmlrpc: $(XMLRPC_OBJS) $(LIBS) $(UTILS)
	$(CCLD) -o $@ $(LDFLAGS) $(XMLRPC_OBJS) $(UTILS) $(CLIENT_LDLIBS)

%.o:%.c $(BLDDIR)/include/xmlrpc-c/config.h
	$(CC) -c $(CFLAGS) $<

# This Makefile.config dependency makes sure the symlinks get built before
# this make file is used for anything.

$(BLDDIR)/Makefile.config: srcdir

include Makefile.depend

.PHONY: dep
dep: dep-common

.PHONY: clean
clean: clean-common
	rm -f xmlrpc

.PHONY: distclean
distclean: clean distclean-common
