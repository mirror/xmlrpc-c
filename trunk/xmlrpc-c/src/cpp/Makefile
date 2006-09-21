ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  srcDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(srcDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := src/cpp

include $(BLDDIR)/Makefile.config

default: all

# libxmlrpc_cpp is the legacy C++ wrapper library.  The others are the
# more elaborate replacements.

TARGET_LIB_NAMES_PP := \
  libxmlrpc_cpp \
  libxmlrpc++ \
  libxmlrpc_server++ \
  libxmlrpc_server_abyss++ \
  libxmlrpc_client++ \

STATIC_LIBRARIES_TO_INSTALL = $(TARGET_STATIC_LIBRARIES)

SHARED_LIBS_TO_BUILD := $(TARGET_LIB_NAMES_PP)
SHARED_LIBS_TO_INSTALL := $(TARGET_LIB_NAMES_PP)

# INCLUDES and DEP_SOURCES are used by dep-common target
INCLUDES = $(BASIC_INCLUDES) $(CLIENT_INCLUDES) $(LIBXML_INCLUDES) \
  $(SERVER_INCLUDES) $(SERVER_ABYSS_INCLUDES) $(TRANSPORT_INCLUDES)
DEP_SOURCES = *.cpp

WININET_TRANSPORT_DIR = srcdir/lib/wininet_transport
CURL_TRANSPORT_DIR    = srcdir/lib/curl_transport
LIBWWW_TRANSPORT_DIR  = srcdir/lib/libwww_transport

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML_INCLUDES = $(LIBXML2_CFLAGS)
else
  LIBXML_INCLUDES = -Isrcdir/lib/expat/xmlparse
endif

LIBXMLRPCPP_OBJS = \
	base64.o \
	env_wrap.o \
	fault.o \
	girerr.o \
	girmem.o \
	outcome.o \
	param_list.o \
	value.o \
	xml.o \

LIBXMLRPC_SERVERPP_OBJS = registry.o
LIBXMLRPC_SERVER_ABYSSPP_OBJS = server_abyss.o
LIBXMLRPC_CLIENTPP_OBJS = client.o client_simple.o curl.o libwww.o wininet.o

ALL_OBJS = \
  XmlRpcCpp.o \
  $(LIBXMLRPCCPP_OBJS) \
  $(LIBXMLRPC_SERVERPP_OBJS) \
  $(LIBXMLRPC_SERVER_ABYSSPP_OBJS) \
  $(LIBXMLRPC_CLIENTPP_OBJS)

include $(SRCDIR)/Makefile.common


# This 'Makefile.common' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/Makefile.common: srcdir blddir

TRANSPORT_INCLUDES = \
  -I$(WININET_TRANSPORT_DIR) \
  -I$(CURL_TRANSPORT_DIR) \
  -I$(LIBWWW_TRANSPORT_DIR) \

BASIC_INCLUDES = -Isrcdir/include -Iblddir -Isrcdir \
  -Isrcdir/lib/util/include

# LIBXMLRPC_CPP_SHLIB is e.g. "libxmlrpc_cpp.so.3.1"

LIBXMLRPC_CPP_SHLIB      = $(call shlibfn, libxmlrpc_cpp)
LIBXMLRPCPP_SHLIB        = $(call shlibfn, libxmlrpc++)
LIBXMLRPC_SERVERPP       = $(call shlibfn, libxmlrpc_server++)
LIBXMLRPC_SERVER_ABYSSPP = $(call shlibfn, libxmlrpc_server_abyss++)
LIBXMLRPC_CLIENTPP       = $(call shlibfn, libxmlrpc_client++)


all: \
  $(TARGET_STATIC_LIBRARIES) \
  $(TARGET_SHARED_LIBS_PP) \
  $(TARGET_SHARED_LE_LIBS)


# Rules for these are in Makefile.common, courtesy of TARGET_SHARED_LIBS_PP:

$(LIBXMLRPC_CPP_SHLIB): XmlRpcCpp.o
$(LIBXMLRPC_CPP_SHLIB): OBJECTS = XmlRpcCpp.o

$(LIBXMLRPCPP_SHLIB): $(LIBXMLRPCPP_OBJS)
$(LIBXMLRPCPP_SHLIB): OBJECTS = $(LIBXMLRPCPP_OBJS)

$(LIBXMLRPC_SERVERPP): $(LIBXMLRPC_SERVERPP_OBJS)
$(LIBXMLRPC_SERVERPP): OBJECTS = $(LIBXMLRPC_SERVERPP_OBJS)

$(LIBXMLRPC_SERVER_ABYSSPP): $(LIBXMLRPC_SERVER_ABYSSPP_OBJS)
$(LIBXMLRPC_SERVER_ABYSSPP): OBJECTS = $(LIBXMLRPC_SERVER_ABYSSPP_OBJS)

$(LIBXMLRPC_CLIENTPP): $(LIBXMLRPC_CLIENTPP_OBJS)
$(LIBXMLRPC_CLIENTPP): OBJECTS = $(LIBXMLRPC_CLIENTPP_OBJS)

# Rules for these are in Makefile.common, courtesy of TARGET_STATIC_LIBRARIES:

libxmlrpc_cpp.a: XmlRpcCpp.o
libxmlrpc_cpp.a: OBJECTS = XmlRpcCpp.o

libxmlrpc++.a: $(LIBXMLRPCPP_OBJS)
libxmlrpc++.a: OBJECTS = $(LIBXMLRPCPP_OBJS)

libxmlrpc_server++.a: $(LIBXMLRPC_SERVERPP_OBJS)
libxmlrpc_server++.a: OBJECTS = $(LIBXMLRPC_SERVERPP_OBJS)

libxmlrpc_server_abyss++.a: $(LIBXMLRPC_SERVER_ABYSSPP_OBJS)
libxmlrpc_server_abyss++.a: OBJECTS=$(LIBXMLRPC_SERVER_ABYSSPP_OBJS)

libxmlrpc_client++.a: $(LIBXMLRPC_CLIENTPP_OBJS)
libxmlrpc_client++.a: OBJECTS = $(LIBXMLRPC_CLIENTPP_OBJS)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

CXXFLAGS = $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

XmlRpcCpp.o:%.o:%.cpp
	$(CXX) -c $(BASIC_INCLUDES) $(CXXFLAGS) $<

$(LIBXMLRPCPP_OBJS):%.o:%.cpp
	$(CXX) -c $(BASIC_INCLUDES) $(CXXFLAGS) $<

SERVER_INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES)

