ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCDIR := $(call updir,$(CURDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := src

include $(BLDDIR)/Makefile.config

default: all

SUBDIRS =

ifeq ($(ENABLE_CPLUSPLUS),yes)
  SUBDIRS += cpp
endif

OMIT_XMLRPC_LIB_RULE=Y

include $(SRCDIR)/Makefile.common

# TRANSPORT_OBJS is the list of modules that have to go into the client
# library to provide the client XML transport functions.  Since there is
# some variability in what XML transports we can build into the client
# library, this is a little complicated.

# TRANSPORT_LIBDEP is linker -l options to declare what libraries contain
# things to which the transport objects refer.  (like LIBxxx_LIBDEP --
# see below)

TRANSPORT_OBJS =
TRANSPORT_LIBDEP =
ifeq ($(MUST_BUILD_WININET_CLIENT),yes)
  TRANSPORT_OBJS += $(BLDDIR)/lib/wininet_transport/xmlrpc_wininet_transport.lo
  TRANSPORT_LIBDEP += $(shell wininet-config --libs)
endif
ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  TRANSPORT_OBJS += $(BLDDIR)/lib/curl_transport/xmlrpc_curl_transport.lo
  TRANSPORT_LIBDEP += $(shell curl-config --libs)
endif
ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  TRANSPORT_OBJS += $(BLDDIR)/lib/libwww_transport/xmlrpc_libwww_transport.lo
  TRANSPORT_LIBDEP += $(shell libwww-config --libs)
endif

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML_INCLUDES = $(shell xml2-config --cflags)
else
  LIBXML_INCLUDES = -I$(SRCDIR)/lib/expat/xmlparse
endif

ABYSS_INCLUDES = -I$(SRCDIR)/lib/abyss/src

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  XMLRPC_XML_PARSER = xmlrpc_libxml2.lo
  XML_PARSER_LIBDEP = $(shell xml2-config --libs)
  XML_PARSER_LIBDEP_DEP =
else
  XMLRPC_XML_PARSER = xmlrpc_expat.lo
  XML_PARSER_LIBDEP = -L$(BLDDIR)/lib/expat/xmlparse/.libs -lxmlrpc_xmlparse
  XML_PARSER_LIBDEP_DEP = $(LIBXMLRPC_XMLPARSE)
endif

# LIBxxx_OBJS is the list of object files that make up library libxxx.

# LIBxxx_LIBDEP is linker -l options to declare what libraries contain
# things to which the library being built refers.  This information
# makes its way to the dynamic linker, so it knows to load the
# referred-to library before it loads libxxx.  Note that the link command
# may require the necessary -L options in addition.

# LIBxxx_LIBDEP_DEP is the actual files that the linker will access
# when it resolves the LIBxxx_LIBDEP linker options.  The build of
# library LIBxxx depends upon those files existing.  For example, if
# LIBxxx_LIBDEP contains "-L ... -lfoo", LIBxxx_LIBDEP_DEP might contain
# ".../libfoo.so".

LIBXMLRPC_CLIENT_OBJS = xmlrpc_client.lo xmlrpc_client_global.lo
LIBXMLRPC_CLIENT_LIBDEP = -lxmlrpc_util -lxmlrpc
LIBXMLRPC_CLIENT_LIBDEP_DEP = $(LIBXMLRPC_UTIL) $(LIBXMLRPC)

LIBXMLRPC_SERVER_OBJS = registry.lo method.lo system_method.lo
LIBXMLRPC_SERVER_LIBDEP = -lxmlrpc_util -lxmlrpc
LIBXMLRPC_SERVER_LIBDEP_DEP = $(LIBXMLRPC_UTIL) $(LIBXMLRPC)

LIBXMLRPC_SERVER_ABYSS_OBJS = xmlrpc_server_abyss.lo
LIBXMLRPC_SERVER_ABYSS_LIBDEP = \
  -lxmlrpc_util -lxmlrpc_server -L$(LIBXMLRPC_ABYSS_DIR) -lxmlrpc_abyss \
  -lxmlrpc
LIBXMLRPC_SERVER_ABYSS_LIBDEP_DEP = \
  $(LIBXMLRPC_UTIL) $(LIBXMLRPC_SERVER) $(LIBXMLRPC_ABYSS) \
  $(LIBXMLRPC)

LIBXMLRPC_SERVER_CGI_OBJS = xmlrpc_server_cgi.lo
LIBXMLRPC_SERVER_CGI_LIBDEP = -lxmlrpc_util -lxmlrpc_server -lxmlrpc
LIBXMLRPC_SERVER_CGI_LIBDEP_DEP = \
  $(LIBXMRPC_UTIL) $(LIBXMLRPC_SERVER) $(LIBXMLRPC)

LIBXMLRPC_OBJS = \
        resource.lo \
	trace.lo \
	xmlrpc_data.lo \
	xmlrpc_datetime.lo \
	xmlrpc_array.lo \
	xmlrpc_struct.lo \
	xmlrpc_build.lo \
	xmlrpc_decompose.lo \
	$(XMLRPC_XML_PARSER) \
	xmlrpc_parse.lo \
	xmlrpc_serialize.lo \
	xmlrpc_base64.lo \
	xmlrpc_utf8.lo \
	xmlrpc_authcookie.lo \

LIBXMLRPC_LIBDEP = -lxmlrpc_util $(XML_PARSER_LIBDEP)
LIBXMLRPC_LIBDEP_DEP = $(LIBXMLRPC_UTIL) $(XML_PARSER_LIBDEP_DEP)

LIB_OBJS = $(LIBXMLRPC_CLIENT_OBJS) $(LIBXMLRPC_SERVER_OBJS)

TRANSPORT_INCLUDES = \
  -I$($SRCDIR)/lib/wininet_transport \
  -I$(SRCDIR)/lib/curl_transport \
  -I$(SRCDIR)/lib/libwww_transport \

INCLUDES = \
  -I$(BLDDIR) \
  -I$(BLDDIR)/include \
  -I$(SRCDIR)/include \
  -I$(SRCDIR)/lib/util/include \
  -I$(SRCDIR)/lib/abyss/src \
  $(TRANSPORT_INCLUDES) \
  $(LIBXML_INCLUDES) \

TARGET_LTLIBRARIES = libxmlrpc.la libxmlrpc_server.la

# We cannot build libxmlrpc_server_abyss if we can't also build Abyss,
# because we need libxmlrpc_abyss in order to build the proper runtime
# dependencies into libxmlrpc_server_abyss.

ifeq ($(ENABLE_ABYSS_SERVER),yes)
  TARGET_LTLIBRARIES += libxmlrpc_server_abyss.la
endif

ifeq ($(MUST_BUILD_CLIENT),yes)
  TARGET_LTLIBRARIES += libxmlrpc_client.la
endif

ifeq ($(ENABLE_CGI_SERVER),yes)
  TARGET_LTLIBRARIES += libxmlrpc_server_cgi.la
endif

all: $(TARGET_LTLIBRARIES) $(SUBDIRS:%=%/all)

# Extra dependencies to make parallel make work in spite of all the submakes
# (See top level make file for details)
ifeq ($MUST_BUILD_CLIENT),yes)
cpp/all: $(BLDDIR)/transport_config.h
endif

LDFLAGS = $(LADD)

LIBPATHS = -L.libs -L$(LIBXMLRPC_UTIL_DIR)

LIBLDFLAGS = $(LDFLAGS_VERSINFO) $(RPATH) $(LIBPATHS) $(LADD)


libxmlrpc.la: $(LIBXMLRPC_OBJS) $(LIBXMLRPC_LIBDEP_DEP)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) \
	  $(LIBXMLRPC_OBJS) $(LIBXMLRPC_LIBDEP) 

