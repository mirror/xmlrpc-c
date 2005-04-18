# Since the programs in this directories are examples for the user, this
# make file should be as ordinary as possible.  It should not rely heavily
# on included make files or configuration parameters.  It should not use
# libtool.  Also, we don't try to build or rebuild the libraries on which
# these programs depend.


ifeq ($(SRCDIR)x,x)
SRCDIR = $(CURDIR)/../..
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


SERVERPROGS_ABYSS = \
  xmlrpc_sample_add_server

CLIENTPROGS = \
  meerkat-app-list

PROGS = $(SERVERPROGS_ABYSS) $(CLIENTPROGS)

INCLUDES = $(shell $(XMLRPC_C_CONFIG) c++ client abyss-server --cflags)

LDADD_SERVER_ABYSS = \
  $(shell $(XMLRPC_C_CONFIG) c++ abyss-server --ldadd)

LDADD_BASE = \
  $(shell $(XMLRPC_C_CONFIG) --ldadd)

all: $(PROGS)

$(SERVERPROGS_ABYSS):%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_SERVER_ABYSS)

$(CLIENTPROGS):%:%.o
	$(CXXLD) -o $@ $(LDFLAGS) $^ $(LDADD_BASE)

%.o:%.cpp
	$(CXX) -c $(INCLUDES) $(CFLAGS) $<

*.c: config.h xmlrpc_amconfig.h

config.h:
	$(LN_S) $(BUILDDIR)/xmlrpc_config.h $@
xmlrpc_amconfig.h:
	$(LN_S) $(BUILDDIR)/$@ .

include $(SRCDIR)/Makefile.common

.PHONY: clean
clean: clean-common
	rm -f $(PROGS) config.h xmlrpc_amconfig.h

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

.PHONY: dep depend
dep depend:
# We don't do dependencies in this directory, because it's supposed to be
# an example of what a program outside this package would do, so we can't
# go weaving it into the rest of the package.  Ergo, a developer must
# carefully clean and remake examples as he updates other parts of the tree.
