# Since the programs in this directories are examples for the user, this
# make file should be as ordinary as possible.  It should not rely heavily
# on included make files or configuration parameters.  It should not use
# libtool.  Also, we don't try to build or rebuild the libraries on which
# these programs depend.


ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/..
BUILDDIR = $(SRCDIR)
endif

default: all

include $(BUILDDIR)/Makefile.config

CXXFLAGS = 

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = $(LADD)

# If this were a real application, working from an installed copy of
# Xmlrpc-c, XMLRPC_C_CONFIG would just be 'xmlrpc-c-config'.  It would be
# found in the user's PATH.
XMLRPC_C_CONFIG = $(BUILDDIR)/xmlrpc-c-config.test

CLIENTPROGS_C = \
  auth_client \
  query-meerkat \
  synch_client \
  xmlrpc_sample_add_client \
  xmlrpc_asynch_client \

CLIENTPROGS_CPP = \
  meerkat-app-list \
  interop-client \

SERVERPROGS_CGI = \
  sample-cgi.cgi \
  interop-cgi.cgi \

SERVERPROGS_ABYSS = \
  xmlrpc_loop_server \
  xmlrpc_sample_add_server \
  xmlrpc_server_validatee \

PROGS = $(CLIENTPROGS_C) $(SERVERPROGS_ABYSS) \
  gen_sample_add_xml

ifeq ($(ENABLE_CPLUSPLUS),yes)
  PROGS += $(CLIENTPROGS_CPP) 
endif
ifeq ($(ENABLE_CGI_SERVER),yes)
  PROGS += $(SERVERPROGS_CGI) 
endif

INCLUDES = -I$(SRCDIR) -I$(SRCDIR)/src -I$(BUILDDIR)/lib/abyss/src

LDADD_CLIENT = \
  $(shell $(XMLRPC_C_CONFIG) client --ldadd)

LDADD_CLIENT_CPP = \
  $(shell $(XMLRPC_C_CONFIG) c++ client --ldadd)

LDADD_SERVER_ABYSS = \
  $(shell $(XMLRPC_C_CONFIG) abyss-server --ldadd)

LDADD_SERVER_CGI = \
  $(shell $(XMLRPC_C_CONFIG) cgi-server --ldadd)

LDADD_BASE = \
  $(shell $(XMLRPC_C_CONFIG) --ldadd)

all: $(PROGS)

$(CLIENTPROGS_C):%:%.o
	$(CCLD) -o $@ $(LDFLAGS) $^ $(LDADD_CLIENT)

$(CLIENTPROGS_CPP):%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_CLIENT_CPP)

$(SERVERPROGS_CGI):%.cgi:%.o
	$(CCLD) -o $@ $(LDFLAGS) $^ $(LDADD_SERVER_CGI)

$(SERVERPROGS_ABYSS):%:%.o
	$(CCLD) -o $@ $(LDFLAGS) $^ $(LDADD_SERVER_ABYSS)

gen_sample_add_xml:%:%.o
	$(CCLD) -o $@ $(LDFLAGS) $^ $(LDADD_BASE)

%.o:%.c
	$(CC) -c $(INCLUDES) $(CFLAGS) $<

%.o:%.cc
	$(CXX) -c $(INCLUDES) $(CFLAGS) $<

*.c: config.h

config.h:
	$(LN_S) $(BUILDDIR)/xmlrpc_config.h $@

include $(SRCDIR)/Makefile.common

.PHONY: clean
clean: clean-common
	rm -f $(PROGS) config.h

.PHONY: distclean
distclean: clean

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
