SUBDIRS = testdata

# Turn on warnings, if asked to do so.
CFLAGS = @CFLAGS@ @CC_WARN_FLAGS@
CPPFLAGS = @CPPFLAGS@ @CPP_WARN_FLAGS@

# Declare our libraries.
lib_LTLIBRARIES   = libxmlrpc.la \
                    @LIBXMLRPC_CLIENT_LA@ \
                    @LIBXMLRPC_ABYSS_SERVER_LA@ \
		    @LIBXMLRPC_CGI_LA@
EXTRA_LTLIBRARIES = libxmlrpc_client.la \
		    libxmlrpc_abyss_server.la \
		    libxmlrpc_cgi.la

# We always statically-link our C++ code because it's tiny, and C++ is
# no good at shared libraries, anyway.
lib_LIBRARIES   = @LIBXMLRPC_CPP_A@
EXTRA_LIBRARIES = libxmlrpc_cpp.a

# Declare our programs.
noinst_PROGRAMS = rpctest @EFRPCTEST@ @CPPTEST@ \
                  @CLIENTTEST@ @SERVERTEST@ @VALIDATEE@
EXTRA_PROGRAMS  = cpptest efrpctest clienttest servertest validatee

# Declare our headers (installed ones only!).
include_HEADERS = xmlrpc.h @XMLRPC_CLIENT_H@ @XMLRPC_ABYSS_H@ @XMLRPC_CGI_H@ \
                  @XMLRPCCPP_H@
EXTRA_HEADERS = xmlrpc_client.h xmlrpc_abyss.h xmlrpc_cgi.h XmlRpcCpp.h

# Our scripts.
noinst_SCRIPTS = efrpctest_wrapper

# Our test programs.
TESTS = rpctest @EFRPCTEST_WRAPPER@ @CPPTEST@

INCLUDES = -I.. -I$(srcdir)/../lib/expat/xmlparse -I$(srcdir)/../lib/abyss/src

LIBEXPAT = ../lib/expat/xmlparse/libxmlrpc_xmlparse.la \
           ../lib/expat/xmltok/libxmlrpc_xmltok.la
LIBABYSS = ../lib/abyss/src/libxmlrpc_abyss.la @LIBABYSS_LDADD@

# A library containing just the XML-RPC core.
libxmlrpc_la_SOURCES = xmlrpc_support.c xmlrpc_data.c xmlrpc_struct.c \
		       xmlrpc_xmlparser.h xmlrpc_expat.c xmlrpc_parse.c \
                       xmlrpc_serialize.c xmlrpc_registry.c xmlrpc_base64.c \
		       xmlrpc_utf8.c
libxmlrpc_la_LDFLAGS = @VERSION_INFO@

# A library containing the client-related components.
libxmlrpc_client_la_SOURCES = xmlrpc_client.c 
libxmlrpc_client_la_LDFLAGS = @VERSION_INFO@ -rpath $(libdir)

# A library containing the server-related components.
libxmlrpc_abyss_server_la_SOURCES = xmlrpc_abyss.c
libxmlrpc_abyss_server_la_LDFLAGS = @VERSION_INFO@ -rpath $(libdir)

# A library supporting CGI-based servers.
libxmlrpc_cgi_la_SOURCES = xmlrpc_cgi.c
libxmlrpc_cgi_la_LDFLAGS = @VERSION_INFO@ -rpath $(libdir)

# A library containing our C++ wrappers.
libxmlrpc_cpp_a_SOURCES = XmlRpcCpp.cc

# The test suites for the XML-RPC core.
rpctest_SOURCES = rpctest.c 
rpctest_LDADD   = libxmlrpc.la $(LIBEXPAT)

# The test suites for the XML-RPC core w/Electric Fence.
efrpctest_SOURCES = rpctest.c 
efrpctest_LDADD   = libxmlrpc.la $(LIBEXPAT) -lefence

# The test suites for our C++ core.
cpptest_SOURCES = cpptest.cc XmlRpcCpp.cc XmlRpcCpp.h
cpptest_LDADD   = libxmlrpc_cpp.a libxmlrpc.la $(LIBEXPAT)

# A test program for the client library.
clienttest_SOURCES = clienttest.c 
clienttest_LDADD   = libxmlrpc_client.la @LIBWWW_LDADD@ \
                     libxmlrpc.la $(LIBEXPAT)
clienttest_LDFLAGS = @LIBWWW_RPATH@

# A test program for the server library.
servertest_SOURCES = servertest.c 
servertest_LDADD   = libxmlrpc_abyss_server.la $(LIBABYSS) \
                     libxmlrpc.la $(LIBEXPAT)

# A server which attempts to pass the UserLand validator1 suite.
validatee_SOURCES  = validatee.c
validatee_LDADD    = libxmlrpc_abyss_server.la $(LIBABYSS) \
                     libxmlrpc.la $(LIBEXPAT)

# Handle our wrapper script.
efrpctest_wrapper: $(srcdir)/efrpctest_wrapper.sh
	cp $(srcdir)/efrpctest_wrapper.sh $@
	chmod +x $@
EXTRA_DIST = efrpctest_wrapper.sh
MOSTLYCLEANFILES = efrpctest_wrapper
