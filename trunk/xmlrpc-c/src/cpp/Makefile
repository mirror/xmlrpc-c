ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif
SUBDIR = src/cpp
BUILDDIR = $(SRCDIR)
VPATH = .:$(SRCDIR)

include $(BUILDDIR)/Makefile.config

default: all

TESTDATADIR = $(SRCDIR)/src/testdata

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML_INCLUDES = $(LIBXML2_CFLAGS)
  LIBXML = $(LIBXML2_LIBS)
else
  LIBXML_INCLUDES = -I$(SRCDIR)/lib/expat/xmlparse
  LIBXML = $(BUILDDIR)/lib/expat/xmlparse/libxmlrpc_xmlparse.la \
           $(BUILDDIR)/lib/expat/xmltok/libxmlrpc_xmltok.la
endif

ifeq ($(ENABLE_LIBXML2),yes)
  XMLRPC_XML_PARSER = xmlrpc_libxml2.lo
else
  XMLRPC_XML_PARSER = xmlrpc_expat.lo
endif

LIBXMLRPC = $(BUILDDIR)/src/libxmlrpc.la
LIBXMLRPC_SERVER = $(BUILDDIR)/src/libxmlrpc_server.la

LIBXMLRPCPP_OBJS = value.o
LIBXMLRPC_SERVERPP_OBJS = registry.o server_abyss.o

INCLUDES = -I $(BUILDDIR)/src -I$(BUILDDIR) -I$(SRCDIR) \
	   -I$(SRCDIR)/lib/util/include \
	   $(LIBXML_INCLUDES) -I$(SRCDIR)/lib/abyss/src \

# We always statically-link our C++ code because it's tiny, and C++ is
# no good at shared libraries, anyway.  It's actually getting bigger,
# but since I'm determined not to expand the use of libtool (because
# it obfuscates the build process), I haven't figured out yet how to
# get the shared libraries to build for all the platforms.

# libxmlrpc_cpp.a is the legacy C++ wrapper library.  The others are the
# more elaborate replacements.

TARGET_LIBRARIES = libxmlrpc_cpp.a libxmlrpc++.a libxmlrpc_server++.a

TARGET_PROGS = cpptest

all: $(TARGET_LIBRARIES) $(TARGET_PROGS)

LDFLAGS = $(LADD)

cpptest:%:%.o libxmlrpc++.a libxmlrpc_server++.a libxmlrpc_cpp.a \
  $(LIBXMLRPC) $(LIBXMLRPC_SERVER) $(LIBXML)
	$(LIBTOOL) --mode=link $(CXXLD) -o $@ $(LDFLAGS) $^

LIBLDFLAGS = $(LADD)

libxmlrpc_cpp.a: XmlRpcCpp.o
	-rm -f $@
	$(AR) cru $@ $^
	$(RANLIB) $@

libxmlrpc++.a: $(LIBXMLRPCPP_OBJS)
	-rm -f $@
	$(AR) cru $@ $^
	$(RANLIB) $@

libxmlrpc_server++.a: $(LIBXMLRPC_SERVERPP_OBJS)
	-rm -f $@
	$(AR) cru $@ $^
	$(RANLIB) $@

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

cpptest.o:%.o:%.cpp
	$(CXX) -c $(INCLUDES) -DTESTDATADIR=\"$(TESTDATADIR)\" $(CFLAGS) $<

XmlRpcCpp.o:%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CFLAGS) $<

$(LIBXMLRPCPP_OBJS):%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CFLAGS) $<

$(LIBXMLRPC_SERVERPP_OBJS):%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CFLAGS) $<

LIBRARIES_TO_INSTALL = $(TARGET_LIBRARIES)
HEADERS_TO_INSTALL = xmlrpc.hpp xmlrpc_server.hpp \
		     XmlRpcCpp.h

check: cpptest
	for tst in $^; do \
	  ./$$tst; \
	done;

.PHONY: install
install: install-common

.PHONY: clean clean-local distclean
clean: clean-common clean-local
clean-local:
	rm -f transport_config.h
	rm -f $(TARGET_PROGS)

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

include $(SRCDIR)/Makefile.common

include Makefile.depend
