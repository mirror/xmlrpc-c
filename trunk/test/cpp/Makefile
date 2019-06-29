ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  testDIR := $(call updir,$(CURDIR))
  SRCDIR := $(call updir,$(testDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := test/cpp

include $(BLDDIR)/config.mk

PROGS = test

default: all

all: $(PROGS)

XMLRPC_C_CONFIG = $(BLDDIR)/xmlrpc-c-config.test

LIBS := 

LIBS += $(SOCKETLIBOPT) $(THREAD_LIBS)

INCLUDES = -Isrcdir/include -I$(BLDDIR) -Isrcdir -Isrcdir/lib/util/include

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

include $(SRCDIR)/common.mk

TEST_OBJS = \
  test.o \
  base64.o \
  registry.o \
  server_pstream.o \
  tools.o \
  value.o \
  xml.o \

ifeq ($(MUST_BUILD_CLIENT),yes)
  TEST_OBJS += testclient.o
  LIBS += $(shell $(XMLRPC_C_CONFIG) client --ldadd)
else
  TEST_OBJS += testclient_dummy.o
endif

ifeq ($(ENABLE_ABYSS_SERVER),yes)
  TEST_OBJS += abyss.o server_abyss.o
  LIBS += $(shell $(XMLRPC_C_CONFIG) abyss-server --ldadd)
else
  TEST_OBJS += abyss_dummy.o server_abyss_dummy.o
endif

#-----------------------------------------------------------------------------
#  TEST_LIBS
#-----------------------------------------------------------------------------
TEST_LIBS =
ifeq ($(ENABLE_ABYSS_SERVER),yes)
  TEST_LIBS += $(LIBXMLRPC_SERVER_ABYSSPP_A)
endif
TEST_LIBS += $(LIBXMLRPC_SERVER_PSTREAMPP_A)
TEST_LIBS += $(LIBXMLRPC_SERVERPP_A)
ifeq ($(MUST_BUILD_CLIENT),yes)
  TEST_LIBS +=  $(LIBXMLRPC_CLIENTPP_A) $(LIBXMLRPC_CLIENT_A)
endif
TEST_LIBS += $(LIBXMLRPCPP_A)
TEST_LIBS += $(LIBXMLRPC_CPP_A)
ifeq ($(ENABLE_ABYSS_SERVER),yes)
  TEST_LIBS +=   $(LIBXMLRPC_SERVER_ABYSS_A)
endif
TEST_LIBS += $(LIBXMLRPC_SERVER_A)
TEST_LIBS +=  $(LIBXMLRPC_A)
ifeq ($(ENABLE_ABYSS_SERVER),yes)
  TEST_LIBS += $(LIBXMLRPC_ABYSS_A)
  TEST_LIBS += $(LIBXMLRPC_ABYSSPP_A)
endif
TEST_LIBS += $(LIBXMLRPC_PACKETSOCKET_A)
TEST_LIBS += $(LIBXMLRPC_UTILPP_A)
TEST_LIBS += $(LIBXMLRPC_UTIL_A)
ifeq ($(MSVCRT),yes)
  TEST_LIBS += socketpair.o
endif
ifneq ($(ENABLE_LIBXML2_BACKEND),yes)
  # We're using the internal Expat XML parser
  TEST_LIBS += $(LIBXMLRPC_XMLPARSE_A)
  TEST_LIBS += $(LIBXMLRPC_XMLTOK_A)
endif

ifneq ($(ENABLE_LIBXML2_BACKEND),yes)
  # We're using the internal Expat XML parser
  LIB_XML =
  registry.o: D_INTERNAL_EXPAT=-DINTERNAL_EXPAT
else
  LIB_XML = $(shell xml2-config --libs)
endif

test: $(TEST_OBJS) $(TEST_LIBS)
	$(CXXLD) -o $@ $(LDFLAGS_ALL) $^ $(LIB_XML) $(LIBS)

%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS_ALL) $(D_INTERNAL_EXPAT) $<

socketpair.o: $(SRCDIR)/Windows/socketpair.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS_ALL) $(D_INTERNAL_EXPAT) $<

# Note the difference between 'check' and 'runtests'.  'check' means to check
# our own correctness.  'runtests' means to run the tests that check our
# parent's correctness

.PHONY: check
check:

.PHONY: runtests
runtests: test
	./test

.PHONY: install
install:

.PHONY: uninstall
uninstall:

.PHONY: clean clean-local distclean
clean: clean-common clean-local
clean-local:
	rm -f $(PROGS)

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

include depend.mk
