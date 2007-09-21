ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
endif
SUBDIR = src/test
BUILDDIR = $(SRCDIR)
VPATH = .:$(SRCDIR)

include $(BUILDDIR)/Makefile.config

XMLRPC_C_CONFIG = $(BUILDDIR)/xmlrpc-c-config.test

LIBXMLRPC              = ../.libs/libxmlrpc.a
LIBXMLRPC_SERVER       = ../.libs/libxmlrpc_server.a
LIBXMLRPC_SERVER_ABYSS = ../.libs/libxmlrpc_server_abyss.a
LIBXMLRPC_SERVER_CGI   = ../.libs/libxmlrpc_server_cgi.a
LIBXMLRPC_CLIENT       = ../.libs/libxmlrpc_client.a

LDADD_CLIENT = \
  $(shell $(XMLRPC_C_CONFIG) client --ldadd)
LDADD_SERVER = \
  $(shell $(XMLRPC_C_CONFIG) abyss-server --ldadd)

ifeq ($(ENABLE_LIBXML2_BACKEND),yes)
  LIBXML = $(LIBXML2_LIBS)
else
  LIBXML = $(BUILDDIR)/lib/expat/xmlparse/.libs/libxmlrpc_xmlparse.a \
           $(BUILDDIR)/lib/expat/xmltok/.libs/libxmlrpc_xmltok.a
endif

LIBXMLRPC_ABYSS = $(BUILDDIR)/lib/abyss/src/.libs/libxmlrpc_abyss.a
LIBXMLRPC_UTIL  = $(BUILDDIR)/lib/libutil/.libs/libxmlrpc_util.a

default: all

CASPRINTF_OBJ = $(BUILDDIR)/lib/util/casprintf.lo

INCLUDES = -I$(BUILDDIR) -I$(SRCDIR) \
           -I $(SRCDIR)/include -I$(SRCDIR)/lib/util/include \

PROGS = test cgitest1

ifeq ($(ENABLE_EFENCE),yes)
  PROGS += eftest eftest_wrapper
endif

all: $(PROGS)

LDFLAGS = $(LADD)

TEST_OBJS = \
  test.o value.o serialize.o parse_xml.o cgi.o server_abyss.o \
  xml_data.o

ifeq ($(MUST_BUILD_CLIENT),yes)
  TEST_OBJS += client.o
  LIBXMLRPC_CLIENT_DEP = $(LIBXMLRPC_CLIENT)
else
  TEST_OBJS += client_dummy.o
  LIBXMLRPC_CLIENT_DEP =
endif

test: $(TEST_OBJS) $(LIBXMLRPC) $(LIBXMLRPC_UTIL) \
  $(LIBXMLRPC_SERVER) $(LIBXMLRPC_SERVER_ABYSS) $(LIBXML) \
   $(LIBXMLRPC_CLIENT_DEP) $(LIBXMLRPC_ABYSS) $(CASPRINTF_OBJ)
	$(CCLD) -o $@ $(LDFLAGS) \
	    $(TEST_OBJS) $(LDADD_CLIENT) $(LDADD_SERVER) $(CASPRINTF_OBJ)

cgitest1:%:%.o $(LIBXMLRPC_SERVER) $(LIBXMLRPC_SERVER_CGI) $(LIBXMLRPC) \
  $(LIBXMLRPC_UTIL) $(LIBXML)
	$(CCLD) -o $@ $(LDFLAGS) $^

eftest:test.o $(LIBXMLRPC) $(LIBXMLRPC_UTIL) $(LIBXML) $(LIBXMLRPC_SERVER)
	$(CCLD) -o $@ $(LDFLAGS) $^ -lefence

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)

OBJS = $(TEST_OBJS) cgitest1.o

$(OBJS):%.o:%.c
	$(CC) -c $(INCLUDES) $(CFLAGS) $<

eftest_wrapper: eftest_wrapper.sh
	cp $< $@
	chmod +x $@

# Note the difference between 'check' and 'runtests'.  'check' means to check
# our own correctness.  'runtests' means to run the tests that check our
# parent's correctness

.PHONY: check
check:

.PHONY: runtests
runtests: test cgitest1
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

include $(SRCDIR)/Makefile.common

include Makefile.depend