libxmlrpc_client.la: \
  $(LIBXMLRPC_CLIENT_OBJS) $(TRANSPORT_OBJS) $(LIBXMLRPC_CLIENT_LIBDEP_DEP)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) \
	  $(LIBXMLRPC_CLIENT_OBJS) $(TRANSPORT_OBJS) \
	  $(LIBXMLRPC_CLIENT_LIBDEP) $(TRANSPORT_LIBDEP) \

libxmlrpc_server.la: $(LIBXMLRPC_SERVER_OBJS) $(LIBXMLRPC_SERVER_LIBDEP_DEP)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) \
	  $(LIBXMLRPC_SERVER_OBJS) $(LIBXMLRPC_SERVER_LIBDEP)

libxmlrpc_server_abyss.la: \
  $(LIBXMLRPC_SERVER_ABYSS_OBJS) $(LIBXMLRPC_SERVER_ABYSS_LIBDEP_DEP)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) \
	  $(LIBXMLRPC_SERVER_ABYSS_OBJS) $(LIBXMLRPC_SERVER_ABYSS_LIBDEP)

libxmlrpc_server_cgi.la: \
  $(LIBXMLRPC_SERVER_CGI_OBJS) $(LIBXMLRPC_SERVER_CGI_LIBDEP_DEP)
	$(LIBTOOL) --mode=link $(CCLD) -o $@ $(LIBLDFLAGS) \
	  $(LIBXMLRPC_SERVER_CGI_OBJS) $(LIBXMLRPC_SERVER_CGI_LIBDEP)


CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

$(LIBXMLRPC_CLIENT_OBJS) $(LIBXMLRPC_SERVER_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(CFLAGS) $<

$(LIBXMLRPC_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(LIBXML_INCLUDES) \
	  $(CFLAGS) $<

$(LIBXMLRPC_SERVER_ABYSS_OBJS):%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) $(ABYSS_INCLUDES) \
	  $(CFLAGS) $<

xmlrpc_server_cgi.lo:%.lo:%.c
	$(LIBTOOL) --mode=compile $(CC) -c $(INCLUDES) \
	  $(CFLAGS) $<

# A built header virtually everyone needs:
$(LIB_OBJS): $(BLDDIR)/include/xmlrpc-c/config.h

LTLIBRARIES_TO_INSTALL = $(TARGET_LTLIBRARIES)

check:
	$(MAKE) -C test runtests
	$(MAKE) -C cpp  check

.PHONY: install
install: install-common $(SUBDIRS:%=%/install)

.PHONY: clean clean-local distclean
clean: clean-common clean-local $(SUBDIRS:%=%/clean)
clean-local:
	$(MAKE) -C test clean

distclean: clean-common clean-local distclean-common $(SUBDIRS:%=%/distclean)

.PHONY: dep
dep: $(SUBDIRS:%=%/dep) $(BLDDIR)/transport_config.h dep-common

xmlrpc_client.lo: $(BLDDIR)/transport_config.h

include Makefile.depend
