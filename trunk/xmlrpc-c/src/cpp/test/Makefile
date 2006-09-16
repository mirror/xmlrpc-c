ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCCPPDIR := $(call updir,$(CURDIR))
  srcDIR := $(call updir,$(SRCCPPDIR))
  SRCDIR := $(call updir,$(srcDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := src/test/cpp

include $(BLDDIR)/Makefile.config

PROGS = test

default: all

all: $(PROGS)

XMLRPC_C_CONFIG = $(BLDDIR)/xmlrpc-c-config.test

CXXFLAGS = $(CXXFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

LDFLAGS += $(shell $(XMLRPC_C_CONFIG) client --ldadd)

ifeq ($(MUST_BUILD_CURL_CLIENT),yes)
  LDFLAGS += $(shell curl-config --libs)
endif
ifeq ($(MUST_BUILD_LIBWWW_CLIENT),yes)
  LDFLAGS += $(shell libwww-config --libs)
endif

LDFLAGS += "-lpthread"

LDFLAGS += $(LADD)

INCLUDES = -Isrcdir/include -I$(BLDDIR) -Isrcdir -Isrcdir/lib/util/include

# This 'Makefile' dependency makes sure the symlinks get built before
# this make file is used for anything.

Makefile: srcdir

include $(SRCDIR)/Makefile.common


TEST_OBJS = test.o server_abyss.o tools.o

ifeq ($(MUST_BUILD_CLIENT),yes)
  TEST_OBJS += testclient.o
  CLIENT_LIBS = $(LIBXMLRPC_CLIENT++) $(LIBXMLRPC_CLIENT_A)
else
  TEST_OBJS += testclient_dummy.o
  CLIENT_LIBS =
endif


test:$(TEST_OBJS) $(LIBXMLRPC_SERVER_ABYSS++) $(LIBXMLRPC_SERVER++) \
  $(CLIENT_LIBS) $(LIBXMLRPC++) $(LIBXMLRPC_CPP) \
  $(LIBXMLRPC_SERVER_ABYSS_A) $(LIBXMLRPC_SERVER_A) \
  $(LIBXMLRPC_A) $(LIBXMLRPC_ABYSS_A) $(LIBXMLRPC_XML) $(LIBXMLRPC_UTIL_A) 
	$(LIBTOOL) --mode=link $(CXXLD) -o $@ $(LDFLAGS) $^

%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CXXFLAGS) $<

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

.PHONY: clean clean-local distclean
clean: clean-common clean-local
clean-local:
	rm -f $(PROGS)

distclean: clean distclean-common

.PHONY: dep
dep: dep-common

include Makefile.depend
