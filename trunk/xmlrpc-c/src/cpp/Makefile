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

LIBXMLRPCPP_MODS = \
  base64 env_wrap fault girerr girmem outcome param_list value xml
LIBXMLRPC_SERVERPP_MODS = registry
LIBXMLRPC_SERVER_ABYSSPP_MODS = server_abyss
LIBXMLRPC_CLIENTPP_MODS = client client_simple curl libwww wininet

TARGET_MODS_PP = \
  XmlRpcCpp \
  $(LIBXMLRPCPP_MODS) \
  $(LIBXMLRPC_SERVERPP_MODS) \
  $(LIBXMLRPC_SERVER_ABYSSPP_MODS) \
  $(LIBXMLRPC_CLIENTPP_MODS)

OMIT_CPP_LIB_RULES = Y

include $(SRCDIR)/Makefile.common


# This 'Makefile.common' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/Makefile.common: srcdir blddir

TRANSPORT_INCLUDES = \
  -I$(WININET_TRANSPORT_DIR) \
  -I$(CURL_TRANSPORT_DIR) \
  -I$(LIBWWW_TRANSPORT_DIR) \

BASIC_INCLUDES = \
  -Iblddir/include \
  -Isrcdir/include \
  -Iblddir \
  -Isrcdir/lib/util/include


# LIBXMLRPC_CPP is e.g. "libxmlrpc_cpp.so.3.1"

LIBXMLRPC_CPP            = $(call shlibfn, libxmlrpc_cpp)
LIBXMLRPCPP              = $(call shlibfn, libxmlrpc++)
LIBXMLRPC_SERVERPP       = $(call shlibfn, libxmlrpc_server++)
LIBXMLRPC_SERVER_ABYSSPP = $(call shlibfn, libxmlrpc_server_abyss++)
LIBXMLRPC_CLIENTPP       = $(call shlibfn, libxmlrpc_client++)


all: \
  $(TARGET_STATIC_LIBRARIES) \
  $(TARGET_SHARED_LIBS_PP) \
  $(TARGET_SHARED_LE_LIBS)


#-----------------------------------------------------------------------------
#  RULES TO LINK LIBRARIES
#-----------------------------------------------------------------------------

# Rules for these are in Makefile.common, courtesy of TARGET_SHARED_LIBS_PP:

$(LIBXMLRPC_CPP): XmlRpcCpp.o
$(LIBXMLRPC_CPP): LIBOBJECTS = XmlRpcCpp.o

$(LIBXMLRPCPP): $(LIBXMLRPCPP_MODS:%=%.osh)
$(LIBXMLRPCPP): LIBOBJECTS = $(LIBXMLRPCPP_MODS:%=%.osh)

$(LIBXMLRPC_SERVERPP): $(LIBXMLRPC_SERVERPP_MODS:%=%.osh )
$(LIBXMLRPC_SERVERPP): LIBOBJECTS = $(LIBXMLRPC_SERVERPP_MODS:%=%.osh)

$(LIBXMLRPC_SERVER_ABYSSPP): $(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.osh)
$(LIBXMLRPC_SERVER_ABYSSPP): LIBOBJECTS = $(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.osh)

$(LIBXMLRPC_CLIENTPP): $(LIBXMLRPC_CLIENTPP_MODS:%=%.osh)
$(LIBXMLRPC_CLIENTPP): LIBOBJECTS = $(LIBXMLRPC_CLIENTPP_MODS:%=%.osh)

# Rules for these are in Makefile.common, courtesy of TARGET_STATIC_LIBRARIES:

libxmlrpc_cpp.a: XmlRpcCpp.o
libxmlrpc_cpp.a: LIBOBJECTS = XmlRpcCpp.o

libxmlrpc++.a: $(LIBXMLRPCPP_MODS:%=%.o)
libxmlrpc++.a: LIBOBJECTS = $(LIBXMLRPCPP_MODS:%=%.o)

libxmlrpc_server++.a: $(LIBXMLRPC_SERVERPP_MODS:%=%.o)
libxmlrpc_server++.a: LIBOBJECTS = $(LIBXMLRPC_SERVERPP_MODS:%=%.o)

libxmlrpc_server_abyss++.a: $(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.o)
libxmlrpc_server_abyss++.a: LIBOBJECTS=$(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.o)

libxmlrpc_client++.a: $(LIBXMLRPC_CLIENTPP_MODS:%=%.o)
libxmlrpc_client++.a: LIBOBJECTS = $(LIBXMLRPC_CLIENTPP_MODS:%=%.o)

#-----------------------------------------------------------------------------
#  RULES TO COMPILE OBJECT MODULES FOR LIBRARIES
#-----------------------------------------------------------------------------

# Rules for these are in Makefile.common, courtesy of TARGET_MODS_PP:

XmlRpcCpp.o% XmlrpcCpp.osh: \
  INCLUDES = $(BASIC_INCLUDES)

$(LIBXMLRPCPP_MODS:%=%.o) \
$(LIBXMLRPCPP_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES)

SERVER_INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES)

$(LIBXMLRPC_SERVERPP_MODS:%=%.o) \
$(LIBXMLRPC_SERVERPP_MODS:%=%.osh): \
  INCLUDES = $(SERVER_INCLUDES)

$(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.o) \
$(LIBXMLRPC_SERVER_ABYSSPP_MODS:%=%.o): \
  INCLUDES = $(SERVER_INCLUDES) -Isrcdir/lib/abyss/src

$(LIBXMLRPC_CLIENTPP_MODS:%=%.o) \
$(LIBXMLRPC_CLIENTPP_MODS:%=%.osh): \
  INCLUDES = $(BASIC_INCLUDES) $(LIBXML_INCLUDES) $(TRANSPORT_INCLUDES)

CXXFLAGS = $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

TRANSPORT_CONFIG_USERS = client curl libwww wininet

$(TRANSPORT_CONFIG_USERS:%=%.o) $(TRANSPORT_CONFIG_USERS:%=%.osh): \
  $(BLDDIR)/transport_config.h

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
