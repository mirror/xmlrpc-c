# We need the following make variables to be defined by Makefile.config
# or be passed in:
#
# LIBWWW_CFLAGS, LIBWWW_LDADD, LIBWWW_LIBDIR, LIBWWW_RPATH, LIBWWW_WL_PATH:
#   As set by the 'libwww-config' program.  These tell how libwww is installed
#   on this system.

ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif

default: all

include $(SRCDIR)/Makefile.config

CXXFLAGS = 

CFLAGS = $(CFLAGS_COMMON) $(LIBWWW_CFLAGS) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = $(LADD)


INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/src

LIBEXPAT = $(SRCDIR)/lib/expat/xmlparse/libxmlrpc_xmlparse.la \
           $(SRCDIR)/lib/expat/xmltok/libxmlrpc_xmltok.la

ifneq ($(ENABLE_LIBXML2)x,x)
  LIBXML = $(LIBXML2_LIBS)
else
  LIBXML = $(LIBEXPAT)
endif

LIBXMLRPC = $(SRCDIR)/src/libxmlrpc.la
LIBXMLRPC_CPP = $(SRCDIR)/src/libxmlrpc_cpp.a
LIBXMLRPC_CLIENT = $(SRCDIR)/src/libxmlrpc_client.la
LIBXMLRPC_SERVER = $(SRCDIR)/src/libxmlrpc_server.la

XMLRPC_LIBS = $(LIBXMLRPC) $(LIBXMLRPC_CPP) $(LIBXMLRPC_CLIENT) \
	$(LIBXMLRPC_SERVER) $(LIBEXPAT)

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
	$(LIBTOOL) --mode=link $(CXXLD) -o $@ $(LDFLAGS) $^ $(LIBWWW_LDADD) 

%.o:%.cc
	$(LIBTOOL) --mode=compile $(CXX) -c $(INCLUDES) $(CFLAGS) $<

$(XMLRPC_LIBS):
	$(MAKE) -C $(dir $@) $(notdir $@)

include $(SRCDIR)/Makefile.common

include Makefile.depend

.PHONY: clean
clean: clean-common
	rm -f xml-rpc-api2cpp

.PHONY: distclean
distclean: clean

.PHONY: dep
dep: dep-common

BINDIR=$(DESTDIR)$(bindir)

FILENAME_GENERATOR = "echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'"

INSTCMD =  "$(LIBTOOL) --mode=install $(INSTALL_PROGRAM) $$p \
  $(BINDIR)/`$(FILENAME_GENERATOR)`"

.PHONY: install
install: $(PROGS)
	@$(NORMAL_INSTALL)
	$(MKINSTALLDIRS) $(BINDIR)
	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo "$(INSTCMD)"; $(INSTCMD); \
	  else :; \
          fi; \
	done

.PHONY: check
check:
