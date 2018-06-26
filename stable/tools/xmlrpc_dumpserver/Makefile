ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  TOOLSDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(TOOLSDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := tools/xmlrpc_dumpserver

default: all

include $(BLDDIR)/config.mk

PROGS := xmlrpc_dumpserver

PROGRAMS_TO_INSTALL = $(PROGS)

include $(SRCDIR)/tools/common.mk

INCLUDES = \
  -I../lib/include \
  -Iblddir \
  -Iblddir/include \
  -Isrcdir/include \
  -Isrcdir/lib/util/include

all: xmlrpc_dumpserver

# cmdline.parser.o references getoptx.o and string_parser.o.
# string_parser.o references stripcaseeq.o

UTIL_OBJS = \
  casprintf.o \
  cmdline_parser.o \
  getoptx.o \
  string_parser.o \
  stripcaseeq.o \

UTILS = $(UTIL_OBJS:%=$(UTIL_DIR)/%)

DUMPVALUE = blddir/tools/lib/dumpvalue.o

XMLRPC_OBJS = xmlrpc_dumpserver.o $(DUMPVALUE)

xmlrpc_dumpserver: $(XMLRPC_OBJS) $(SERVER_ABYSS_LIBS_DEP) $(UTILS)
	$(CCLD) -o $@ $(LDFLAGS_ALL) $(XMLRPC_OBJS) $(UTILS) \
	  $(SERVER_ABYSS_LDLIBS)

%.o:%.c $(BLDDIR)/include/xmlrpc-c/config.h
	$(CC) -c $(CFLAGS_ALL) $<

# This common.mk dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/tools/common.mk: srcdir blddir

include depend.mk

.PHONY: dep
dep: dep-common

.PHONY: install
install: install-common

.PHONY: uninstall
uninstall: uninstall-common

.PHONY: clean
clean: clean-common
	rm -f $(PROGS)

.PHONY: distclean
distclean: clean distclean-common
