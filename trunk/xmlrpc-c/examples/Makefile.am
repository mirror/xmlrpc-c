SUBDIRS = @INTEROP_CLIENT_SUBDIR@

# Turn on warnings, if asked to do so.
CFLAGS = @CFLAGS@ @CC_WARN_FLAGS@
CPPFLAGS = @CPPFLAGS@ @CPP_WARN_FLAGS@

noinst_PROGRAMS = @SYNCH_CLIENT@ @ASYNCH_CLIENT@ @AUTH_CLIENT@ \
                  @SERVER@ @SAMPLE_CGI_CGI@ @INTEROP_CGI@
bin_PROGRAMS    = @QUERY_MEERKAT@ @MEERKAT_APP_LIST@
EXTRA_PROGRAMS  = synch_client asynch_client auth_client \
                  server sample-cgi.cgi interop.cgi \
		  query-meerkat meerkat-app-list

INCLUDES = -I$(srcdir)/.. -I$(srcdir)/../src

LIBEXPAT  = ../lib/expat/xmlparse/libxmlrpc_xmlparse.la \
            ../lib/expat/xmltok/libxmlrpc_xmltok.la
LIBABYSS  = ../lib/abyss/src/libxmlrpc_abyss.la
LIBXMLRPC = ../src/libxmlrpc.la $(LIBEXPAT)
LIBCLIENT = ../src/libxmlrpc_client.la @LIBWWW_LDADD@ $(LIBXMLRPC)
LIBSERVER = ../src/libxmlrpc_abyss_server.la $(LIBABYSS) $(LIBXMLRPC)
LIBCGI    = ../src/libxmlrpc_cgi.la $(LIBXMLRPC)

synch_client_SOURCES     = synch_client.c
synch_client_LDADD       = $(LIBCLIENT)
synch_client_LDFLAGS     = @LIBWWW_RPATH@

asynch_client_SOURCES    = asynch_client.c
asynch_client_LDADD      = $(LIBCLIENT)
asynch_client_LDFLAGS    = @LIBWWW_RPATH@

server_SOURCES           = server.c
server_LDADD             = $(LIBSERVER)

sample_cgi_cgi_SOURCES   = sample-cgi.c
sample_cgi_cgi_LDADD     = $(LIBCGI)
sample_cgi_cgi_LDFLAGS   = -static

interop_cgi_SOURCES   = interop-cgi.c
interop_cgi_LDADD     = $(LIBCGI)
interop_cgi_LDFLAGS   = -static

query_meerkat_SOURCES    = query-meerkat.c
query_meerkat_LDADD      = $(LIBCLIENT)
query_meerkat_LDFLAGS    = @LIBWWW_RPATH@

meerkat_app_list_SOURCES = meerkat-app-list.cc
meerkat_app_list_LDADD   = ../src/libxmlrpc_cpp.a $(LIBCLIENT)
meerkat_app_list_LDFLAGS = @LIBWWW_RPATH@

auth_client_SOURCES      = auth_client.c
auth_client_LDADD        = $(LIBCLIENT)
auth_client_LDFLAGS      = @LIBWWW_RPATH@
