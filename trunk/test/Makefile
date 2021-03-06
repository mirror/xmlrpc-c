ifeq ($(SRCDIR),)
  updir = $(shell echo $(dir $(1)) | sed 's/.$$//')
  SRCDIR := $(call updir,$(CURDIR))
  BLDDIR := $(SRCDIR)
endif
SUBDIR := test

include $(BLDDIR)/config.mk

SUBDIRS = cpp

XMLRPC_C_CONFIG = $(BLDDIR)/xmlrpc-c-config.test

LDADD_CGI_SERVER = \
  $(shell $(XMLRPC_C_CONFIG) cgi-server --ldadd)

default: all

INCLUDES = -I$(BLDDIR) -Isrcdir/include -Isrcdir/lib/util/include \

PROGS = test cgitest1

all: $(PROGS) $(SUBDIRS:%=%/all)

TEST_OBJS = \
  testtool.o \
  test.o \
  cgi.o \
  memblock.o \
  method_registry.o \
  parse_xml.o \
  serialize.o \
  serialize_value.o \
  value.o \
  value_datetime.o \
  xml_data.o \

ifeq ($(MUST_BUILD_CLIENT),yes)
  TEST_OBJS += client.o
  LIBXMLRPC_CLIENT_DEP = $(LIBXMLRPC_CLIENT_A)
  LDADD_CLIENT = $(shell $(XMLRPC_C_CONFIG) client --ldadd)
else
  TEST_OBJS += client_dummy.o
  LIBXMLRPC_CLIENT_DEP =
  LDADD_CLIENT =
endif

ifeq ($(ENABLE_ABYSS_SERVER),yes)
  TEST_OBJS += abyss.o server_abyss.o
  LIBXMLRPC_ABYSS_DEP = $(LIBXMLRPC_ABYSS_A)
  LIBXMLRPC_SERVER_ABYSS_DEP = $(LIBXMLRPC_ABYSS_A)
  LDADD_ABYSS_SERVER = \
    $(shell $(XMLRPC_C_CONFIG) abyss-server --ldadd)
else
  TEST_OBJS += abyss_dummy.o server_abyss_dummy.o
  LIBXMLRPC_ABYSS_DEP =
  LIBXMLRPC_SERVER_ABYSS_DEP =
  LDADD_ABYSS_SERVER =
endif

include $(SRCDIR)/common.mk

# This 'common.mk' dependency makes sure the symlinks get built before
# this make file is used for anything.

$(SRCDIR)/common.mk: srcdir blddir

test: \
  $(XMLRPC_C_CONFIG) \
  $(TEST_OBJS) $(LIBXMLRPC_A) $(LIBXMLRPC_UTIL_A) \
  $(LIBXMLRPC_SERVER_A) $(LIBXMLRPC_SERVER_ABYSS_DEP) $(LIBXMLRPC_XML) \
  $(LIBXMLRPC_CLIENT_DEP) $(LIBXMLRPC_ABYSS_DEP) \
  $(LIBXMLRPC_XMLPARSE_A) $(LIBXMLRPC_XMLTOK_A) \
  $(CASPRINTF)
	$(CCLD) -o $@ $(LDFLAGS_ALL) \
	    $(TEST_OBJS) $(LDADD_CLIENT) $(LDADD_ABYSS_SERVER) $(CASPRINTF)

CGITEST1_OBJS = cgitest1.o testtool.o

cgitest1: $(CGITEST1_OBJS) $(LIBXMLRPC_SERVER_A) $(LIBXMLRPC_SERVER_CGI_A) \
  $(LIBXMLRPC_A) $(LIBXMLRPC_UTIL_A) $(LIBXMLRPC_XML)
	$(CCLD) -o $@ $(CGITEST1_OBJS) $(LDFLAGS_ALL) $(LDADD_CGI_SERVER)

OBJS = $(TEST_OBJS) cgitest1.o

$(OBJS):%.o:%.c
	$(CC) -c $(INCLUDES) $(CFLAGS_ALL) $<

# Note the difference between 'check' and 'runtests'.  'check' means to check
# our own correctness.  'runtests' means to run the tests that check our
# parent's correctness

.PHONY: check
check:

.PHONY: runtests_local
runtests_local: test cgitest1
	./test

.PHONY: runtests
runtests: runtests_local cpp/runtests

cpp/runtests: FORCE
	$(MAKE) -C $(dir $@) $(notdir $@)

.PHONY: install
install:

.PHONY: uninstall
uninstall:

.PHONY: clean clean-local distclean
clean: clean-common clean-local
clean-local: $(SUBDIRS:%=%/clean) 
	rm -f $(PROGS)

distclean: clean $(SUBDIRS:%=%/distclean) distclean-common

.PHONY: dep
dep: dep-common $(SUBDIRS:%=%/dep)

include depend.mk
