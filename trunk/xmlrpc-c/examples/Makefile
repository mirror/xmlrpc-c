# We need the following make variables to be defined by Makefile.config
# or be passed in:
#
# LIBWWW_CFLAGS, LIBWWW_LDADD, LIBWWW_LIBDIR, LIBWWW_RPATH, LIBWWW_WL_PATH:
#   As set by the 'libwww-config' program.  These tell how libwww is installed
#   on this system.

ifeq ($(SRCDIR)x,x)
SRCDIR = ..
endif

include ../Makefile.common

MKINSTALLDIRS = $(SRCDIR)/mkinstalldirs
LIBTOOL = $(SRCDIR)/libtool
SYMLINK = ln -s

CXXFLAGS = 
CXXCOMPILE = $(CXX) $(INCLUDES) $(CPPFLAGS) $(CXXFLAGS)
LTCXXCOMPILE = $(LIBTOOL) --mode=compile $(CXX) $(INCLUDES) \
  $(CPPFLAGS) $(CXXFLAGS)
CXXLD = $(CXX)
CXXLINK = $(LIBTOOL) --mode=link $(CXXLD)
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS)$(CFLAGS)
LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(LIBTOOL) --mode=link $(CCLD)

CFLAGS = $(CFLAGS_COMMON) $(LIBWWW_CFLAGS) $(CFLAGS_PERSONAL) $(CADD)
LDFLAGS = $(LADD)


CLIENTPROGS_C = \
  synch_client \
  asynch_client \
  auth_client \
  xmlrpc_sample_add_client \
  query-meerkat \

CLIENTPROGS_CC = \
  meerkat-app-list \
  interop-client \

CGICLIENTPROGS = \
  sample-cgi.cgi \
  interop-cgi.cgi \

SERVERPROGS = \
  xmlrpc_sample_add_server \
  xmlrpc_server_validatee

PROGS = $(CLIENTPROGS_C) $(CLIENTPROGS_CC) $(CGICLIENTPROGS) $(SERVERPROGS)

INCLUDES = -I.. -I../src

ifneq ($(ENABLE_LIBXML2)x,x)
  LIBXML = $(LIBXML2_LIBS)
else
  LIBXML = ../lib/expat/xmlparse/libxmlrpc_xmlparse.la \
           ../lib/expat/xmltok/libxmlrpc_xmltok.la
endif

LIBABYSS = ../lib/abyss/src/libxmlrpc_abyss.la
LIBXMLRPC = ../src/libxmlrpc.la $(LIBXML)
LIBXMLRPC_CPP = ../src/libxmlrpc_cpp.a
LIBCLIENT = ../src/libxmlrpc_client.la ../src/libxmlrpc_cpp.a \
	    $(LIBWWW_LDADD) $(LIBXMLRPC)
LIBSERVER = ../src/libxmlrpc_abyss_server.la $(LIBABYSS) $(LIBXMLRPC)
LIBCGI = -static ../src/libxmlrpc_cgi.la $(LIBXMLRPC)

all: $(PROGS)

$(CLIENTPROGS_C):%:%.o
	$(LINK) -o $@ $(LDFLAGS) $(LIBCLIENT) $^

$(CLIENTPROGS_CC):%:%.o
	$(CXXLINK) -o $@ $(LDFLAGS) $^ $(LIBXMLRPC_CPP) $(LIBCLIENT)

$(CGICLIENTPROGS):%.cgi:%.o
	$(LINK) -o $@ $(LDFLAGS) $^ $(LIBCGI)

$(SERVERPROGS):%:%.o
	$(LINK) -o $@ $(LDFLAGS) $(LIBSERVER) $^


%.o:%.c
	$(COMPILE) -c $(CFLAGS) $<

%.o:%.cc
	$(CXXCOMPILE) -c $(CFLAGS) $<

*.c: config.h

config.h:
	$(SYMLINK) $(SRCDIR)/xmlrpc_config.h $@

include Makefile.depend

.PHONY: dep
dep:
# This rule is meant to be used by a developer.  The shipped version of
# Makefile.depend is empty.  Only a developer would modify a dependency
# and rebuild.  We don't want non-developer users automatically generating
# dependency files, because is too complicated -- there's too much opportunity
# for something to fail and the fix not to be obvious.
	gcc -MM $(CFLAGS) >Makefile.depend

.PHONY: clean
clean:
	rm -f $(PROGS) *.o *.i *.s CORE config.h

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
