ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif

default: all

include $(SRCDIR)/Makefile.config

include ../Makefile.common

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/src

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS = $(CLIENT_LDFLAGS) $(LADD)

all: xml-rpc-api2cpp

OBJECTS = \
  xml-rpc-api2cpp.o \
  DataType.o \
  XmlRpcFunction.o \
  XmlRpcClass.o \
  SystemProxy.o \

xml-rpc-api2cpp: \
  $(OBJECTS) \
  $(LIBXMLRPC_CLIENT) \
  $(LIBXMLRPC_SERVER) \
  $(LIBXMLRPC) \
  $(LIBXMLRPC_CPP) \
  $(LIBXML)
	$(LIBTOOL) --mode=link $(CXXLD) -o $@ $(LDFLAGS) $^

%.o:%.cc
	$(LIBTOOL) --mode=compile $(CXX) -c $(CFLAGS) $<

include Makefile.depend

.PHONY: clean
clean: clean-common
	rm -f xml-rpc-api2cpp

.PHONY: distclean
distclean: clean distclean-common

.PHONY: dep
dep: dep-common
