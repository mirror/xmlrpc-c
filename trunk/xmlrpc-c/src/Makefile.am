# Declare our libraries.
lib_LTLIBRARIES = libxmlrpc.la libxmlrpc_client.la libxmlrpc_abyss_server.la \
		  libxmlrpc_cgi.la

# We always statically-link our C++ code because it's tiny, and C++ is
# no good at shared libraries, anyway.
lib_LIBRARIES = libxmlrpc_cpp.a

# Declare our programs.
noinst_PROGRAMS = rpctest cpptest clienttest servertest validatee

# Declare our headers (installed ones only!).
include_HEADERS = xmlrpc.h xmlrpc_client.h xmlrpc_abyss.h xmlrpc_cgi.h \
                  XmlRpcCpp.h

# Our test programs.
TESTS = rpctest cpptest

INCLUDES = -I.. -I$(srcdir)/../lib/expat/xmlparse -I$(srcdir)/../lib/abyss/src

LIBEXPAT = ../lib/expat/xmlparse/libxmlrpc_xmlparse.la \
           ../lib/expat/xmltok/libxmlrpc_xmltok.la
LIBABYSS = ../lib/abyss/src/libxmlrpc_abyss.la

# A library containing just the XML-RPC core.
libxmlrpc_la_SOURCES = xmlrpc_support.c xmlrpc_data.c xmlrpc_struct.c \
		       xmlrpc_expat.h xmlrpc_expat.c xmlrpc_parse.c \
                       xmlrpc_serialize.c xmlrpc_registry.c xmlrpc_base64.c
libxmlrpc_la_LDFLAGS = @VERSION_INFO@

# A library containing the client-related components.
libxmlrpc_client_la_SOURCES = xmlrpc_client.c 
libxmlrpc_client_la_LDFLAGS = @VERSION_INFO@

# A library containing the server-related components.
libxmlrpc_abyss_server_la_SOURCES = xmlrpc_abyss.c
libxmlrpc_abyss_server_la_LDFLAGS = @VERSION_INFO@

# A library supporting CGI-based servers.
libxmlrpc_cgi_la_SOURCES = xmlrpc_cgi.c
libxmlrpc_cgi_la_LDFLAGS = @VERSION_INFO@

# A library containing our C++ wrappers.
libxmlrpc_cpp_a_SOURCES = XmlRpcCpp.cc

# The test suites for the XML-RPC core.
rpctest_SOURCES = rpctest.c 
rpctest_LDADD   = libxmlrpc.la $(LIBEXPAT)

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