$(LIBXMLRPC_SERVERPP_OBJS):%.o:%.cpp
	$(CXX) -c $(SERVER_INCLUDES) $(CXXFLAGS) $<

SERVER_ABYSS_INCLUDES = $(SERVER_INCLUDES) -Isrcdir/lib/abyss/src

$(LIBXMLRPC_SERVER_ABYSSPP_OBJS):%.o:%.cpp
	$(CXX) -c $(SERVER_ABYSS_INCLUDES) $(CXXFLAGS) $<

CLIENT_INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES) $(TRANSPORT_INCLUDES)

$(LIBXMLRPC_CLIENTPP_OBJS):%.o:%.cpp
	$(CXX) -c $(CLIENT_INCLUDES) $(CXXFLAGS) $<

client.o curl.o libwww.o wininet.o: $(BLDDIR)/transport_config.h

#-----------------------------------------------------------------------------
#  MISCELLANEOUS RULES
#-----------------------------------------------------------------------------

check:
	$(MAKE) -C test runtests

.PHONY: install
install: install-common

.PHONY: clean clean-local distclean distclean-local
clean: clean-common clean-local
clean-local:
	$(MAKE) -C test clean

distclean: clean distclean-local distclean-common

distclean-local:

.PHONY: dep
dep: dep-common $(BLDDIR)/transport_config.h

include Makefile.depend
