ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../../..
endif
SUBDIR = src/test/cpp
# BLDDIR is for use in places where a symbolic link won't work.
# BUILDDIR is for places in Makefile.common that can use the 'blddir'
# symbolic link (but in other directories, doesn't).
BLDDIR = ../../..
BUILDDIR = blddir
VPATH = .:$(SRCDIR)

include $(BLDDIR)/Makefile.config

PROGS = test

default: all

all: $(PROGS)

ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  LDFLAGS += $(shell curl-config --libs)
endif
ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  LDFLAGS += $(shell libwww-config --libs)
endif

LDFLAGS += $(LADD)

INCLUDES = -Isrcdir/include -Iblddir -Isrcdir -Isrcdir/lib/util/include

# This 'Makefile' dependency makes sure the symlinks get built before
# this make file is used for anything.

Makefile: blddir srcdir

include $(SRCDIR)/Makefile.common

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML = $(LIBXML2_LIBS)
else
  LIBXML = blddir/lib/expat/xmlparse/libxmlrpc_xmlparse.la \
           blddir/lib/expat/xmltok/libxmlrpc_xmltok.la
endif

test:%:%.o $(LIBXMLRPC_SERVER_ABYSS++) $(LIBXMLRPC_SERVER++) \
  $(LIBXMLRPC_CLIENT++) $(LIBXMLRPC++) $(LIBXMLRPC_CPP) \
  $(LIBXMLRPC_SERVER) $(LIBXMLRPC_CLIENT) $(LIBXMLRPC) $(LIBXMLRPC_UTIL) \
  $(LIBXML)
	$(LIBTOOL) --mode=link $(CXXLD) -o $@ $(LDFLAGS) $^

%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS) $<

check: test
	./test

.PHONY: install
install:

.PHONY: clean clean-local distclean
clean: clean-common clean-local
clean-local:
	rm -f $(PROGS)

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

include Makefile.depend
